#include "qualification.h"
#include "unity.h"

#include <stdint.h>

static Lobby lobby;
static Qualification round_state;
static const Question question = {.text = "Q", .options = {"1", "2", "3", "4"}, .answer = 'D'};

void setUp(void) {
    lobby = (Lobby){0};
    for (size_t i = 0; i < 3; ++i) {
        lobby.sessions[i].joined = 1;
    }
    round_state = (Qualification){0};
    TEST_ASSERT_EQUAL_INT(0, qualification_start(&round_state, &lobby, &question, 7, 1000, 30000));
}

void tearDown(void) {}

static void test_fastest_correct_player_wins_after_all_answers(void) {
    TEST_ASSERT_EQUAL(QUALIFY_RECEIVED, qualification_answer(&round_state, 2, 7, 'D', 1100));
    TEST_ASSERT_EQUAL(QUALIFY_RECEIVED, qualification_answer(&round_state, 0, 7, 'D', 1200));
    TEST_ASSERT_EQUAL(QUALIFY_PENDING, qualification_finish(&round_state, 1200));
    TEST_ASSERT_EQUAL(QUALIFY_RECEIVED, qualification_answer(&round_state, 1, 7, 'A', 1300));
    TEST_ASSERT_EQUAL(QUALIFY_MAIN_CHOSEN, qualification_finish(&round_state, 1300));
    TEST_ASSERT_EQUAL_INT(2, round_state.main_slot);
    TEST_ASSERT_EQUAL_INT64(1100, round_state.players[2].received_at);
    TEST_ASSERT_EQUAL(QUALIFY_PENDING, qualification_finish(&round_state, 1400));
}

static void test_equal_timestamps_use_receive_order_not_slot(void) {
    qualification_answer(&round_state, 2, 7, 'D', 1100);
    qualification_answer(&round_state, 0, 7, 'D', 1100);
    qualification_answer(&round_state, 1, 7, 'D', 1100);
    TEST_ASSERT_EQUAL(QUALIFY_MAIN_CHOSEN, qualification_finish(&round_state, 1100));
    TEST_ASSERT_EQUAL_INT(2, round_state.main_slot);
}

static void test_first_answer_cannot_be_replaced(void) {
    TEST_ASSERT_EQUAL(QUALIFY_RECEIVED, qualification_answer(&round_state, 0, 7, 'A', 1100));
    TEST_ASSERT_EQUAL(QUALIFY_ALREADY_ANSWERED,
                      qualification_answer(&round_state, 0, 7, 'D', 1200));
    TEST_ASSERT_EQUAL_CHAR('A', round_state.players[0].choice);
    TEST_ASSERT_EQUAL_INT64(1100, round_state.players[0].received_at);
}

static void test_wrong_question_and_nonparticipant_do_not_consume_answer(void) {
    TEST_ASSERT_EQUAL(QUALIFY_WRONG_QUESTION, qualification_answer(&round_state, 0, 8, 'D', 1100));
    TEST_ASSERT_EQUAL(QUALIFY_NOT_ALLOWED, qualification_answer(&round_state, 3, 7, 'D', 1100));
    TEST_ASSERT_EQUAL(QUALIFY_NOT_ALLOWED,
                      qualification_answer(&round_state, SESSION_LIMIT, 7, 'D', 1100));
    TEST_ASSERT_EQUAL(QUALIFY_NOT_ALLOWED, qualification_answer(&round_state, 0, 7, 'E', 1100));
    TEST_ASSERT_EQUAL(QUALIFY_NOT_ALLOWED, qualification_answer(&round_state, 0, 7, 'D', 999));
    TEST_ASSERT_FALSE(round_state.players[0].answered);
    TEST_ASSERT_EQUAL(QUALIFY_RECEIVED, qualification_answer(&round_state, 0, 7, 'D', 1100));
}

static void test_deadline_is_exclusive_and_resolves_unanswered_players(void) {
    TEST_ASSERT_EQUAL(QUALIFY_RECEIVED, qualification_answer(&round_state, 1, 7, 'D', 30999));
    TEST_ASSERT_EQUAL(QUALIFY_PENDING, qualification_finish(&round_state, 30999));
    TEST_ASSERT_EQUAL(QUALIFY_TIMEOUT, qualification_answer(&round_state, 0, 7, 'D', 31000));
    TEST_ASSERT_EQUAL(QUALIFY_MAIN_CHOSEN, qualification_finish(&round_state, 31000));
    TEST_ASSERT_EQUAL_INT(1, round_state.main_slot);
}

static void test_no_answers_finishes_without_winner_once(void) {
    TEST_ASSERT_EQUAL(QUALIFY_FINISHED_EMPTY, qualification_finish(&round_state, 31000));
    TEST_ASSERT_EQUAL(QUALIFY_NO_WINNER, round_state.phase);
    TEST_ASSERT_EQUAL_INT(-1, round_state.main_slot);
    TEST_ASSERT_EQUAL(QUALIFY_PENDING, qualification_finish(&round_state, 32000));
}

static void test_all_wrong_finishes_early_without_winner(void) {
    for (size_t i = 0; i < 3; ++i) {
        qualification_answer(&round_state, i, 7, 'A', 1100);
    }
    TEST_ASSERT_EQUAL(QUALIFY_FINISHED_EMPTY, qualification_finish(&round_state, 1100));
}

static void test_disconnected_correct_player_is_not_selected(void) {
    qualification_answer(&round_state, 0, 7, 'D', 1100);
    qualification_answer(&round_state, 1, 7, 'D', 1200);
    qualification_leave(&round_state, 0);
    qualification_leave(&round_state, 2);
    TEST_ASSERT_EQUAL(QUALIFY_MAIN_CHOSEN, qualification_finish(&round_state, 1300));
    TEST_ASSERT_EQUAL_INT(1, round_state.main_slot);
    TEST_ASSERT_EQUAL(QUALIFY_NOT_ALLOWED, qualification_answer(&round_state, 0, 7, 'D', 1400));
}

static void test_main_departure_promotes_next_correct_player(void) {
    qualification_answer(&round_state, 0, 7, 'D', 1100);
    qualification_answer(&round_state, 1, 7, 'D', 1200);
    qualification_answer(&round_state, 2, 7, 'A', 1300);
    TEST_ASSERT_EQUAL(QUALIFY_MAIN_CHOSEN, qualification_finish(&round_state, 1300));
    qualification_leave(&round_state, 0);
    TEST_ASSERT_EQUAL(QUALIFY_MAIN_CHOSEN, qualification_finish(&round_state, 1400));
    TEST_ASSERT_EQUAL_INT(1, round_state.main_slot);
    qualification_leave(&round_state, 1);
    TEST_ASSERT_EQUAL(QUALIFY_FINISHED_EMPTY, qualification_finish(&round_state, 1500));
}

static void test_empty_room_resets_and_new_round_clears_old_answers(void) {
    qualification_answer(&round_state, 0, 7, 'D', 1100);
    for (size_t i = 0; i < 3; ++i) {
        qualification_leave(&round_state, i);
    }
    TEST_ASSERT_EQUAL(QUALIFY_EMPTY_ROOM, qualification_finish(&round_state, 1200));
    TEST_ASSERT_EQUAL(QUALIFY_IDLE, round_state.phase);
    TEST_ASSERT_EQUAL_INT(0, qualification_start(&round_state, &lobby, &question, 8, 2000, 30000));
    TEST_ASSERT_FALSE(round_state.players[0].answered);
    TEST_ASSERT_EQUAL(QUALIFY_WRONG_QUESTION, qualification_answer(&round_state, 0, 7, 'D', 2100));
    TEST_ASSERT_EQUAL(QUALIFY_RECEIVED, qualification_answer(&round_state, 0, 8, 'D', 2100));
}

static void test_hundred_players_can_answer_and_select_main(void) {
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        lobby.sessions[i].joined = 1;
    }
    TEST_ASSERT_EQUAL_INT(0, qualification_start(&round_state, &lobby, &question, 8, 1000, 30000));
    for (size_t i = SESSION_LIMIT; i > 0; --i) {
        TEST_ASSERT_EQUAL(QUALIFY_RECEIVED,
                          qualification_answer(&round_state, i - 1, 8, 'D', 1100));
    }
    TEST_ASSERT_EQUAL(QUALIFY_MAIN_CHOSEN, qualification_finish(&round_state, 1100));
    TEST_ASSERT_EQUAL_INT(99, round_state.main_slot);
}

static void test_invalid_start_preserves_current_round(void) {
    TEST_ASSERT_EQUAL_INT(-1, qualification_start(&round_state, &lobby, &question, 0, 1000, 30000));
    TEST_ASSERT_EQUAL_INT(-1, qualification_start(&round_state, &lobby, &question, 8, -1, 30000));
    TEST_ASSERT_EQUAL_INT(-1, qualification_start(&round_state, &lobby, &question, 8, 1000, 0));
    TEST_ASSERT_EQUAL_INT(-1,
                          qualification_start(&round_state, &lobby, &question, 8, INT64_MAX, 1));
    TEST_ASSERT_EQUAL_UINT64(7, round_state.qid);
    TEST_ASSERT_EQUAL(QUALIFY_OPEN, round_state.phase);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fastest_correct_player_wins_after_all_answers);
    RUN_TEST(test_equal_timestamps_use_receive_order_not_slot);
    RUN_TEST(test_first_answer_cannot_be_replaced);
    RUN_TEST(test_wrong_question_and_nonparticipant_do_not_consume_answer);
    RUN_TEST(test_deadline_is_exclusive_and_resolves_unanswered_players);
    RUN_TEST(test_no_answers_finishes_without_winner_once);
    RUN_TEST(test_all_wrong_finishes_early_without_winner);
    RUN_TEST(test_disconnected_correct_player_is_not_selected);
    RUN_TEST(test_main_departure_promotes_next_correct_player);
    RUN_TEST(test_empty_room_resets_and_new_round_clears_old_answers);
    RUN_TEST(test_hundred_players_can_answer_and_select_main);
    RUN_TEST(test_invalid_start_preserves_current_round);
    return UNITY_END();
}
