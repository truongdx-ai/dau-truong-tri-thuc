#include "scoring.h"
#include "unity.h"

static Player players[SESSION_LIMIT];

void setUp(void) {
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        players[i] = (Player){0};
    }
    for (size_t i = 0; i < 4; ++i) {
        players[i] = (Player){.active = 1, .points = 10, .answered_at = (int64_t)i};
    }
}
void tearDown(void) {}

static void test_main_correct_collects_eliminated_points(void) {
    players[1].correct = 1;
    TEST_ASSERT_EQUAL_INT(0, score_apply(players, 0, SCORE_CORRECT));
    TEST_ASSERT_EQUAL_INT64(30, players[0].points);
    TEST_ASSERT_TRUE(players[1].active);
    TEST_ASSERT_FALSE(players[2].active);
    TEST_ASSERT_EQUAL_INT64(0, players[3].points);
}

static void test_main_wrong_splits_only_main_points_and_selects_fastest(void) {
    players[0].points = 11;
    players[1].correct = players[2].correct = 1;
    players[2].answered_at = 0;
    TEST_ASSERT_EQUAL_INT(2, score_apply(players, 0, SCORE_WRONG));
    TEST_ASSERT_FALSE(players[0].active);
    TEST_ASSERT_FALSE(players[3].active);
    TEST_ASSERT_EQUAL_INT64(15, players[1].points);
    TEST_ASSERT_EQUAL_INT64(16, players[2].points);
}

static void test_skip_splits_half_and_wrong_sub_points_with_remainder(void) {
    players[0].points = 11;
    players[1].correct = players[2].correct = 1;
    TEST_ASSERT_EQUAL_INT(0, score_apply(players, 0, SCORE_SKIP));
    TEST_ASSERT_EQUAL_INT64(6, players[0].points);
    TEST_ASSERT_EQUAL_INT64(18, players[1].points);
    TEST_ASSERT_EQUAL_INT64(17, players[2].points);
    TEST_ASSERT_EQUAL_UINT(1, players[0].skips);
    TEST_ASSERT_FALSE(players[3].active);
}

static void test_no_correct_subs(void) {
    TEST_ASSERT_EQUAL_INT(0, score_apply(players, 0, SCORE_SKIP));
    TEST_ASSERT_EQUAL_INT64(10, players[0].points);
    TEST_ASSERT_TRUE(players[0].active);
    setUp();
    TEST_ASSERT_EQUAL_INT(-1, score_apply(players, 0, SCORE_WRONG));
    for (int i = 0; i < 4; ++i) {
        TEST_ASSERT_FALSE(players[i].active);
    }
    setUp();
    score_apply(players, 0, SCORE_CORRECT);
    TEST_ASSERT_EQUAL_INT64(40, players[0].points);
}

static void test_100_player_skip_preserves_points_and_assigns_remainder(void) {
    int64_t total_before = 997, total_after = 0;
    for (int i = 0; i < SESSION_LIMIT; ++i) {
        players[i] = (Player){.active = 1, .points = 10, .correct = i % 3 == 0, .answered_at = i};
    }
    players[99].points = 997;
    total_before += 99 * 10;
    TEST_ASSERT_EQUAL_INT(99, score_apply(players, 99, SCORE_SKIP));
    for (int i = 0; i < SESSION_LIMIT; ++i) {
        TEST_ASSERT_TRUE(players[i].points >= 0);
        total_after += players[i].points;
    }
    TEST_ASSERT_EQUAL_INT64(total_before, total_after);
    TEST_ASSERT_EQUAL_INT64(499, players[99].points);
    TEST_ASSERT_EQUAL_INT64(48, players[0].points);
    TEST_ASSERT_EQUAL_INT64(45, players[3].points);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_main_correct_collects_eliminated_points);
    RUN_TEST(test_main_wrong_splits_only_main_points_and_selects_fastest);
    RUN_TEST(test_skip_splits_half_and_wrong_sub_points_with_remainder);
    RUN_TEST(test_no_correct_subs);
    RUN_TEST(test_100_player_skip_preserves_points_and_assigns_remainder);
    return UNITY_END();
}
