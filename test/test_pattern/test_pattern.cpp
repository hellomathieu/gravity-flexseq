#include <stdint.h>
#include <unity.h>

#include "flexseq/Pattern.h"

using flexseq::Pattern;

void setUp() {}
void tearDown() {}

void test_pattern_has_expected_memory_footprint() {
    TEST_ASSERT_EQUAL_UINT16(4, sizeof(Pattern));
}

void test_pattern_defaults_to_length_16_and_all_steps_off() {
    Pattern pattern;

    TEST_ASSERT_EQUAL_UINT8(16, pattern.getBaseLength());

    for (uint8_t i = 0; i < Pattern::STEP_COUNT; ++i) {
        bool active = true;
        TEST_ASSERT_TRUE(pattern.readStep(i, active));
        TEST_ASSERT_FALSE(active);
    }
}

void test_pattern_accepts_every_valid_base_length() {
    Pattern pattern;

    for (uint8_t length = Pattern::MIN_PATTERN_LENGTH;
         length <= Pattern::MAX_PATTERN_LENGTH;
         ++length) {
        TEST_ASSERT_TRUE(pattern.setBaseLength(length));
        TEST_ASSERT_EQUAL_UINT8(length, pattern.getBaseLength());
    }
}

void test_pattern_rejects_invalid_base_lengths_without_mutation() {
    Pattern pattern;
    TEST_ASSERT_TRUE(pattern.setBaseLength(12));

    TEST_ASSERT_FALSE(pattern.setBaseLength(0));
    TEST_ASSERT_EQUAL_UINT8(12, pattern.getBaseLength());

    TEST_ASSERT_FALSE(pattern.setBaseLength(25));
    TEST_ASSERT_EQUAL_UINT8(12, pattern.getBaseLength());
    TEST_ASSERT_FALSE(pattern.setBaseLength(255));
    TEST_ASSERT_EQUAL_UINT8(12, pattern.getBaseLength());
}

void test_pattern_writes_and_reads_all_24_steps() {
    Pattern pattern;

    for (uint8_t i = 0; i < Pattern::STEP_COUNT; ++i) {
        TEST_ASSERT_TRUE(pattern.writeStep(i, (i % 2) == 0));
    }

    for (uint8_t i = 0; i < Pattern::STEP_COUNT; ++i) {
        bool active = false;
        TEST_ASSERT_TRUE(pattern.readStep(i, active));
        TEST_ASSERT_EQUAL((i % 2) == 0, active);
    }
}

void test_pattern_covers_bit_boundaries_0_7_8_15_16_23() {
    Pattern pattern;
    const uint8_t boundarySteps[] = {0, 7, 8, 15, 16, 23};

    for (uint8_t i = 0; i < sizeof(boundarySteps) / sizeof(boundarySteps[0]); ++i) {
        TEST_ASSERT_TRUE(pattern.writeStep(boundarySteps[i], true));
    }

    for (uint8_t i = 0; i < Pattern::STEP_COUNT; ++i) {
        bool active = false;
        TEST_ASSERT_TRUE(pattern.readStep(i, active));

        bool expected =
            i == 0 || i == 7 || i == 8 || i == 15 || i == 16 || i == 23;
        TEST_ASSERT_EQUAL(expected, active);
    }
}

void test_pattern_rejects_step_index_24_without_mutation() {
    Pattern pattern;
    bool active = true;

    TEST_ASSERT_FALSE(pattern.readStep(24, active));
    TEST_ASSERT_TRUE(active);

    TEST_ASSERT_TRUE(pattern.writeStep(23, true));
    TEST_ASSERT_FALSE(pattern.writeStep(24, false));

    active = false;
    TEST_ASSERT_TRUE(pattern.readStep(23, active));
    TEST_ASSERT_TRUE(active);
}

void test_pattern_changing_length_preserves_steps_outside_length() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setBaseLength(8));
    TEST_ASSERT_TRUE(pattern.writeStep(8, true));
    TEST_ASSERT_TRUE(pattern.writeStep(23, true));

    TEST_ASSERT_TRUE(pattern.setBaseLength(4));
    TEST_ASSERT_TRUE(pattern.setBaseLength(24));

    bool active = false;
    TEST_ASSERT_TRUE(pattern.readStep(8, active));
    TEST_ASSERT_TRUE(active);
    TEST_ASSERT_TRUE(pattern.readStep(23, active));
    TEST_ASSERT_TRUE(active);
}

void test_pattern_clear_turns_all_24_steps_off_and_preserves_length() {
    Pattern pattern;
    TEST_ASSERT_TRUE(pattern.setBaseLength(12));

    for (uint8_t i = 0; i < Pattern::STEP_COUNT; ++i) {
        TEST_ASSERT_TRUE(pattern.writeStep(i, true));
    }

    pattern.clear();

    TEST_ASSERT_EQUAL_UINT8(12, pattern.getBaseLength());

    for (uint8_t i = 0; i < Pattern::STEP_COUNT; ++i) {
        bool active = true;
        TEST_ASSERT_TRUE(pattern.readStep(i, active));
        TEST_ASSERT_FALSE(active);
    }
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_pattern_has_expected_memory_footprint);
    RUN_TEST(test_pattern_defaults_to_length_16_and_all_steps_off);
    RUN_TEST(test_pattern_accepts_every_valid_base_length);
    RUN_TEST(test_pattern_rejects_invalid_base_lengths_without_mutation);
    RUN_TEST(test_pattern_writes_and_reads_all_24_steps);
    RUN_TEST(test_pattern_covers_bit_boundaries_0_7_8_15_16_23);
    RUN_TEST(test_pattern_rejects_step_index_24_without_mutation);
    RUN_TEST(test_pattern_changing_length_preserves_steps_outside_length);
    RUN_TEST(test_pattern_clear_turns_all_24_steps_off_and_preserves_length);

    return UNITY_END();
}
