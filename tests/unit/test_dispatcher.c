#include "dispatcher.h"
#include "lobby.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

static char output[8192];
static size_t used;
static size_t destination;
static FILE *events;
static Dispatcher server;
static NetConfig config;

static int emit(void *context, size_t slot, const char *line) {
    (void)context;
    destination = slot;
    if (slot == 0) {
        size_t length = strlen(line);
        if (length + used >= sizeof(output)) {
            return -1;
        }
        memcpy(output + used, line, length + 1);
        used += length;
    }
    return 0;
}

void setUp(void) {
    used = 0;
    output[0] = '\0';
    events = tmpfile();
    TEST_ASSERT_NOT_NULL(events);
    config = (NetConfig){.users_path = "/dev/null/users", .log = events};
    server = (Dispatcher){.config = &config, .emit = emit};
}

void tearDown(void) { fclose(events); }

static void command(const char *line, const char *expected) {
    char copy[128];
    snprintf(copy, sizeof(copy), "%s", line);
    used = 0;
    output[0] = '\0';
    TEST_ASSERT_EQUAL_INT(0, dispatcher_command(&server, 0, copy, 123));
    TEST_ASSERT_EQUAL_STRING(expected, output);
}

static void test_validation_and_authorization(void) {
    command("", "400|BAD_REQUEST\n");
    command("UNKNOWN", "400|UNSUPPORTED_COMMAND\n");
    command("LOGIN|an", "400|BAD_REQUEST\n");
    command("PING|extra", "400|BAD_REQUEST\n");
    command("REGISTER|an|123", "500|STORAGE_ERROR\n");
    command("LOGIN|an|123", "500|STORAGE_ERROR\n");
    command("LOGOUT", "401|LOGIN_REQUIRED\n");
    command("START", "401|LOGIN_REQUIRED\n");
    command("LEAVE", "401|LOGIN_REQUIRED\n");
    TEST_ASSERT_EQUAL_INT(0, session_login(&server.lobby, 0, "an", 0));
    command("LOGIN|an|123", "409|ALREADY_LOGGED_IN\n");
    command("PING", "PONG\n");
    TEST_ASSERT_EQUAL_INT64(123, server.lobby.sessions[0].last_active);
    command("JOIN", "200|OK\nPLAYERS|1|an\n");
    dispatcher_disconnect(&server, 0);
    TEST_ASSERT_EQUAL_STRING("", server.lobby.sessions[0].username);
    TEST_ASSERT_EQUAL_INT(0, session_login(&server.lobby, 0, "an", 0));
    command("JOIN", "200|OK\nPLAYERS|1|an\n");
    command("LOGOUT", "200|OK\n");
}

static void test_roster_chunks_fit_wire_limit(void) {
    for (size_t i = 0; i < SESSION_LIMIT; ++i) {
        char name[33];
        snprintf(name, sizeof(name), "player_%025zu", i);
        TEST_ASSERT_EQUAL_INT(0, session_login(&server.lobby, i, name, 0));
        TEST_ASSERT_EQUAL_STRING("200|OK\n", lobby_join(&server.lobby, i));
    }
    dispatcher_broadcast(&server);
    TEST_ASSERT_NOT_NULL(strstr(output, "PLAYERS_BEGIN\n"));
    TEST_ASSERT_NOT_NULL(strstr(output, "PLAYERS_END\n"));
    const char *cursor = output;
    while (*cursor) {
        const char *end = strchr(cursor, '\n');
        TEST_ASSERT_NOT_NULL(end);
        TEST_ASSERT_TRUE(end - cursor + 1 <= 1024);
        cursor = end + 1;
    }
    TEST_ASSERT_EQUAL_UINT(99, destination);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_validation_and_authorization);
    RUN_TEST(test_roster_chunks_fit_wire_limit);
    return UNITY_END();
}
