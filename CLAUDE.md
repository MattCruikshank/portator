# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Portator is an in-process x86-64 emulator platform built on a fork of [Blink](https://github.com/jart/blink). It runs guest programs (static x86-64 ELFs) inside a single portable APE (Actually Portable Executable) binary built with Cosmopolitan C. Guest apps communicate with the host via custom syscalls (0x7000–0x7007).

## Build Commands

### Full rebuild (Blink + Portator + guests)
```bash
# From repo root:
cd blink && make clean && CFLAGS="-g -O2 -Wno-cast-align" ./configure CC=cosmocc AR=cosmoar --enable-vfs && make CC=cosmocc AR=cosmoar -j4
cd .. && make
```

### Build just Portator (after Blink is already built)
```bash
make
```
This runs: `clean-portator → portator → apps → package → publish`

### Build a single guest app
```bash
./bin/portator build <name>    # e.g. ./bin/portator build snake
```

### Run a guest app
```bash
./bin/portator run <name>      # e.g. ./bin/portator run snake
```

### Inspect the zip contents of the binary
```bash
zipinfo -1 bin/portator
```

### Test from clean publish directory
```bash
cd publish && ./portator list
```

## Architecture

**Host/Guest split**: The host (main.c) links against Blink's `blink.a` and `zlib.a`, compiled with `cosmocc`. Guest apps are compiled with `musl-gcc -static` (NOT cosmocc or glibc — cosmocc causes a deadlock in Cosmopolitan's `__zipos_init` when the guest's executable path is under `/zip/`, and glibc's complex NPTL/TLS exit cleanup crashes under Blink's emulation). The final binary is a ZIP archive containing the executable plus embedded resources (wwwroot, include/, src/, and guest app binaries under apps/).

**Key source files**:
- `main.c` — CLI dispatcher, Blink integration, custom syscall handler, program discovery, ZIP extraction
- `web_server.c` — CivetWeb wrapper for `portator web`
- `include/portator.h` — Guest-side API: syscall numbers (0x7000–0x7007), event structs, inline asm helpers
- `Makefile` — Builds host, then uses `./bin/portator build` to compile each guest app

**Guest app convention**: Each app lives in `guests/<name>/<name>.c` and compiles to `guests/<name>/bin/<name>`. The Makefile packages these into the ZIP under `apps/<name>/bin/<name>`. App data goes in `guests/<name>/zip/` (or legacy paths `guests/<name>/data/`, `guests/<name>/templates/`).

**Program discovery**: Scans two locations, local takes precedence:
1. `guests/<name>/bin/<name>` (relative to cwd)
2. `/zip/apps/<name>/bin/<name>` (inside APE binary)

**Custom syscalls**: Defined in `portator.h`, handled by `HandlePortatorSyscall()` in main.c. Variable-length responses (version, list) use probe/fill: call with NULL to get size, then with buffer to fill.

## Dependencies

- **cosmocc/cosmoar** — Cosmopolitan C compiler for the host (must be on PATH). Use `cosmocc`, NOT `fatcosmocc` — the project switched from fat builds to single-arch. See PREREQS.md for details.
- **musl-gcc** — musl-libc GCC wrapper for compiling guest apps (`musl-gcc -static`; install via `apt install musl-tools`)
- **Blink** — x86-64 emulator in `blink/` submodule (`blink/o//blink/blink.a`)
- **TinyCC** — Embedded C compiler in `tcc/` submodule. Compiled with musl-gcc as a guest app, bundled in the APE zip with musl headers and libc.a. See TCC.md.
- **CivetWeb** — Embedded HTTP/WebSocket server in `civetweb/`
- **cJSON** — JSON parser vendored in `include/cjson/` and `src/`
- **Mustach** — Mustache template engine vendored in `include/` and `src/`

## Guest Apps

| App | Purpose |
|-----|---------|
| snake | Terminal snake game |
| list | Program discovery (calls LIST syscall, parses JSON with cJSON) |
| new | Project scaffolding (uses Mustach templates from `new/templates/`) |
| license | License/credits display (reads from `zip/apps/license/data/`) |
| mojozork | Z-machine interactive fiction (reads `zip/apps/mojozork/data/zork1.dat`) |
| tcc | Embedded C compiler (TinyCC with musl libc) |
| hello-go | Go guest app demo |

## Guest Zip Access

Guests access bundled files via **relative** `zip/apps/<name>/...` paths — NOT absolute `/zip/...` paths. This works because:

1. `FLAG_prefix` is set to cwd in main.c, making cwd the VFS root
2. `VfsMountZip()` is called in `CmdRunForked()`, which uses Cosmopolitan's zipos to expose the APE's zip contents
3. Cosmopolitan makes zip contents accessible at the relative path `zip/` from the binary's location
4. The VFS hostfs layer passes these paths through to Cosmopolitan's libc, which handles zipos transparently

So a guest doing `fopen("zip/apps/license/data/portator/LICENSE", "r")` reads directly from the APE binary's zip store — no extraction to disk needed.

**Testing zip access**: Always test from the `publish/` directory (`cd publish && ./portator ...`). From the repo root, local files on disk may shadow zip contents, hiding bugs.

**Guest filesystem visibility**: `FLAG_prefix` maps the guest's `/` to the host's cwd. Guests see `.` = `/` = host cwd. Guests **can** traverse `..` to access the host filesystem above cwd — there is no sandbox. `zip/` provides access to APE zip contents. Absolute host paths like `/etc` or `/home` are not directly accessible (they resolve relative to the prefix). Guest apps are trusted code — this is an app platform, not a security sandbox.

## Important Patterns

- The APE binary doubles as a ZIP archive. After linking, the Makefile appends resources via `zip -qr`. Guest apps are added in the `package` step.
- Blink's VFS is enabled (`--enable-vfs`). `VfsMountZip()` in `CmdRunForked()` gives guests access to bundled files via relative `zip/apps/...` paths.
- JIT is currently disabled (`--disable-jit`) — re-enabling is tracked in Claude-TODO.md.
- `CmdRunForked()` forks before executing guests so the emulator can be invoked repeatedly without re-initialization issues.
- Console guests use standard C I/O. Only graphical/web guests need the custom syscall API.
- App data goes in `guests/<name>/zip/` in the source tree. The Makefile's `package` step copies `guests/<name>/zip/*` → `apps/<name>/` in the APE zip. Legacy paths (`guests/<name>/data/`, `guests/<name>/templates/`) are also supported.
- Always run full `make` (not just `make portator`) to ensure `publish/portator` is updated. The user tests from `publish/`.
