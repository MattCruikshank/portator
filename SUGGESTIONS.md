# Suggestions

## High Impact

### 1. Re-enable JIT
JIT is disabled (`--disable-jit`), tracked in Claude-TODO.md. This is likely a significant performance win for guest apps, especially compute-heavy ones like TCC compiling code. Worth investigating what's blocking it and getting it back on.

### 2. Implement remaining custom syscalls (0x7000–0x7005)
`portator.h` defines syscalls for PRESENT, POLL, EXIT, WS_SEND, WS_RECV, and APP_TYPE, but only VERSION (0x7006), LIST (0x7007), and LAUNCH (0x7008) are implemented in `HandlePortatorSyscall()`. The graphical and web app types depend on these — POLL for input events, WS_SEND/WS_RECV for WebSocket communication, PRESENT for framebuffer display. Without them, only console apps work.

### 3. Revert loader.c anonymous mmap change
Tracked in Claude-TODO.md. This Blink fork change is flagged as needing reversion — presumably it's causing issues or was a temporary workaround.

### 4. Expose Cosmopolitan cross-platform APIs to guests (Future-Cosmo-APIs.md)
The design doc proposes DNS resolution (Phase 1), HTTP fetch (Phase 2), and other conveniences (Phase 3) via new syscalls. This would solve the core problem: musl-gcc guests fail on non-Linux platforms because Blink's syscall translation is incomplete for networking. Even just Phase 1 (DNS) would unblock network-capable guest apps.

### 5. Video streaming for graphical guest apps (StreamingVideo.md)
The design doc recommends JPEG-over-WebSocket using `stb_image_write.h` as Phase 1. This would enable graphical guest apps to render to a browser canvas — currently the GFX app type exists in the syscall definitions but has no working path. The PRESENT syscall (0x7000) needs this.

### 6. `portator run` is undocumented but required internally
`CmdRunForked` expects `argv[2]` as the app name, and the LAUNCH syscall and build system both construct `"run"` argv. But "run" isn't in the help text. The direct dispatch (`portator <name>`) works by synthesizing a `run` argv. Meanwhile, the `Commandv` fallback at `main.c:703` means arbitrary ELF paths can be passed as a command. Decide whether to document `run` or remove the fallback.

### 7. Guest sandbox boundaries
Guests can traverse `..` to access the host filesystem above cwd. The CLAUDE.md acknowledges this ("not a security sandbox"), but if Portator is ever distributed for running untrusted guest apps, filesystem sandboxing would be needed. The VFS layer could potentially enforce path restrictions.

## Medium Impact

### 8. Build system always links all vendored C files
`BuildWithMuslGcc` and `BuildWithTcc` hardcode linking `cJSON.c`, `mustach.c`, etc. for every guest app. Apps like `snake` that don't use templates or JSON pay in binary size. Consider a per-app manifest (`guests/<name>/build.conf`) or letting apps opt out of unused libraries.

### 9. `BuildAppListJson` dedup uses `strstr` (potential false match)
At `main.c:349`, duplicate app names are checked by searching the entire JSON string for `"<name>"`. This could false-match if one app name is a substring of another (e.g., `"ls"` matching inside `"false"`). Use a separate seen-names array instead.

### 10. Web UI is minimal
`wwwroot/` has just an `index.html` and an image. WebSocket support is compiled in (`-DUSE_WEBSOCKET`) but no handler is registered. This could become a web terminal or IDE for guest apps. Plan.md mentions a web server app-launching design — connecting these would make `portator web` much more useful.

### 11. No incremental builds for `portator build`
Every build recompiles from scratch. Fine for single-file guests today, but as apps grow (multi-file projects, C++ guests), consider timestamp checks or dependency tracking.

### 12. Windows self-modifying APE packaging (JUGGLE-WINDOWS-PACKAGE.md)
The design for exec-chain-based self-modification on Windows is written up but not implemented. This would solve Windows's file-locking problem when `portator package` needs to modify the running binary. The design is sound (fork to helper, modify, re-exec).

### 13. Broader language support beyond C
Plan.md lists tier-1 support for C++, Rust, Zig, and Go, and tier-2 for C#, Swift, and Nim. Currently only C (musl-gcc/TCC), C++ (g++), and Go are implemented in the build system. README.md mentions compiler detection for rustc, zig, dotnet, swiftc, and nim, but `CmdBuild` in main.c doesn't handle them. Either implement these or remove the claims from README.md.

### 14. libportator C++ wrapper library
Plan.md lists this as a next step — a C++ wrapper around the raw syscall API in `portator.h`. Would make guest development friendlier for C++ apps.

### 15. README.md lists commands that don't exist
README.md mentions `get`, `test`, `watch`, and `clean` as CLI commands, but none of these are implemented in `main.c`. Either implement them or remove from the docs.

### 16. Testing gaps
Testing.md documents manual Windows testing from 2026-02-04. There are no automated tests. Consider:
- A smoke test that builds and runs each guest app
- A syscall test guest that exercises VERSION, LIST, and LAUNCH
- CI integration (even just `make && cd publish && ./portator list`)

## Low Impact / Polish

### 17. Credits say "Claude Opus 4.5"
At `main.c:650`, the credits reference "Anthropic (Claude Opus 4.5)". Update or remove the version to avoid staleness.

### 18. Legacy data paths in Makefile
Lines 131-133 have hardcoded legacy path copies for `new/templates`, `mojozork/data`, `license/data`. These should be migrated to the standard `guests/<name>/zip/` convention so the legacy code can be removed.

### 19. `HasCommand` only checks 3 directories
`main.c:72` checks `/usr/local/bin`, `/usr/bin`, `/bin` but not `$PATH`. The actual build uses `execlp` which does search `$PATH`, so `HasCommand` can give false negatives (e.g., `musl-gcc` installed in `~/bin`). Either search `$PATH` properly or skip the check and let the build attempt fail gracefully.

### 20. Go guest support is undocumented
`hello-go` is built in the Makefile but not mentioned in help text or `new` scaffolding. Either document it or add a Go template to `portator new`.

### 21. `portator new` templates may be incomplete
The `new` guest app scaffolds projects using Mustach templates, but only console/gui/web types are listed. With Go, Rust, Zig, etc. mentioned in Plan.md, the scaffolding should eventually support these languages too.

### 22. Web server hardcodes `/zip/wwwroot`
`CmdWeb` passes `/zip/wwwroot` to CivetWeb, which works for bundled content, but there's no way to serve a local `wwwroot/` directory for development. Consider falling back to local `wwwroot/` if it exists.

### 23. TCC bootstrap dependency
TCC.md notes that building TCC requires musl-gcc on the host. If the goal is full self-hosting (build TCC with TCC), this circular dependency needs a bootstrap path — perhaps ship a pre-built TCC binary for initial bootstrapping.

### 24. Stale/aspirational documentation
Several docs describe features as if implemented when they're still planned. A pass through README.md, Plan.md, and Building.md to clearly separate "done" from "planned" would reduce confusion. Plan.md's "Next" section is a good model — apply the same clarity everywhere.

### 25. `portator web` blocks on `getchar()`
The web server starts, then `main.c:575` waits for Enter to stop. This works interactively but makes it unusable as a background service. Consider adding a `--daemon` flag or signal-based shutdown.
