#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

#include "portator.h"
#include <cjson/cJSON.h>
#include "mustach-cjson.h"

static int makedir(const char *path) {
    if (mkdir(path, 0755) && errno != EEXIST) {
        fprintf(stderr, "new: cannot create directory %s\n", path);
        return -1;
    }
    return 0;
}

/* Read an entire file into a malloc'd buffer. Returns NULL on failure. */
static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, len, f);
    buf[n] = '\0';
    fclose(f);
    if (out_len) *out_len = n;
    return buf;
}


/* Process a template directory: for each file in apps/new/templates/<type>/,
   apply mustache substitution and write to <name>/ */
static int process_templates(const char *type, const char *name) {
    char tmpldir[4096];
    snprintf(tmpldir, sizeof(tmpldir), "zip/apps/new/templates/%s", type);

    fprintf(stderr, "I %s(%d): tmpldir: %s\n", __FILE__, __LINE__, tmpldir);

    DIR *d = opendir(tmpldir);
    if (!d) {
        fprintf(stderr, "new: template not found for type: %s\n", type);
        return -1;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "name", name);
    fprintf(stderr, "I %s(%d): name: %s\n", __FILE__, __LINE__, name);

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        fprintf(stderr, "I %s(%d): ent->d_name: %s\n", __FILE__, __LINE__, ent->d_name);

        char srcpath[4096];
        snprintf(srcpath, sizeof(srcpath), "%s/%s", tmpldir, ent->d_name);

        fprintf(stderr, "I %s(%d): srcpath: %s\n", __FILE__, __LINE__, srcpath);

        /* Read template */
        size_t tmpl_len;
        char *tmpl = read_file(srcpath, &tmpl_len);
        if (!tmpl) continue;

        fprintf(stderr, "I %s(%d): tmpl: \n=====\n%s\n=====\n", __FILE__, __LINE__, tmpl);

        /* Determine output filename: replace __NAME__ with project name */
        char outname[256];
        const char *p = strstr(ent->d_name, "__NAME__");
        if (p) {
            size_t prefix_len = p - ent->d_name;
            snprintf(outname, sizeof(outname), "%.*s%s%s",
                     (int)prefix_len, ent->d_name, name, p + 8);
        } else {
            snprintf(outname, sizeof(outname), "%s", ent->d_name);
        }
        fprintf(stderr, "I %s(%d): outname: %s\n", __FILE__, __LINE__, outname);

        /* Render template with mustache */
        char *result = NULL;
        size_t result_len = 0;
        int rc = mustach_cJSON_mem(tmpl, tmpl_len, data, 0, &result, &result_len);
        free(tmpl);

        fprintf(stderr, "I %s(%d): rc: %d\n", __FILE__, __LINE__, rc);

        if (rc != MUSTACH_OK || !result) {
            fprintf(stderr, "new: template error in %s\n", ent->d_name);
            continue;
        }

        fprintf(stderr, "I %s(%d): result: \n=====\n%s\n=====\n", __FILE__, __LINE__, result);

        /* Write output */
        char outpath[4096];
        snprintf(outpath, sizeof(outpath), "%s/%s", name, outname);
        FILE *f = fopen(outpath, "w");
        if (f) {
            fwrite(result, 1, result_len, f);
            fclose(f);
            printf("  %s\n", outpath);
        }
        free(result);
    }
    closedir(d);
    cJSON_Delete(data);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: portator new <type> <name>\n");
        fprintf(stderr, "Types: console, gui, web\n");
        return 1;
    }

    const char *type = argv[1];
    const char *name = argv[2];

    if (strcmp(type, "console") != 0 &&
        strcmp(type, "gui") != 0 &&
        strcmp(type, "web") != 0) {
        fprintf(stderr, "new: unknown type: %s\n", type);
        fprintf(stderr, "Types: console, gui, web\n");
        return 1;
    }

    /* Create project directories */
    if (makedir(name)) return 1;
    char path[4096];
    snprintf(path, sizeof(path), "%s/bin", name);
    if (makedir(path)) return 1;

    /* Process templates */
    printf("Created %s/\n", name);
    if (process_templates(type, name)) return 1;

    printf("  %s/bin/\n", name);
    printf("\nBuild with:\n");
    printf("  portator build %s\n", name);
    printf("Run with:\n");
    printf("  portator run %s\n", name);
    printf("\nHint: Ensure -I./include is in your editor's include path\n");

    return 0;
}
