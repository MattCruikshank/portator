#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include "portator.h"

static void print_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        fwrite(buf, 1, n, stdout);
    fclose(f);
}

int main(void) {
    char ver[64];
    if (portator_version(ver, sizeof(ver)) > 0)
        printf("%s — Licenses\n\n", ver);

    const char *datadir = "zip/apps/license/data";
    DIR *d = opendir(datadir);
    if (!d) {
        fprintf(stderr, "license: cannot open %s\n", datadir);
        return 1;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        printf("────────────────────────────────────────────────────────────\n");
        printf("  %s\n", ent->d_name);
        printf("────────────────────────────────────────────────────────────\n\n");

        char path[4096];
        snprintf(path, sizeof(path), "%s/%s/LICENSE", datadir, ent->d_name);
        print_file(path);
        printf("\n");
    }
    closedir(d);
    return 0;
}
