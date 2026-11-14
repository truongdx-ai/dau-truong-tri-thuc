#include "linebuf.h"
#include "net.h"
#include "unity.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t stopped;
static pid_t server;
static int listener = -1;
static int peer = -1;

static void stop(int signal_number) {
    (void)signal_number;
    stopped = 1;
}

void setUp(void) {}

void tearDown(void) {
    if (peer >= 0) {
        close(peer);
        peer = -1;
    }
    if (listener >= 0) {
        close(listener);
        listener = -1;
    }
    if (server > 0) {
        int status;
        kill(server, SIGTERM);
        while (waitpid(server, &status, 0) < 0 && errno == EINTR) {
        }
        server = 0;
    }
}

static void send_all(const char *bytes, size_t length) {
    while (length > 0) {
        ssize_t count = send(peer, bytes, length, MSG_NOSIGNAL);
        TEST_ASSERT_TRUE(count > 0);
        bytes += count;
        length -= (size_t)count;
    }
}

static void expect(const char *expected) {
    char actual[64];
    size_t remaining = strlen(expected);
    TEST_ASSERT_TRUE(remaining < sizeof(actual));
    char *cursor = actual;
    while (remaining > 0) {
        ssize_t count = recv(peer, cursor, remaining, 0);
        TEST_ASSERT_TRUE(count > 0);
        cursor += count;
        remaining -= (size_t)count;
    }
    *cursor = '\0';
    TEST_ASSERT_EQUAL_STRING(expected, actual);
}

static void test_server_handles_stream_and_recovers(void) {
    listener = net_listen(0);
    TEST_ASSERT_TRUE(listener >= 0);
    struct sockaddr_in address;
    socklen_t length = sizeof(address);
    TEST_ASSERT_EQUAL_INT(0, getsockname(listener, (struct sockaddr *)&address, &length));
    TEST_ASSERT_EQUAL_INT(-1, net_listen(ntohs(address.sin_port)));

    server = fork();
    TEST_ASSERT_TRUE(server >= 0);
    if (server == 0) {
        struct sigaction action = {.sa_handler = stop};
        sigemptyset(&action.sa_mask);
        sigaction(SIGTERM, &action, NULL);
        int result = net_run(listener, &stopped);
        close(listener);
        exit(result == 0 ? 0 : 1);
    }

    peer = socket(AF_INET, SOCK_STREAM, 0);
    TEST_ASSERT_TRUE(peer >= 0);
    struct timeval timeout = {.tv_sec = 2};
    TEST_ASSERT_EQUAL_INT(0, setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)));
    TEST_ASSERT_EQUAL_INT(0, connect(peer, (struct sockaddr *)&address, length));
    send_all("PI", 2);
    const char batch[] = "NG\r\nPING\nREGISTER|a|b\nPING\0junk\n";
    send_all(batch, sizeof(batch) - 1);
    expect("PONG\nPONG\n400|UNSUPPORTED_COMMAND\n400|BAD_REQUEST\n");
    char oversized[LINEBUF_MAX];
    memset(oversized, 'X', sizeof(oversized));
    send_all(oversized, sizeof(oversized));
    send_all("\nPING\n", 6);
    expect("400|BAD_REQUEST\nPONG\n");
    TEST_ASSERT_EQUAL_INT(0, shutdown(peer, SHUT_WR));
    char byte;
    TEST_ASSERT_EQUAL_INT(0, recv(peer, &byte, 1, 0));
}

static void test_server_reports_invalid_listener(void) {
    listener = net_listen(0);
    TEST_ASSERT_TRUE(listener >= 0);
    int invalid = listener;
    close(listener);
    listener = -1;
    TEST_ASSERT_EQUAL_INT(-1, net_run(invalid, &stopped));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_server_handles_stream_and_recovers);
    RUN_TEST(test_server_reports_invalid_listener);
    return UNITY_END();
}
