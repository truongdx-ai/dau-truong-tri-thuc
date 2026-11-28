#include "game.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

static Game game;
static unsigned counts[SESSION_LIMIT];
static char rows[8192];

static int emit(void *context, size_t slot, const char *line) {
    (void)context;
    ++counts[slot];
    TEST_ASSERT_TRUE(strlen(line) <= 1024);
    if (slot == 0) {
        size_t length = strlen(rows);
        TEST_ASSERT_TRUE(length + strlen(line) < sizeof(rows));
        memcpy(rows + length, line, strlen(line) + 1);
    }
    return 0;
}

void setUp(void) {
    game = (Game){.qid = 42, .emit = emit};
    memset(counts, 0, sizeof(counts));
    rows[0] = '\0';
}
void tearDown(void) {}

static void test_snapshot_includes_eliminated_and_disconnected_players(void) {
    game.players[0] = (Player){.username = "an", .points = 20, .active = 1};
    game.players[1] = (Player){.username = "tu", .points = 0};
    game.players[2] = (Player){.username = "chi", .points = 10, .departed = 1};
    game_scoreboard(&game);
    TEST_ASSERT_EQUAL_STRING("SCOREBOARD_BEGIN|42\nSCORE|an|20|ACTIVE\n"
                             "SCORE|tu|0|ELIMINATED\nSCORE|chi|10|DISCONNECTED\n"
                             "SCOREBOARD_END|42\n",
                             rows);
    TEST_ASSERT_EQUAL_UINT(5, counts[0]);
    TEST_ASSERT_EQUAL_UINT(5, counts[1]);
    TEST_ASSERT_EQUAL_UINT(0, counts[2]);
}

static void test_snapshot_supports_100_maximum_length_names(void) {
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        snprintf(game.players[i].username, 33, "player_%025zu", i);
        game.players[i].active = 1;
        game.players[i].points = 10;
    }
    game_scoreboard(&game);
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        TEST_ASSERT_EQUAL_UINT(102, counts[i]);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_snapshot_includes_eliminated_and_disconnected_players);
    RUN_TEST(test_snapshot_supports_100_maximum_length_names);
    return UNITY_END();
}
