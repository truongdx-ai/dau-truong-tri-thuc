#include "project.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

static void test_project_name_matches_title(void) {
    TEST_ASSERT_EQUAL_STRING("Dau truong tri thuc", project_name());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_project_name_matches_title);
    return UNITY_END();
}
