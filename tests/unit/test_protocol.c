#include "protocol.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_parse_register(void) {
    char line[] = "REGISTER|an|123";
    ProtocolMessage message;
    TEST_ASSERT_EQUAL_INT(0, protocol_parse(line, &message));
    TEST_ASSERT_EQUAL_UINT(3, message.count);
    TEST_ASSERT_EQUAL_STRING("REGISTER", message.fields[0]);
    TEST_ASSERT_EQUAL_STRING("an", message.fields[1]);
    TEST_ASSERT_EQUAL_STRING("123", message.fields[2]);
}

static void test_parse_rejects_malformed_fields(void) {
    char cases[][32] = {"", "|A", "CMD||A", "CMD|", "CMD\rX", "A|B|C|D|E|F|G|H|I|J"};
    ProtocolMessage message;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        TEST_ASSERT_EQUAL_INT(-1, protocol_parse(cases[i], &message));
        TEST_ASSERT_EQUAL_UINT(0, message.count);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_register);
    RUN_TEST(test_parse_rejects_malformed_fields);
    return UNITY_END();
}
