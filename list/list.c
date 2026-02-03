#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include "portator.h"

int main(void) {
    char ver[64];
    if (portator_version(ver, sizeof(ver)) > 0)
        printf("Running on %s\n", ver);

    DIR *dir = opendir("/zipfs/apps");
    if (!dir) {
        printf("Failed to open /zipfs/apps\n");
        return 1;
    }

    printf("Available apps:\n");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        printf("  %s\n", entry->d_name);
    }

    closedir(dir);
    return 0;
}
