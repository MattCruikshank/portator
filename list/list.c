#include <stdio.h>
#include "portator.h"

int main(void) {
    char ver[64];
    if (portator_version(ver, sizeof(ver)) > 0)
        printf("Running on %s\n", ver);
    printf("Hello from list!\n");
    return 0;
}
