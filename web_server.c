#include "web_server.h"
#include "civetweb/civetweb.h"

#include <stdio.h>
#include <string.h>

static struct mg_context *s_ctx;

int WebServerStart(int port, const char *wwwroot) {
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);

    const char *options[] = {
        "listening_ports", portstr,
        "document_root", wwwroot ? wwwroot : "/zip/wwwroot",
        "num_threads", "1",
        NULL
    };

    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));

    s_ctx = mg_start(&callbacks, NULL, options);
    if (!s_ctx) {
        fprintf(stderr, "portator: cannot start web server on port %s\n", portstr);
        return -1;
    }

    fprintf(stderr, "portator: web server listening on http://localhost:%d\n", port);
    return 0;
}

void WebServerStop(void) {
    if (s_ctx) {
        mg_stop(s_ctx);
        s_ctx = NULL;
    }
}
