CC = fatcosmocc
AR = fatcosmoar
BLINK_DIR = blink

# Blink's default mode uses o// (empty MODE)
BLINK_A = $(BLINK_DIR)/o//blink/blink.a
ZLIB_A = $(BLINK_DIR)/o//third_party/libz/zlib.a

CFLAGS = -O2 -fno-common -pthread -fcf-protection=none
CPPFLAGS = -D_FILE_OFFSET_BITS=64 -D_DARWIN_C_SOURCE -D_DEFAULT_SOURCE \
           -D_BSD_SOURCE -D_GNU_SOURCE -iquote$(BLINK_DIR) -isystem $(BLINK_DIR)/third_party/libz
LDFLAGS = -pthread
LDLIBS = -lrt -lm

.PHONY: all clean

all: bin/portator

bin:
	mkdir -p bin

bin/portator.o: main.c web_server.h | bin
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

bin/web_server.o: web_server.c web_server.h mongoose.h | bin
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

bin/mongoose.o: mongoose.c mongoose.h | bin
	$(CC) $(CFLAGS) $(CPPFLAGS) -DMG_ENABLE_LINES=0 -c -o $@ $<

OBJS = bin/portator.o bin/web_server.o bin/mongoose.o

bin/portator: $(OBJS) $(BLINK_A) $(ZLIB_A)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
	zip -qr bin/portator wwwroot include src
	@rm -rf bin/apps
	@for f in */bin/*; do \
	  name=$$(basename "$$f"); \
	  mkdir -p "bin/apps/$$name/bin"; \
	  cp "$$f" "bin/apps/$$name/bin/$$name"; \
	done
	@cd bin && zip -qr portator apps
	@rm -rf bin/apps

clean:
	rm -rf bin
