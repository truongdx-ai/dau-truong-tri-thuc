#include "game.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

static Game game;
static Lobby lobby;
static Question questions[12];
static char events[SESSION_LIMIT][8192];

static int emit(void *context, size_t slot, const char *line) {
    (void)context;
    size_t used = strlen(events[slot]);
    TEST_ASSERT_TRUE(used + strlen(line) < sizeof(events[slot]));
    memcpy(events[slot] + used, line, strlen(line) + 1);
    return 0;
}

void setUp(void) {
    game = (Game){0};
    lobby = (Lobby){0};
    memset(events, 0, sizeof(events));
    for (size_t i = 0; i < 3; ++i) {
        snprintf(lobby.sessions[i].username, 33, "player%zu", i);
        lobby.sessions[i].joined = 1;
    }
    for (size_t i = 0; i < 12; ++i) {
        questions[i] = (Question){.text = "Q", .options = {"1", "2", "3", "4"}, .answer = 'A'};
    }
}
void tearDown(void) {}

static void start(void) {
    TEST_ASSERT_EQUAL_INT(0, game_start(&game, &lobby, questions, 12, 0, emit, NULL));
}

static void select_main(void) {
    start();
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\n", game_answer(&game, 2, game.qid, 'A', 1));
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\n", game_answer(&game, 0, game.qid, 'B', 2));
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\n", game_answer(&game, 1, game.qid, 'A', 2));
    game_tick(&game, 3);
    TEST_ASSERT_EQUAL_INT(GAME_TURN, game.phase);
    TEST_ASSERT_EQUAL_INT(2, game.main_slot);
    TEST_ASSERT_NOT_NULL(strstr(events[2], "ROLE|MAIN\n"));
}

static void test_correct_main_wins_and_next_game_resets_points(void) {
    select_main();
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\n", game_answer(&game, 2, game.qid, 'A', 4));
    game_tick(&game, 30002);
    TEST_ASSERT_EQUAL_INT(GAME_TURN, game.phase);
    game_tick(&game, 30003);
    TEST_ASSERT_EQUAL_INT(GAME_IDLE, game.phase);
    TEST_ASSERT_EQUAL_INT64(30, game.players[2].points);
    TEST_ASSERT_NOT_NULL(strstr(events[0], "ELIMINATED|TIMEOUT"));
    TEST_ASSERT_NOT_NULL(strstr(events[1], "GAME_OVER|WINNER|player2|30"));
    unsigned previous = game.qid;
    TEST_ASSERT_EQUAL_INT(0, game_start(&game, &lobby, questions, 12, 40000, emit, NULL));
    TEST_ASSERT_TRUE(game.qid > previous);
    TEST_ASSERT_EQUAL_INT64(10, game.players[2].points);
}

static void test_wrong_main_and_timeout_transition(void) {
    select_main();
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\n", game_answer(&game, 0, game.qid, 'A', 4));
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\n", game_answer(&game, 1, game.qid, 'A', 5));
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\n", game_answer(&game, 2, game.qid, 'B', 6));
    game_tick(&game, 30003);
    TEST_ASSERT_EQUAL_INT(0, game.main_slot);
    TEST_ASSERT_EQUAL_INT64(15, game.players[0].points);
    TEST_ASSERT_EQUAL_INT64(15, game.players[1].points);
    TEST_ASSERT_FALSE(game.players[2].active);
    TEST_ASSERT_EQUAL_INT(30000, game_poll_timeout(&game, 30003));
    game_tick(&game, 60003);
    TEST_ASSERT_EQUAL_INT(30000, game_poll_timeout(&game, 60003));
    game_tick(&game, 90003);
    TEST_ASSERT_EQUAL_INT(GAME_IDLE, game.phase);
    TEST_ASSERT_NOT_NULL(strstr(events[0], "GAME_OVER|NO_WINNER"));
}

static void test_skip_limit_and_validation(void) {
    select_main();
    TEST_ASSERT_EQUAL_STRING("403|NOT_MAIN\n", game_skip(&game, 1, game.qid, 4));
    TEST_ASSERT_EQUAL_STRING("409|STALE_QUESTION\n", game_skip(&game, 2, 999, 4));
    TEST_ASSERT_EQUAL_STRING("408|TIMEOUT\n", game_skip(&game, 2, game.qid, 60003));
    for (unsigned round = 0; round < 2; ++round) {
        int64_t now = game.opened_at;
        game_answer(&game, 0, game.qid, 'A', now + 1);
        game_answer(&game, 1, game.qid, 'A', now + 2);
        TEST_ASSERT_EQUAL_STRING("200|OK\n", game_skip(&game, 2, game.qid, now + 3));
        TEST_ASSERT_EQUAL_STRING("409|ALREADY_ANSWERED\n", game_skip(&game, 2, game.qid, now + 4));
        game_tick(&game, now + 30000);
    }
    TEST_ASSERT_EQUAL_UINT(2, game.players[2].skips);
    TEST_ASSERT_EQUAL_STRING("403|NO_SKIP_LEFT\n", game_skip(&game, 2, game.qid, game.opened_at));
}

static void test_answers_reject_stale_duplicate_invalid_and_late(void) {
    TEST_ASSERT_EQUAL_STRING("403|NOT_ALLOWED\n", game_answer(&game, 0, 1, 'A', 0));
    start();
    TEST_ASSERT_EQUAL_INT(-1, game_start(&game, &lobby, questions, 12, 0, emit, NULL));
    TEST_ASSERT_EQUAL_STRING("409|STALE_QUESTION\n", game_answer(&game, 0, 999, 'A', 0));
    TEST_ASSERT_EQUAL_STRING("400|BAD_REQUEST\n", game_answer(&game, 0, game.qid, 'Z', 0));
    TEST_ASSERT_EQUAL_STRING("408|TIMEOUT\n", game_answer(&game, 0, game.qid, 'A', 30000));
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\n", game_answer(&game, 0, game.qid, 'A', 1));
    TEST_ASSERT_EQUAL_STRING("409|ALREADY_ANSWERED\n", game_answer(&game, 0, game.qid, 'A', 2));
    TEST_ASSERT_EQUAL_INT(0, game_poll_timeout(&game, 30000));
    game_tick(&game, 30000);
    TEST_ASSERT_EQUAL_INT(GAME_TURN, game.phase);
    TEST_ASSERT_EQUAL_STRING("408|TIMEOUT\n", game_answer(&game, 1, game.qid, 'A', 60000));
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\n", game_answer(&game, 0, game.qid, 'A', 60000));
}

static void test_qualification_retries_cancel_and_question_exhaustion(void) {
    start();
    for (int i = 0; i < 3; ++i) {
        game_tick(&game, (i + 1) * 30000);
    }
    TEST_ASSERT_EQUAL_INT(GAME_IDLE, game.phase);
    TEST_ASSERT_EQUAL_INT(-1, game_poll_timeout(&game, 90000));
    TEST_ASSERT_NOT_NULL(strstr(events[0], "GAME_OVER|CANCELLED"));
    setUp();
    game_start(&game, &lobby, questions, 1, 0, emit, NULL);
    game_answer(&game, 0, game.qid, 'A', 1);
    game_tick(&game, 30000);
    TEST_ASSERT_EQUAL_INT(GAME_IDLE, game.phase);
    TEST_ASSERT_NOT_NULL(strstr(events[0], "GAME_OVER|WINNER"));
    setUp();
    TEST_ASSERT_EQUAL_INT(-1, game_start(&game, &lobby, NULL, 0, 0, emit, NULL));
    lobby.sessions[1].joined = lobby.sessions[2].joined = 0;
    TEST_ASSERT_EQUAL_INT(-1, game_start(&game, &lobby, questions, 1, 0, emit, NULL));
}

static void test_departure_counts_as_wrong(void) {
    select_main();
    game_answer(&game, 0, game.qid, 'A', 4);
    game_answer(&game, 1, game.qid, 'A', 5);
    game_leave(&game, 2, 6);
    game_tick(&game, 30003);
    TEST_ASSERT_EQUAL_INT(0, game.main_slot);
    TEST_ASSERT_EQUAL_INT64(15, game.players[0].points);
    game_leave(&game, 1, 30004);
    game_answer(&game, 0, game.qid, 'A', 30005);
    game_tick(&game, 60003);
    TEST_ASSERT_EQUAL_INT(GAME_IDLE, game.phase);
    TEST_ASSERT_EQUAL_INT64(30, game.players[0].points);
    game_leave(&game, 0, 60004);
    setUp();
    start();
    game_leave(&game, 0, 1);
    game_leave(&game, 1, 2);
    TEST_ASSERT_EQUAL_INT(GAME_IDLE, game.phase);
}

static void test_first_received_answer_breaks_same_millisecond_tie(void) {
    start();
    game_answer(&game, 2, game.qid, 'A', 1);
    game_answer(&game, 0, game.qid, 'A', 1);
    game_answer(&game, 1, game.qid, 'A', 1);
    game_tick(&game, 1);
    TEST_ASSERT_EQUAL_INT(2, game.main_slot);
}

static void test_skip_without_correct_subs_keeps_main_points(void) {
    select_main();
    TEST_ASSERT_EQUAL_STRING("200|OK\n", game_skip(&game, 2, game.qid, 4));
    game_tick(&game, 30003);
    TEST_ASSERT_EQUAL_INT(GAME_IDLE, game.phase);
    TEST_ASSERT_EQUAL_INT64(10, game.players[2].points);
    TEST_ASSERT_NOT_NULL(strstr(events[2], "TURN_RESULT|A|SKIPPED|10"));
    TEST_ASSERT_NOT_NULL(strstr(events[2], "GAME_OVER|WINNER|player2|10"));
}

static void test_departed_main_cancels_pending_skip_and_all_wrong_loses(void) {
    select_main();
    game_skip(&game, 2, game.qid, 4);
    game_leave(&game, 2, 5);
    TEST_ASSERT_FALSE(game.skipped);
    game_leave(&game, 0, 6);
    game_tick(&game, 30003);
    TEST_ASSERT_EQUAL_INT(GAME_IDLE, game.phase);
    TEST_ASSERT_NOT_NULL(strstr(events[1], "GAME_OVER|NO_WINNER"));
    TEST_ASSERT_EQUAL_UINT(0, game.players[2].skips);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_correct_main_wins_and_next_game_resets_points);
    RUN_TEST(test_wrong_main_and_timeout_transition);
    RUN_TEST(test_skip_limit_and_validation);
    RUN_TEST(test_answers_reject_stale_duplicate_invalid_and_late);
    RUN_TEST(test_qualification_retries_cancel_and_question_exhaustion);
    RUN_TEST(test_departure_counts_as_wrong);
    RUN_TEST(test_first_received_answer_breaks_same_millisecond_tie);
    RUN_TEST(test_skip_without_correct_subs_keeps_main_points);
    RUN_TEST(test_departed_main_cancels_pending_skip_and_all_wrong_loses);
    return UNITY_END();
}
