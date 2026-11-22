#include "session.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_unique_login_and_expiration(void) {
    Lobby lobby = {0};
    TEST_ASSERT_EQUAL_INT(0, session_login(&lobby, 0, "an", 100));
    TEST_ASSERT_EQUAL_INT(-1, session_login(&lobby, 1, "an", 100));
    TEST_ASSERT_EQUAL_INT(-1, session_login(&lobby, 0, "tu", 100));
    TEST_ASSERT_EQUAL_INT(-1, session_login(&lobby, 100, "tu", 100));
    TEST_ASSERT_EQUAL_INT(-1, session_login(&lobby, 1, "", 100));
    TEST_ASSERT_FALSE(session_expired(&lobby.sessions[0], 399, 300));
    TEST_ASSERT_TRUE(session_expired(&lobby.sessions[0], 400, 300));
    TEST_ASSERT_FALSE(session_expired(&lobby.sessions[1], 400, 300));
    lobby.sessions[1].joined = 1;
    session_logout(&lobby, 0);
    TEST_ASSERT_EQUAL_INT(0, session_login(&lobby, 0, "an", 500));
    lobby.sessions[1].joined = 0;
    lobby.running = 1;
    session_logout(&lobby, 0);
    TEST_ASSERT_FALSE(lobby.running);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unique_login_and_expiration);
    return UNITY_END();
}
