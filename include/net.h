#ifndef DTTT_NET_H
#define DTTT_NET_H

#include "question.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    const char *users_path;
    FILE *log;
    int64_t (*now_ms)(void);
    int64_t idle_timeout_ms;
    /* Borrowed question bank; the caller keeps it alive during net_run. */
    const Question *questions;
    size_t question_count;
    /* Zero selects the default 30-second qualification window. */
    int64_t qualification_timeout_ms;
} NetConfig;

int net_listen(uint16_t port);
int net_run(int listener, const volatile sig_atomic_t *stop, const NetConfig *config);

#endif
