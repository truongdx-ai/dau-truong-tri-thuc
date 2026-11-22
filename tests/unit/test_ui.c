#include "client.h"
#include "linebuf.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

static ClientUI ui;
static FILE *screen;
static char wire[LINEBUF_MAX + 1];
static const char opening[] = "QUESTION|7|QUALIFY|30|2 + 2?|1|2|3|4";

void setUp(void) {
    ui = (ClientUI){0};
    screen = tmpfile();
    TEST_ASSERT_NOT_NULL(screen);
    memset(wire, 0, sizeof(wire));
}

void tearDown(void) { fclose(screen); }

static const char *display(void) {
    static char text[8192];
    fflush(screen);
    rewind(screen);
    size_t length = fread(text, 1, sizeof(text) - 1, screen);
    text[length] = '\0';
    fseek(screen, 0, SEEK_END);
    return text;
}

static int input(const char *line) { return ui_prepare(&ui, line, wire, sizeof(wire), screen); }

static void test_question_renders_choices_and_normalizes_short_answer(void) {
    ui_receive(&ui, opening, screen);
    TEST_ASSERT_TRUE(ui.has_question);
    TEST_ASSERT_EQUAL_STRING("7", ui.question_id);
    TEST_ASSERT_NOT_NULL(strstr(display(), "[Cau hoi #7 - Vong mo man]"));
    TEST_ASSERT_NOT_NULL(strstr(display(), "2 + 2?\nA. 1\nB. 2\nC. 3\nD. 4\n"));
    TEST_ASSERT_NOT_NULL(strstr(display(), "Thoi gian: 30 giay"));
    TEST_ASSERT_EQUAL_INT(11, input(" \tb \t"));
    TEST_ASSERT_EQUAL_STRING("ANSWER|7|B\n", wire);
    TEST_ASSERT_TRUE(ui.answer_sent);
}

static void test_short_answer_requires_question_and_valid_choice(void) {
    TEST_ASSERT_EQUAL_INT(0, input("A"));
    TEST_ASSERT_NOT_NULL(strstr(display(), "Chua co cau hoi"));
    ui_receive(&ui, opening, screen);
    TEST_ASSERT_EQUAL_INT(0, input("E"));
    TEST_ASSERT_FALSE(ui.answer_sent);
    TEST_ASSERT_NOT_NULL(strstr(display(), "Chi chon A, B, C hoac D"));
    TEST_ASSERT_TRUE(input("a") > 0);
    TEST_ASSERT_EQUAL_STRING("ANSWER|7|A\n", wire);
}

static void test_duplicate_short_answer_and_duplicate_question_keep_first_choice(void) {
    ui_receive(&ui, opening, screen);
    TEST_ASSERT_TRUE(input("A") > 0);
    TEST_ASSERT_EQUAL_INT(0, input("B"));
    ui_receive(&ui, opening, screen);
    TEST_ASSERT_EQUAL_INT(0, input("C"));
    TEST_ASSERT_NOT_NULL(strstr(display(), "Ban da gui dap an"));
}

static void test_new_question_resets_answer_and_accepts_turn_frame(void) {
    ui_receive(&ui, opening, screen);
    input("A");
    ui_receive(&ui, "QUESTION|8|TURN|60|Next?|a|b|c|d", screen);
    TEST_ASSERT_FALSE(ui.answer_sent);
    TEST_ASSERT_NOT_NULL(strstr(display(), "[Cau hoi #8 - Luot choi]"));
    TEST_ASSERT_TRUE(input("D") > 0);
    TEST_ASSERT_EQUAL_STRING("ANSWER|8|D\n", wire);
}

static void test_roles_close_answer_window_and_identify_main(void) {
    ui_receive(&ui, opening, screen);
    ui_receive(&ui, "ROLE|MAIN", screen);
    TEST_ASSERT_FALSE(ui.has_question);
    TEST_ASSERT_EQUAL_INT(0, input("A"));
    TEST_ASSERT_NOT_NULL(strstr(display(), "Ban la nguoi choi chinh."));
    ui_receive(&ui, opening, screen);
    ui_receive(&ui, "ROLE|SUB|an", screen);
    TEST_ASSERT_FALSE(ui.has_question);
    TEST_ASSERT_NOT_NULL(strstr(display(), "Nguoi choi chinh: an."));
}

static void test_game_over_clears_question_and_allows_next_round(void) {
    ui_receive(&ui, opening, screen);
    input("A");
    ui_receive(&ui, "GAME_OVER|NO_WINNER", screen);
    TEST_ASSERT_FALSE(ui.has_question);
    TEST_ASSERT_FALSE(ui.answer_sent);
    TEST_ASSERT_NOT_NULL(strstr(display(), "khong co nguoi chien thang"));
    ui_receive(&ui, "QUESTION|8|QUALIFY|30|New?|a|b|c|d", screen);
    TEST_ASSERT_TRUE(input("C") > 0);
    TEST_ASSERT_EQUAL_STRING("ANSWER|8|C\n", wire);
}

static void test_leave_and_logout_clear_question_when_queued(void) {
    ui_receive(&ui, opening, screen);
    TEST_ASSERT_EQUAL_INT(6, input("LEAVE"));
    TEST_ASSERT_EQUAL_STRING("LEAVE\n", wire);
    TEST_ASSERT_FALSE(ui.has_question);
    ui_receive(&ui, opening, screen);
    TEST_ASSERT_EQUAL_INT(7, input("LOGOUT"));
    TEST_ASSERT_EQUAL_STRING("LOGOUT\n", wire);
    TEST_ASSERT_FALSE(ui.has_question);
}

static void test_raw_mode_preserves_events_and_still_tracks_question(void) {
    ui.raw = 1;
    ui_receive(&ui, opening, screen);
    ui_receive(&ui, "200|RECEIVED", screen);
    TEST_ASSERT_EQUAL_STRING("QUESTION|7|QUALIFY|30|2 + 2?|1|2|3|4\n200|RECEIVED\n", display());
    TEST_ASSERT_TRUE(input("D") > 0);
    TEST_ASSERT_EQUAL_STRING("ANSWER|7|D\n", wire);
    ui_receive(&ui, "ROLE|SUB|an", screen);
    TEST_ASSERT_FALSE(ui.has_question);
    TEST_ASSERT_NOT_NULL(strstr(display(), "ROLE|SUB|an\n"));
    ui_receive(&ui, "GAME_OVER|NO_WINNER", screen);
    TEST_ASSERT_NOT_NULL(strstr(display(), "GAME_OVER|NO_WINNER\n"));
}

static void test_full_protocol_commands_preserve_payload_and_track_manual_answer(void) {
    const char *registration = "REGISTER|an| secret with spaces ";
    TEST_ASSERT_EQUAL_INT((int)strlen(registration) + 1, input(registration));
    TEST_ASSERT_EQUAL_STRING("REGISTER|an| secret with spaces \n", wire);
    ui_receive(&ui, opening, screen);
    input("ANSWER|99|D");
    TEST_ASSERT_FALSE(ui.answer_sent);
    input("ANSWER|7|Z");
    TEST_ASSERT_FALSE(ui.answer_sent);
    input("ANSWER|0007|C");
    TEST_ASSERT_EQUAL_STRING("ANSWER|0007|C\n", wire);
    TEST_ASSERT_TRUE(ui.answer_sent);
    TEST_ASSERT_EQUAL_INT(0, input("D"));
    TEST_ASSERT_TRUE(input("ANSWER|7|D") >
                     0); /* Explicit wire commands remain available for debugging. */
}

static void test_malformed_questions_do_not_replace_valid_question(void) {
    const char *invalid[] = {
        "QUESTION|8|QUALIFY|30|Q|a|b|c",    "QUESTION|0|QUALIFY|30|Q|a|b|c|d",
        "QUESTION|-1|QUALIFY|30|Q|a|b|c|d", "QUESTION|18446744073709551616|QUALIFY|30|Q|a|b|c|d",
        "QUESTION|8|QUALIFY|bad|Q|a|b|c|d", "QUESTION|8|OTHER|30|Q|a|b|c|d",
        "QUESTION|8|QUALIFY|30|Q|a||c|d"};
    ui_receive(&ui, opening, screen);
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        ui_receive(&ui, invalid[i], screen);
        TEST_ASSERT_EQUAL_STRING("7", ui.question_id);
    }
    TEST_ASSERT_TRUE(input("A") > 0);
    TEST_ASSERT_EQUAL_STRING("ANSWER|7|A\n", wire);
}

static void test_maximum_qid_is_safe_and_leading_zeros_are_canonicalized(void) {
    ui_receive(&ui, "QUESTION|18446744073709551615|QUALIFY|30|Q|a|b|c|d", screen);
    TEST_ASSERT_TRUE(input("B") > 0);
    TEST_ASSERT_EQUAL_STRING("ANSWER|18446744073709551615|B\n", wire);
    ui_receive(&ui, "QUESTION|0007|QUALIFY|30|Q|a|b|c|d", screen);
    TEST_ASSERT_TRUE(input("C") > 0);
    TEST_ASSERT_EQUAL_STRING("ANSWER|7|C\n", wire);
}

static void test_small_buffer_does_not_consume_answer_and_wire_limit_includes_lf(void) {
    ui_receive(&ui, opening, screen);
    TEST_ASSERT_EQUAL_INT(-1, ui_prepare(&ui, "A", wire, 5, screen));
    TEST_ASSERT_FALSE(ui.answer_sent);
    TEST_ASSERT_EQUAL_INT(-1, ui_prepare(&ui, "PING", wire, 5, screen));
    char maximum[LINEBUF_MAX + 1];
    memset(maximum, 'X', LINEBUF_MAX - 1);
    maximum[LINEBUF_MAX - 1] = '\0';
    TEST_ASSERT_EQUAL_INT(LINEBUF_MAX, input(maximum));
    TEST_ASSERT_EQUAL_CHAR('\n', wire[LINEBUF_MAX - 1]);
    TEST_ASSERT_EQUAL_CHAR('\0', wire[LINEBUF_MAX]);
    maximum[LINEBUF_MAX - 1] = 'X';
    maximum[LINEBUF_MAX] = '\0';
    TEST_ASSERT_EQUAL_INT(-1, input(maximum));
    TEST_ASSERT_EQUAL_INT(1, input(""));
    TEST_ASSERT_EQUAL_STRING("\n", wire);
}

static void test_status_roster_and_unknown_events_render_without_losing_question(void) {
    ui_receive(&ui, opening, screen);
    ui_receive(&ui, "200|RECEIVED", screen);
    ui_receive(&ui, "408|TIMEOUT", screen);
    ui_receive(&ui, "409|ALREADY_ANSWERED", screen);
    ui_receive(&ui, "409|WRONG_QUESTION", screen);
    ui_receive(&ui, "PLAYERS|2|an,tu", screen);
    ui_receive(&ui, "PLAYERS_BEGIN", screen);
    ui_receive(&ui, "PLAYERS_PART|an,tu", screen);
    ui_receive(&ui, "PLAYERS_END", screen);
    ui_receive(&ui, "FUTURE|event", screen);
    TEST_ASSERT_NOT_NULL(strstr(display(), "Server da nhan dap an"));
    TEST_ASSERT_NOT_NULL(strstr(display(), "Da het thoi gian"));
    TEST_ASSERT_NOT_NULL(strstr(display(), "Ban da tra loi"));
    TEST_ASSERT_NOT_NULL(strstr(display(), "Ma cau hoi da cu"));
    TEST_ASSERT_NOT_NULL(strstr(display(), "Phong cho (2 nguoi): an,tu"));
    TEST_ASSERT_NOT_NULL(strstr(display(), "Danh sach nguoi choi:\n  an,tu\nKet thuc danh sach."));
    TEST_ASSERT_NOT_NULL(strstr(display(), "FUTURE|event\n"));
    TEST_ASSERT_TRUE(ui.has_question);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_question_renders_choices_and_normalizes_short_answer);
    RUN_TEST(test_short_answer_requires_question_and_valid_choice);
    RUN_TEST(test_duplicate_short_answer_and_duplicate_question_keep_first_choice);
    RUN_TEST(test_new_question_resets_answer_and_accepts_turn_frame);
    RUN_TEST(test_roles_close_answer_window_and_identify_main);
    RUN_TEST(test_game_over_clears_question_and_allows_next_round);
    RUN_TEST(test_leave_and_logout_clear_question_when_queued);
    RUN_TEST(test_raw_mode_preserves_events_and_still_tracks_question);
    RUN_TEST(test_full_protocol_commands_preserve_payload_and_track_manual_answer);
    RUN_TEST(test_malformed_questions_do_not_replace_valid_question);
    RUN_TEST(test_maximum_qid_is_safe_and_leading_zeros_are_canonicalized);
    RUN_TEST(test_small_buffer_does_not_consume_answer_and_wire_limit_includes_lf);
    RUN_TEST(test_status_roster_and_unknown_events_render_without_losing_question);
    return UNITY_END();
}
