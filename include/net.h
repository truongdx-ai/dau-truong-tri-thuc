#ifndef DTTT_NET_H
#define DTTT_NET_H

#include <signal.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    const char *users_path;
    FILE *log;
    int64_t (*now_ms)(void);
    int64_t idle_timeout_ms;
} NetConfig;

int net_listen(uint16_t port);
int net_run(int listener, const volatile sig_atomic_t *stop, const NetConfig *config);

#endif
