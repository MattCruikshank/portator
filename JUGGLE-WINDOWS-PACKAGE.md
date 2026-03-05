# Juggle Packaging: Exec Chain for Self-Modifying APE on Windows

## Problem

Portator's APE binary doubles as a ZIP archive. After building a guest app, we want to package it back into the binary's zip store. On Linux/macOS, this is straightforward — you can modify a running executable's file. On Windows, the OS locks the running executable, so no process can write to it while portator is running.

## Solution: Exec Chain

Use a sequence of `exec` calls to ensure the binary being modified is never the one currently running.

### Flow

```
User runs: portator package foo
```

1. **portator builds foo** — compiles `foo/bin/foo` with musl-gcc as usual
2. **portator extracts `package`** — writes the bundled `package` APE from `/zip/apps/package/bin/package` to disk (e.g. `package/bin/package`)
3. **portator execs `package`** — `exec("package/bin/package", "foo", "/absolute/path/to/portator")`. Portator's process is replaced (on Windows, Cosmopolitan emulates this by spawning `package` and exiting). The portator binary file is now unlocked.
4. **`package` modifies portator** — adds `foo/bin/foo` (and any `foo/zip/` data) into portator's zip store. Writes to a temp file and renames for crash safety.
5. **`package` execs portator** — `exec("/absolute/path/to/portator", "--post-package", "package/bin/package", "foo")`. The `package` process is replaced/exits. The `package` binary file is now unlocked.
6. **portator cleans up** — deletes `package/bin/package` from disk, prints "Packaged foo into portator".

### Why This Works on Windows

At each step, the file being modified is not the running executable:

| Step | Running process | File modified | Locked? |
|------|----------------|---------------|---------|
| 3 | portator → execs package | (none yet) | portator unlocked when process exits |
| 4 | package | portator binary | portator is not running, not locked |
| 5 | package → execs portator | (none yet) | package unlocked when process exits |
| 6 | portator | deletes package binary | package is not running, not locked |

On Linux/macOS, `exec` is a true process replacement, so the same logic holds — the old binary is unreferenced after exec.

### User Experience

The user runs one command and ends up back in portator with the app packaged:

```
$ portator package foo
Building foo...
Built foo/bin/foo
Packaging foo into portator...
Packaged foo into portator.
$
```

The exec chain is invisible to the user. It looks like a single operation.

## Design Details

### `package` is a host APE, not a guest

`package` must be compiled with cosmocc (not musl-gcc) because it needs native filesystem access and zip manipulation. It's a different class of bundled tool compared to guest apps like `snake` or `list` that run under Blink emulation.

### Absolute paths

Portator must pass its own absolute path to `package` so that `package` can:
- Find the binary to modify
- Exec back to it after packaging

On Linux, `/proc/self/exe` gives the absolute path. On other platforms, resolve `argv[0]` or use Cosmopolitan's `GetProgramExecutableName()`.

### Crash safety

`package` should never modify the portator binary in-place. Instead:
1. Copy portator to a temp file (e.g. `portator.tmp`)
2. Append the new app's files to the temp file's zip store
3. Rename `portator.tmp` → `portator` (atomic on most filesystems)

If `package` crashes mid-write, the original binary is untouched.

### Deleting `package` on Windows

After exec'ing back to portator, the `package` binary on disk needs to be cleaned up. On Windows, Cosmopolitan's exec emulation spawns a child and exits the parent — but there may be a brief window where the `package` process hasn't fully terminated. If `DeleteFile` fails with a sharing violation:
- Retry after a short delay
- Or mark it for deletion on next startup (e.g. check for stale `package/bin/package` at launch)

### Platform branching

On Linux/macOS, the exec chain is not strictly necessary — `zip -r portator ...` works on a running binary. Portator could skip the exec dance and modify itself directly. However, using the same exec chain on all platforms keeps the code path simple and well-tested. Platform branching (via `IsWindows()`) is an optimization, not a requirement.

## Future: Other Self-Modifying Operations

The same exec chain pattern works for any operation that needs to modify the portator binary:
- `portator package foo` — add a newly built app
- `portator unpackage foo` — remove an app from the zip store
- `portator update` — self-update by downloading a new binary and swapping
- `portator install foo.zip` — add a pre-built app from an external zip
