CC = cosmocc
AR = cosmoar
BLINK_DIR = blink
TCC_DIR = tcc
VERSION = $(shell git describe --tags --always 2>/dev/null || echo "0.0.0-dev")

# Blink's default mode uses o// (empty MODE)
BLINK_A = $(BLINK_DIR)/o//blink/blink.a
ZLIB_A = $(BLINK_DIR)/o//third_party/libz/zlib.a

CFLAGS = -O2 -fno-common -pthread -fcf-protection=none -Wno-cast-align
CPPFLAGS = -D_FILE_OFFSET_BITS=64 -D_DARWIN_C_SOURCE -D_DEFAULT_SOURCE \
           -D_BSD_SOURCE -D_GNU_SOURCE -iquote$(BLINK_DIR) -isystem $(BLINK_DIR)/third_party/libz
LDFLAGS = -pthread
LDLIBS = -lrt -lm

# TCC compile flags (builds TCC itself as a static x86-64 ELF guest)
TCC_DEFINES = -DONE_SOURCE=1 -DTCC_TARGET_X86_64 \
  -DCONFIG_TCCDIR='"zip/apps/tcc"' \
  -DCONFIG_TCC_CRTPREFIX='"zip/apps/tcc/musl-lib"' \
  '-DCONFIG_TCC_SYSINCLUDEPATHS="zip/apps/tcc/tcc-include:zip/apps/tcc/musl-include"' \
  '-DCONFIG_TCC_LIBPATHS="zip/apps/tcc/musl-lib:zip/apps/tcc/tcc-lib"' \
  '-DCONFIG_TCC_SWITCHES="-static"'

# Guest apps live under guests/
GUEST_DIR = guests

# Guest apps to build (guests/<name>/<name>.c)
APPS = snake list new license mojozork shell ls init

# Go guest apps (guests/<name>/<name>.go)
GO_APPS = hello-go

.PHONY: all clean clean-portator portator apps tcc package publish

# Default: build everything
all: clean-portator portator apps tcc package publish

# Remove just the portator binary (keeps .o files)
clean-portator:
	rm -f bin/portator

# Remove everything
clean:
	rm -rf bin
	rm -rf $(TCC_DIR)/bin $(TCC_DIR)/zip $(TCC_DIR)/c2str $(TCC_DIR)/tccdefs_.h

# Create bin directory
bin:
	mkdir -p bin

# Compile object files
bin/portator.o: main.c web_server.h | bin
	$(CC) $(CFLAGS) $(CPPFLAGS) -DPORTATOR_VERSION='"$(VERSION)"' -c -o $@ $<

bin/web_server.o: web_server.c web_server.h civetweb/civetweb.h | bin
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

bin/civetweb.o: civetweb/civetweb.c civetweb/civetweb.h | bin
	$(CC) $(CFLAGS) $(CPPFLAGS) -DUSE_WEBSOCKET -DNO_SSL -DNO_CGI -c -o $@ $<

OBJS = bin/portator.o bin/web_server.o bin/civetweb.o

# Link portator and add base resources to zip
portator: $(OBJS) $(BLINK_A) $(ZLIB_A)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o bin/portator
	mv bin/portator bin/portator.zip
	zip -qr bin/portator.zip wwwroot include src
	mv bin/portator.zip bin/portator

# Build all guest apps
apps: portator
	@for app in $(APPS); do \
	  if [ -f "$(GUEST_DIR)/$$app/$$app.c" ]; then \
	    echo "Building $$app..."; \
	    ./bin/portator build $$app || exit 1; \
	  fi; \
	done
	@for app in $(GO_APPS); do \
	  if [ -f "$(GUEST_DIR)/$$app/$$app.go" ]; then \
	    echo "Building $$app (Go)..."; \
	    mkdir -p "$(GUEST_DIR)/$$app/bin"; \
	    CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -o "$(GUEST_DIR)/$$app/bin/$$app" "./$(GUEST_DIR)/$$app/$$app.go" || exit 1; \
	  fi; \
	done

# Build TCC as a static guest binary and stage its toolchain data
tcc: portator
	@echo "Building TCC..."
	@mkdir -p $(TCC_DIR)/bin
	@cd $(TCC_DIR) && musl-gcc -DC2STR conftest.c -o c2str && ./c2str include/tccdefs.h tccdefs_.h
	@cd $(TCC_DIR) && musl-gcc -static $(TCC_DEFINES) -o bin/tcc tcc.c -lm
	@echo "Staging TCC toolchain data..."
	@mkdir -p $(TCC_DIR)/zip/musl-include $(TCC_DIR)/zip/musl-lib
	@mkdir -p $(TCC_DIR)/zip/tcc-include $(TCC_DIR)/zip/tcc-lib
	@cp -r /usr/include/x86_64-linux-musl/* $(TCC_DIR)/zip/musl-include/
	@cp /usr/lib/x86_64-linux-musl/libc.a /usr/lib/x86_64-linux-musl/crt*.o $(TCC_DIR)/zip/musl-lib/
	@cp /usr/lib/x86_64-linux-gnu/tcc/include/* $(TCC_DIR)/zip/tcc-include/
	@cp /usr/lib/x86_64-linux-gnu/tcc/libtcc1.a $(TCC_DIR)/zip/tcc-lib/
	@echo "Built TCC"

# Package app binaries and data into the zip
package:
	@rm -rf bin/apps
	@for f in $(GUEST_DIR)/*/bin/*; do \
	  if [ -f "$$f" ]; then \
	    name=$$(basename "$$f"); \
	    mkdir -p "bin/apps/$$name/bin"; \
	    cp "$$f" "bin/apps/$$name/bin/$$name"; \
	  fi; \
	done
	@# Also pick up tcc/bin/tcc (not under guests/)
	@if [ -f "$(TCC_DIR)/bin/tcc" ]; then \
	  mkdir -p "bin/apps/tcc/bin"; \
	  cp "$(TCC_DIR)/bin/tcc" "bin/apps/tcc/bin/tcc"; \
	fi
	@# Copy app data directories (<name>/zip/* -> apps/<name>/)
	@for app in $(GUEST_DIR)/*/; do \
	  app=$$(basename "$$app"); \
	  if [ -d "$(GUEST_DIR)/$$app/zip" ]; then \
	    mkdir -p "bin/apps/$$app"; \
	    cp -r "$(GUEST_DIR)/$$app/zip/"* "bin/apps/$$app/"; \
	  fi; \
	done
	@# Copy TCC toolchain data
	@if [ -d "$(TCC_DIR)/zip" ]; then \
	  mkdir -p "bin/apps/tcc"; \
	  cp -r "$(TCC_DIR)/zip/"* "bin/apps/tcc/"; \
	fi
	@# Copy legacy data paths for compatibility
	@if [ -d $(GUEST_DIR)/new/templates ]; then mkdir -p bin/apps/new && cp -r $(GUEST_DIR)/new/templates bin/apps/new/; fi
	@if [ -d $(GUEST_DIR)/mojozork/data ]; then mkdir -p bin/apps/mojozork && cp -r $(GUEST_DIR)/mojozork/data bin/apps/mojozork/; fi
	@if [ -d $(GUEST_DIR)/license/data ]; then mkdir -p bin/apps/license && cp -r $(GUEST_DIR)/license/data bin/apps/license/; fi
	@cd bin && mv portator portator.zip && zip -qr portator.zip apps && mv portator.zip portator
	@rm -rf bin/apps
	@echo "Packaged apps into bin/portator"

# Copy binary to publish folder for clean testing
publish: package
	@mkdir -p publish
	@cp bin/portator publish/
	@echo "Published to publish/"
