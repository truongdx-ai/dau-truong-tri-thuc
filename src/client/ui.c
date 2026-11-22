#include "client.h"
#include "linebuf.h"
#include "protocol.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ui_help(void) {
    puts("Dau truong tri thuc");
    puts("Dang ky       REGISTER|ten|mat_khau");
    puts("Dang nhap     LOGIN|ten|mat_khau");
    puts("Vao phong     JOIN");
    puts("Roi phong     LEAVE");
    puts("Bat dau       START  (host, it nhat 2 nguoi)");
    puts("Tra loi       A, B, C hoac D (chap nhan chu thuong)");
    puts("Lenh day du   ANSWER|qid|A van duoc ho tro");
    puts("Dang xuat     LOGOUT");
    puts("Thoat         Ctrl+D");
    puts("PLAYERS hien danh sach phong cho. Host la nguoi JOIN som nhat con trong phong.");
    puts("Chay client voi -r de xem cac dong giao thuc goc.");
    fflush(stdout);
}

void ui_message(const char *line) {
    puts(line);
    fflush(stdout);
}

static int number(const char *text, uint64_t *value) {
    if (text[0] == '\0' || strlen(text) > 20 || strspn(text, "0123456789") != strlen(text)) {
        return -1;
    }
    errno = 0;
    char *end;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno == ERANGE || *end != '\0') {
        return -1;
    }
    *value = (uint64_t)parsed;
    return 0;
}

static void clear_question(ClientUI *ui) {
    ui->question_id[0] = '\0';
    ui->has_question = 0;
    ui->answer_sent = 0;
}

void ui_receive(ClientUI *ui, const char *line, FILE *output) {
    char copy[LINEBUF_MAX];
    ProtocolMessage message;
    size_t length = strlen(line);
    if (length >= sizeof(copy)) {
        fprintf(output, "%s\n", line);
        fflush(output);
        return;
    }
    memcpy(copy, line, length + 1);
    int parsed = protocol_parse(copy, &message) == 0;
    uint64_t qid, seconds;
    if (parsed && message.count == 9 && strcmp(message.fields[0], "QUESTION") == 0 &&
        (strcmp(message.fields[2], "QUALIFY") == 0 || strcmp(message.fields[2], "TURN") == 0) &&
        number(message.fields[1], &qid) == 0 && qid > 0 &&
        number(message.fields[3], &seconds) == 0) {
        char id[21];
        snprintf(id, sizeof(id), "%llu", (unsigned long long)qid);
        if (!ui->has_question || strcmp(ui->question_id, id) != 0) {
            ui->answer_sent = 0;
        }
        memcpy(ui->question_id, id, strlen(id) + 1);
        ui->has_question = 1;
        if (ui->raw) {
            fprintf(output, "%s\n", line);
        } else {
            fprintf(output, "\n[Cau hoi #%s - %s]\n%s\n", id,
                    strcmp(message.fields[2], "QUALIFY") == 0 ? "Vong mo man" : "Luot choi",
                    message.fields[4]);
            for (size_t i = 0; i < 4; ++i) {
                fprintf(output, "%c. %s\n", (int)('A' + i), message.fields[5 + i]);
            }
            fprintf(output, "Thoi gian: %llu giay. Nhap A/B/C/D roi nhan Enter.\n",
                    (unsigned long long)seconds);
        }
    } else if (parsed && strcmp(message.fields[0], "ROLE") == 0 &&
               ((message.count == 2 && strcmp(message.fields[1], "MAIN") == 0) ||
                (message.count == 3 && strcmp(message.fields[1], "SUB") == 0))) {
        clear_question(ui);
        if (ui->raw) {
            fprintf(output, "%s\n", line);
        } else if (message.count == 2) {
            fprintf(output, "Ban la nguoi choi chinh.\n");
        } else {
            fprintf(output, "Ban la nguoi choi phu. Nguoi choi chinh: %s.\n", message.fields[2]);
        }
    } else if (parsed && message.count == 2 && strcmp(message.fields[0], "GAME_OVER") == 0 &&
               strcmp(message.fields[1], "NO_WINNER") == 0) {
        clear_question(ui);
        fprintf(output, "%s\n", ui->raw ? line : "Ket thuc: khong co nguoi chien thang.");
    } else if (!ui->raw && parsed && message.count == 3 &&
               strcmp(message.fields[0], "PLAYERS") == 0 && number(message.fields[1], &qid) == 0) {
        fprintf(output, "Phong cho (%llu nguoi): %s\n", (unsigned long long)qid, message.fields[2]);
    } else if (!ui->raw && strcmp(line, "PLAYERS_BEGIN") == 0) {
        fprintf(output, "Danh sach nguoi choi:\n");
    } else if (!ui->raw && parsed && message.count == 2 &&
               strcmp(message.fields[0], "PLAYERS_PART") == 0) {
        fprintf(output, "  %s\n", message.fields[1]);
    } else if (!ui->raw && strcmp(line, "PLAYERS_END") == 0) {
        fprintf(output, "Ket thuc danh sach.\n");
    } else {
        static const struct {
            const char *wire;
            const char *text;
        } responses[] = {
            {"200|OK", "Thanh cong."},
            {"200|RECEIVED", "Server da nhan dap an. Dang cho ket qua..."},
            {"400|BAD_REQUEST", "Lenh khong hop le."},
            {"400|UNSUPPORTED_COMMAND", "Lenh chua duoc ho tro."},
            {"401|LOGIN_REQUIRED", "Ban can dang nhap truoc."},
            {"401|WRONG_CREDENTIAL", "Ten dang nhap hoac mat khau khong dung."},
            {"403|NOT_HOST", "Chi chu phong moi duoc bat dau."},
            {"403|NOT_ALLOWED", "Ban khong duoc tra loi cau hoi nay."},
            {"408|TIMEOUT", "Da het thoi gian tra loi."},
            {"409|ALREADY_ANSWERED", "Ban da tra loi cau hoi nay."},
            {"409|WRONG_QUESTION", "Ma cau hoi da cu hoac khong dung."},
            {"409|USER_EXISTS", "Tai khoan da ton tai."},
            {"409|ALREADY_LOGGED_IN", "Tai khoan hoac ket noi nay da dang nhap."},
            {"409|GAME_RUNNING", "Tran dau dang dien ra."},
            {"412|NOT_ENOUGH_PLAYERS", "Can it nhat 2 nguoi de bat dau."},
            {"500|STORAGE_ERROR", "Server gap loi luu tru."},
            {"500|NO_QUESTIONS", "Server chua co bo cau hoi."},
            {"500|QUESTION_ERROR", "Server khong the tao cau hoi."},
        };
        const char *display = line;
        if (!ui->raw) {
            for (size_t i = 0; i < sizeof(responses) / sizeof(responses[0]); ++i) {
                if (strcmp(line, responses[i].wire) == 0) {
                    display = responses[i].text;
                    break;
                }
            }
        }
        fprintf(output, "%s\n", display);
    }
    fflush(output);
}

int ui_prepare(ClientUI *ui, const char *line, char *output, size_t capacity, FILE *feedback) {
    size_t length = strlen(line);
    const char *start = line;
    while (*start == ' ' || *start == '\t') {
        ++start;
    }
    size_t trimmed = strlen(start);
    while (trimmed > 0 && (start[trimmed - 1] == ' ' || start[trimmed - 1] == '\t')) {
        --trimmed;
    }
    char letter = start[0];
    if (letter >= 'a' && letter <= 'z') {
        letter = (char)(letter - 'a' + 'A');
    }
    if (trimmed == 1 && letter >= 'A' && letter <= 'Z') {
        const char *error = NULL;
        if (letter > 'D') {
            error = "Chi chon A, B, C hoac D.";
        } else if (!ui->has_question) {
            error = "Chua co cau hoi de tra loi.";
        } else if (ui->answer_sent) {
            error = "Ban da gui dap an cho cau hoi nay. Hay cho ket qua.";
        }
        if (error != NULL) {
            fprintf(feedback, "%s\n", error);
            fflush(feedback);
            return 0;
        }
        char choice[] = {letter, '\0'};
        const char *fields[] = {"ANSWER", ui->question_id, choice};
        int result = protocol_format(output, capacity, fields, 3);
        if (result >= 0) {
            ui->answer_sent = 1;
        }
        return result;
    }
    if (length >= LINEBUF_MAX || capacity < length + 2) {
        return -1;
    }
    memcpy(output, line, length);
    output[length] = '\n';
    output[length + 1] = '\0';
    if (strcmp(line, "LEAVE") == 0 || strcmp(line, "LOGOUT") == 0) {
        clear_question(ui);
    } else if (ui->has_question) {
        char copy[LINEBUF_MAX];
        memcpy(copy, line, length + 1);
        ProtocolMessage message;
        uint64_t id, current;
        if (protocol_parse(copy, &message) == 0 && message.count == 3 &&
            strcmp(message.fields[0], "ANSWER") == 0 && number(message.fields[1], &id) == 0 &&
            number(ui->question_id, &current) == 0 && id == current &&
            message.fields[2][0] >= 'A' && message.fields[2][0] <= 'D' &&
            message.fields[2][1] == '\0') {
            ui->answer_sent = 1;
        }
    }
    return (int)(length + 1);
}
