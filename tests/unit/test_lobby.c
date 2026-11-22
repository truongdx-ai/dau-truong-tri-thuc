#include "lobby.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_membership_host_transfer_and_start(void) {
    Lobby lobby = {0};
    char roster[128];
    TEST_ASSERT_EQUAL_STRING("401|LOGIN_REQUIRED\n", lobby_join(&lobby, 0));
    TEST_ASSERT_EQUAL_INT(0, session_login(&lobby, 0, "an", 0));
    TEST_ASSERT_EQUAL_INT(0, session_login(&lobby, 1, "tu", 0));
    TEST_ASSERT_EQUAL_STRING("200|OK\n", lobby_join(&lobby, 1));
    TEST_ASSERT_EQUAL_STRING("200|OK\n", lobby_join(&lobby, 1));
    TEST_ASSERT_EQUAL_INT(1, lobby_host(&lobby));
    TEST_ASSERT_EQUAL_STRING("403|NOT_HOST\n", lobby_start(&lobby, 0));
    TEST_ASSERT_EQUAL_STRING("412|NOT_ENOUGH_PLAYERS\n", lobby_start(&lobby, 1));
    TEST_ASSERT_EQUAL_STRING("200|OK\n", lobby_join(&lobby, 0));
    TEST_ASSERT_TRUE(lobby_players(&lobby, roster, sizeof(roster)) > 0);
    TEST_ASSERT_EQUAL_STRING("PLAYERS|2|an,tu\n", roster);
    TEST_ASSERT_EQUAL_INT(-1, lobby_players(&lobby, roster, 2));
    TEST_ASSERT_EQUAL_STRING("200|OK\n", lobby_start(&lobby, 1));
    TEST_ASSERT_EQUAL_STRING("409|GAME_RUNNING\n", lobby_start(&lobby, 1));
    TEST_ASSERT_EQUAL_STRING("409|GAME_RUNNING\n", lobby_join(&lobby, 0));
    lobby_leave(&lobby, 1);
    TEST_ASSERT_EQUAL_INT(0, lobby_host(&lobby));
    lobby_leave(&lobby, 0);
    TEST_ASSERT_FALSE(lobby.running);
    TEST_ASSERT_EQUAL_INT(-1, lobby_host(&lobby));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_membership_host_transfer_and_start);
    return UNITY_END();
}
