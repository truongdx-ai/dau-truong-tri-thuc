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

static void test_format_frame_and_errors(void) {
    char output[16];
    const char *fields[] = {"200", "OK"};
    TEST_ASSERT_EQUAL_INT(7, protocol_format(output, 8, fields, 2));
    TEST_ASSERT_EQUAL_STRING("200|OK\n", output);
    TEST_ASSERT_EQUAL_INT(-1, protocol_format(output, 7, fields, 2));
    TEST_ASSERT_EQUAL_INT(-1, protocol_format(output, sizeof(output), fields, 0));
    fields[1] = "O|K";
    TEST_ASSERT_EQUAL_INT(-1, protocol_format(output, sizeof(output), fields, 2));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_register);
    RUN_TEST(test_parse_rejects_malformed_fields);
    RUN_TEST(test_format_frame_and_errors);
    return UNITY_END();
}
