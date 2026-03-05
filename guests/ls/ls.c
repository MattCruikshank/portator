#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
    const char *pwd = getenv("PWD");
    if (!pwd) pwd = ".";

    char path[4096];
    if (argc > 1) {
        if (argv[1][0] == '/')
            snprintf(path, sizeof(path), "zip%s", argv[1]);
        else
            snprintf(path, sizeof(path), "%s/%s", pwd, argv[1]);
    } else {
        snprintf(path, sizeof(path), "%s", pwd);
    }

    DIR *d = opendir(path);
    if (!d) {
        fprintf(stderr, "ls: cannot open '%s'\n", argc > 1 ? argv[1] : path);
        return 1;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char fullpath[4096];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);

        struct stat st;
        if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode))
            printf("%s/\n", ent->d_name);
        else
            printf("%s\n", ent->d_name);
    }
    closedir(d);
    return 0;
}
