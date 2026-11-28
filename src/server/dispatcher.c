#include "dispatcher.h"
#include "account.h"
#include "lobby.h"
#include "log.h"
#include "protocol.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void log_game_event(Dispatcher *server, size_t slot, const char *line) {
    char event[256];
    size_t length = strncmp(line, "QUESTION|", 9) == 0 ? 8 : strcspn(line, "\r\n");
    snprintf(event, sizeof(event), "GAME user=%s qid=%u %.*s",
             server->lobby.sessions[slot].username, server->game.qid, (int)length, line);
    if (log_event(server->config->log, LOG_INFO, event) < 0) {
        fputs("Could not write game log\n", stderr);
    }
}

static int game_emit(void *context, size_t slot, const char *line) {
    Dispatcher *server = context;
    const Session *session = &server->lobby.sessions[slot];
    if (session->joined && strcmp(session->username, server->game.players[slot].username) == 0) {
        if (strncmp(line, "SCORE", 5) != 0) {
            log_game_event(server, slot, line);
        }
        return server->emit(server->context, slot, line);
    }
    return 0;
}

void dispatcher_tick(Dispatcher *server, int64_t now) {
    server->now = now;
    if (server->game.questions != NULL) {
        game_tick(&server->game, now);
        server->lobby.running = server->game.phase != GAME_IDLE;
    }
}

static const char *play_command(Dispatcher *server, size_t slot, ProtocolMessage *message,
                                int64_t now) {
    char *end;
    errno = 0;
    unsigned long id = strtoul(message->fields[1], &end, 10);
    if (errno != 0 || *end != '\0' || id == 0 || id > UINT_MAX || message->fields[1][0] < '0' ||
        message->fields[1][0] > '9') {
        return "400|BAD_REQUEST\n";
    }
    if (strcmp(message->fields[0], "SKIP") == 0) {
        return game_skip(&server->game, slot, (unsigned)id, now);
    }
    if (strlen(message->fields[2]) != 1) {
        return "400|BAD_REQUEST\n";
    }
    return game_answer(&server->game, slot, (unsigned)id, message->fields[2][0], now);
}

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
    if (server->game.phase != GAME_IDLE && server->game.players[slot].active) {
        log_game_event(server, slot, "DISCONNECTED");
    }
    session_logout(&server->lobby, slot);
    game_leave(&server->game, slot, server->now);
    dispatcher_tick(server, server->now);
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
        if (server->config->questions == NULL || server->config->question_count == 0) {
            return "503|NO_QUESTIONS\n";
        }
        return lobby_start(&server->lobby, slot);
    }
    if (strcmp(command, "ANSWER") == 0 || strcmp(command, "SKIP") == 0) {
        return play_command(server, slot, message, now);
    }
    if (strcmp(command, "LEAVE") == 0) {
        if (server->game.phase != GAME_IDLE) {
            log_game_event(server, slot, "LEAVE");
        }
        lobby_leave(&server->lobby, slot);
    } else {
        if (server->game.phase != GAME_IDLE) {
            log_game_event(server, slot, "LOGOUT");
        }
        session_logout(&server->lobby, slot);
    }
    game_leave(&server->game, slot, now);
    dispatcher_tick(server, now);
    return "200|OK\n";
}

int dispatcher_command(Dispatcher *server, size_t slot, char *line, int64_t now) {
    ProtocolMessage message;
    const char *reply = "400|BAD_REQUEST\n";
    if (protocol_parse(line, &message) < 0) {
        return server->emit(server->context, slot, reply);
    }
    const char *commands[] = {"REGISTER", "LOGIN",  "PING",   "JOIN", "START",
                              "LEAVE",    "LOGOUT", "ANSWER", "SKIP"};
    size_t index;
    for (index = 0; index < sizeof(commands) / sizeof(commands[0]); ++index) {
        if (strcmp(message.fields[0], commands[index]) == 0) {
            break;
        }
    }
    if (index == sizeof(commands) / sizeof(commands[0])) {
        return server->emit(server->context, slot, "400|UNSUPPORTED_COMMAND\n");
    }
    size_t expected = index < 2 || index == 7 ? 3U : (index == 8 ? 2U : 1U);
    if (message.count != expected) {
        return server->emit(server->context, slot, reply);
    }
    server->now = now;
    server->lobby.sessions[slot].last_active = now;
    reply = handle(server, slot, &message, now);
    if (index < 2 || index == 4 || index == 6 || index >= 7) {
        char event[96];
        snprintf(event, sizeof(event), "%s %s", commands[index],
                 strcmp(reply, "200|OK\n") == 0 ? "accepted" : "rejected");
        if (index >= 7 && strcmp(reply, "200|RECEIVED\n") == 0) {
            snprintf(event, sizeof(event), "%s accepted", commands[index]);
        }
        if (log_event(server->config->log, LOG_INFO, event) < 0) {
            fputs("Could not write authentication log\n", stderr);
        }
    }
    int result = server->emit(server->context, slot, reply);
    if (index >= 3 && index <= 6 && strcmp(reply, "200|OK\n") == 0) {
        dispatcher_broadcast(server);
    }
    if (index == 4 && strcmp(reply, "200|OK\n") == 0) {
        game_start(&server->game, &server->lobby, server->config->questions,
                   server->config->question_count, now, game_emit, server);
    }
    return result;
}
