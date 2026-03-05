# Portator

A single portable binary that runs x86-64 guest apps via in-process emulation. No dependencies required — just download and run.

## Quick Start

```bash
curl -LO https://github.com/MattCruikshank/portator/releases/download/v0.1.0/portator
chmod +x portator
./portator
```

## Usage

```
$ ./portator list
Running on Portator v0.1.0
  list
  tcc
  license
  hello-go
  snake
  new
  mojozork

$ ./portator run snake
```

## Create Your Own App

```
$ ./portator new console hello
Created hello/
  hello/hello.c
  hello/portator.h
  hello/bin/

$ ./portator build hello
Building hello...
Built hello/bin/hello

$ ./portator run hello
Running on Portator v0.1.0
Hello from hello!
```

A bundled C compiler (TCC) is included, so `portator build` works with zero external dependencies.

## Building from Source

Requires [cosmocc](https://github.com/jart/cosmopolitan) and `musl-gcc` (`apt install musl-tools`).

```bash
cd blink && make clean && CFLAGS="-g -O2 -Wno-cast-align" ./configure CC=cosmocc AR=cosmoar --enable-vfs && make CC=cosmocc AR=cosmoar -j4
cd .. && make
```