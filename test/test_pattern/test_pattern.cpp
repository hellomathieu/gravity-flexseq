#include <stdint.h>
#include <unity.h>

#include "flexseq/Pattern.h"

using flexseq::Pattern;

void setUp() {}
void tearDown() {}

void test_pattern_has_expected_memory_footprint() {
    TEST_ASSERT_EQUAL_UINT16(7, sizeof(Pattern));
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

    const uint8_t boundarySteps[] = {
        0, 7, 8, 15, 16, 23
    };

    for (uint8_t i = 0;
         i < sizeof(boundarySteps) / sizeof(boundarySteps[0]);
         ++i) {
        TEST_ASSERT_TRUE(pattern.writeStep(boundarySteps[i], true));
    }

    for (uint8_t i = 0; i < Pattern::STEP_COUNT; ++i) {
        bool active = false;
        TEST_ASSERT_TRUE(pattern.readStep(i, active));

        bool expected =
            i == 0 ||
            i == 7 ||
            i == 8 ||
            i == 15 ||
            i == 16 ||
            i == 23;

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

    for (uint8_t i = 0; i < Pattern::STEP_COUNT; ++i) {
        TEST_ASSERT_FALSE(pattern.isTripletStep(i));
    }
}

/*
 * Triplet tests
 */

void test_pattern_has_no_triplets_by_default() {
    Pattern pattern;

    for (uint8_t i = 0; i < Pattern::STEP_COUNT; ++i) {
        TEST_ASSERT_FALSE(pattern.isTripletStart(i));
        TEST_ASSERT_FALSE(pattern.isTripletStep(i));
    }
}

void test_pattern_adds_triplet_at_beginning() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.addTriplet(0));

    TEST_ASSERT_TRUE(pattern.isTripletStart(0));

    TEST_ASSERT_TRUE(pattern.isTripletStep(0));
    TEST_ASSERT_TRUE(pattern.isTripletStep(1));
    TEST_ASSERT_TRUE(pattern.isTripletStep(2));

    TEST_ASSERT_FALSE(pattern.isTripletStep(3));
}

void test_pattern_adds_triplet_at_end_of_24_steps() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setBaseLength(24));

    TEST_ASSERT_TRUE(pattern.addTriplet(21));

    TEST_ASSERT_TRUE(pattern.isTripletStart(21));

    TEST_ASSERT_TRUE(pattern.isTripletStep(21));
    TEST_ASSERT_TRUE(pattern.isTripletStep(22));
    TEST_ASSERT_TRUE(pattern.isTripletStep(23));

    TEST_ASSERT_FALSE(pattern.isTripletStep(20));
}

void test_pattern_rejects_triplet_start_22_and_23() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setBaseLength(24));

    TEST_ASSERT_FALSE(pattern.addTriplet(22));
    TEST_ASSERT_FALSE(pattern.addTriplet(23));
}

void test_pattern_rejects_triplet_outside_base_length() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setBaseLength(12));

    TEST_ASSERT_TRUE(pattern.addTriplet(9));
    TEST_ASSERT_FALSE(pattern.addTriplet(10));
}

void test_pattern_allows_triplet_start_at_any_valid_position() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setBaseLength(24));

    TEST_ASSERT_TRUE(pattern.addTriplet(0));
    TEST_ASSERT_TRUE(pattern.removeTriplet(0));

    TEST_ASSERT_TRUE(pattern.addTriplet(1));
    TEST_ASSERT_TRUE(pattern.removeTriplet(1));

    TEST_ASSERT_TRUE(pattern.addTriplet(2));
    TEST_ASSERT_TRUE(pattern.removeTriplet(2));

    TEST_ASSERT_TRUE(pattern.addTriplet(21));
}

void test_pattern_rejects_overlapping_triplets() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setBaseLength(24));

    TEST_ASSERT_TRUE(pattern.addTriplet(3));

    TEST_ASSERT_FALSE(pattern.addTriplet(1));
    TEST_ASSERT_FALSE(pattern.addTriplet(2));
    TEST_ASSERT_FALSE(pattern.addTriplet(4));
    TEST_ASSERT_FALSE(pattern.addTriplet(5));

    TEST_ASSERT_TRUE(pattern.isTripletStart(3));
}

void test_pattern_allows_adjacent_triplets_without_overlap() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setBaseLength(24));

    TEST_ASSERT_TRUE(pattern.addTriplet(0));
    TEST_ASSERT_TRUE(pattern.addTriplet(3));

    TEST_ASSERT_TRUE(pattern.isTripletStep(0));
    TEST_ASSERT_TRUE(pattern.isTripletStep(1));
    TEST_ASSERT_TRUE(pattern.isTripletStep(2));

    TEST_ASSERT_TRUE(pattern.isTripletStep(3));
    TEST_ASSERT_TRUE(pattern.isTripletStep(4));
    TEST_ASSERT_TRUE(pattern.isTripletStep(5));
}

void test_pattern_allows_maximum_eight_triplets() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setBaseLength(24));

    const uint8_t starts[] = {
        0, 3, 6, 9, 12, 15, 18, 21
    };

    for (uint8_t i = 0;
         i < sizeof(starts) / sizeof(starts[0]);
         ++i) {
        TEST_ASSERT_TRUE(pattern.addTriplet(starts[i]));
    }
}

void test_pattern_rejects_duplicate_triplet() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setBaseLength(24));

    TEST_ASSERT_TRUE(pattern.addTriplet(6));
    TEST_ASSERT_FALSE(pattern.addTriplet(6));

    TEST_ASSERT_TRUE(pattern.isTripletStart(6));
}

void test_pattern_removes_triplet() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setBaseLength(24));

    TEST_ASSERT_TRUE(pattern.addTriplet(6));

    TEST_ASSERT_TRUE(pattern.isTripletStep(6));
    TEST_ASSERT_TRUE(pattern.isTripletStep(7));
    TEST_ASSERT_TRUE(pattern.isTripletStep(8));

    TEST_ASSERT_TRUE(pattern.removeTriplet(6));

    TEST_ASSERT_FALSE(pattern.isTripletStart(6));
    TEST_ASSERT_FALSE(pattern.isTripletStep(6));
    TEST_ASSERT_FALSE(pattern.isTripletStep(7));
    TEST_ASSERT_FALSE(pattern.isTripletStep(8));
}

void test_pattern_rejects_removing_nonexistent_triplet() {
    Pattern pattern;

    TEST_ASSERT_FALSE(pattern.removeTriplet(6));
}

void test_pattern_clear_triplets_preserves_steps_and_length() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setBaseLength(12));

    TEST_ASSERT_TRUE(pattern.writeStep(0, true));
    TEST_ASSERT_TRUE(pattern.writeStep(6, true));

    TEST_ASSERT_TRUE(pattern.addTriplet(0));
    TEST_ASSERT_TRUE(pattern.addTriplet(6));

    pattern.clearTriplets();

    TEST_ASSERT_FALSE(pattern.isTripletStart(0));
    TEST_ASSERT_FALSE(pattern.isTripletStart(6));

    TEST_ASSERT_TRUE(pattern.writeStep(0, true));

    bool active = false;

    TEST_ASSERT_TRUE(pattern.readStep(0, active));
    TEST_ASSERT_TRUE(active);

    TEST_ASSERT_TRUE(pattern.readStep(6, active));
    TEST_ASSERT_TRUE(active);

    TEST_ASSERT_EQUAL_UINT8(12, pattern.getBaseLength());
}

void test_pattern_clear_removes_triplets() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setBaseLength(24));
    TEST_ASSERT_TRUE(pattern.addTriplet(0));
    TEST_ASSERT_TRUE(pattern.addTriplet(6));

    pattern.clear();

    TEST_ASSERT_FALSE(pattern.isTripletStart(0));
    TEST_ASSERT_FALSE(pattern.isTripletStart(6));

    for (uint8_t i = 0; i < Pattern::STEP_COUNT; ++i) {
        TEST_ASSERT_FALSE(pattern.isTripletStep(i));
    }
}

void test_pattern_triplet_metadata_survives_length_reduction() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setBaseLength(16));
    TEST_ASSERT_TRUE(pattern.addTriplet(13));

    TEST_ASSERT_TRUE(pattern.isTripletStart(13));
    TEST_ASSERT_TRUE(pattern.isTripletStep(13));
    TEST_ASSERT_TRUE(pattern.isTripletStep(14));
    TEST_ASSERT_TRUE(pattern.isTripletStep(15));

    TEST_ASSERT_TRUE(pattern.setBaseLength(12));

    // The triplet metadata must remain stored.
    TEST_ASSERT_TRUE(pattern.isTripletStart(13));
}

void test_pattern_triplet_can_be_reactivated_after_length_increase() {
    Pattern pattern;

    TEST_ASSERT_TRUE(pattern.setBaseLength(16));
    TEST_ASSERT_TRUE(pattern.addTriplet(13));

    TEST_ASSERT_TRUE(pattern.setBaseLength(12));
    TEST_ASSERT_TRUE(pattern.isTripletStart(13));

    TEST_ASSERT_TRUE(pattern.setBaseLength(16));

    TEST_ASSERT_TRUE(pattern.isTripletStart(13));
    TEST_ASSERT_TRUE(pattern.isTripletStep(13));
    TEST_ASSERT_TRUE(pattern.isTripletStep(14));
    TEST_ASSERT_TRUE(pattern.isTripletStep(15));
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

    RUN_TEST(test_pattern_has_no_triplets_by_default);
    RUN_TEST(test_pattern_adds_triplet_at_beginning);
    RUN_TEST(test_pattern_adds_triplet_at_end_of_24_steps);
    RUN_TEST(test_pattern_rejects_triplet_start_22_and_23);
    RUN_TEST(test_pattern_rejects_triplet_outside_base_length);
    RUN_TEST(test_pattern_allows_triplet_start_at_any_valid_position);
    RUN_TEST(test_pattern_rejects_overlapping_triplets);
    RUN_TEST(test_pattern_allows_adjacent_triplets_without_overlap);
    RUN_TEST(test_pattern_allows_maximum_eight_triplets);
    RUN_TEST(test_pattern_rejects_duplicate_triplet);
    RUN_TEST(test_pattern_removes_triplet);
    RUN_TEST(test_pattern_rejects_removing_nonexistent_triplet);
    RUN_TEST(test_pattern_clear_triplets_preserves_steps_and_length);
    RUN_TEST(test_pattern_clear_removes_triplets);
    RUN_TEST(test_pattern_triplet_metadata_survives_length_reduction);
    RUN_TEST(test_pattern_triplet_can_be_reactivated_after_length_increase);

    return UNITY_END();
}