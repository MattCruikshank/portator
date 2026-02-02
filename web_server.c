#include "web_server.h"
#include "mongoose.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

static struct mg_mgr s_mgr;
static pthread_t s_thread;
static volatile int s_running;
static char s_wwwroot[512];

static void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        struct mg_http_serve_opts opts = {.root_dir = s_wwwroot};
        mg_http_serve_dir(c, hm, &opts);
    }
}

static void *server_thread(void *arg) {
    (void)arg;
    while (s_running) {
        mg_mgr_poll(&s_mgr, 100);
    }
    return NULL;
}

int WebServerStart(int port, const char *wwwroot) {
    char addr[64];
    struct mg_connection *c;

    snprintf(s_wwwroot, sizeof(s_wwwroot), "%s", wwwroot ? wwwroot : "wwwroot");

    mg_mgr_init(&s_mgr);
    snprintf(addr, sizeof(addr), "http://0.0.0.0:%d", port);

    c = mg_http_listen(&s_mgr, addr, ev_handler, NULL);
    if (!c) {
        fprintf(stderr, "portator: cannot listen on %s\n", addr);
        mg_mgr_free(&s_mgr);
        return -1;
    }

    s_running = 1;
    if (pthread_create(&s_thread, NULL, server_thread, NULL) != 0) {
        fprintf(stderr, "portator: cannot create server thread\n");
        s_running = 0;
        mg_mgr_free(&s_mgr);
        return -1;
    }

    fprintf(stderr, "portator: web server listening on http://localhost:%d\n", port);
    return 0;
}

void WebServerStop(void) {
    if (s_running) {
        s_running = 0;
        pthread_join(s_thread, NULL);
        mg_mgr_free(&s_mgr);
    }
}
