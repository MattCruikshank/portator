#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "portator.h"

static void try_read(const char *path) {
    printf("\nTrying: %s\n", path);
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("  fopen failed: %s (errno=%d)\n", strerror(errno), errno);
        return;
    }
    char buf[256];
    if (fgets(buf, sizeof(buf), f)) {
        printf("  SUCCESS: %s", buf);
    } else {
        printf("  fgets failed: %s\n", strerror(errno));
    }
    fclose(f);
}

int main(void) {
    char ver[64];
    if (portator_version(ver, sizeof(ver)) > 0)
        printf("Running on %s\n", ver);

    printf("\n=== VFS Access Test ===\n");

    /* Try various paths to see what the guest can access */
    printf("\n-- Direct /zip/ paths (expect ENOENT) --\n");
    try_read("/zip/apps/test_vfs/data/hello.txt");
    try_read("/zip/test_vfs/data/hello.txt");
    try_read("/zip/data/hello.txt");

    printf("\n-- Relative paths (expect ENOENT) --\n");
    try_read("apps/test_vfs/data/hello.txt");
    try_read("./apps/test_vfs/data/hello.txt");
    try_read("data/hello.txt");
    try_read("./data/hello.txt");

    printf("\n-- /app/ mount (should work!) --\n");
    try_read("/app/data/hello.txt");
    try_read("/app/bin/test_vfs");

    printf("\n=== End Test ===\n");
    return 0;
}
