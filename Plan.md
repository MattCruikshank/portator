# Portator: In-Process Emulated App Platform

![Portator](https://portator.net/PortatorSmall.png)

**Website: [portator.net](http://portator.net/)**

## Overview

Portator is a modified fork of [Blink](https://github.com/jart/blink) (ISC license), an x86-64 emulator. It runs guest programs in-process, eliminating the need to extract executables to disk. Guest programs can link against `libportator`, a small C++ library that provides app abstractions via custom syscalls.  Console applications do not need to link against `libportator`, they just use normal write and read operations.

Portator can also run a web-native host. When it does, it serves all content to a browser — console apps get a terminal (GhostTTY), graphical apps stream pixels over WebSocket (or WebRTC if feasible), and web apps get their own pages served directly.

When Portator runs as a console-native application, console apps are just forwarded.  We'll need a plan for graphical apps or web apps.

## Architecture

```
┌──────────────────────────────────────────────────┐
│  Host (Portator)                                 │
│  - APE binary built with fatcosmocc              │
│  - Contains Blink emulator (modified)            │
│  - Web server with menu UI                       │
│  - Serves terminal, framebuffer, or web apps     │
│  - Queues input events for guests                │
│  - Discovers guest ELFs from multiple paths      │
│  - May fork to serve multiple apps               │
├──────────────────────────────────────────────────┤
│  Guest (e.g. Snake)                              │
│  - Static x86-64 ELF                             │
│  - Links against libportator                     │
│  - Subclasses PortatorConsoleApp,                │
│    PortatorGraphicalApp, or PortatorWebApp       │
│  - No platform dependencies                      │
└──────────────────────────────────────────────────┘
```

## Program Discovery

Portator discovers guest programs from multiple locations:

1. **`/zip/bin/`** — programs bundled inside the Portator APE binary
2. **`*/bin/`** — any subfolder's `bin/` directory relative to where Portator is launched

Both are scanned and merged into a single list. If the same program name appears in multiple locations, the one with the most recent modification time is used by default.

This means project folders, extracted tools, and bundled programs are all discovered the same way.

## Command Line Interface

Portator is invoked as a single binary with subcommands. Most subcommands are themselves guest programs — `portator <cmd> <args>` is equivalent to `portator run <cmd> <args>`.

### `portator` (no args)

Launches a console application.  In it, the user can type `run <program> [args...]` or `new <type> <name>`, etc.

### `portator web`

Launches a web-based UI, that provides the console application, over GhostTTY.  In it, the user can type `run <program> [args...]` or `new <type> <name>`, etc.

In this form, the console application is just a window.  There's also a Desktop graphical application running, with shortcuts to programs that the user selected, and also a Start bar with all programs, and a Taskbar with programs the user selected.

### `portator run <program> [args...]`

Runs a discovered program in the emulator, passing arguments.

### `portator new <type> <name>`

Scaffolds a new project. Equivalent to `portator run new <type> <name>`. The `new` program is itself a guest.

Creates a project folder with conventional structure:

```
portator new console hello    →  hello/hello.cpp, hello/portator.hpp, hello/bin/
portator new gui snake        →  snake/snake.cpp, snake/portator.hpp, snake/bin/
portator new web dashboard    →  dashboard/dashboard.cpp, dashboard/portator.hpp,
                                 dashboard/bin/, dashboard/wwwroot/, dashboard/docs/
```

`portator.hpp` is extracted into the project as a reference/include.

If cosmocc is not available, prompts to download it (see `portator get`).

### `portator build <name>`

Compiles a project. Equivalent to `portator run build <name>`. The `build` program is itself a guest.

Follows the convention: finds `<name>/<name>.cpp`, compiles with cosmocc, outputs to `<name>/bin/<name>`. Once built, the program is immediately discoverable by Portator via `*/bin/` scanning.

### `portator get <name>`

Extracts or downloads a tool.

- **Embedded tools** (e.g. `package`): If `<name>` is in the zip store, extract it to `<name>/bin/<name>`. Notes that it was already bundled.
- **Remote tools** (e.g. `cosmocc`): If `<name>` is not embedded, download it from a known official URL to `<name>/bin/`.

Once extracted/downloaded, the tool is in a `*/bin/` folder and becomes discoverable like any other program.

### `portator get package`

Extracts the `package` tool (bundled in the zip store) to `package/bin/package`. This is a standalone APE that adds compiled ELFs back into the Portator binary's zip store:

```
package hello           # adds hello/bin/hello to portator's zip
portator build hello && package hello   # rebuild and re-embed
```

This works on platforms where APE binaries can self-modify. On platforms where the running executable is locked (e.g. Windows), `package` writes to a copy.

### `portator get cosmocc`

Downloads cosmocc from the official cosmopolitan distribution (`https://cosmo.zip/pub/cosmocc/`) to `cosmocc/bin/cosmocc`.

### `portator list`

Lists all discovered programs — name, app type (console/gui/web), source location (zip store vs disk path), and modification time. Useful for verifying what Portator can see.

```
$ portator list
NAME       TYPE      SOURCE              MODIFIED
snake      gui       snake/bin/snake      2 min ago
hello      console   /zip/bin/hello       (bundled)
editor     web       editor/bin/editor    1 hour ago
build      console   /zip/bin/build       (bundled)
new        console   /zip/bin/new         (bundled)
```

### `portator test <name>`

Runs tests for a project. Equivalent to `portator run test <name>`. The `test` program is itself a guest.

Follows a convention: looks for `<name>/<name>_test.cpp` (or similar), compiles and runs it. Test framework TBD — could be as simple as assert-based tests that exit non-zero on failure.

### `portator watch <name>`

Watches a project's source files for changes, automatically rebuilds and relaunches on save. Equivalent to running `portator build <name> && portator run <name>` on every file change.

Uses platform file-watching (`inotify` on Linux, `kqueue` on macOS/BSD, `ReadDirectoryChangesW` on Windows via cosmo abstractions). Useful during development for a fast edit-compile-run loop.

### `portator clean <name>`

Removes build artifacts for a project. Deletes `<name>/bin/` and its contents.

```
$ portator clean hello
Removed hello/bin/
```

## Distributions

- **portator** — lightweight distribution. Downloads cosmocc on demand via `portator get cosmocc`.
- **portator-dev** — includes cosmocc embedded in the zip store. `portator get cosmocc` extracts locally instead of downloading. Works fully offline.

## Directory Conventions

A Portator project follows a standard layout. The type of project determines which folders are present:

```
hello/                  # console project
  hello.cpp
  portator.hpp
  bin/
    hello               # compiled ELF (discovered by portator)

snake/                  # gui project
  snake.cpp
  portator.hpp
  favicon.ico           # an icon to show on the Desktop.
  bin/
    snake

editor/                 # web project
  editor.cpp
  portator.hpp
  bin/
    editor
  wwwroot/
    index.html
    style.css
    app.js
  docs/

cosmocc/                # downloaded tool
  bin/
    cosmocc

package/                # extracted bundled tool
  bin/
    package
```

Portator scans `*/bin/` to find everything — projects, tools, all of it. The convention makes discovery automatic.

## App Types

Guest programs subclass one of three intermediate classes, each inheriting from `PortatorApp`:

### PortatorConsoleApp

For terminal/text-based programs. The host serves a web-based terminal emulator (xterm.js/GhostTTY) and bridges stdin/stdout over WebSocket.

```cpp
class PortatorConsoleApp : public PortatorApp {
public:
    virtual void run() = 0;
    // stdin/stdout work normally — host bridges them to the browser terminal
};
```

### PortatorGraphicalApp

For pixel-based graphical programs. The host streams the framebuffer to the browser over WebSocket (or WebRTC).

```cpp
class PortatorGraphicalApp : public PortatorApp {
public:
    PortatorGraphicalApp(int width, int height);

    int width() const;
    int height() const;

    virtual void draw(uint32_t *framebuf) = 0;
    virtual void mouse_down(int x, int y, int button) {}
    virtual void mouse_up(int x, int y, int button) {}
    virtual void mouse_move(int x, int y) {}
    virtual void key_down(int key) {}
    virtual void key_up(int key) {}
};
```

### PortatorWebApp

For programs that serve their own web UI. The guest specifies an HTML file to serve and communicates with the browser via messages over WebSocket.

```cpp
class PortatorWebApp : public PortatorApp {
public:
    virtual const char *index_html() = 0; // filename to serve
    virtual void on_message(const char *msg, size_t len) {}
    void send_message(const char *msg, size_t len);
};
```

### Base class

```cpp
class PortatorApp {
public:
    virtual ~PortatorApp() = default;
    void exit(int code = 0);
};

void portator_run(PortatorApp *app);
```

`portator_run` inspects the app's runtime type to determine how to run it. The host uses this to decide the rendering/serving strategy.

## Custom Syscall ABI

| Number | Name     | Args                          | Returns              |
|--------|----------|-------------------------------|----------------------|
| 0x7000 | present  | (buf_ptr, width, height)      | 0 on success         |
| 0x7001 | poll     | (event_ptr)                   | 1 if event, 0 if not |
| 0x7002 | exit     | (code)                        | does not return       |
| 0x7003 | ws_send  | (msg_ptr, len)                | 0 on success         |
| 0x7004 | ws_recv  | (msg_ptr, max_len)            | bytes received, 0 if none |
| 0x7005 | app_type | ()                            | 0=console, 1=gfx, 2=web |

Event structure (passed to guest via poll):

```c
struct PortatorEvent {
    uint32_t type;    // PORTATOR_KEY_DOWN, PORTATOR_KEY_UP, PORTATOR_MOUSE_DOWN, etc.
    int32_t  x, y;    // mouse position (for mouse events)
    int32_t  key;     // key code (for key events)
    int32_t  button;  // button number (for mouse events)
};
```

## Portator Host

A fork of Blink with the following changes:

- **ELF loading from memory**: Load guest ELFs directly from buffers (read from zip store or disk) instead of file paths
- **Custom syscall handlers**: See ABI table above
- **Web server**: Serves a menu page listing discovered programs. Launching a program opens it in the appropriate mode:
  - Console apps: xterm.js terminal, I/O over WebSocket
  - Graphical apps: canvas element, framebuffer streamed over WebSocket (or WebRTC)
  - Web apps: new page serving the guest's `index.html`, messages over WebSocket
- **Multiple concurrent apps**: Fork to serve multiple apps simultaneously (if Blink's code permits reentrancy; may require investigation)

## Native APE Bypass (Future Idea)

If a discovered program is itself an APE binary (rather than a plain x86-64 ELF), Portator could launch it directly via fork/exec instead of emulating it. This would give native performance. However, this requires the APE program to implement the Portator API (syscalls or a shared protocol), so this is documented for future consideration and not planned for initial implementation.

## Guest Examples

### Snake (graphical)

```cpp
class SnakeApp : public PortatorGraphicalApp {
public:
    SnakeApp() : PortatorGraphicalApp(640, 320) {}
    void draw(uint32_t *framebuf) override { /* ... */ }
    void key_down(int key) override { /* ... */ }
};

int main() {
    SnakeApp app;
    portator_run(&app);
}
```

### Hello (console)

```cpp
class HelloApp : public PortatorConsoleApp {
public:
    void run() override {
        printf("Hello, world!\n");
    }
};

int main() {
    HelloApp app;
    portator_run(&app);
}
```

### Dashboard (web)

```cpp
class DashboardApp : public PortatorWebApp {
public:
    const char *index_html() override { return "dashboard.html"; }
    void on_message(const char *msg, size_t len) override { /* handle */ }
};

int main() {
    DashboardApp app;
    portator_run(&app);
}
```

### Editor (web — Monaco)

A code editor built as a PortatorWebApp. The browser-side UI is [Monaco](https://microsoft.github.io/monaco-editor/) (the editor component from VS Code), which is pure client-side JavaScript. The guest handles file I/O over WebSocket messages — open, save, list directory, etc.

Monaco's JS/CSS assets are bundled as static files in the project's `wwwroot/`. The guest program itself is a thin backend: it receives "open file" / "save file" messages, reads/writes via Portator syscalls, and sends content back. All the editing, syntax highlighting, IntelliSense, and UI logic runs entirely in the browser.

```cpp
class EditorApp : public PortatorWebApp {
public:
    const char *index_html() override { return "editor.html"; }
    void on_message(const char *msg, size_t len) override {
        // parse message: open, save, list, etc.
        // respond with file contents or directory listing
    }
};

int main() {
    EditorApp app;
    portator_run(&app);
}
```

This gives Portator a usable code editor without needing to port Node.js, VS Code, or code-server.

## Build Structure (Portator source)

```
portator/
  blink/              # forked Blink source (modified)
  libportator/
    portator.hpp      # PortatorApp class hierarchy
    portator.cpp      # portator_run loop, syscall wrappers
  guests/
    new/              # scaffolding tool (console guest)
    build/            # compiler wrapper (console guest)
    snake/            # SnakeApp (graphical)
    hello/            # HelloApp (console)
    editor/           # EditorApp (web, Monaco)
  Makefile
```

## Build Flow

1. Build libportator as a static library (`libportator.a`) with a static x86-64 toolchain
2. Build each guest program, linking against libportator, producing static x86-64 ELFs
3. Build the Portator host with `fatcosmocc` (APE binary)
4. Zip guest ELFs (and `package` tool) into the host binary
5. For portator-dev: also zip cosmocc into the host binary

## Progress

### Completed

1. **Forked Blink** — [github.com/MattCruikshank/blink](https://github.com/MattCruikshank/blink)
2. **Built Blink with `fatcosmocc`** — Produces a fat APE binary (x86-64 + aarch64). Required these changes to the fork:
   - `blink/machine.h` — Guard `libc/dce.h` include with `!defined(__COSMOCC__)`
   - `blink/loader.c` — Guard `IsWindows()` macro for `cosmocc` builds
   - `blink/cpuid.c` — Guard OS detection macro for `cosmocc` builds
   - `build/rules.mk` — Disabled `.h.ok` header check targets (incompatible with `fatcosmocc`)
   - `blink/clear_cache.c` — New file providing `__clear_cache` for aarch64
3. **Created Portator `main.c`** — Minimal wrapper around blink's internals (`LoadProgram`, `Blink`, etc.), linking against `blink.a` and `zlib.a`. Provides `TerminateSignal` (required by `blink.a` but defined outside the archive in blink's own `blink.c`).
4. **Created standalone `Makefile`** — Compiles `main.c` with `fatcosmocc`, links against blink's pre-built `blink.a` and `zlib.a`. Output goes to `bin/portator`.
5. **Created `.gitignore`** — Ignores `bin/` and `blink/` directories.
6. **Verified guest execution** — Compiled a test hello world with `cosmocc -static -fno-pie -no-pie` and ran it successfully under Portator.
7. **Created `hello` console guest** — `hello/hello.c` and `hello/portator.h` with syscall constants, event struct, and inline syscall wrappers. Compiles and runs under Portator.
8. **Implemented `portator new <type> <name>`** — Built-in subcommand that scaffolds a project directory with `<name>.c`, `portator.h`, and `bin/`. Supports `console`, `gui`, and `web` types.
9. **Implemented `portator build <name>`** — Built-in subcommand that shells out to `cosmocc` to compile `<name>/<name>.c` into `<name>/bin/<name>`.
10. **Implemented `portator run <name>`** — Built-in subcommand that resolves `<name>/bin/<name>` and runs it in the Blink emulator. Fixed a segfault caused by calling `Exec` before Blink runtime init (HandleSigs, InitBus, overlays, VFS).

### Next Steps

1. **Add custom syscall handlers** — Hook into blink's syscall dispatch to handle Portator-specific syscalls (0x7000–0x7005). This is needed before graphical or web guests can work.
2. **Build `libportator`** — The guest-side library providing `PortatorApp` class hierarchy and syscall wrappers. Currently `portator.h` has inline syscall wrappers; this step builds out the full C++ class hierarchy (`PortatorConsoleApp`, `PortatorGraphicalApp`, `PortatorWebApp`).
3. **Program discovery** — Implement `*/bin/` scanning and `/zip/bin/` lookup. Currently `portator run <name>` only checks `<name>/bin/<name>` by convention.
4. **Web server** — Serve a menu page listing discovered programs. Console apps get a terminal (GhostTTY/xterm.js), graphical apps get a canvas with framebuffer streaming, web apps get their own pages.
5. **`portator list`** — List all discovered programs with type, source, and modification time.
6. **Scaffold `gui` and `web` templates** — `portator new gui` and `portator new web` currently produce stub files. Flesh out with working examples once syscall handlers and the web server are in place.

### Notes

- Binary size is ~1.8MB for the fat APE build. This is larger than expected (~200KB for single-arch blink). The fat binary contains both x86-64 and aarch64 code plus the APE loader. Stripping debug symbols and `MODE=tiny` did not reduce size. Needs further investigation.
- `fatcosmocc --update` may be needed if the cosmocc installation is stale — saw build failures until this was run.
- Blink's `.bin` test files (e.g. `third_party/games/basic.bin`) are raw boot sector binaries for blinkenlights, not ELFs — they cannot be used to test portator.

## Open Questions

- **Frame streaming**: What encoding/protocol for streaming graphical framebuffers to the browser? Raw pixels over WebSocket is simplest. WebRTC would allow lower latency. Could also encode as JPEG/PNG per frame for bandwidth.
- **Frame timing**: Does `portator_run` drive a fixed frame rate (e.g. 60fps), or does the guest control timing? Host-driven vsync is simpler.
- **Audio**: Not in scope yet, but the syscall pattern extends naturally (e.g. `0x7006` to submit audio samples, streamed to browser via Web Audio API).
- **Guest toolchain**: cosmocc is the default. Could also support musl-gcc or plain gcc with `-static`.
- **Blink reentrancy**: Can multiple emulator instances run concurrently in forked processes? Needs investigation.
- **Web app static assets**: Served from the project's `wwwroot/` folder. Host needs a syscall or convention for guests to reference these files.
- **cosmocc download URL**: Need to pin the official cosmopolitan release URL for `portator get cosmocc`.

## Why This Approach

- **No SmartScreen issues on Windows**: No executables extracted to disk (except explicitly via `portator get`)
- **Single portable binary**: Host is one APE that runs everywhere
- **Browser-native UI**: No platform-specific rendering code; everything goes through the browser
- **Three app models**: Console, graphical, and web apps all supported with the same infrastructure
- **Convention over configuration**: Standard folder layout, automatic discovery via `*/bin/`
- **Self-contained tooling**: `new`, `build`, `package` are all guest programs — extensible and dogfooded
- **Simple guest development**: Subclass, implement a few methods, link against libportator, done
- **Clean separation**: Guests know nothing about the platform; host handles everything
- **Offline capable**: portator-dev bundles cosmocc for fully offline development
