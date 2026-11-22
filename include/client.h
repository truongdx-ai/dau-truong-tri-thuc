#ifndef DTTT_CLIENT_H
#define DTTT_CLIENT_H

int client_run(const char *host, const char *port);
void ui_help(void);
void ui_message(const char *line);

#endif
