# ZipFS: Virtual Filesystem for APE Zip Store

## Problem

Cosmopolitan's `/zip/` is a virtual path provided by the host runtime. When mounted directly as hostfs, guest programs hang indefinitely on file access. The underlying issue is that hostfs expects normal directory semantics, but `/zip/` is synthesized by cosmopolitan and doesn't behave like a real directory.

We need guests to read from `/zip/apps/<name>/...` for bundled app data, and the Blink loader needs to load executables from `/zip/apps/<name>/bin/<name>`.

## Solution

Implement a new `zipfs` filesystem type in Blink's VFS layer. Instead of mounting `/zip/` as hostfs, zipfs provides a custom read path that queries cosmopolitan's `/zip/` at the host level with full control over access patterns.

## Blink VFS Architecture

Blink's VFS uses three core structures:

| Structure | Purpose |
|-----------|---------|
| `VfsSystem` | Registered filesystem type (hostfs, devfs, procfs, zipfs) |
| `VfsDevice` | A mounted instance of a filesystem |
| `VfsInfo` | A file or directory node in the VFS tree |

Filesystem types implement operations through `struct VfsOps`, which defines ~100 function pointers for file operations. Not all must be implemented—unsupported operations can be NULL or return EROFS/ENOSYS.

### How Mounting Works

```c
int VfsMount(const char *source, const char *target, const char *fstype,
             u64 flags, const void *data);
```

1. VFS traverses to `target` directory
2. Finds `fstype` in registered systems
3. Calls `fstype->ops.Init()` to create device and mount
4. Links mount to target directory
5. Future path traversals through `target` switch to the new device

## ZipFS Design

### Data Structures

```c
// zipfs.h

struct ZipfsDevice {
    // Could cache zip file handle or directory listing
    // For now, just marker that this is zipfs
};

struct ZipfsInfo {
    u32 mode;           // S_IFDIR or S_IFREG
    u64 size;           // File size (0 for directories)
    char *hostpath;     // Full host path: "/zip/apps/foo/..."
    DIR *dirstream;     // Open directory stream (for readdir)
    int fd;             // Open file descriptor (for read)
    off_t pos;          // Current read position
};
```

### Operations to Implement

**Essential (must implement):**

| Operation | Purpose |
|-----------|---------|
| `Init` | Create device, root VfsInfo |
| `Freeinfo` | Clean up ZipfsInfo |
| `Freedevice` | Clean up ZipfsDevice |
| `Finddir` | Find child by name in directory |
| `Traverse` | Walk a path, create VfsInfo nodes |
| `Open` | Open a file for reading |
| `Close` | Close file |
| `Read` | Read bytes from file |
| `Pread` | Read at offset |
| `Seek` | Seek in file |
| `Stat` | Get file metadata by path |
| `Fstat` | Get file metadata by VfsInfo |
| `Access` | Check file permissions |
| `Opendir` | Open directory for iteration |
| `Readdir` | Read next directory entry |
| `Closedir` | Close directory |
| `Rewinddir` | Reset directory position |

**Read-only stubs (return EROFS):**

```c
Write, Pwrite, Writev, Pwritev    // No writing
Mkdir, Mkfifo                      // No creation
Unlink, Rmdir                      // No deletion
Link, Symlink, Rename              // No linking
Chmod, Fchmod, Chown, Fchown       // No permission changes
Ftruncate                          // No truncation
Utime, Futime                      // No time changes
```

### Key Implementation Details

#### ZipfsInit

```c
int ZipfsInit(const char *source, u64 flags, const void *data,
              struct VfsDevice **device, struct VfsMount **mount) {
    // Allocate device
    struct VfsDevice *dev = calloc(1, sizeof(*dev));
    dev->ops = &g_zipfs.ops;
    dev->data = calloc(1, sizeof(struct ZipfsDevice));

    // Create root VfsInfo for /zip
    struct VfsInfo *root = calloc(1, sizeof(*root));
    root->device = dev;
    root->mode = S_IFDIR | 0555;
    root->data = calloc(1, sizeof(struct ZipfsInfo));
    ((struct ZipfsInfo *)root->data)->hostpath = strdup("/zip");
    ((struct ZipfsInfo *)root->data)->mode = S_IFDIR;

    dev->root = root;

    // Create mount
    *mount = calloc(1, sizeof(struct VfsMount));
    (*mount)->device = dev;

    *device = dev;
    return 0;
}
```

#### ZipfsFinddir / ZipfsTraverse

The key insight: call host-level `stat()` and `opendir()` on the constructed `/zip/...` path.

```c
int ZipfsFinddir(struct VfsInfo *parent, const char *name,
                 struct VfsInfo **output) {
    struct ZipfsInfo *pinfo = parent->data;

    // Construct host path: parent->hostpath + "/" + name
    char hostpath[PATH_MAX];
    snprintf(hostpath, sizeof(hostpath), "%s/%s", pinfo->hostpath, name);

    // Stat on host
    struct stat st;
    if (stat(hostpath, &st) == -1) {
        return -1;  // errno already set (ENOENT, etc.)
    }

    // Create VfsInfo for this entry
    struct VfsInfo *info = calloc(1, sizeof(*info));
    info->device = parent->device;
    info->parent = parent;
    info->name = strdup(name);
    info->namelen = strlen(name);
    info->mode = st.st_mode;
    info->ino = st.st_ino;

    struct ZipfsInfo *zinfo = calloc(1, sizeof(*zinfo));
    zinfo->hostpath = strdup(hostpath);
    zinfo->mode = st.st_mode;
    zinfo->size = st.st_size;
    zinfo->fd = -1;
    info->data = zinfo;

    VfsAcquireInfo(parent);
    *output = info;
    return 0;
}
```

#### ZipfsOpen / ZipfsRead

```c
int ZipfsOpen(struct VfsInfo *parent, const char *name, int flags,
              int mode, struct VfsInfo **output) {
    // Only allow read-only access
    if ((flags & O_ACCMODE) != O_RDONLY) {
        return erofs();
    }

    // Find the file
    struct VfsInfo *info;
    if (ZipfsFinddir(parent, name, &info) == -1) {
        return -1;
    }

    // Open the host file
    struct ZipfsInfo *zinfo = info->data;
    zinfo->fd = open(zinfo->hostpath, O_RDONLY);
    if (zinfo->fd == -1) {
        VfsFreeInfo(info);
        return -1;
    }
    zinfo->pos = 0;

    *output = info;
    return 0;
}

ssize_t ZipfsRead(struct VfsInfo *info, void *buf, size_t size) {
    struct ZipfsInfo *zinfo = info->data;
    if (zinfo->fd == -1) {
        return ebadf();
    }

    ssize_t n = read(zinfo->fd, buf, size);
    if (n > 0) {
        zinfo->pos += n;
    }
    return n;
}
```

#### ZipfsReaddir

```c
int ZipfsOpendir(struct VfsInfo *info, struct VfsInfo **output) {
    struct ZipfsInfo *zinfo = info->data;

    if (!S_ISDIR(zinfo->mode)) {
        return enotdir();
    }

    zinfo->dirstream = opendir(zinfo->hostpath);
    if (!zinfo->dirstream) {
        return -1;
    }

    VfsAcquireInfo(info);
    *output = info;
    return 0;
}

struct dirent *ZipfsReaddir(struct VfsInfo *info) {
    struct ZipfsInfo *zinfo = info->data;
    if (!zinfo->dirstream) {
        return NULL;
    }
    return readdir(zinfo->dirstream);
}
```

### Registration

In `vfs.c`, add to `VfsInit()`:

```c
extern struct VfsSystem g_zipfs;

int VfsInit(const char *prefix) {
    // ... existing registrations ...
    unassert(!VfsRegister(&g_hostfs));
    unassert(!VfsRegister(&g_devfs));
    unassert(!VfsRegister(&g_procfs));
    unassert(!VfsRegister(&g_zipfs));  // Add this
    // ...
}
```

Define the VfsSystem in `zipfs.c`:

```c
struct VfsSystem g_zipfs = {
    .name = "zipfs",
    .nodev = true,
    .ops = {
        .Init = ZipfsInit,
        .Freeinfo = ZipfsFreeInfo,
        .Freedevice = ZipfsFreeDevice,
        .Finddir = ZipfsFinddir,
        .Traverse = ZipfsTraverse,
        .Open = ZipfsOpen,
        .Close = ZipfsClose,
        .Read = ZipfsRead,
        .Pread = ZipfsPread,
        .Seek = ZipfsSeek,
        .Stat = ZipfsStat,
        .Fstat = ZipfsFstat,
        .Access = ZipfsAccess,
        .Opendir = ZipfsOpendir,
        .Readdir = ZipfsReaddir,
        .Closedir = ZipfsClosedir,
        .Rewinddir = ZipfsRewinddir,
        // All write operations NULL or return EROFS
    }
};
```

### Mounting in Portator

In `main.c`, after `VfsInit()`:

```c
// Mount zipfs at /zip so guests can access bundled content
if (VfsMount("", "/zip", "zipfs", 0, NULL) == -1) {
    perror("VfsMount /zip");
    // Non-fatal: local apps still work
}
```

## Why This Should Work

The hang issue with hostfs mounting `/zip/` likely occurs because:

1. Hostfs tries to open `/zip/` as a file descriptor
2. Cosmopolitan's `/zip/` intercept may not behave correctly when used this way
3. Path resolution loops or blocks

Zipfs avoids this by:

1. Never mounting `/zip/` as a device
2. Using direct `stat()` / `open()` / `opendir()` calls on `/zip/...` paths
3. Managing our own file descriptors and state
4. Having full control over when and how `/zip/` is accessed

## Files to Create

```
blink/blink/zipfs.h     # ZipfsDevice, ZipfsInfo structures
blink/blink/zipfs.c     # All zipfs operations, g_zipfs definition
```

## Files to Modify

```
blink/blink/vfs.c       # Add VfsRegister(&g_zipfs) in VfsInit()
main.c                  # Add VfsMount("", "/zip", "zipfs", 0, NULL)
```

## Testing

Create a test guest:

```c
// test_zipfs.c
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

int main() {
    printf("Testing /zip access...\n\n");

    // List /zip root
    DIR *d = opendir("/zip");
    if (!d) {
        perror("opendir /zip");
        return 1;
    }

    printf("/zip contents:\n");
    struct dirent *ent;
    while ((ent = readdir(d))) {
        printf("  %s\n", ent->d_name);
    }
    closedir(d);

    // Stat a known file
    printf("\nStat /zip/apps/hello/bin/hello:\n");
    struct stat st;
    if (stat("/zip/apps/hello/bin/hello", &st) == 0) {
        printf("  size: %ld\n", (long)st.st_size);
        printf("  mode: 0%o\n", st.st_mode & 0777);
    } else {
        perror("  stat");
    }

    // Read a file
    printf("\nRead /zip/apps/hello/LICENSE:\n");
    FILE *f = fopen("/zip/apps/hello/LICENSE", "r");
    if (f) {
        char buf[256];
        if (fgets(buf, sizeof(buf), f)) {
            printf("  First line: %s", buf);
        }
        fclose(f);
    } else {
        perror("  fopen");
    }

    return 0;
}
```

## Alternative: Cached Directory Tree

If direct host access to `/zip/` is still problematic, an alternative is to cache the entire zip directory structure at mount time:

1. At `ZipfsInit()`, walk `/zip/` recursively using host APIs
2. Store entries in a hash table: `{path -> (mode, size, offset)}`
3. `Finddir` / `Traverse` look up in hash table
4. `Read` opens the specific file path on demand

This adds memory overhead but isolates zipfs from any `/zip/` access quirks during normal operation.

## Conclusion

Zipfs provides a clean abstraction for exposing cosmopolitan's `/zip/` store to Blink guests. By implementing our own filesystem type, we control exactly how `/zip/` is accessed and avoid the hanging issues encountered with direct hostfs mounting.
