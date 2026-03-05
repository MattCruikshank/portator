#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "portator.h"

static char cwd[4096] = "zip";

/* Normalize path: resolve ".." and remove trailing slash */
static void normalize_path(char *path) {
    /* Remove trailing slash */
    size_t len = strlen(path);
    while (len > 0 && path[len - 1] == '/')
        path[--len] = '\0';
}

/* Apply a relative or absolute cd target to cwd */
static void apply_cd(const char *target) {
    if (!target || strcmp(target, "/") == 0) {
        strcpy(cwd, "zip");
        return;
    }

    char tmp[4096];

    if (target[0] == '/') {
        /* Absolute: root is zip */
        snprintf(tmp, sizeof(tmp), "zip%s", target);
    } else if (strcmp(target, "..") == 0) {
        /* Go up one level, but not above zip */
        strncpy(tmp, cwd, sizeof(tmp));
        tmp[sizeof(tmp) - 1] = '\0';
        char *slash = strrchr(tmp, '/');
        if (slash && slash != tmp) {
            *slash = '\0';
            /* Don't go above "zip" */
            if (strlen(tmp) < 3 || strncmp(tmp, "zip", 3) != 0)
                strcpy(tmp, "zip");
        } else {
            strcpy(tmp, "zip");
        }
    } else {
        /* Relative */
        snprintf(tmp, sizeof(tmp), "%s/%s", cwd, target);
    }

    normalize_path(tmp);
    strncpy(cwd, tmp, sizeof(cwd));
    cwd[sizeof(cwd) - 1] = '\0';
}

/* Convert internal cwd (zip/...) to display path (/...) */
static const char *display_path(void) {
    if (strcmp(cwd, "zip") == 0) return "/";
    if (strncmp(cwd, "zip/", 4) == 0) return cwd + 3;
    return cwd;
}

/* Parse command line into argv-style tokens (modifies input) */
static int parse_args(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max_args - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    argv[argc] = NULL;
    return argc;
}

int main(void) {
    char ver[256];
    if (portator_version(ver, sizeof(ver)) > 0)
        printf("%s\n", ver);

    char line[256];
    for (;;) {
        printf("%s> ", display_path());
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin))
            break;
        line[strcspn(line, "\n")] = '\0';
        if (!line[0])
            continue;

        char *argv[32];
        int argc = parse_args(line, argv, 32);
        if (argc == 0) continue;

        if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0)
            break;

        if (strcmp(argv[0], "help") == 0) {
            printf("Commands:\n");
            printf("  ls [path]   - list directory contents\n");
            printf("  cd <path>   - change directory\n");
            printf("  cd ..       - go up one level\n");
            printf("  pwd         - print current directory\n");
            printf("  <app>       - run a guest app\n");
            printf("  exit        - leave the shell\n");
            continue;
        }

        if (strcmp(argv[0], "pwd") == 0) {
            printf("%s\n", display_path());
            continue;
        }

        if (strcmp(argv[0], "cd") == 0) {
            apply_cd(argc > 1 ? argv[1] : NULL);
            continue;
        }

        /* Launch guest app with PWD set */
        char pwd_env[4096];
        snprintf(pwd_env, sizeof(pwd_env), "PWD=%s", cwd);
        char *envp[] = { pwd_env, NULL };
        char **launch_argv = (argc > 1) ? argv + 1 : NULL;

        long rc = portator_launch_ex(argv[0], launch_argv, envp);
        if (rc < 0)
            printf("shell: not found: %s\n", argv[0]);
        else if (rc != 0)
            printf("shell: %s exited with status %ld\n", argv[0], rc);
    }
    return 0;
}
