#include "client.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_countdown_uses_deadline_and_stops_on_result(void) {
    UiState state = {0};
    TEST_ASSERT_EQUAL_INT(-1, ui_remaining(&state, 0));
    ui_receive(&state, "QUESTION|7|TURN|30|Q|1|2|3|4", 1000);
    TEST_ASSERT_EQUAL_UINT(7, state.qid);
    TEST_ASSERT_EQUAL_INT(30, ui_remaining(&state, 1000));
    TEST_ASSERT_EQUAL_INT(30, ui_remaining(&state, 1001));
    TEST_ASSERT_EQUAL_INT(29, ui_remaining(&state, 2000));
    TEST_ASSERT_EQUAL_INT(0, ui_remaining(&state, 31000));
    TEST_ASSERT_EQUAL_INT(0, ui_remaining(&state, 90000));
    FILE *display = tmpfile();
    TEST_ASSERT_NOT_NULL(display);
    ui_tick(&state, 2000, display);
    long first = ftell(display);
    ui_tick(&state, 2001, display);
    long second = ftell(display);
    ui_tick(&state, 31000, display);
    rewind(display);
    char rendered[256] = {0};
    size_t read = fread(rendered, 1, sizeof(rendered) - 1, display);
    fclose(display);
    TEST_ASSERT_TRUE(read > 0);
    TEST_ASSERT_EQUAL_INT(first, second);
    TEST_ASSERT_NOT_NULL(strstr(rendered, "Con lai 29 giay (qid 7)"));
    TEST_ASSERT_NOT_NULL(strstr(rendered, "Con lai 0 giay (qid 7)"));
    ui_receive(&state, "TURN_RESULT|A|CORRECT|30", 31000);
    TEST_ASSERT_EQUAL_INT(-1, ui_remaining(&state, 31001));
}

static void test_new_question_and_terminal_outcomes_reset_countdown(void) {
    UiState state = {0};
    ui_receive(&state, "QUESTION|8|TURN|60|Q|1|2|3|4", 0);
    TEST_ASSERT_EQUAL_INT(60, ui_remaining(&state, 0));
    ui_receive(&state, "200|RECEIVED", 1);
    TEST_ASSERT_EQUAL_INT(-1, ui_remaining(&state, 2));
    ui_receive(&state, "QUESTION|9|TURN|30|Q|1|2|3|4", 100);
    TEST_ASSERT_EQUAL_UINT(9, state.qid);
    ui_receive(&state, "ELIMINATED|TIMEOUT", 200);
    TEST_ASSERT_EQUAL_INT(-1, ui_remaining(&state, 200));
    ui_receive(&state, "GAME_OVER|NO_WINNER", 300);
    ui_receive(&state, "GAME_OVER|CANCELLED", 301);
    ui_receive(&state, "GAME_OVER|WINNER|an|30", 302);
    TEST_ASSERT_EQUAL_INT(-1, ui_remaining(&state, 303));
}

static void test_invalid_questions_do_not_start_timer_and_snapshot_renders(void) {
    UiState state = {0};
    const char *invalid[] = {"QUESTION|bad", "QUESTION|-1|TURN|30|Q|1|2|3|4",
                             "QUESTION|1|TURN|61|Q|1|2|3|4", "QUESTION|1|TURN|bad|Q|1|2|3|4"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        ui_receive(&state, invalid[i], 0);
        TEST_ASSERT_FALSE(state.timing);
    }
    ui_tick(&state, 0, NULL);
    ui_receive(&state, "SCOREBOARD_BEGIN|1", 0);
    ui_receive(&state, "SCORE|an|30|ACTIVE", 0);
    ui_receive(&state, "SCOREBOARD_END|1", 0);
    TEST_ASSERT_FALSE(state.timing);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_countdown_uses_deadline_and_stops_on_result);
    RUN_TEST(test_new_question_and_terminal_outcomes_reset_countdown);
    RUN_TEST(test_invalid_questions_do_not_start_timer_and_snapshot_renders);
    return UNITY_END();
}
