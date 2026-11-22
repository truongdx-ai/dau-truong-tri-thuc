#ifndef DTTT_CLIENT_H
#define DTTT_CLIENT_H

#include <stddef.h>
#include <stdio.h>

typedef struct {
    char question_id[21];
    int has_question;
    int answer_sent;
    int raw;
} ClientUI;

int client_run(const char *host, const char *port, int raw);
void ui_help(void);
void ui_message(const char *line);
void ui_receive(ClientUI *ui, const char *line, FILE *output);
/* Return the wire length including LF, 0 for local feedback, or -1 if too large. */
int ui_prepare(ClientUI *ui, const char *line, char *output, size_t capacity, FILE *feedback);

#endif
