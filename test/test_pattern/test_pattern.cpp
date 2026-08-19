#include <stdint.h>
#include <unity.h>

#include "flexseq/Pattern.h"

using flexseq::Pattern;

void setUp() {}
void tearDown() {}

void test_pattern_has_expected_memory_footprint() {
    TEST_ASSERT_EQUAL_UINT16(15, sizeof(Pattern)); // 3 steps + 12 ratchet nibbles
}

void test_pattern_defaults_to_all_steps_off() {
    Pattern pattern;

    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        bool active = true;
        TEST_ASSERT_TRUE(pattern.readStep(i, active));
        TEST_ASSERT_FALSE(active);
    }
}

void test_pattern_writes_and_reads_all_24_steps() {
    Pattern pattern;

    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        TEST_ASSERT_TRUE(pattern.writeStep(i, (i % 2) == 0));
    }

    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        bool active = false;
        TEST_ASSERT_TRUE(pattern.readStep(i, active));
        TEST_ASSERT_EQUAL((i % 2) == 0, active);
    }
}

void test_pattern_covers_bit_boundaries_0_7_8_15_16_23() {
    Pattern pattern;

    const uint8_t boundarySteps[] = {0, 7, 8, 15, 16, 23};

    for (uint8_t i = 0;
         i < sizeof(boundarySteps) / sizeof(boundarySteps[0]);
         ++i) {
        TEST_ASSERT_TRUE(pattern.writeStep(boundarySteps[i], true));
    }

    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
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

void test_pattern_defaults_to_no_ratchet() {
    Pattern pattern;
    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(i));
    }
}

void test_pattern_sets_and_reads_ratchet_per_step() {
    Pattern pattern;
    TEST_ASSERT_TRUE(pattern.setRatchet(0, flexseq::RATCHET_2));
    TEST_ASSERT_TRUE(pattern.setRatchet(1, flexseq::RATCHET_3));
    TEST_ASSERT_TRUE(pattern.setRatchet(23, flexseq::RATCHET_TRIPLET));

    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_2, pattern.getRatchet(0));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_3, pattern.getRatchet(1));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, pattern.getRatchet(23));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(2));
}

void test_pattern_ratchet_nibbles_do_not_bleed() {
    // Neighbouring steps share a byte: writing one must not disturb the other.
    Pattern pattern;
    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        TEST_ASSERT_TRUE(pattern.setRatchet(i, (i % 2 == 0) ? flexseq::RATCHET_6
                                                            : flexseq::RATCHET_3));
    }
    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        TEST_ASSERT_EQUAL_UINT8((i % 2 == 0) ? flexseq::RATCHET_6 : flexseq::RATCHET_3,
                                pattern.getRatchet(i));
    }
}

void test_pattern_any_step_can_carry_a_ratchet() {
    // No overlap or start-index constraint any more: every step may carry one.
    Pattern pattern;
    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        TEST_ASSERT_TRUE(pattern.setRatchet(i, flexseq::RATCHET_TRIPLET));
    }
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, pattern.getRatchet(21));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, pattern.getRatchet(22));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, pattern.getRatchet(23));
}

void test_pattern_rejects_invalid_ratchet_code_and_index() {
    Pattern pattern;
    TEST_ASSERT_FALSE(pattern.setRatchet(0, 1));  // 1 is meaningless
    TEST_ASSERT_FALSE(pattern.setRatchet(0, 5));  // 5 is not representable
    TEST_ASSERT_FALSE(pattern.setRatchet(0, 15));
    TEST_ASSERT_FALSE(pattern.setRatchet(24, flexseq::RATCHET_2));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(0));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(24));
}

void test_ratchet_trigger_counts_and_spans() {
    TEST_ASSERT_EQUAL_UINT8(1, flexseq::ratchetTriggers(flexseq::RATCHET_NONE));
    TEST_ASSERT_EQUAL_UINT8(2, flexseq::ratchetTriggers(flexseq::RATCHET_2));
    TEST_ASSERT_EQUAL_UINT8(3, flexseq::ratchetTriggers(flexseq::RATCHET_3));
    TEST_ASSERT_EQUAL_UINT8(4, flexseq::ratchetTriggers(flexseq::RATCHET_4));
    TEST_ASSERT_EQUAL_UINT8(6, flexseq::ratchetTriggers(flexseq::RATCHET_6));
    // The triplet fires three times but over TWO step durations.
    TEST_ASSERT_EQUAL_UINT8(3, flexseq::ratchetTriggers(flexseq::RATCHET_TRIPLET));
    TEST_ASSERT_EQUAL_UINT8(2, flexseq::ratchetSpan(flexseq::RATCHET_TRIPLET));
    TEST_ASSERT_EQUAL_UINT8(1, flexseq::ratchetSpan(flexseq::RATCHET_4));
}

void test_pattern_clear_resets_steps_and_ratchets() {
    Pattern pattern;
    pattern.writeStep(3, true);
    pattern.setRatchet(3, flexseq::RATCHET_4);
    pattern.clear();

    bool active = true;
    TEST_ASSERT_TRUE(pattern.readStep(3, active));
    TEST_ASSERT_FALSE(active);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(3));
}

void test_pattern_ratchets_survive_step_edits() {
    Pattern pattern;
    pattern.setRatchet(5, flexseq::RATCHET_3);
    pattern.writeStep(5, true);
    pattern.writeStep(5, false);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_3, pattern.getRatchet(5));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_pattern_has_expected_memory_footprint);
    RUN_TEST(test_pattern_defaults_to_all_steps_off);
    RUN_TEST(test_pattern_writes_and_reads_all_24_steps);
    RUN_TEST(test_pattern_covers_bit_boundaries_0_7_8_15_16_23);
    RUN_TEST(test_pattern_rejects_step_index_24_without_mutation);

    RUN_TEST(test_pattern_defaults_to_no_ratchet);
    RUN_TEST(test_pattern_sets_and_reads_ratchet_per_step);
    RUN_TEST(test_pattern_ratchet_nibbles_do_not_bleed);
    RUN_TEST(test_pattern_any_step_can_carry_a_ratchet);
    RUN_TEST(test_pattern_rejects_invalid_ratchet_code_and_index);
    RUN_TEST(test_ratchet_trigger_counts_and_spans);
    RUN_TEST(test_pattern_clear_resets_steps_and_ratchets);
    RUN_TEST(test_pattern_ratchets_survive_step_edits);

    return UNITY_END();
}
