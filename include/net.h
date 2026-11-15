#ifndef DTTT_NET_H
#define DTTT_NET_H

#include <signal.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    const char *users_path;
    FILE *log;
} NetConfig;

int net_listen(uint16_t port);
int net_run(int listener, const volatile sig_atomic_t *stop, const NetConfig *config);

#endif
