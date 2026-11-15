#include "account.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char directory[] = "/tmp/dttt-account-XXXXXX";
static char database[128];

void setUp(void) {
    memcpy(directory, "/tmp/dttt-account-XXXXXX", sizeof(directory));
    TEST_ASSERT_NOT_NULL(mkdtemp(directory));
    snprintf(database, sizeof(database), "%s/users.db", directory);
}

void tearDown(void) {
    char lock[sizeof(database) + 5];
    snprintf(lock, sizeof(lock), "%s.lock", database);
    unlink(lock);
    unlink(database);
    rmdir(directory);
}

static void test_registration_persists_unique_salted_hashes(void) {
    TEST_ASSERT_EQUAL(ACCOUNT_OK, account_register(database, "an", "same-secret"));
    TEST_ASSERT_EQUAL(ACCOUNT_EXISTS, account_register(database, "an", "different"));
    TEST_ASSERT_EQUAL(ACCOUNT_OK, account_register(database, "anh", "same-secret"));
    FILE *file = fopen(database, "r");
    TEST_ASSERT_NOT_NULL(file);
    char first[256], second[256];
    char *record1 = fgets(first, sizeof(first), file);
    char *record2 = fgets(second, sizeof(second), file);
    int end = fgetc(file);
    fclose(file);
    TEST_ASSERT_NOT_NULL(record1);
    TEST_ASSERT_NOT_NULL(record2);
    TEST_ASSERT_EQUAL_INT(EOF, end);
    const char prefix[] = "an|pbkdf2-sha256|600000|";
    TEST_ASSERT_EQUAL_STRING_LEN(prefix, first, sizeof(prefix) - 1);
    char *salt1 = first + sizeof(prefix) - 1;
    char *salt2 = second + sizeof(prefix);
    TEST_ASSERT_EQUAL_UINT(98, strlen(salt1));
    TEST_ASSERT_EQUAL_CHAR('|', salt1[32]);
    TEST_ASSERT_TRUE(strncmp(salt1, salt2, 32) != 0);
    TEST_ASSERT_TRUE(strncmp(salt1 + 33, salt2 + 33, 64) != 0);
    TEST_ASSERT_NULL(strstr(first, "same-secret"));
}

static void test_invalid_and_storage_failure(void) {
    TEST_ASSERT_EQUAL(ACCOUNT_INVALID, account_register(database, "", "123"));
    TEST_ASSERT_EQUAL(ACCOUNT_INVALID, account_register(database, "bad name", "123"));
    TEST_ASSERT_EQUAL(ACCOUNT_INVALID, account_register(database, "an", ""));
    TEST_ASSERT_EQUAL(ACCOUNT_INVALID, account_register(database, "an", "one|two"));
    TEST_ASSERT_EQUAL(ACCOUNT_ERROR, account_register("/dev/null/users.db", "an", "123"));
    FILE *file = fopen(database, "w");
    TEST_ASSERT_NOT_NULL(file);
    fputs("incomplete", file);
    fclose(file);
    TEST_ASSERT_EQUAL(ACCOUNT_ERROR, account_register(database, "an", "123"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_registration_persists_unique_salted_hashes);
    RUN_TEST(test_invalid_and_storage_failure);
    return UNITY_END();
}
