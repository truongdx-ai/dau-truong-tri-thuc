#ifndef DTTT_DISPATCHER_H
#define DTTT_DISPATCHER_H

#include "net.h"
#include "session.h"

typedef int (*Emit)(void *context, size_t slot, const char *line);

typedef struct {
    Lobby lobby;
    const NetConfig *config;
    Emit emit;
    void *context;
} Dispatcher;

int dispatcher_command(Dispatcher *server, size_t slot, char *line, int64_t now);
void dispatcher_disconnect(Dispatcher *server, size_t slot);
void dispatcher_broadcast(Dispatcher *server);

#endif
