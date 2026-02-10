# Building Portator

## Prerequisites

### 1. Cosmopolitan C Toolchain (cosmocc)

The host binary is built with [Cosmopolitan](https://github.com/jart/cosmopolitan), which produces Actually Portable Executables that run on Linux, macOS, Windows, FreeBSD, and more.

Download the latest release:

```bash
mkdir -p ~/cosmocc && cd ~/cosmocc
wget https://cosmo.zip/pub/cosmocc/cosmocc.zip
unzip cosmocc.zip
```

Add to your PATH (add to `~/.bashrc` or `~/.zshrc`):

```bash
export PATH="$HOME/cosmocc/bin:$PATH"
```

Verify:

```bash
cosmocc --version
cosmoar --version
```

### 2. GCC

A system GCC is required (used by `musl-gcc` as the underlying compiler).

```bash
# Ubuntu/Debian
sudo apt install build-essential
```

### 3. musl libc

Guest apps are compiled with `musl-gcc -static` to produce clean static x86-64 ELF binaries. Do not use `gcc -static` (glibc) or `cosmocc` for guests -- both cause crashes under Blink's emulation.

```bash
# Ubuntu/Debian
sudo apt install musl-tools
```

Verify:

```bash
musl-gcc --version
```

### 4. zip

Used to package resources into the APE binary.

```bash
# Ubuntu/Debian
sudo apt install zip
```

## Clone and Build

### Clone the repository

```bash
git clone --recurse-submodules https://github.com/MattCruikshank/portator.git
cd portator
```

If you already cloned without `--recurse-submodules`:

```bash
git submodule update --init
```

### Build Blink (first time only)

Blink is an x86-64 emulator that lives in the `blink/` submodule. It needs to be configured and compiled once:

```bash
cd blink
CFLAGS="-g -O2 -Wno-cast-align" ./configure CC=cosmocc AR=cosmoar --enable-vfs
make CC=cosmocc AR=cosmoar -j4
cd ..
```

After a clean configure, always start with `make clean` before `make`:

```bash
cd blink
make clean
CFLAGS="-g -O2 -Wno-cast-align" ./configure CC=cosmocc AR=cosmoar --enable-vfs
make CC=cosmocc AR=cosmoar -j4
cd ..
```

### Build Portator

```bash
make -j4
```

This builds the host binary, compiles all guest apps with `musl-gcc`, packages everything into a single APE binary at `bin/portator`, and copies it to `publish/`.

## Verify

```bash
./bin/portator list
```

You should see the list of bundled apps (snake, list, license, mojozork, new) with no errors.

Test from the publish directory to verify the packaged binary works standalone:

```bash
cd publish && ./portator list && cd ..
```

## Common Tasks

### Rebuild just Portator (after editing main.c, etc.)

```bash
make -j4
```

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

### Full clean rebuild (Blink + Portator)

```bash
cd blink && make clean && CFLAGS="-g -O2 -Wno-cast-align" ./configure CC=cosmocc AR=cosmoar --enable-vfs && make CC=cosmocc AR=cosmoar -j4 && cd ..
make -j4
```

## Troubleshooting

- **`cosmocc: command not found`** -- Add `~/cosmocc/bin` to your PATH.
- **`musl-gcc: command not found`** -- Install with `sudo apt install musl-tools`.
- **Blink build errors after reconfiguring** -- Always `make clean` before `make` after changing configure options. The Makefile does not track `blink/config.h` as a dependency.
- **Guest app crashes with "killed by signal 11"** -- Make sure guest apps are compiled with `musl-gcc -static`, not `gcc -static`. glibc's static exit cleanup is incompatible with Blink's emulation.
