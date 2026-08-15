#include <unity.h>

#include <flexseq/PatternStore.h>

using flexseq::Pattern;
using flexseq::PatternStore;

void test_pattern_store_has_expected_memory_footprint() {
    TEST_ASSERT_EQUAL_UINT(
        384,
        sizeof(PatternStore)
    );
}

void test_pattern_store_returns_first_pattern() {
    PatternStore store;

    Pattern* pattern = store.getPattern(0, 0);

    TEST_ASSERT_NOT_NULL(pattern);
    TEST_ASSERT_EQUAL_UINT8(16, pattern->getBaseLength());
}

void test_pattern_store_returns_last_pattern() {
    PatternStore store;

    Pattern* pattern = store.getPattern(5, 15);

    TEST_ASSERT_NOT_NULL(pattern);
    TEST_ASSERT_EQUAL_UINT8(16, pattern->getBaseLength());
}

void test_pattern_store_rejects_invalid_channel() {
    PatternStore store;

    Pattern* pattern = store.getPattern(6, 0);

    TEST_ASSERT_NULL(pattern);
}

void test_pattern_store_rejects_invalid_pattern_index() {
    PatternStore store;

    Pattern* pattern = store.getPattern(0, 16);

    TEST_ASSERT_NULL(pattern);
}

void test_pattern_store_isolates_patterns_within_same_channel() {
    PatternStore store;

    Pattern* a1 = store.getPattern(0, 0);
    Pattern* a2 = store.getPattern(0, 1);

    TEST_ASSERT_NOT_NULL(a1);
    TEST_ASSERT_NOT_NULL(a2);

    TEST_ASSERT_TRUE(a1->writeStep(23, true));

    bool active = false;

    TEST_ASSERT_TRUE(a1->readStep(23, active));
    TEST_ASSERT_TRUE(active);

    TEST_ASSERT_TRUE(a2->readStep(23, active));
    TEST_ASSERT_FALSE(active);
}

void test_pattern_store_isolates_patterns_between_channels() {
    PatternStore store;

    Pattern* channel1 = store.getPattern(0, 0);
    Pattern* channel2 = store.getPattern(1, 0);

    TEST_ASSERT_NOT_NULL(channel1);
    TEST_ASSERT_NOT_NULL(channel2);

    TEST_ASSERT_TRUE(channel1->writeStep(23, true));

    bool active = false;

    TEST_ASSERT_TRUE(channel1->readStep(23, active));
    TEST_ASSERT_TRUE(active);

    TEST_ASSERT_TRUE(channel2->readStep(23, active));
    TEST_ASSERT_FALSE(active);
}

void test_pattern_store_maps_first_eight_patterns_to_bank_a() {
    PatternStore store;

    for (uint8_t patternIndex = 0; patternIndex < 8; ++patternIndex) {
        Pattern* pattern = store.getPattern(0, patternIndex);

        TEST_ASSERT_NOT_NULL(pattern);
        TEST_ASSERT_EQUAL_UINT8(16, pattern->getBaseLength());
    }
}

void test_pattern_store_maps_last_eight_patterns_to_bank_b() {
    PatternStore store;

    for (uint8_t patternIndex = 8; patternIndex < 16; ++patternIndex) {
        Pattern* pattern = store.getPattern(0, patternIndex);

        TEST_ASSERT_NOT_NULL(pattern);
        TEST_ASSERT_EQUAL_UINT8(16, pattern->getBaseLength());
    }
}

void test_pattern_store_returns_const_pattern_from_const_store() {
    const PatternStore store;

    const Pattern* pattern = store.getPattern(0, 0);

    TEST_ASSERT_NOT_NULL(pattern);
    TEST_ASSERT_EQUAL_UINT8(16, pattern->getBaseLength());
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_pattern_store_has_expected_memory_footprint);
    RUN_TEST(test_pattern_store_returns_first_pattern);
    RUN_TEST(test_pattern_store_returns_last_pattern);
    RUN_TEST(test_pattern_store_rejects_invalid_channel);
    RUN_TEST(test_pattern_store_rejects_invalid_pattern_index);
    RUN_TEST(test_pattern_store_isolates_patterns_within_same_channel);
    RUN_TEST(test_pattern_store_isolates_patterns_between_channels);
    RUN_TEST(test_pattern_store_maps_first_eight_patterns_to_bank_a);
    RUN_TEST(test_pattern_store_maps_last_eight_patterns_to_bank_b);
    RUN_TEST(test_pattern_store_returns_const_pattern_from_const_store);

    return UNITY_END();
}