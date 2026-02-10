# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Portator is an in-process x86-64 emulator platform built on a fork of [Blink](https://github.com/jart/blink). It runs guest programs (static x86-64 ELFs) inside a single portable APE (Actually Portable Executable) binary built with Cosmopolitan C. Guest apps communicate with the host via custom syscalls (0x7000–0x7007).

## Build Commands

### Full rebuild (Blink + Portator + guests)
```bash
# From repo root:
cd blink && make clean && ./configure CC=cosmocc AR=cosmoar --enable-vfs --disable-jit && make CC=cosmocc AR=cosmoar -j4
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

**Host/Guest split**: The host (main.c) links against Blink's `blink.a` and `zlib.a`, compiled with `cosmocc`. Guest apps are compiled with `gcc -static` (NOT cosmocc — using cosmocc for guests causes a deadlock in Cosmopolitan's internal `__zipos_init` when the guest's executable path is under `/zip/`). The final binary is a ZIP archive containing the executable plus embedded resources (wwwroot, include/, src/, and guest app binaries under apps/).

**Key source files**:
- `main.c` — CLI dispatcher, Blink integration, custom syscall handler, program discovery, ZIP extraction
- `web_server.c` — CivetWeb wrapper for `portator web`
- `include/portator.h` — Guest-side API: syscall numbers (0x7000–0x7007), event structs, inline asm helpers
- `Makefile` — Builds host, then uses `./bin/portator build` to compile each guest app

**Guest app convention**: Each app lives in `<name>/<name>.c` and compiles to `<name>/bin/<name>`. The Makefile packages these into the ZIP under `apps/<name>/bin/<name>`. App data goes in `<name>/zip/` (or legacy paths `<name>/data/`, `<name>/templates/`).

**Program discovery**: Scans two locations, local takes precedence:
1. `<name>/bin/<name>` (relative to cwd)
2. `/zip/apps/<name>/bin/<name>` (inside APE binary)

**Custom syscalls**: Defined in `portator.h`, handled by `HandlePortatorSyscall()` in main.c. Variable-length responses (version, list) use probe/fill: call with NULL to get size, then with buffer to fill.

## Dependencies

- **cosmocc/cosmoar** — Cosmopolitan C compiler for the host (must be on PATH)
- **gcc** — System GCC for compiling guest apps (`gcc -static`)
- **Blink** — x86-64 emulator in `blink/` directory (pre-built `blink/o//blink/blink.a`)
- **CivetWeb** — Embedded HTTP/WebSocket server in `civetweb/`
- **cJSON** — JSON parser vendored in `include/cjson/` and `src/`
- **Mustach** — Mustache template engine vendored in `include/` and `src/`

## Guest Apps

| App | Purpose |
|-----|---------|
| snake | Terminal snake game |
| list | Program discovery (calls LIST syscall, parses JSON with cJSON) |
| new | Project scaffolding (uses Mustach templates from `new/templates/`) |
| license | License/credits display |
| mojozork | Z-machine interactive fiction (reads `mojozork/data/zork1.dat`) |

## Important Patterns

- The APE binary doubles as a ZIP archive. After linking, the Makefile appends resources via `zip -qr`. Guest apps are added in the `package` step.
- Blink's VFS is enabled (`--enable-vfs`). `/zip/` is mounted via `VfsMountZip()` in `CmdRunForked()`, giving guests access to bundled files.
- JIT is currently disabled (`--disable-jit`) — re-enabling is tracked in Claude-TODO.md.
- `CmdRunForked()` forks before executing guests so the emulator can be invoked repeatedly without re-initialization issues.
- Console guests use standard C I/O. Only graphical/web guests need the custom syscall API.
