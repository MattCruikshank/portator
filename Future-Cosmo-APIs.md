# Future Cosmo APIs for Portator

Cosmopolitan libc provides cross-platform implementations of many APIs that guest apps (built with musl-gcc) cannot use reliably through Blink's syscall emulation. This document tracks which cosmo capabilities are worth exposing to guests via Portator syscalls.

## The Problem

Guest apps are compiled with `musl-gcc -static`. musl's userspace implementations of complex APIs (e.g. DNS resolution via `/etc/resolv.conf` + UDP) fail on non-Linux platforms because Blink's syscall translation doesn't cover every OS-specific path. The host binary, built with cosmocc, has cosmo's genuinely cross-platform implementations that work on Linux, macOS, Windows, FreeBSD, OpenBSD, and NetBSD.

**Confirmed failure:** `httpget` guest calls musl's `getaddrinfo()` which fails on Windows with "Try again" — musl tries to read `/etc/resolv.conf` and talk UDP to nameservers, neither of which works on Windows through Blink.

## Available in cosmocc Today

Libraries shipped with cosmocc (`/cosmocc/x86_64-linux-cosmo/lib/`):

| Library | Key APIs | Notes |
|---------|----------|-------|
| `libresolv.a` | `getaddrinfo()`, `freeaddrinfo()`, `gai_strerror()`, `getnameinfo()` | Cross-platform DNS; reads Windows registry for nameservers via `getntnameservers.c` |
| `libcosmo.a` + `libc.a` | `socket()`, `connect()`, `send()`, `recv()`, `bind()`, `listen()`, `accept()` | Full cross-platform socket API |
| `libcrypt.a` | SHA-1, SHA-256, SHA-512, MD5, etc. | Hash primitives |
| `libpthread.a` | Full POSIX threads | Cross-platform threading |
| `libc.a` | `getifaddrs()`, `syslog()`, `clock_gettime()`, `dlopen()`, `pledge()`, `unveil()` | Misc cross-platform utilities |

### Not shipped with cosmocc (requires vendoring)

- **mbedTLS** — TLS/SSL. Present in full Cosmopolitan source tree under `third_party/mbedtls/` (used by redbean), but not included in the cosmocc binary distribution. Would need to be vendored and compiled with cosmocc to enable HTTPS.

## Planned Portator Syscalls

### Phase 1: DNS Resolution (unblocks httpget on Windows)

**Syscall: `PORTATOR_RESOLVE` (0x7008)**

Guest sends a hostname string; host calls cosmo's `getaddrinfo()` and returns resolved addresses in a flat serialized format.

```
Guest → Host:
  RDI = pointer to hostname string (null-terminated)
  RSI = pointer to port string (null-terminated, e.g. "80" or "443")
  RDX = pointer to output buffer
  R10 = output buffer size

Host → Guest (serialized into output buffer):
  uint32_t count;             // number of addresses
  struct {
      uint16_t family;        // AF_INET or AF_INET6
      uint16_t port;          // network byte order
      uint8_t  addr[16];      // IPv4 in first 4 bytes, or full IPv6
  } entries[];

Return value:
  >= 0: number of bytes written to buffer
  -1:   error (errno set)
  If buffer is NULL or size is 0: returns required buffer size (probe/fill pattern)
```

This avoids the complexity of marshalling `struct addrinfo` linked lists. The guest can iterate the flat array and call `socket()`/`connect()` directly — Blink's socket syscall passthrough handles the rest.

### Phase 2: HTTP(S) Fetch (host-side networking)

**Syscall: `PORTATOR_HTTP_FETCH` (0x7009)**

Performs a complete HTTP or HTTPS request on the host side — DNS, TCP, TLS handshake, and HTTP protocol all handled by cosmo. This gives guests HTTPS support without any TLS code in the guest.

```
Guest → Host:
  RDI = pointer to URL string (null-terminated, "http://..." or "https://...")
  RSI = pointer to output buffer (response body)
  RDX = output buffer size
  R10 = pointer to status_code output (uint32_t*, optional, NULL to skip)

Return value:
  >= 0: number of body bytes written
  -1:   error
  If buffer is NULL or size is 0: returns required buffer size (probe/fill pattern)
```

**Requires:** Vendoring mbedTLS and compiling with cosmocc for HTTPS support. HTTP would work immediately with cosmo's socket API.

**Open design questions:**
- Should request headers be configurable (extra syscall arg or separate header-setting syscall)?
- POST/PUT support: separate syscall or method parameter?
- Response headers: separate syscall to retrieve, or prefix them in the buffer?
- Streaming large responses: chunked callback mechanism vs. fixed buffer?

### Phase 3: Other Cosmo Conveniences

Potential future syscalls for capabilities that don't work cross-platform through Blink:

| Syscall | Purpose | Cosmo API |
|---------|---------|-----------|
| `PORTATOR_GETIFADDRS` | Enumerate network interfaces | `getifaddrs()` |
| `PORTATOR_CLOCK` | High-resolution cross-platform time | `clock_gettime(CLOCK_MONOTONIC)` |
| `PORTATOR_HASH` | Compute SHA-256/SHA-512/etc. | `libcrypt.a` primitives |
| `PORTATOR_GETNAMEINFO` | Reverse DNS (IP → hostname) | `getnameinfo()` |

These are lower priority — evaluate based on what guest apps actually need.

## Implementation Notes

- Existing syscall range is 0x7000–0x7007. New syscalls start at 0x7008.
- All syscalls use the probe/fill pattern (call with NULL buffer to get size, then with buffer to fill) where response size is variable.
- Guest-side wrappers go in `include/portator.h` as inline functions.
- Host-side dispatch is in `HandlePortatorSyscall()` in `main.c`.
- The host already links against cosmo's `libc.a` and `libresolv.a` (via cosmocc), so DNS functions are available with no additional linking.
