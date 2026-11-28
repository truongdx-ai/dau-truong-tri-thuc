#include "client.h"
#include "protocol.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void display_result(const ProtocolMessage *message) {
    const char *event = message->fields[0];
    if (strcmp(event, "SCOREBOARD_BEGIN") == 0 && message->count == 2) {
        printf("Bang diem (qid %s)\n", message->fields[1]);
        puts("Nguoi choi                       Diem   Trang thai");
    } else if (strcmp(event, "SCORE") == 0 && message->count == 4) {
        printf("%-32s %5s   %s\n", message->fields[1], message->fields[2], message->fields[3]);
    } else if (strcmp(event, "GAME_OVER") == 0 && message->count >= 2) {
        if (strcmp(message->fields[1], "WINNER") == 0 && message->count == 4) {
            printf("Ket thuc van. Nguoi thang: %s, diem: %s\n", message->fields[2],
                   message->fields[3]);
        } else {
            printf("Ket thuc van: %s\n", message->fields[1]);
        }
        puts("Co the START lai khi phong du nguoi.");
    } else if (strcmp(event, "TURN_RESULT") == 0 && message->count == 4) {
        printf("Dap an dung: %s. Ket qua: %s. Diem cua ban: %s\n", message->fields[1],
               message->fields[2], message->fields[3]);
    }
}

void ui_help(void) {
    puts("Dau truong tri thuc");
    puts("Dang ky       REGISTER|ten|mat_khau");
    puts("Dang nhap     LOGIN|ten|mat_khau");
    puts("Vao phong     JOIN");
    puts("Roi phong     LEAVE");
    puts("Bat dau       START  (host, it nhat 2 nguoi)");
    puts("Tra loi       ANSWER|qid|A  (hoac B, C, D)");
    puts("Bo qua        SKIP|qid  (chi MAIN, toi da 2 lan)");
    puts("Dang xuat     LOGOUT");
    puts("Thoat         Ctrl+D");
    puts("PLAYERS hien danh sach phong cho. Host la nguoi JOIN som nhat con trong phong.");
    fflush(stdout);
}

void ui_message(const char *line) {
    puts(line);
    if (strlen(line) < 1025) {
        char copy[1025];
        memcpy(copy, line, strlen(line) + 1);
        ProtocolMessage message;
        if (protocol_parse(copy, &message) == 0) {
            display_result(&message);
        }
    }
    if (strncmp(line, "QUESTION|", 9) == 0 && strlen(line) < 1025) {
        char copy[1025];
        memcpy(copy, line, strlen(line) + 1);
        ProtocolMessage message;
        if (protocol_parse(copy, &message) == 0 && message.count == 9) {
            printf("Cau hoi %s (%s, %s giay)\n%s\n", message.fields[1], message.fields[2],
                   message.fields[3], message.fields[4]);
            printf("A) %s   B) %s   C) %s   D) %s\n", message.fields[5], message.fields[6],
                   message.fields[7], message.fields[8]);
            printf("Nhap ANSWER|%s|A/B/C/D hoac SKIP|%s\n", message.fields[1], message.fields[1]);
        }
    }
    fflush(stdout);
}

static unsigned positive_number(const char *text) {
    char *end;
    errno = 0;
    unsigned long number = strtoul(text, &end, 10);
    if (errno != 0 || *end != '\0' || number == 0 || number > UINT_MAX || text[0] < '0' ||
        text[0] > '9') {
        return 0;
    }
    return (unsigned)number;
}

void ui_receive(UiState *state, const char *line, int64_t now_ms) {
    ui_message(line);
    if (strncmp(line, "TURN_RESULT|", 12) == 0 || strncmp(line, "GAME_OVER|", 10) == 0 ||
        strncmp(line, "ELIMINATED|", 11) == 0 || strcmp(line, "200|RECEIVED") == 0) {
        state->timing = 0;
    }
    if (strncmp(line, "QUESTION|", 9) != 0 || strlen(line) >= 1025) {
        return;
    }
    char copy[1025];
    memcpy(copy, line, strlen(line) + 1);
    ProtocolMessage message;
    if (protocol_parse(copy, &message) == 0 && message.count == 9) {
        unsigned qid = positive_number(message.fields[1]);
        unsigned seconds = positive_number(message.fields[3]);
        if (qid > 0 && seconds > 0 && seconds <= 60) {
            state->qid = qid;
            state->deadline_ms = now_ms + (int64_t)seconds * 1000;
            state->timing = 1;
            state->last_remaining = -1;
        }
    }
}

int ui_remaining(const UiState *state, int64_t now_ms) {
    if (!state->timing) {
        return -1;
    }
    int64_t remaining = state->deadline_ms - now_ms;
    return remaining > 0 ? (int)((remaining + 999) / 1000) : 0;
}

void ui_tick(UiState *state, int64_t now_ms, FILE *output) {
    int remaining = ui_remaining(state, now_ms);
    if (remaining >= 0 && remaining != state->last_remaining) {
        if (output != NULL) {
            fprintf(output, "\rCon lai %d giay (qid %u)   %s", remaining, state->qid,
                    remaining == 0 ? "\n" : "");
            fflush(output);
        }
        state->last_remaining = remaining;
    }
}
