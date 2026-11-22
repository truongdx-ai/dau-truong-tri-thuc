#include "dispatcher.h"
#include "account.h"
#include "lobby.h"
#include "log.h"
#include "protocol.h"

#include <stdio.h>
#include <string.h>

static void broadcast_line(Dispatcher *server, const char *line) {
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        if (server->lobby.sessions[i].joined) {
            server->emit(server->context, i, line);
        }
    }
}

void dispatcher_broadcast(Dispatcher *server) {
    char roster[SESSION_LIMIT * 33 + 32];
    int length = lobby_players(&server->lobby, roster, sizeof(roster));
    if (length <= 1024) {
        broadcast_line(server, roster);
        return;
    }
    /* Long rosters use a bracketed list of bounded chunks. */
    broadcast_line(server, "PLAYERS_BEGIN\n");
    char *names = strchr(strchr(roster, '|') + 1, '|') + 1;
    char *cursor = names;
    while (*cursor != '\n' && *cursor != '\0') {
        char chunk[1025] = "PLAYERS_PART|";
        size_t used = strlen(chunk);
        while (*cursor != '\n' && *cursor != '\0') {
            size_t name_length = strcspn(cursor, ",\n");
            if (used + name_length + 2 > 1024) {
                break;
            }
            memcpy(chunk + used, cursor, name_length);
            used += name_length;
            cursor += name_length;
            if (*cursor == ',') {
                chunk[used++] = ',';
                ++cursor;
            }
        }
        if (chunk[used - 1] == ',') {
            --used;
        }
        chunk[used++] = '\n';
        chunk[used] = '\0';
        broadcast_line(server, chunk);
    }
    broadcast_line(server, "PLAYERS_END\n");
}

void dispatcher_disconnect(Dispatcher *server, size_t slot) {
    int joined = server->lobby.sessions[slot].joined;
    if (server->lobby.sessions[slot].username[0] != '\0') {
        if (log_event(server->config->log, LOG_INFO, "SESSION closed") < 0) {
            fputs("Could not write session log\n", stderr);
        }
    }
    session_logout(&server->lobby, slot);
    if (joined) {
        dispatcher_broadcast(server);
    }
}

static const char *authenticate(Dispatcher *server, size_t slot, ProtocolMessage *message,
                                int64_t now) {
    if (server->lobby.sessions[slot].username[0] != '\0') {
        return "409|ALREADY_LOGGED_IN\n";
    }
    AccountResult result =
        account_authenticate(server->config->users_path, message->fields[1], message->fields[2]);
    if (result == ACCOUNT_ERROR) {
        return "500|STORAGE_ERROR\n";
    }
    if (result != ACCOUNT_OK) {
        return "401|WRONG_CREDENTIAL\n";
    }
    return session_login(&server->lobby, slot, message->fields[1], now) == 0
               ? "200|OK\n"
               : "409|ALREADY_LOGGED_IN\n";
}

static const char *register_user(Dispatcher *server, ProtocolMessage *message) {
    AccountResult result =
        account_register(server->config->users_path, message->fields[1], message->fields[2]);
    const char *responses[] = {"200|OK\n", "400|BAD_REQUEST\n", "409|USER_EXISTS\n",
                               "500|STORAGE_ERROR\n"};
    return responses[result];
}

static const char *handle(Dispatcher *server, size_t slot, ProtocolMessage *message, int64_t now) {
    const char *command = message->fields[0];
    if (strcmp(command, "REGISTER") == 0) {
        return register_user(server, message);
    }
    if (strcmp(command, "LOGIN") == 0) {
        return authenticate(server, slot, message, now);
    }
    if (strcmp(command, "PING") == 0) {
        return "PONG\n";
    }
    if (strcmp(command, "JOIN") == 0) {
        return lobby_join(&server->lobby, slot);
    }
    if (server->lobby.sessions[slot].username[0] == '\0') {
        return "401|LOGIN_REQUIRED\n";
    }
    if (strcmp(command, "START") == 0) {
        return lobby_start(&server->lobby, slot);
    }
    if (strcmp(command, "LEAVE") == 0) {
        lobby_leave(&server->lobby, slot);
    } else {
        session_logout(&server->lobby, slot);
    }
    return "200|OK\n";
}

int dispatcher_command(Dispatcher *server, size_t slot, char *line, int64_t now) {
    ProtocolMessage message;
    const char *reply = "400|BAD_REQUEST\n";
    if (protocol_parse(line, &message) < 0) {
        return server->emit(server->context, slot, reply);
    }
    const char *commands[] = {"REGISTER", "LOGIN", "PING", "JOIN", "START", "LEAVE", "LOGOUT"};
    size_t index;
    for (index = 0; index < sizeof(commands) / sizeof(commands[0]); ++index) {
        if (strcmp(message.fields[0], commands[index]) == 0) {
            break;
        }
    }
    if (index == sizeof(commands) / sizeof(commands[0])) {
        return server->emit(server->context, slot, "400|UNSUPPORTED_COMMAND\n");
    }
    if (message.count != (index < 2 ? 3U : 1U)) {
        return server->emit(server->context, slot, reply);
    }
    server->lobby.sessions[slot].last_active = now;
    reply = handle(server, slot, &message, now);
    if (index < 2 || index == 6) {
        char event[96];
        snprintf(event, sizeof(event), "%s %s", commands[index],
                 strcmp(reply, "200|OK\n") == 0 ? "accepted" : "rejected");
        if (log_event(server->config->log, LOG_INFO, event) < 0) {
            fputs("Could not write authentication log\n", stderr);
        }
    }
    int result = server->emit(server->context, slot, reply);
    if (index >= 3 && strcmp(reply, "200|OK\n") == 0) {
        dispatcher_broadcast(server);
    }
    return result;
}
