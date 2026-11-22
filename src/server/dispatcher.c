#include "dispatcher.h"
#include "account.h"
#include "linebuf.h"
#include "lobby.h"
#include "log.h"
#include "protocol.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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

void dispatcher_tick(Dispatcher *server, int64_t now) {
    QualificationResult result = qualification_finish(&server->qualification, now);
    if (result == QUALIFY_MAIN_CHOSEN) {
        int main_slot = server->qualification.main_slot;
        const char *name = server->lobby.sessions[main_slot].username;
        for (size_t i = 0; i < SESSION_LIMIT; ++i) {
            if (!server->qualification.players[i].participating ||
                !server->lobby.sessions[i].joined) {
                continue;
            }
            char role[64];
            if ((int)i == main_slot) {
                snprintf(role, sizeof(role), "ROLE|MAIN\n");
            } else {
                snprintf(role, sizeof(role), "ROLE|SUB|%s\n", name);
            }
            server->emit(server->context, i, role);
        }
        char event[80];
        snprintf(event, sizeof(event), "QUALIFY MAIN %s", name);
        if (log_event(server->config->log, LOG_INFO, event) < 0) {
            fputs("Could not write qualification log\n", stderr);
        }
    } else if (result == QUALIFY_FINISHED_EMPTY) {
        broadcast_line(server, "GAME_OVER|NO_WINNER\n");
        server->lobby.running = 0;
        if (log_event(server->config->log, LOG_INFO, "QUALIFY NO_WINNER") < 0) {
            fputs("Could not write qualification log\n", stderr);
        }
    } else if (result == QUALIFY_EMPTY_ROOM) {
        server->lobby.running = 0;
    }
}

void dispatcher_disconnect(Dispatcher *server, size_t slot, int64_t now) {
    int joined = server->lobby.sessions[slot].joined;
    if (server->lobby.sessions[slot].username[0] != '\0') {
        if (log_event(server->config->log, LOG_INFO, "SESSION closed") < 0) {
            fputs("Could not write session log\n", stderr);
        }
    }
    qualification_leave(&server->qualification, slot);
    session_logout(&server->lobby, slot);
    if (joined) {
        dispatcher_broadcast(server);
    }
    dispatcher_tick(server, now);
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

static const char *start_game(Dispatcher *server, size_t slot, int64_t now, char *opening,
                              size_t capacity) {
    const char *reply = lobby_start(&server->lobby, slot);
    if (strcmp(reply, "200|OK\n") != 0) {
        return reply;
    }
    if (server->config->questions == NULL || server->config->question_count == 0) {
        server->lobby.running = 0;
        return "500|NO_QUESTIONS\n";
    }
    if (server->last_qid == UINT64_MAX) {
        server->lobby.running = 0;
        return "500|QUESTION_ERROR\n";
    }
    const Question *question =
        &server->config->questions[server->last_qid % server->config->question_count];
    uint64_t qid = server->last_qid + 1;
    int64_t duration = server->config->qualification_timeout_ms > 0
                           ? server->config->qualification_timeout_ms
                           : QUALIFICATION_DURATION_MS;
    char id[24], seconds[24];
    snprintf(id, sizeof(id), "%llu", (unsigned long long)qid);
    snprintf(seconds, sizeof(seconds), "%lld",
             (long long)(duration / 1000 + (duration % 1000 != 0)));
    const char *fields[] = {"QUESTION",
                            id,
                            "QUALIFY",
                            seconds,
                            question->text,
                            question->options[0],
                            question->options[1],
                            question->options[2],
                            question->options[3]};
    if (protocol_format(opening, capacity, fields, 9) < 0 ||
        qualification_start(&server->qualification, &server->lobby, question, qid, now, duration) <
            0) {
        server->lobby.running = 0;
        opening[0] = '\0';
        return "500|QUESTION_ERROR\n";
    }
    server->last_qid = qid;
    return reply;
}

static const char *answer_question(Dispatcher *server, size_t slot, ProtocolMessage *message,
                                   int64_t now) {
    const char *id = message->fields[1];
    const char *choice = message->fields[2];
    if (strspn(id, "0123456789") != strlen(id) || choice[0] < 'A' || choice[0] > 'D' ||
        choice[1] != '\0') {
        return "400|BAD_REQUEST\n";
    }
    errno = 0;
    char *end;
    unsigned long long qid = strtoull(id, &end, 10);
    if (errno == ERANGE || *end != '\0' || qid == 0) {
        return "400|BAD_REQUEST\n";
    }
    QualificationAnswerResult result =
        qualification_answer(&server->qualification, slot, (uint64_t)qid, choice[0], now);
    const char *responses[] = {"200|RECEIVED\n", "403|NOT_ALLOWED\n", "408|TIMEOUT\n",
                               "409|ALREADY_ANSWERED\n", "409|WRONG_QUESTION\n"};
    if (result == QUALIFY_RECEIVED &&
        log_event(server->config->log, LOG_INFO, "ANSWER received") < 0) {
        fputs("Could not write answer log\n", stderr);
    }
    return responses[result];
}

static const char *handle(Dispatcher *server, size_t slot, ProtocolMessage *message, int64_t now,
                          char *opening, size_t capacity) {
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
        return start_game(server, slot, now, opening, capacity);
    }
    if (strcmp(command, "ANSWER") == 0) {
        return answer_question(server, slot, message, now);
    }
    qualification_leave(&server->qualification, slot);
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
    const char *commands[] = {"REGISTER", "LOGIN", "PING",   "JOIN",
                              "START",    "LEAVE", "LOGOUT", "ANSWER"};
    size_t index;
    for (index = 0; index < sizeof(commands) / sizeof(commands[0]); ++index) {
        if (strcmp(message.fields[0], commands[index]) == 0) {
            break;
        }
    }
    if (index == sizeof(commands) / sizeof(commands[0])) {
        return server->emit(server->context, slot, "400|UNSUPPORTED_COMMAND\n");
    }
    if (message.count != (index < 2 || index == 7 ? 3U : 1U)) {
        return server->emit(server->context, slot, reply);
    }
    server->lobby.sessions[slot].last_active = now;
    char opening[LINEBUF_MAX + 1] = {0};
    reply = handle(server, slot, &message, now, opening, sizeof(opening));
    if (index < 2 || index == 6) {
        char event[96];
        snprintf(event, sizeof(event), "%s %s", commands[index],
                 strcmp(reply, "200|OK\n") == 0 ? "accepted" : "rejected");
        if (log_event(server->config->log, LOG_INFO, event) < 0) {
            fputs("Could not write authentication log\n", stderr);
        }
    }
    int result = server->emit(server->context, slot, reply);
    if (index >= 3 && index <= 6 && strcmp(reply, "200|OK\n") == 0) {
        dispatcher_broadcast(server);
        if (opening[0] != '\0') {
            broadcast_line(server, opening);
        }
    }
    dispatcher_tick(server, now);
    return result;
}
