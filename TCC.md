# Embedding TCC in Portator

## Goal

Bundle a complete C toolchain inside the portator APE binary so that `portator build` works out of the box with zero external dependencies. No `apt install`, no `musl-gcc`, no `gcc` — just the one binary.

## Why TCC

| | GCC | TCC |
|---|---|---|
| Compiler binary | 30 MB (`cc1`) + 1 MB (`gcc`) | 283 KB (single binary) |
| Needs assembler? | Yes (`as`, 729 KB) | No (built-in) |
| Needs linker? | Yes (`ld`, ~1 MB) | No (built-in) |
| Optimization | Excellent (`-O2`, `-O3`) | Minimal (no optimizer) |
| C standard | Full C23 | C99 + extensions |
| Total with musl | ~36 MB | ~4 MB uncompressed, **~950 KB compressed in zip** |

TCC is a compiler, assembler, and linker in one binary. Combined with musl headers and `libc.a`, the entire toolchain compresses to under 1 MB in the APE zip store.

We verified that TCC + musl produces static x86-64 ELFs that run correctly under Blink/portator:

```bash
tcc -static -I/usr/include/x86_64-linux-musl -L/usr/lib/x86_64-linux-musl hello.c -lc -o hello
# produces 27 KB static ELF, runs under portator ✓
```

## Approach: TCC as a Guest App

TCC is a guest application, just like `snake`, `list`, or `mojozork`. It is compiled with `musl-gcc -static` to produce a static x86-64 ELF, bundled in the APE zip, and executed under Blink emulation.

This is the same pattern as every other guest app:
- No need to compile TCC with cosmocc
- No libtcc integration into the host
- No cosmocc compatibility concerns
- TCC runs on every platform Blink runs on — because Blink is the platform

### How `portator build` uses it

`portator build foo` runs TCC as a guest under Blink, the same way `portator run list` runs the list program. The host extracts TCC's data (musl headers, musl libs, TCC runtime) if needed, then invokes TCC through the emulator with the right arguments.

Conceptually:

```
portator build foo
  → extract toolchain data from /zip/apps/tcc/ if not already on disk
  → portator run tcc -static \
      -I./include -I./include/cjson \
      -Iapps/tcc/musl-include \
      -Lapps/tcc/musl-lib \
      -Lapps/tcc/tcc-lib \
      -DNO_OPEN_MEMSTREAM \
      -o foo/bin/foo foo/foo.c ./src/cJSON.c ./src/mustach.c ... \
      -lc -lm
```

The host's `CmdBuild()` in main.c changes from `execlp("musl-gcc", ...)` to running the bundled TCC guest through `CmdRunForked()` (or equivalent).

### Fallback chain

1. **Bundled TCC** (guest under Blink) — always available, zero dependencies
2. **System musl-gcc** — if installed, better optimization
3. **System gcc** — last resort

If `musl-gcc` is on the system, `portator build` could prefer it for better-optimized output. A `--compiler` flag or config could let the user choose.

## Fork and Submodule

**Yes, fork TCC.** Same pattern as Blink:

1. Fork `https://github.com/TinyCC/tinycc` to `https://github.com/MattCruikshank/tinycc`
2. Add as a git submodule: `git submodule add https://github.com/MattCruikshank/tinycc.git tcc`
3. The fork allows us to:
   - Fix any musl-specific or Blink-specific issues
   - Hardcode default paths for the embedded use case
   - Pin a known-good version
   - Make targeted changes without waiting on upstream

The submodule means `git submodule update --init` pulls both `blink/` and `tcc/`, and the Makefile can build both.

### Building TCC itself

TCC is compiled once during `make`, the same way other guest apps are built:

```bash
cd tcc && ./configure --cc=musl-gcc --cpu=x86_64 && make
# or: musl-gcc -static -o tcc/bin/tcc tcc/tcc.c -DONE_SOURCE=1 -DTCC_TARGET_X86_64
```

TCC supports a single-file build mode (`ONE_SOURCE=1`) where one `.c` file includes everything. This could be compiled directly by the existing `portator build` infrastructure or by a Makefile step that runs `musl-gcc` during the portator build.

**Bootstrap note:** Building TCC requires `musl-gcc` (or any C compiler). Once TCC is built and bundled, portator no longer needs `musl-gcc` at runtime — only at build time for producing the portator binary itself. End users who receive the portator binary need nothing installed.

## What Goes in the Zip

TCC follows the standard guest app data convention (`tcc/zip/` → `apps/tcc/` in the zip):

```
apps/tcc/
  bin/tcc                  # TCC binary (~283 KB), static x86-64 ELF
  musl-include/            # musl headers (~1.1 MB uncompressed, 218 files)
    stdio.h
    stdlib.h
    sys/
    bits/
    ...
  tcc-include/             # TCC's built-in headers (8 files, ~20 KB)
    stdarg.h
    stddef.h
    float.h
    ...
  musl-lib/                # musl static library + crt objects (~2.5 MB uncompressed)
    libc.a
    crt1.o
    crti.o
    crtn.o
  tcc-lib/                 # TCC runtime (~8 KB)
    libtcc1.a
```

Total uncompressed: ~4 MB. Compressed in zip: ~950 KB.

The portator `include/` and `src/` files (portator.h, cJSON, mustach) are already in the zip at the top level and are extracted to the working directory by `portator build`.

## Source Layout

```
portator/
  tcc/                     # git submodule (TinyCC fork)
    tcc.c
    libtcc.c
    libtcc.h
    ...
  tcc-data/                # staged during make (not in git)
    zip/                   # becomes apps/tcc/ in the APE zip
      musl-include/
      musl-lib/
      tcc-include/
      tcc-lib/
```

The Makefile:
1. Compiles TCC with `musl-gcc -static` → `tcc/bin/tcc`
2. Copies musl headers from `/usr/include/x86_64-linux-musl/` → `tcc-data/zip/musl-include/`
3. Copies musl libs from `/usr/lib/x86_64-linux-musl/` → `tcc-data/zip/musl-lib/`
4. Copies TCC's own include and runtime → `tcc-data/zip/tcc-include/`, `tcc-data/zip/tcc-lib/`
5. The `package` step picks up `tcc/bin/tcc` and `tcc-data/zip/*` into the APE zip

## Changes to `portator build` in main.c

### Current behavior (line 214)

```c
execlp("musl-gcc", "musl-gcc", "-static", "-fno-pie", "-no-pie",
       "-I./include", "-I./include/cjson", "-DNO_OPEN_MEMSTREAM",
       "-o", out, src, "./src/cJSON.c", ...);
```

Forks a child, execs system `musl-gcc`. Requires musl-gcc installed.

### New behavior

```c
if (has_command("musl-gcc")) {
    // System compiler available — use it (better optimization)
    execlp("musl-gcc", "musl-gcc", "-static", ...);
} else if (has_bundled_tcc()) {
    // No system compiler — use bundled TCC under Blink
    extract_tcc_data_if_needed();
    run_guest_tcc(name, src, out);
} else {
    Print(2, "portator: no compiler available\n");
    return 1;
}
```

`run_guest_tcc()` calls `CmdRunForked()` with the TCC binary and appropriate arguments. TCC runs under Blink emulation, reads source files from the host filesystem, writes the output ELF to disk.

## musl Headers and Libs Sourcing

The musl headers and `libc.a` come from the musl libc project, not from TCC. Options:

1. **Copy from the build system's musl-tools** during `make`. Requires `apt install musl-tools` at build time (already required today). Simple.
2. **Vendor into the repo** under `tcc-data/`. Self-contained but adds ~4 MB to git.
3. **Download during build** from musl.libc.org. Keeps repo small but adds network dependency.

**Recommendation: Option 1 for now.** `musl-tools` is already a build prerequisite. The Makefile copies the headers and libs into the staging area during build. Later, vendor them for fully reproducible builds.

## Open Questions

### VFS access for TCC

TCC running as a guest under Blink needs to read musl headers and `libc.a` from disk. The host must extract these from the zip before running TCC. This is the same pattern as `mojozork` needing `zork1.dat` — the host extracts app data, then the guest reads it from the filesystem.

With Blink's VFS enabled, we could potentially mount `apps/tcc/` to a guest-visible path without extracting to disk. Per Plan.md, local per-app VFS mounts work (`VfsMount("<name>/zip", "/app", "hostfs")`). This would avoid writing ~4 MB of toolchain files to disk on every build. Worth testing.

### TCC compiling under Blink

TCC does significant work — preprocessing, parsing, code generation, linking. This all runs emulated under Blink. Performance should be fine for small guest apps (milliseconds to seconds), but could be slow for larger programs. If this becomes an issue, the user can install `musl-gcc` for native compilation speed.

### TCC version

The Ubuntu package is from 2020 (0.9.27). The upstream repo (`mob` branch) has continued development. The fork should start from a recent upstream commit for better C99/C11 support and bug fixes.

### Bootstrap chicken-and-egg

Building portator requires `musl-gcc` to compile TCC (and all other guest apps). Once built, the portator binary contains TCC and no longer needs `musl-gcc`. This means:
- **Developers** building portator from source need `musl-gcc`
- **End users** receiving the portator binary need nothing — TCC is embedded

This is the same bootstrap pattern as any self-hosting compiler.

## Implementation Steps

1. Fork TinyCC, add as submodule (`git submodule add`)
2. Add Makefile rules to compile TCC with `musl-gcc -static`
3. Add Makefile rules to stage musl headers/libs into `tcc-data/zip/`
4. Package TCC + data into the APE zip alongside other apps
5. Modify `CmdBuild()` in main.c to detect and use bundled TCC as fallback
6. Test: `portator build` on a system with no `musl-gcc` installed
7. Test: guest apps built by TCC run correctly under Blink
8. Test: TCC compilation speed under Blink for typical guest apps
