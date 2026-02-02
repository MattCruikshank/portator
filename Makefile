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

bin/portator.o: main.c | bin
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

bin/portator: bin/portator.o $(BLINK_A) $(ZLIB_A)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -rf bin
