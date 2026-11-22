#include "client.h"

#include <stdio.h>

void ui_help(void) {
    puts("Dau truong tri thuc");
    puts("Dang ky       REGISTER|ten|mat_khau");
    puts("Dang nhap     LOGIN|ten|mat_khau");
    puts("Vao phong     JOIN");
    puts("Roi phong     LEAVE");
    puts("Bat dau       START  (host, it nhat 2 nguoi)");
    puts("Tra loi       ANSWER|qid|A  (A, B, C hoac D; qid trong QUESTION)");
    puts("Dang xuat     LOGOUT");
    puts("Thoat         Ctrl+D");
    puts("PLAYERS hien danh sach phong cho. Host la nguoi JOIN som nhat con trong phong.");
    fflush(stdout);
}

void ui_message(const char *line) {
    puts(line);
    fflush(stdout);
}
