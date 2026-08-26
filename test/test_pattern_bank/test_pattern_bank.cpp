#include <stdint.h>
#include <unity.h>

#include <flexseq/PatternBank.h>

using flexseq::Pattern;
using flexseq::PatternBank;
using flexseq::PATTERN_COUNT;

void setUp() {}
void tearDown() {}

void test_pattern_bank_has_expected_memory_footprint() {
    TEST_ASSERT_EQUAL_UINT(368, sizeof(PatternBank));
    TEST_ASSERT_EQUAL_UINT(PATTERN_COUNT * sizeof(Pattern), sizeof(PatternBank));
}

void test_pattern_bank_exposes_16_patterns() {
    PatternBank bank;

    for (uint8_t i = 0; i < PATTERN_COUNT; ++i) {
        TEST_ASSERT_NOT_NULL(bank.getPattern(i));
    }
}

void test_pattern_bank_rejects_out_of_range_index() {
    PatternBank bank;
    TEST_ASSERT_NULL(bank.getPattern(16));
    TEST_ASSERT_NULL(bank.getPattern(255));
}

void test_pattern_bank_returns_same_instance_for_index() {
    PatternBank bank;

    Pattern* a = bank.getPattern(3);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_TRUE(a->writeStep(0, true));

    Pattern* again = bank.getPattern(3);
    TEST_ASSERT_EQUAL_PTR(a, again);

    bool active = false;
    TEST_ASSERT_TRUE(again->readStep(0, active));
    TEST_ASSERT_TRUE(active);
}

void test_pattern_bank_keeps_distinct_patterns_independent() {
    PatternBank bank;

    TEST_ASSERT_TRUE(bank.getPattern(0)->writeStep(5, true));

    bool active = true;
    TEST_ASSERT_TRUE(bank.getPattern(1)->readStep(5, active));
    TEST_ASSERT_FALSE(active);
}

void test_pattern_bank_returns_const_pattern_from_const_bank() {
    const PatternBank bank;

    const Pattern* pattern = bank.getPattern(0);
    TEST_ASSERT_NOT_NULL(pattern);

    bool active = true;
    TEST_ASSERT_TRUE(pattern->readStep(0, active));
    TEST_ASSERT_FALSE(active);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_pattern_bank_has_expected_memory_footprint);
    RUN_TEST(test_pattern_bank_exposes_16_patterns);
    RUN_TEST(test_pattern_bank_rejects_out_of_range_index);
    RUN_TEST(test_pattern_bank_returns_same_instance_for_index);
    RUN_TEST(test_pattern_bank_keeps_distinct_patterns_independent);
    RUN_TEST(test_pattern_bank_returns_const_pattern_from_const_bank);

    return UNITY_END();
}
