#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

static int mkdirs(const char *path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) && errno != EEXIST) return -1;
    return 0;
}

static int copy_file(const char *src, const char *dst) {
    /* Create parent directory */
    char parent[4096];
    snprintf(parent, sizeof(parent), "%s", dst);
    char *slash = strrchr(parent, '/');
    if (slash) {
        *slash = '\0';
        if (mkdirs(parent)) {
            fprintf(stderr, "init: cannot create directory %s\n", parent);
            return -1;
        }
    }

    FILE *fin = fopen(src, "r");
    if (!fin) {
        fprintf(stderr, "init: not found: %s\n", src);
        return -1;
    }
    FILE *fout = fopen(dst, "w");
    if (!fout) {
        fclose(fin);
        fprintf(stderr, "init: cannot create %s\n", dst);
        return -1;
    }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fin)) > 0)
        fwrite(buf, 1, n, fout);
    fclose(fin);
    fclose(fout);
    return 0;
}

/* Recursively copy zipdir/ to outdir/ */
static int copy_tree(const char *zipdir, const char *outdir) {
    DIR *d = opendir(zipdir);
    if (!d) {
        fprintf(stderr, "init: cannot open %s\n", zipdir);
        return -1;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char src[4096], dst[4096];
        snprintf(src, sizeof(src), "%s/%s", zipdir, ent->d_name);
        snprintf(dst, sizeof(dst), "%s/%s", outdir, ent->d_name);

        struct stat st;
        if (stat(src, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (copy_tree(src, dst)) { closedir(d); return -1; }
        } else {
            printf("  %s\n", dst);
            if (copy_file(src, dst)) { closedir(d); return -1; }
        }
    }
    closedir(d);
    return 0;
}

int main(void) {
    printf("Extracting shared files...\n");

    if (copy_tree("zip/include", "include")) return 1;
    if (copy_tree("zip/src", "src")) return 1;

    printf("Done.\n");
    return 0;
}
