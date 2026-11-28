#ifndef DTTT_CLIENT_H
#define DTTT_CLIENT_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    unsigned qid;
    int64_t deadline_ms;
    int timing;
    int last_remaining;
} UiState;

int client_run(const char *host, const char *port);
void ui_help(void);
void ui_message(const char *line);
void ui_receive(UiState *state, const char *line, int64_t now_ms);
int ui_remaining(const UiState *state, int64_t now_ms);
void ui_tick(UiState *state, int64_t now_ms, FILE *output);

#endif
