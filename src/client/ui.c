#include "client.h"
#include "protocol.h"

#include <stdio.h>
#include <string.h>

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
