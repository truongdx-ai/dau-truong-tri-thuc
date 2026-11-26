#include "question.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static int load(const char *data, size_t length, Question *questions, size_t capacity,
                size_t *count) {
    FILE *file = tmpfile();
    TEST_ASSERT_NOT_NULL(file);
    size_t written = fwrite(data, 1, length, file);
    rewind(file);
    int result = question_load(file, questions, capacity, count);
    fclose(file);
    TEST_ASSERT_EQUAL_UINT(length, written);
    return result;
}

static void test_load_questions(void) {
    const char data[] = "2 + 2?|1|2|3|4|D\r\n\nCapital?|Hue|Ha Noi|Da Nang|Can Tho|B\n";
    Question questions[2];
    size_t count;
    TEST_ASSERT_EQUAL_INT(0, load(data, sizeof(data) - 1, questions, 2, &count));
    TEST_ASSERT_EQUAL_UINT(2, count);
    TEST_ASSERT_EQUAL_STRING("2 + 2?", questions[0].text);
    TEST_ASSERT_EQUAL_STRING("Ha Noi", questions[1].options[1]);
    TEST_ASSERT_EQUAL_CHAR('D', questions[0].answer);
    TEST_ASSERT_EQUAL_CHAR('B', questions[1].answer);
}

static void test_reject_invalid_questions(void) {
    const char *cases[] = {"", "Q|1|2|3|4|E\n", "Q|1|2|3|4|D", "Q|1|2|3|4|DA\n", "Q|1\n"};
    Question question;
    size_t count;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        TEST_ASSERT_EQUAL_INT(-1, load(cases[i], strlen(cases[i]), &question, 1, &count));
        TEST_ASSERT_EQUAL_UINT(0, count);
    }
    TEST_ASSERT_EQUAL_INT(-1, load("x\0\n", 3, &question, 1, &count));
    const char twice[] = "Q|1|2|3|4|D\nQ|1|2|3|4|C\n";
    TEST_ASSERT_EQUAL_INT(-1, load(twice, sizeof(twice) - 1, &question, 1, &count));
    char oversized[1024];
    memset(oversized, 'x', sizeof(oversized));
    TEST_ASSERT_EQUAL_INT(-1, load(oversized, sizeof(oversized), &question, 1, &count));
}

static void test_shuffle_preserves_question_and_answer_pairs(void) {
    Question questions[4] = {0};
    for (size_t i = 0; i < 4; ++i) {
        questions[i].text[0] = (char)('A' + i);
        questions[i].answer = (char)('A' + i);
    }
    TEST_ASSERT_EQUAL_INT(0, question_shuffle(questions, 0));
    TEST_ASSERT_EQUAL_INT(0, question_shuffle(questions, 1));
    TEST_ASSERT_EQUAL_INT(-1, question_shuffle(questions, QUESTION_MAX_COUNT + 1));
    TEST_ASSERT_EQUAL_INT(0, question_shuffle(questions, 4));
    unsigned seen = 0;
    for (size_t i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_CHAR(questions[i].text[0], questions[i].answer);
        seen |= 1U << (questions[i].answer - 'A');
    }
    TEST_ASSERT_EQUAL_UINT(15, seen);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_load_questions);
    RUN_TEST(test_reject_invalid_questions);
    RUN_TEST(test_shuffle_preserves_question_and_answer_pairs);
    return UNITY_END();
}
