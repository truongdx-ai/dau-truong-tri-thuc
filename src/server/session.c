#include "session.h"

#include <string.h>

int session_login(Lobby *lobby, size_t slot, const char *username, int64_t now) {
    if (slot >= SESSION_LIMIT || username[0] == '\0' || strlen(username) > 32) {
        return -1;
    }
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        if (strcmp(lobby->sessions[i].username, username) == 0) {
            return -1;
        }
    }
    Session *session = &lobby->sessions[slot];
    if (session->username[0] != '\0') {
        return -1;
    }
    memcpy(session->username, username, strlen(username) + 1);
    session->last_active = now;
    return 0;
}

void session_logout(Lobby *lobby, size_t slot) {
    lobby->sessions[slot] = (Session){0};
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        if (lobby->sessions[i].joined) {
            return;
        }
    }
    lobby->running = 0;
}

int session_expired(const Session *session, int64_t now, int64_t timeout) {
    return session->username[0] != '\0' && now - session->last_active >= timeout;
}
