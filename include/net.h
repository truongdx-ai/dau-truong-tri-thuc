#ifndef DTTT_NET_H
#define DTTT_NET_H

#include <signal.h>
#include <stdint.h>

int net_listen(uint16_t port);
int net_run(int listener, const volatile sig_atomic_t *stop);

#endif
