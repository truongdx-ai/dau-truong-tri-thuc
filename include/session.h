#ifndef DTTT_SESSION_H
#define DTTT_SESSION_H

#include <stddef.h>
#include <stdint.h>

#define SESSION_LIMIT 100
#define SESSION_IDLE_MS 300000

typedef struct {
    char username[33];
    int joined;
    uint64_t joined_order;
    int64_t last_active;
} Session;

typedef struct {
    Session sessions[SESSION_LIMIT];
    uint64_t next_order;
    int running;
} Lobby;

/* Slot belongs to an accepted connection. Username comes from successful authentication. */
int session_login(Lobby *lobby, size_t slot, const char *username, int64_t now);
void session_logout(Lobby *lobby, size_t slot);
int session_expired(const Session *session, int64_t now, int64_t timeout);

#endif
