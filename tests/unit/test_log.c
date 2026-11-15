#include "log.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_timestamp_level_and_event(void) {
    FILE *stream = tmpfile();
    TEST_ASSERT_NOT_NULL(stream);
    int logged = log_event(stream, LOG_WARN, "register duplicate");
    int invalid = log_event(stream, LOG_INFO, "new\nline");
    rewind(stream);
    char line[128];
    char *read = fgets(line, sizeof(line), stream);
    int end = fgetc(stream);
    fclose(stream);
    TEST_ASSERT_EQUAL_INT(0, logged);
    TEST_ASSERT_EQUAL_INT(-1, invalid);
    TEST_ASSERT_NOT_NULL(read);
    TEST_ASSERT_EQUAL_INT(EOF, end);
    TEST_ASSERT_EQUAL_CHAR('T', line[10]);
    TEST_ASSERT_EQUAL_STRING("Z [WARN] register duplicate\n", line + 19);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_timestamp_level_and_event);
    return UNITY_END();
}
