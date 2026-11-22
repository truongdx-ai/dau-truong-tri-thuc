#include "account.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char path[] = "/tmp/dttt-auth-XXXXXX";
void setUp(void) {
    memcpy(path, "/tmp/dttt-auth-XXXXXX", sizeof(path));
    int fd = mkstemp(path);
    TEST_ASSERT_TRUE(fd >= 0);
    close(fd);
}
void tearDown(void) {
    char lock[128];
    snprintf(lock, sizeof(lock), "%s.lock", path);
    unlink(lock);
    unlink(path);
}

static void test_reads_existing_registration_and_checks_password(void) {
    TEST_ASSERT_EQUAL(ACCOUNT_OK, account_register(path, "an", "123"));
    TEST_ASSERT_EQUAL(ACCOUNT_OK, account_register(path, "tu", "456"));
    TEST_ASSERT_EQUAL(ACCOUNT_OK, account_authenticate(path, "an", "123"));
    TEST_ASSERT_EQUAL(ACCOUNT_OK, account_authenticate(path, "tu", "456"));
    TEST_ASSERT_EQUAL(ACCOUNT_INVALID, account_authenticate(path, "an", "456"));
    TEST_ASSERT_EQUAL(ACCOUNT_INVALID, account_authenticate(path, "unknown", "123"));
    TEST_ASSERT_EQUAL(ACCOUNT_INVALID, account_authenticate(path, "", "123"));
    unlink(path);
    TEST_ASSERT_EQUAL(ACCOUNT_INVALID, account_authenticate(path, "an", "123"));
    TEST_ASSERT_EQUAL(ACCOUNT_ERROR, account_authenticate("/dev/null/users", "an", "123"));
}

static void test_corrupt_records_fail_closed(void) {
    const char *records[] = {"broken",
                             "an|broken\n",
                             "an|other|600000|a|b\n",
                             "an|pbkdf2-sha256|9999999|a|b\n",
                             "an|pbkdf2-sha256|600000|a|b\n",
                             "an|pbkdf2-sha256|600000|zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz|b\n"};
    for (size_t i = 0; i < sizeof(records) / sizeof(records[0]); ++i) {
        FILE *file = fopen(path, "w");
        TEST_ASSERT_NOT_NULL(file);
        fputs(records[i], file);
        fclose(file);
        TEST_ASSERT_EQUAL(ACCOUNT_ERROR, account_authenticate(path, "an", "123"));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reads_existing_registration_and_checks_password);
    RUN_TEST(test_corrupt_records_fail_closed);
    return UNITY_END();
}
