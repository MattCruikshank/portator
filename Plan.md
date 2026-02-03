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

1. **`/zip/apps/<name>/bin/<name>`** — programs bundled inside the Portator APE binary's zip store, under `apps/`
2. **`<name>/bin/<name>`** — any subfolder's `bin/` directory relative to where Portator is launched

Both are scanned and merged into a single list. If the same program name appears in multiple locations, the local one takes precedence.

The zip store mirrors the same `<name>/bin/<name>` structure under an `apps/` prefix, so bundled and local programs follow the same convention. This means project folders, extracted tools, and bundled programs are all discovered the same way.

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

Scaffolds a new project. Creates a project folder with conventional structure:

```
portator new console hello    →  hello/hello.c, hello/bin/
portator new gui snake        →  snake/snake.c, snake/bin/
portator new web dashboard    →  dashboard/dashboard.c, dashboard/bin/,
                                 dashboard/wwwroot/, dashboard/docs/
```

Extracts shared `include/` and `src/` files if not already present. Reports which compilers are available on the system (see Compiler Detection below).

### `portator build <name>`

Compiles a project. Finds `<name>/<name>.c` (or `.cpp`, `.rs`, `.zig`, `.go`, `.cs`, `.swift`, `.nim`), selects the appropriate compiler, and outputs to `<name>/bin/<name>`. Once built, the program is immediately discoverable by Portator via `*/bin/` scanning. Extracts shared files and reports available compilers if needed.

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
NAME       SOURCE                            MODIFIED
snake      snake/bin/snake                    2 min ago
hello      /zip/apps/hello/bin/hello          (bundled)
editor     editor/bin/editor                  1 hour ago
build      /zip/apps/build/bin/build          (bundled)
new        /zip/apps/new/bin/new              (bundled)
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

## Compiler Detection

The commands `portator new`, `portator build`, `portator run`, and `portator init` all probe for available compilers on the system using `which` (or equivalent). The detected compilers determine which languages are available for guest development and what `portator build` can compile.

### Detected compilers

| Compiler | Language | Check |
|----------|----------|-------|
| `cosmocc` / `cosmo++` | C / C++ | `which cosmocc` |
| `rustc` | Rust | `which rustc` |
| `zig` | Zig | `which zig` |
| `go` | Go | `which go` |
| `dotnet` | C# (.NET NativeAOT) | `which dotnet` |
| `swiftc` | Swift | `which swiftc` |
| `nim` | Nim | `which nim` |
| `gcc` / `musl-gcc` | C (fallback) | `which musl-gcc` or `which gcc` |

### Behavior

- **`portator init`** — After extracting shared files, prints which compilers are available.
- **`portator new`** — Reports available compilers. Could offer language choice in the future (e.g. `portator new console hello --lang rust`).
- **`portator build`** — Detects the source file extension in `<name>/` and selects the compiler automatically:
  - `.c` → `cosmocc` (or `musl-gcc` / `gcc` as fallback)
  - `.cpp` → `cosmo++` (or `g++` as fallback)
  - `.rs` → `rustc` with `--target x86_64-unknown-linux-musl`
  - `.zig` → `zig build-exe` with `-target x86_64-linux-musl`
  - `.go` → `go build` with `GOOS=linux GOARCH=amd64 CGO_ENABLED=0`
  - `.cs` → `dotnet publish -r linux-x64` with NativeAOT
  - `.swift` → `swiftc` with static linking
  - `.nim` → `nim c` with `--os:linux --cpu:amd64`
  - If the required compiler is not found, prints an error with install instructions.
- **`portator run`** — If the requested program isn't built, suggests `portator build` and notes which compilers are available.

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

## Guest Language Support

Portator runs any static x86-64 Linux ELF. The guest language doesn't matter — only the binary format does. Portator syscalls can be accessed via inline assembly, FFI/C interop, or a thin C shim depending on the language.

### Tier 1 — Straightforward

These languages produce static x86-64 ELFs easily and have natural access to inline asm or C interop for Portator syscalls.

| Language | Target / Toolchain | Syscall Access | Notes |
|----------|-------------------|----------------|-------|
| **C** | `cosmocc -static -fno-pie -no-pie` | Inline asm via `portator.h` | Primary supported language. Shared `include/` and `src/` provided. |
| **C++** | `cosmo++ -static -fno-pie -no-pie` | `#include "portator.hpp"` wrapping `portator.h` | Optional convenience layer with classes. |
| **Rust** | `x86_64-unknown-linux-musl` | Inline asm or FFI to `portator.h` via `extern "C"` | Produces small static ELFs. Great demo candidate. |
| **Zig** | `zig build -Dtarget=x86_64-linux-musl` | Built-in inline asm, or `@cImport` of `portator.h` | Excellent C interop. Produces very small binaries. |
| **Go** | Default `GOOS=linux GOARCH=amd64` (static by default) | cgo shim calling C syscall wrapper | Inline asm not native to Go; needs a small C bridge. |

### Tier 2 — Possible, More Work

These languages can produce static Linux ELFs but require more setup, larger runtimes, or indirect syscall access.

| Language | Target / Toolchain | Syscall Access | Notes |
|----------|-------------------|----------------|-------|
| **C# / .NET** | `dotnet publish -r linux-x64` with NativeAOT | P/Invoke to a C shim | Produces large binaries (~10MB+). NativeAOT is required; JIT won't work. |
| **Swift** | Static Linux toolchain (`swift build --static-swift-stdlib`) | C interop via bridging header | Requires Swift's Linux toolchain. Static linking support is maturing. |
| **Nim** | Compiles through C with `--os:linux --cpu:amd64 --passL:-static` | `{.importc.}` pragmas against `portator.h` | Naturally produces static ELFs via its C backend. Very lightweight. |

### Tier 3 — Unlikely to Work Well

These have fundamental obstacles but are documented for completeness.

| Language | Issue |
|----------|-------|
| **Java (GraalVM native-image)** | Produces static ELFs but very large runtime overhead. Syscall access through JNI is painful. |
| **JIT-based languages** (Node.js, Python, Ruby, etc.) | Require an interpreter/runtime as a separate process. Cannot produce a single static ELF. Could theoretically run if the interpreter itself is compiled as a static ELF and bundled, but this is not a natural fit. |

### Shared Files

The guest-side shared files are embedded in the APE zip and extracted by `portator init`, `portator new`, or `portator build`:

```
include/portator.h       # C syscall wrappers (all languages can use via FFI)
include/cJSON.h          # C JSON parser
include/portator.hpp     # C++ convenience classes (optional)
src/cJSON.c              # C JSON parser implementation
src/portator.cpp         # C++ class implementations (optional)
```

For non-C/C++ languages, `portator.h` serves as the canonical reference for syscall numbers and calling conventions. Language-specific bindings (e.g. a Rust `portator` crate, a Zig `portator.zig` module) can be added to `include/` and `src/` as demand warrants.

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
| 0x7006 | version  | (buf_ptr, len)                | bytes written, or -1     |
| 0x7007 | list_programs | (buf_ptr, len)           | see probe/fill pattern   |
| 0x7008 | launch   | (name_ptr, name_len)          | TBD                      |

### Probe/Fill Calling Convention

Syscalls that return variable-length data (e.g. `list_programs`, `version`) follow a Win32-style probe/fill pattern:

1. **Probe**: Call with `(NULL, 0)`. Returns the number of bytes required for the full response.
2. **Fill**: Allocate a buffer of at least that size, call again with `(buf_ptr, len)`. Returns the number of bytes written.
3. **Undersize**: If `len` is non-zero but smaller than required, returns `-1`.

```c
/* Example: getting the program list */
long needed = portator_syscall(PORTATOR_SYS_LIST_PROGRAMS, 0, 0, 0);
char *buf = malloc(needed);
long written = portator_syscall(PORTATOR_SYS_LIST_PROGRAMS, (long)buf, needed, 0);
/* buf now contains JSON */
```

Variable-length responses are returned as JSON. Guest programs use [cJSON](https://github.com/DaveGamble/cJSON) (vendored `cJSON.c`/`cJSON.h`, written out by `portator build` alongside `portator.h`) for parsing.

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

## Web Server

Portator embeds [CivetWeb](https://github.com/civetweb/civetweb) (vendored in `civetweb/`, MIT license) as its HTTP/WebSocket server. The `portator web` subcommand starts the server on port 6711 (overridable via `portator web <port>`).

Static assets are embedded in the APE binary's zip store under `wwwroot/` and served from `/zip/wwwroot/`. This means `portator web` works as a self-contained binary with no external file dependencies.

### Markdown Rendering

Portator uses [md4c](https://github.com/mity/md4c) (vendored `md4c.c`/`md4c.h`, `md4c-html.c`/`md4c-html.h`) to render Markdown files as HTML on the fly.

When the web server receives a GET request for any `.md` file (e.g. `/docs/README.md`), it reads the file, converts it to HTML via md4c, wraps it in a styled page template, and returns it as `text/html`.

To retrieve the raw Markdown source instead of the rendered HTML, append `?raw` to the URL:

```
GET /docs/README.md        → rendered HTML page
GET /docs/README.md?raw    → raw Markdown text
```

md4c is configured with CommonMark extensions enabled (tables, strikethrough, autolinks).

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
4. Zip guest ELFs into the host binary under `apps/<name>/bin/<name>`
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
11. **Shared `include/` and `src/` folders** — Moved `portator.h` from an embedded C string to `include/portator.h`. Vendored cJSON (`include/cJSON.h`, `src/cJSON.c`). All three are embedded in the APE zip and extracted on demand via `portator init`, `portator new`, or `portator build`.
12. **Custom syscall: `version` (0x7006)** — Returns the Portator version string to the guest. Uses probe/fill calling convention (call with NULL to get size, then with buffer to fill).
13. **Custom syscall: `list` (0x7007)** — Dynamically scans for built programs and returns a JSON array to the guest. Scans both local `<name>/bin/<name>` and bundled `/zip/apps/<name>/bin/<name>`.
14. **Program discovery** — `portator run` checks local `<name>/bin/<name>` first, then falls back to `/zip/apps/<name>/bin/<name>`. The `list` syscall scans both locations, with local taking precedence.
15. **Bundled apps in APE zip** — `make` stages all built app binaries into `apps/<name>/bin/<name>` inside the APE zip store.
16. **`portator list`** — Guest program that calls the `list` syscall, parses the JSON with cJSON, and prints discovered program names. Works as both `portator list` and `portator run list`.
17. **`portator help`** — Displays version, available commands, and website URL. Also shown when running `portator` with no arguments.
18. **Snake game** — Console-based snake game running as a guest under Portator.

### Next Steps

1. **Implement remaining syscalls (0x7000–0x7005)** — `present`, `poll`, `exit`, `ws_send`, `ws_recv`, `app_type`. These are needed before graphical or web guests can work.
2. **Web server app launching** — `portator web` currently serves static files. It needs to launch guest apps in the browser: terminal (xterm.js/GhostTTY) for console apps, canvas for graphical apps, custom pages for web apps.
3. **Scaffold `gui` and `web` templates** — `portator new gui` and `portator new web` currently produce stub files. Flesh out with working examples once syscall handlers and the web server are in place.
4. **`libportator` guest library** — Decide whether to build a C++ class hierarchy (`PortatorConsoleApp`, `PortatorGraphicalApp`, `PortatorWebApp`) or stay with the current C + inline syscall approach. C++ would give cleaner structure for gui/web apps but adds complexity for console apps that don't need it.
5. **Consolidate app data packaging** — Needs design work. Currently, app data (templates, license files, etc.) is handled ad-hoc: the host extracts specific paths from `/zip/` before launching guests, and the Makefile has special-case `cp -r` lines for each app's data. This should be unified:
   - `portator new` should create a `zip/` folder inside the project directory. Any files placed in `<name>/zip/` are the app's bundled data.
   - When `make` packages apps into the APE zip, it should automatically include `<name>/zip/*` under `apps/<name>/` (alongside `bin/`). For example, `license/zip/data/blink/LICENSE` becomes `apps/license/data/blink/LICENSE` in the zip.
   - The host's `portator <cmd>` dispatch should have a generic mechanism to extract an app's data from `/zip/apps/<name>/` before running it, rather than per-command extraction logic.
   - Guests read their data from `apps/<name>/` on local disk (extracted by the host) since they cannot access the host's `/zip/` filesystem directly.
   - This means the current `license/data/` and `new/templates/` folders would move to `license/zip/data/` and `new/zip/templates/` respectively, making the convention explicit: "put your bundled data in `zip/`."
   - The Makefile loop becomes: for each app, copy `<name>/bin/<name>` to `apps/<name>/bin/<name>`, and if `<name>/zip/` exists, copy its contents to `apps/<name>/`.
   - This also solves the web app static assets question: a web app puts its `wwwroot/` inside `zip/`, and it gets bundled automatically.
  
Suggested layout in the zip:

  apps/portator/bin/portator (DOES NOT EXIST, NOT A CLIENT PROGRAM)
  apps/portator/data/wwwroot/*
  apps/portator/LICENSE

  apps/list/bin/list
  apps/list/LICENSE

  apps/license/bin/license
  apps/license/LICENSE
  apps/license/data/<library>/LICENSE (Put other licenses here)

  apps/init/bin/init
  apps/init/LICENSE
  apps/init/data/include/*
  apps/init/data/src/*

  apps/new/bin/new
  apps/new/LICENSE
  apps/new/data/templates/console/__NAME__.c

  apps/snake/bin/snake
  apps/snake/LICENSE


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
