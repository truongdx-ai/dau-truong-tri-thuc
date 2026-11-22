#include "linebuf.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_split_and_consecutive_lines(void) {
    Linebuf buffer = {0};
    TEST_ASSERT_EQUAL(LINEBUF_MORE, linebuf_feed(&buffer, 'A'));
    TEST_ASSERT_EQUAL(LINEBUF_MORE, linebuf_feed(&buffer, '\r'));
    TEST_ASSERT_EQUAL(LINEBUF_READY, linebuf_feed(&buffer, '\n'));
    TEST_ASSERT_EQUAL_STRING("A", buffer.data);
    TEST_ASSERT_EQUAL(LINEBUF_READY, linebuf_feed(&buffer, '\n'));
    TEST_ASSERT_EQUAL_STRING("", buffer.data);
}

static void test_limit_and_recovery(void) {
    Linebuf buffer = {0};
    for (size_t i = 0; i < LINEBUF_MAX - 1; ++i) {
        TEST_ASSERT_EQUAL(LINEBUF_MORE, linebuf_feed(&buffer, 'x'));
    }
    TEST_ASSERT_EQUAL(LINEBUF_READY, linebuf_feed(&buffer, '\n'));
    for (size_t i = 0; i < LINEBUF_MAX - 1; ++i) {
        TEST_ASSERT_EQUAL(LINEBUF_MORE, linebuf_feed(&buffer, 'x'));
    }
    TEST_ASSERT_EQUAL(LINEBUF_TOO_LONG, linebuf_feed(&buffer, 'x'));
    TEST_ASSERT_EQUAL(LINEBUF_MORE, linebuf_feed(&buffer, 'x'));
    TEST_ASSERT_EQUAL(LINEBUF_MORE, linebuf_feed(&buffer, '\n'));
    TEST_ASSERT_EQUAL(LINEBUF_MORE, linebuf_feed(&buffer, 'B'));
    TEST_ASSERT_EQUAL(LINEBUF_READY, linebuf_feed(&buffer, '\n'));
    TEST_ASSERT_EQUAL_STRING("B", buffer.data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_split_and_consecutive_lines);
    RUN_TEST(test_limit_and_recovery);
    return UNITY_END();
}
