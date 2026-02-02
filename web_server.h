#ifndef WEB_SERVER_H_
#define WEB_SERVER_H_

/* Start the web server on the given port, serving files from wwwroot.
   Returns 0 on success, -1 on error. The server runs in a background thread. */
int WebServerStart(int port, const char *wwwroot);

/* Stop the web server and clean up. */
void WebServerStop(void);

#endif /* WEB_SERVER_H_ */
