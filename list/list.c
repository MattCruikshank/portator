#include <stdio.h>
#include <stdlib.h>
#include "portator.h"
#include <cjson/cJSON.h>

int main(void) {
    char ver[64];
    if (portator_version(ver, sizeof(ver)) > 0)
        printf("Running on %s\n", ver);

    long n = portator_list(NULL, 0);
    if (n <= 0) {
        printf("Failed to get app list size\n");
        return 1;
    }

    char *buf = malloc(n);
    if (!buf) {
        printf("Out of memory\n");
        return 1;
    }

    if (portator_list(buf, n) <= 0) {
        printf("Failed to get app list\n");
        free(buf);
        return 1;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        printf("JSON parse error\n");
        return 1;
    }

    cJSON *apps = cJSON_GetObjectItemCaseSensitive(root, "apps");
    cJSON *app;
    cJSON_ArrayForEach(app, apps) {
        cJSON *name = cJSON_GetObjectItemCaseSensitive(app, "name");
        printf("  %s\n", name->valuestring);
    }

    cJSON_Delete(root);
    return 0;
}
