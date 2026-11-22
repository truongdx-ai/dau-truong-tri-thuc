#include "dispatcher.h"
#include "lobby.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

static char output[8192];
static size_t used;
static size_t destination;
static unsigned question_frames[SESSION_LIMIT];
static char received[SESSION_LIMIT][8192];
static size_t received_length[SESSION_LIMIT];
static FILE *events;
static Dispatcher server;
static NetConfig config;

static int emit(void *context, size_t slot, const char *line) {
    (void)context;
    destination = slot;
    size_t n = strlen(line);
    if (n + received_length[slot] >= sizeof(received[slot])) {
        return -1;
    }
    memcpy(received[slot] + received_length[slot], line, n + 1);
    received_length[slot] += n;
    if (strncmp(line, "QUESTION|", 9) == 0) {
        ++question_frames[slot];
    }
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
    memset(question_frames, 0, sizeof(question_frames));
    memset(received, 0, sizeof(received));
    memset(received_length, 0, sizeof(received_length));
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
    dispatcher_disconnect(&server, 0, 123);
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

static void join_two_players(void) {
    TEST_ASSERT_EQUAL_INT(0, session_login(&server.lobby, 0, "an", 0));
    TEST_ASSERT_EQUAL_INT(0, session_login(&server.lobby, 1, "tu", 0));
    TEST_ASSERT_EQUAL_STRING("200|OK\n", lobby_join(&server.lobby, 0));
    TEST_ASSERT_EQUAL_STRING("200|OK\n", lobby_join(&server.lobby, 1));
}

static void test_start_sends_question_only_to_joined_players(void) {
    Question question = {.text = "2 + 2?", .options = {"1", "2", "3", "4"}, .answer = 'D'};
    config.questions = &question;
    config.question_count = 1;
    TEST_ASSERT_EQUAL_INT(0, session_login(&server.lobby, 0, "an", 0));
    TEST_ASSERT_EQUAL_STRING("200|OK\n", lobby_join(&server.lobby, 0));
    command("START", "412|NOT_ENOUGH_PLAYERS\n");
    TEST_ASSERT_EQUAL_UINT(0, question_frames[0]);
    TEST_ASSERT_EQUAL_INT(0, session_login(&server.lobby, 1, "tu", 0));
    TEST_ASSERT_EQUAL_STRING("200|OK\n", lobby_join(&server.lobby, 1));
    TEST_ASSERT_EQUAL_INT(0, session_login(&server.lobby, 2, "spectator", 0));
    char start[] = "START";
    TEST_ASSERT_EQUAL_INT(0, dispatcher_command(&server, 1, start, 123));
    TEST_ASSERT_EQUAL_UINT(0, question_frames[0]);
    command("START", "200|OK\nPLAYERS|2|an,tu\nQUESTION|1|QUALIFY|30|2 + 2?|1|2|3|4\n");
    TEST_ASSERT_TRUE(server.lobby.running);
    TEST_ASSERT_EQUAL_UINT(1, question_frames[0]);
    TEST_ASSERT_EQUAL_UINT(1, question_frames[1]);
    TEST_ASSERT_EQUAL_UINT(0, question_frames[2]);
    command("START", "409|GAME_RUNNING\n");
    TEST_ASSERT_EQUAL_UINT(1, question_frames[0]);
    TEST_ASSERT_EQUAL_UINT(1, question_frames[1]);
}

static void test_start_without_questions_keeps_lobby_waiting(void) {
    join_two_players();
    command("START", "500|NO_QUESTIONS\n");
    TEST_ASSERT_FALSE(server.lobby.running);
    TEST_ASSERT_EQUAL_UINT(0, question_frames[0]);
    TEST_ASSERT_EQUAL_UINT(0, question_frames[1]);
}

static void test_start_with_invalid_question_keeps_lobby_waiting(void) {
    join_two_players();
    Question question = {.text = "Bad|question", .options = {"1", "2", "3", "4"}, .answer = 'D'};
    config.questions = &question;
    config.question_count = 1;
    command("START", "500|QUESTION_ERROR\n");
    TEST_ASSERT_FALSE(server.lobby.running);
    TEST_ASSERT_EQUAL_UINT(0, question_frames[0]);
    TEST_ASSERT_EQUAL_UINT(0, question_frames[1]);
}

static void send_command(size_t slot, const char *line, int64_t now) {
    char copy[128];
    snprintf(copy, sizeof(copy), "%s", line);
    used = 0;
    output[0] = '\0';
    memset(received, 0, sizeof(received));
    memset(received_length, 0, sizeof(received_length));
    TEST_ASSERT_EQUAL_INT(0, dispatcher_command(&server, slot, copy, now));
}

static void prepare_qualification(void) {
    static const Question question = {.text = "Q", .options = {"1", "2", "3", "4"}, .answer = 'D'};
    config.questions = &question;
    config.question_count = 1;
    config.qualification_timeout_ms = 1000;
    join_two_players();
    send_command(0, "START", 1000);
    TEST_ASSERT_EQUAL_STRING("200|OK\nPLAYERS|2|an,tu\nQUESTION|1|QUALIFY|1|Q|1|2|3|4\n",
                             received[0]);
}

static void test_answer_validation_and_authorization(void) {
    command("ANSWER|1|D", "401|LOGIN_REQUIRED\n");
    TEST_ASSERT_EQUAL_INT(0, session_login(&server.lobby, 0, "an", 0));
    command("ANSWER|1|D", "403|NOT_ALLOWED\n");
    command("ANSWER|1", "400|BAD_REQUEST\n");
    command("ANSWER|1|E", "400|BAD_REQUEST\n");
    command("ANSWER|1|DA", "400|BAD_REQUEST\n");
    command("ANSWER|0|D", "400|BAD_REQUEST\n");
    command("ANSWER|-1|D", "400|BAD_REQUEST\n");
    command("ANSWER|+1|D", "400|BAD_REQUEST\n");
    command("ANSWER|18446744073709551616|D", "400|BAD_REQUEST\n");
}

static void test_answers_emit_ack_before_roles_and_block_repeats(void) {
    prepare_qualification();
    send_command(0, "ANSWER|2|D", 1100);
    TEST_ASSERT_EQUAL_STRING("409|WRONG_QUESTION\n", received[0]);
    send_command(0, "ANSWER|1|D", 1100);
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\n", received[0]);
    TEST_ASSERT_EQUAL_STRING("", received[1]);
    send_command(0, "ANSWER|1|A", 1150);
    TEST_ASSERT_EQUAL_STRING("409|ALREADY_ANSWERED\n", received[0]);
    send_command(1, "ANSWER|1|A", 1200);
    TEST_ASSERT_EQUAL_STRING("ROLE|MAIN\n", received[0]);
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\nROLE|SUB|an\n", received[1]);
    TEST_ASSERT_EQUAL_INT(0, server.qualification.main_slot);
    send_command(1, "ANSWER|1|D", 1250);
    TEST_ASSERT_EQUAL_STRING("409|ALREADY_ANSWERED\n", received[1]);
    TEST_ASSERT_EQUAL_STRING("", received[0]);
}

static void test_timer_completes_without_incoming_commands(void) {
    prepare_qualification();
    send_command(1, "ANSWER|1|D", 1999);
    dispatcher_tick(&server, 2000);
    TEST_ASSERT_EQUAL_STRING("ROLE|SUB|tu\n", received[0]);
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\nROLE|MAIN\n", received[1]);
    TEST_ASSERT_EQUAL_INT(1, server.qualification.main_slot);
}

static void test_late_answer_is_rejected_before_no_winner_event(void) {
    prepare_qualification();
    send_command(1, "ANSWER|1|D", 2000);
    TEST_ASSERT_EQUAL_STRING("408|TIMEOUT\nGAME_OVER|NO_WINNER\n", received[1]);
    TEST_ASSERT_EQUAL_STRING("GAME_OVER|NO_WINNER\n", received[0]);
    TEST_ASSERT_FALSE(server.lobby.running);
    send_command(1, "PING", 2001);
    dispatcher_tick(&server, 3000);
    TEST_ASSERT_EQUAL_STRING("PONG\n", received[1]);
    TEST_ASSERT_EQUAL_STRING("", received[0]);
}

static void test_leave_logout_and_disconnect_finish_pending_round(void) {
    prepare_qualification();
    send_command(1, "ANSWER|1|D", 1100);
    send_command(0, "LEAVE", 1200);
    TEST_ASSERT_EQUAL_STRING("200|OK\n", received[0]);
    TEST_ASSERT_EQUAL_STRING("PLAYERS|1|tu\nROLE|MAIN\n", received[1]);
    TEST_ASSERT_FALSE(server.qualification.players[0].participating);
    send_command(0, "ANSWER|1|D", 1250);
    TEST_ASSERT_EQUAL_STRING("403|NOT_ALLOWED\n", received[0]);
    send_command(1, "LOGOUT", 1300);
    TEST_ASSERT_EQUAL_STRING("200|OK\n", received[1]);
    TEST_ASSERT_FALSE(server.lobby.running);
    TEST_ASSERT_EQUAL(QUALIFY_IDLE, server.qualification.phase);
}

static void test_selected_main_disconnect_promotes_remaining_correct_player(void) {
    prepare_qualification();
    send_command(0, "ANSWER|1|D", 1100);
    send_command(1, "ANSWER|1|D", 1200);
    send_command(1, "PING", 1300);
    dispatcher_disconnect(&server, 0, 1300);
    TEST_ASSERT_EQUAL_STRING("PONG\nPLAYERS|1|tu\nROLE|MAIN\n", received[1]);
    TEST_ASSERT_EQUAL_INT(1, server.qualification.main_slot);
    TEST_ASSERT_EQUAL_STRING("", server.lobby.sessions[0].username);
}

static void test_new_round_rotates_question_and_rejects_stale_id(void) {
    prepare_qualification();
    Question bank[] = {{.text = "First", .options = {"1", "2", "3", "4"}, .answer = 'D'},
                       {.text = "Second", .options = {"a", "b", "c", "d"}, .answer = 'C'}};
    config.questions = bank;
    config.question_count = 2;
    send_command(0, "ANSWER|1|A", 1100);
    send_command(1, "ANSWER|1|A", 1200);
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\nGAME_OVER|NO_WINNER\n", received[1]);
    send_command(0, "START", 1300);
    TEST_ASSERT_EQUAL_STRING("200|OK\nPLAYERS|2|an,tu\nQUESTION|2|QUALIFY|1|Second|a|b|c|d\n",
                             received[0]);
    send_command(0, "ANSWER|1|D", 1400);
    TEST_ASSERT_EQUAL_STRING("409|WRONG_QUESTION\n", received[0]);
    send_command(0, "ANSWER|2|C", 1500);
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\n", received[0]);
    send_command(1, "ANSWER|2|C", 1600);
    TEST_ASSERT_EQUAL_STRING("ROLE|MAIN\n", received[0]);
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\nROLE|SUB|an\n", received[1]);
}

static int64_t completion_time;
static int clock_calls;

static int64_t completed_clock(void) {
    ++clock_calls;
    return completion_time;
}

static void test_idle_timeout_starts_after_command_processing(void) {
    TEST_ASSERT_EQUAL_INT(0, session_login(&server.lobby, 0, "an", 0));
    config.now_ms = completed_clock;
    completion_time = 2000;
    clock_calls = 0;
    send_command(0, "PING", 1000);
    TEST_ASSERT_EQUAL_STRING("PONG\n", received[0]);
    TEST_ASSERT_EQUAL_INT64(2000, server.lobby.sessions[0].last_active);
    TEST_ASSERT_FALSE(session_expired(&server.lobby.sessions[0], 2199, 200));
    TEST_ASSERT_TRUE(session_expired(&server.lobby.sessions[0], 2200, 200));
    TEST_ASSERT_EQUAL_INT(1, clock_calls);
    send_command(0, "UNKNOWN", 2100);
    TEST_ASSERT_EQUAL_INT64(2000, server.lobby.sessions[0].last_active);
    TEST_ASSERT_EQUAL_INT(1, clock_calls);
}

static void test_completion_clock_does_not_change_answer_receipt_or_deadline(void) {
    prepare_qualification();
    config.now_ms = completed_clock;
    completion_time = 5000;
    send_command(0, "ANSWER|1|D", 1100);
    TEST_ASSERT_EQUAL_STRING("200|RECEIVED\n", received[0]);
    TEST_ASSERT_EQUAL_INT64(1100, server.qualification.players[0].received_at);
    TEST_ASSERT_EQUAL_INT64(2000, server.qualification.deadline);
    TEST_ASSERT_EQUAL_INT64(5000, server.lobby.sessions[0].last_active);
    dispatcher_tick(&server, 5000);
    TEST_ASSERT_EQUAL_INT(0, server.qualification.main_slot);
}

static void test_failed_or_backward_completion_clock_keeps_receipt_time(void) {
    TEST_ASSERT_EQUAL_INT(0, session_login(&server.lobby, 0, "an", 0));
    config.now_ms = completed_clock;
    completion_time = -1;
    send_command(0, "PING", 1000);
    TEST_ASSERT_EQUAL_INT64(1000, server.lobby.sessions[0].last_active);
    completion_time = 500;
    send_command(0, "PING", 1100);
    TEST_ASSERT_EQUAL_INT64(1100, server.lobby.sessions[0].last_active);
    send_command(0, "LOGOUT", 1200);
    TEST_ASSERT_EQUAL_STRING("", server.lobby.sessions[0].username);
    TEST_ASSERT_EQUAL_INT64(0, server.lobby.sessions[0].last_active);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_validation_and_authorization);
    RUN_TEST(test_roster_chunks_fit_wire_limit);
    RUN_TEST(test_start_sends_question_only_to_joined_players);
    RUN_TEST(test_start_without_questions_keeps_lobby_waiting);
    RUN_TEST(test_start_with_invalid_question_keeps_lobby_waiting);
    RUN_TEST(test_answer_validation_and_authorization);
    RUN_TEST(test_answers_emit_ack_before_roles_and_block_repeats);
    RUN_TEST(test_timer_completes_without_incoming_commands);
    RUN_TEST(test_late_answer_is_rejected_before_no_winner_event);
    RUN_TEST(test_leave_logout_and_disconnect_finish_pending_round);
    RUN_TEST(test_selected_main_disconnect_promotes_remaining_correct_player);
    RUN_TEST(test_new_round_rotates_question_and_rejects_stale_id);
    RUN_TEST(test_idle_timeout_starts_after_command_processing);
    RUN_TEST(test_completion_clock_does_not_change_answer_receipt_or_deadline);
    RUN_TEST(test_failed_or_backward_completion_clock_keeps_receipt_time);
    return UNITY_END();
}
