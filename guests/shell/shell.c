#include <stdio.h>
#include <string.h>
#include "portator.h"

int main(void) {
    char ver[256];
    if (portator_version(ver, sizeof(ver)) > 0)
        printf("%s\n", ver);

    char line[256];
    for (;;) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin))
            break;
        /* Strip newline */
        line[strcspn(line, "\n")] = '\0';
        if (!line[0])
            continue;
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0)
            break;
        if (strcmp(line, "help") == 0) {
            printf("Type an app name to run it, or:\n");
            printf("  list    - show available apps\n");
            printf("  help    - show this message\n");
            printf("  exit    - leave the shell\n");
            continue;
        }
        long rc = portator_launch(line);
        if (rc < 0)
            printf("shell: not found: %s\n", line);
        else if (rc != 0)
            printf("shell: %s exited with status %ld\n", line, rc);
    }
    return 0;
}
