#include <stdint.h>
#include <unity.h>

#include "flexseq/Pattern.h"

using flexseq::Pattern;

void setUp() {}
void tearDown() {}

void test_pattern_has_expected_memory_footprint() {
    TEST_ASSERT_EQUAL_UINT16(23, sizeof(Pattern));
}

void test_pattern_holds_thirty_six_steps() {
    TEST_ASSERT_EQUAL_UINT8(36, Pattern::DEFAULT_TOTAL_STEPS);
}

void test_pattern_defaults_to_all_steps_off() {
    Pattern pattern;

    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        bool active = true;
        TEST_ASSERT_TRUE(pattern.readStep(i, active));
        TEST_ASSERT_FALSE(active);
    }
}

void test_pattern_writes_and_reads_every_step() {
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

void test_pattern_rejects_step_index_36_without_mutation() {
    Pattern pattern;

    bool active = true;
    TEST_ASSERT_FALSE(pattern.readStep(36, active));
    TEST_ASSERT_TRUE(active);

    TEST_ASSERT_TRUE(pattern.writeStep(35, true));
    TEST_ASSERT_FALSE(pattern.writeStep(36, false));

    active = false;
    TEST_ASSERT_TRUE(pattern.readStep(35, active));
    TEST_ASSERT_TRUE(active);
}

void test_pattern_writes_and_reads_the_steps_above_23() {
    Pattern pattern;

    const uint8_t written[4] = {24, 27, 30, 31};

    for (uint8_t i = 0; i < 4; ++i) {
        TEST_ASSERT_TRUE(pattern.writeStep(written[i], true));
    }

    for (uint8_t i = 24; i < 32; ++i) {
        bool active = false;
        TEST_ASSERT_TRUE(pattern.readStep(i, active));
        const bool expected = i == 24 || i == 27 || i == 30 || i == 31;
        TEST_ASSERT_EQUAL(expected, active);
    }

    bool low = true;
    TEST_ASSERT_TRUE(pattern.readStep(23, low));
    TEST_ASSERT_FALSE(low);
}

void test_pattern_covers_the_fourth_byte_boundaries_24_and_31() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.writeStep(23, true));
    TEST_ASSERT_TRUE(pattern.writeStep(24, true));
    TEST_ASSERT_TRUE(pattern.writeStep(31, true));

    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        bool active = false;
        TEST_ASSERT_TRUE(pattern.readStep(i, active));
        const bool expected = i == 23 || i == 24 || i == 31;
        TEST_ASSERT_EQUAL(expected, active);
    }
}

void test_pattern_carries_a_ratchet_on_the_steps_above_23() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setRatchet(24, flexseq::RATCHET_6));
    TEST_ASSERT_TRUE(pattern.setRatchet(31, flexseq::RATCHET_4));

    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_6, pattern.getRatchet(24));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_4, pattern.getRatchet(31));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(25));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(30));
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
    TEST_ASSERT_TRUE(pattern.setRatchet(35, flexseq::RATCHET_TRIPLET));

    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_2, pattern.getRatchet(0));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_3, pattern.getRatchet(1));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, pattern.getRatchet(35));
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
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, pattern.getRatchet(33));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, pattern.getRatchet(34));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, pattern.getRatchet(35));
}

void test_pattern_rejects_invalid_ratchet_code_and_index() {
    Pattern pattern;
    TEST_ASSERT_FALSE(pattern.setRatchet(0, 1));  // 1 is meaningless
    TEST_ASSERT_FALSE(pattern.setRatchet(0, 5));  // 5 is not representable
    TEST_ASSERT_FALSE(pattern.setRatchet(0, 15));
    TEST_ASSERT_FALSE(pattern.setRatchet(36, flexseq::RATCHET_2));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(0));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(36));
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

void test_pattern_clear_reaches_the_steps_above_23() {
    Pattern pattern;
    for (uint8_t step = 24; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
        TEST_ASSERT_TRUE(pattern.writeStep(step, true));
        TEST_ASSERT_TRUE(pattern.setRatchet(step, flexseq::RATCHET_3));
    }

    pattern.clear();

    for (uint8_t step = 24; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
        bool active = true;
        TEST_ASSERT_TRUE(pattern.readStep(step, active));
        TEST_ASSERT_FALSE(active);
        TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(step));
    }
}

void test_pattern_ratchets_survive_step_edits() {
    Pattern pattern;
    pattern.setRatchet(5, flexseq::RATCHET_3);
    pattern.writeStep(5, true);
    pattern.writeStep(5, false);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_3, pattern.getRatchet(5));
}

void test_set_low_step_mask_writes_sixteen_and_spares_the_rest() {
    Pattern pattern;
    TEST_ASSERT_TRUE(pattern.writeStep(20, true));
    pattern.setLowStepMask(0x9111);
    const uint8_t expected[6] = {0, 4, 8, 12, 15, 20};
    uint8_t seen = 0;
    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        bool active = false;
        TEST_ASSERT_TRUE(pattern.readStep(i, active));
        if (active) {
            TEST_ASSERT_LESS_THAN_UINT8(6, seen);
            TEST_ASSERT_EQUAL_UINT8(expected[seen], i);
            ++seen;
        }
    }
    TEST_ASSERT_EQUAL_UINT8(6, seen);
}

void test_pattern_writes_and_reads_the_fifth_byte_steps_32_to_35() {
    Pattern pattern;

    const uint8_t written[2] = {32, 35};

    for (uint8_t i = 0; i < 2; ++i) {
        TEST_ASSERT_TRUE(pattern.writeStep(written[i], true));
    }

    for (uint8_t i = 32; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        bool active = false;
        TEST_ASSERT_TRUE(pattern.readStep(i, active));
        const bool expected = i == 32 || i == 35;
        TEST_ASSERT_EQUAL(expected, active);
    }

    bool low = true;
    TEST_ASSERT_TRUE(pattern.readStep(31, low));
    TEST_ASSERT_FALSE(low);
}

void test_pattern_covers_the_fifth_byte_boundaries_31_and_35() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.writeStep(31, true));
    TEST_ASSERT_TRUE(pattern.writeStep(32, true));
    TEST_ASSERT_TRUE(pattern.writeStep(35, true));

    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        bool active = false;
        TEST_ASSERT_TRUE(pattern.readStep(i, active));
        const bool expected = i == 31 || i == 32 || i == 35;
        TEST_ASSERT_EQUAL(expected, active);
    }
}

void test_pattern_carries_a_ratchet_on_the_fifth_byte_steps() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setRatchet(32, flexseq::RATCHET_6));
    TEST_ASSERT_TRUE(pattern.setRatchet(35, flexseq::RATCHET_4));

    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_6, pattern.getRatchet(32));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_4, pattern.getRatchet(35));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(33));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(34));
}

void test_pattern_refuses_every_index_above_the_last_step() {
    Pattern pattern;

    for (uint16_t i = Pattern::DEFAULT_TOTAL_STEPS; i <= 255; ++i) {
        const uint8_t index = static_cast<uint8_t>(i);

        bool active = true;
        TEST_ASSERT_FALSE(pattern.readStep(index, active));
        TEST_ASSERT_TRUE(active);

        TEST_ASSERT_FALSE(pattern.writeStep(index, true));
        TEST_ASSERT_FALSE(pattern.setRatchet(index, flexseq::RATCHET_4));
        TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(index));
    }

    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        bool active = true;
        TEST_ASSERT_TRUE(pattern.readStep(i, active));
        TEST_ASSERT_FALSE(active);
        TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(i));
    }
}

void test_pattern_clear_reaches_the_fifth_byte() {
    Pattern pattern;
    for (uint8_t step = 32; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
        TEST_ASSERT_TRUE(pattern.writeStep(step, true));
        TEST_ASSERT_TRUE(pattern.setRatchet(step, flexseq::RATCHET_3));
    }

    pattern.clear();

    for (uint8_t step = 32; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
        bool active = true;
        TEST_ASSERT_TRUE(pattern.readStep(step, active));
        TEST_ASSERT_FALSE(active);
        TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(step));
    }
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_pattern_has_expected_memory_footprint);
    RUN_TEST(test_pattern_holds_thirty_six_steps);
    RUN_TEST(test_pattern_defaults_to_all_steps_off);
    RUN_TEST(test_pattern_writes_and_reads_every_step);
    RUN_TEST(test_pattern_covers_bit_boundaries_0_7_8_15_16_23);
    RUN_TEST(test_pattern_rejects_step_index_36_without_mutation);
    RUN_TEST(test_pattern_writes_and_reads_the_steps_above_23);
    RUN_TEST(test_pattern_covers_the_fourth_byte_boundaries_24_and_31);
    RUN_TEST(test_pattern_carries_a_ratchet_on_the_steps_above_23);
    RUN_TEST(test_pattern_writes_and_reads_the_fifth_byte_steps_32_to_35);
    RUN_TEST(test_pattern_covers_the_fifth_byte_boundaries_31_and_35);
    RUN_TEST(test_pattern_carries_a_ratchet_on_the_fifth_byte_steps);
    RUN_TEST(test_pattern_refuses_every_index_above_the_last_step);

    RUN_TEST(test_pattern_defaults_to_no_ratchet);
    RUN_TEST(test_pattern_sets_and_reads_ratchet_per_step);
    RUN_TEST(test_pattern_ratchet_nibbles_do_not_bleed);
    RUN_TEST(test_pattern_any_step_can_carry_a_ratchet);
    RUN_TEST(test_pattern_rejects_invalid_ratchet_code_and_index);
    RUN_TEST(test_ratchet_trigger_counts_and_spans);
    RUN_TEST(test_pattern_clear_resets_steps_and_ratchets);
    RUN_TEST(test_pattern_clear_reaches_the_steps_above_23);
    RUN_TEST(test_pattern_clear_reaches_the_fifth_byte);
    RUN_TEST(test_pattern_ratchets_survive_step_edits);
    RUN_TEST(test_set_low_step_mask_writes_sixteen_and_spares_the_rest);

    return UNITY_END();
}
