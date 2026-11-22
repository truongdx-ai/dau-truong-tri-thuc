#include "lobby.h"

#include <stdio.h>
#include <string.h>

int lobby_host(const Lobby *lobby) {
    int host = -1;
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        if (lobby->sessions[i].joined &&
            (host < 0 || lobby->sessions[i].joined_order < lobby->sessions[host].joined_order)) {
            host = (int)i;
        }
    }
    return host;
}

const char *lobby_join(Lobby *lobby, size_t slot) {
    Session *session = &lobby->sessions[slot];
    if (session->username[0] == '\0') {
        return "401|LOGIN_REQUIRED\n";
    }
    if (lobby->running) {
        return "409|GAME_RUNNING\n";
    }
    if (!session->joined) {
        session->joined = 1;
        session->joined_order = ++lobby->next_order;
    }
    return "200|OK\n";
}

void lobby_leave(Lobby *lobby, size_t slot) {
    lobby->sessions[slot].joined = 0;
    if (lobby_host(lobby) < 0) {
        lobby->running = 0;
    }
}

const char *lobby_start(Lobby *lobby, size_t slot) {
    if (lobby->running) {
        return "409|GAME_RUNNING\n";
    }
    if (lobby_host(lobby) != (int)slot) {
        return "403|NOT_HOST\n";
    }
    size_t count = 0;
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        count += (size_t)lobby->sessions[i].joined;
    }
    if (count < 2) {
        return "412|NOT_ENOUGH_PLAYERS\n";
    }
    lobby->running = 1;
    return "200|OK\n";
}

int lobby_players(const Lobby *lobby, char *output, size_t capacity) {
    /* Chunk lists on the network layer if the roster exceeds the wire limit. */
    char names[SESSION_LIMIT * 33 + 1] = {0};
    size_t length = 0, count = 0;
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        if (lobby->sessions[i].joined) {
            int n = snprintf(names + length, sizeof(names) - length, "%s%s", count ? "," : "",
                             lobby->sessions[i].username);
            length += (size_t)n;
            ++count;
        }
    }
    int n = snprintf(output, capacity, "PLAYERS|%zu|%s\n", count, names);
    return n < 0 || (size_t)n >= capacity ? -1 : n;
}
