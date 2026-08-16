#include <stdint.h>
#include <unity.h>

#include <flexseq/SequencerEngine.h>

using flexseq::SequencerEngine;

void setUp() {}
void tearDown() {}

static const uint16_t STEP = SequencerEngine::TICKS_PER_SIXTEENTH; // 24

/*
 * Transport & masterPhase
 */

void test_starts_stopped_at_phase_zero_with_defaults() {
    SequencerEngine e;
    TEST_ASSERT_EQUAL_UINT32(0, e.masterPhase());
    TEST_ASSERT_FALSE(e.isRunning());
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_EQUAL_UINT8(SequencerEngine::DEFAULT_LENGTH, e.getEffectiveLength(ch));
        TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(ch));
    }
}

void test_does_not_advance_while_stopped() {
    SequencerEngine e;
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT32(0, e.masterPhase());
}

void test_advances_by_ticks_only_while_running() {
    SequencerEngine e;
    e.start();
    e.advance();
    TEST_ASSERT_EQUAL_UINT32(1, e.masterPhase());
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT32(1 + STEP, e.masterPhase());
}

void test_stop_preserves_phase_and_advance_is_noop() {
    SequencerEngine e;
    e.start();
    e.advance(STEP * 3);
    e.stop();
    TEST_ASSERT_EQUAL_UINT32(STEP * 3, e.masterPhase());
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT32(STEP * 3, e.masterPhase());
}

void test_reset_zeroes_phase_without_changing_running() {
    SequencerEngine e;
    e.start();
    e.advance(STEP * 5);
    e.reset();
    TEST_ASSERT_EQUAL_UINT32(0, e.masterPhase());
    TEST_ASSERT_TRUE(e.isRunning());
}

void test_advance_zero_is_noop() {
    SequencerEngine e;
    e.start();
    e.advance(0);
    TEST_ASSERT_EQUAL_UINT32(0, e.masterPhase());
}

/*
 * effectiveStep derivation (smoothed local phase)
 */

void test_derives_step_from_phase_and_ticks_per_step() {
    SequencerEngine e;
    e.start();
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
    e.advance(STEP - 1);
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
    e.advance(1);
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0));
}

void test_wraps_step_at_effective_length() {
    SequencerEngine e;
    e.start();
    TEST_ASSERT_TRUE(e.setEffectiveLength(0, 16));
    e.advance(STEP * 16);
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
}

void test_masterphase_untouched_when_length_changes() {
    SequencerEngine e;
    e.start();
    e.advance(STEP * 10);
    uint32_t before = e.masterPhase();
    TEST_ASSERT_TRUE(e.setEffectiveLength(0, 4));
    TEST_ASSERT_EQUAL_UINT32(before, e.masterPhase());
    TEST_ASSERT_TRUE(e.setEffectiveLength(0, 24));
    TEST_ASSERT_EQUAL_UINT32(before, e.masterPhase());
}

void test_no_jump_when_length_shrinks_within_range() {
    SequencerEngine e;
    e.start();
    e.advance(STEP * 5); // localStep = 5
    TEST_ASSERT_EQUAL_INT8(5, e.effectiveStep(0));
    TEST_ASSERT_TRUE(e.setEffectiveLength(0, 11));
    TEST_ASSERT_EQUAL_INT8(5, e.effectiveStep(0));
    TEST_ASSERT_TRUE(e.setEffectiveLength(0, 8));
    TEST_ASSERT_EQUAL_INT8(5, e.effectiveStep(0));
}

void test_folds_into_range_only_when_length_drops_below() {
    SequencerEngine e;
    e.start();
    e.advance(STEP * 13); // localStep = 13
    TEST_ASSERT_TRUE(e.setEffectiveLength(0, 11));
    TEST_ASSERT_EQUAL_INT8(13 % 11, e.effectiveStep(0)); // 2
}

void test_keeps_step_when_length_grows() {
    SequencerEngine e;
    e.start();
    e.setEffectiveLength(0, 8);
    e.advance(STEP * 3); // localStep = 3
    TEST_ASSERT_TRUE(e.setEffectiveLength(0, 24));
    TEST_ASSERT_EQUAL_INT8(3, e.effectiveStep(0));
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(4, e.effectiveStep(0));
}

void test_global_reset_realigns_all_channels() {
    SequencerEngine e;
    e.start();
    e.setEffectiveLength(1, 3);
    e.advance(STEP * 7);
    TEST_ASSERT_TRUE(e.effectiveStep(0) > 0);
    e.reset();
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(1));
    TEST_ASSERT_EQUAL_UINT32(0, e.masterPhase());
}

void test_rejects_invalid_effective_length_without_mutation() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setEffectiveLength(0, 12));
    TEST_ASSERT_FALSE(e.setEffectiveLength(0, 0));
    TEST_ASSERT_FALSE(e.setEffectiveLength(0, 25));
    TEST_ASSERT_EQUAL_UINT8(12, e.getEffectiveLength(0));
}

void test_isolates_execution_state_between_channels() {
    SequencerEngine e;
    e.start();
    e.setEffectiveLength(0, 16);
    e.setEffectiveLength(1, 3);
    e.advance(STEP * 4);
    TEST_ASSERT_EQUAL_INT8(4 % 16, e.effectiveStep(0)); // 4
    TEST_ASSERT_EQUAL_INT8(4 % 3, e.effectiveStep(1));  // 1
    TEST_ASSERT_EQUAL_UINT32(STEP * 4, e.masterPhase());
}

void test_supports_different_ticks_per_step_per_channel() {
    SequencerEngine e;
    e.start();
    TEST_ASSERT_TRUE(e.setTicksPerStep(1, STEP * 2)); // channel 1 half as fast
    e.advance(STEP * 4);
    TEST_ASSERT_EQUAL_INT8(4, e.effectiveStep(0)); // 96/24 = 4 steps
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(1)); // 96/48 = 2 steps
}

void test_rejects_invalid_channel_and_ticks_per_step() {
    SequencerEngine e;
    TEST_ASSERT_EQUAL_INT8(-1, e.effectiveStep(6));
    TEST_ASSERT_FALSE(e.setEffectiveLength(6, 8));
    TEST_ASSERT_FALSE(e.setTicksPerStep(0, 0));
}

void test_has_stepped_reports_boundary_crossings() {
    SequencerEngine e;
    e.start();

    e.advance(STEP - 1); // no boundary yet
    TEST_ASSERT_FALSE(e.hasStepped(0));

    e.advance(1); // crosses the first boundary
    TEST_ASSERT_TRUE(e.hasStepped(0));

    e.advance(1); // within the step, no new boundary
    TEST_ASSERT_FALSE(e.hasStepped(0));
}

void test_has_stepped_is_false_while_stopped_and_for_invalid_channel() {
    SequencerEngine e;
    e.advance(STEP); // stopped -> no crossing
    TEST_ASSERT_FALSE(e.hasStepped(0));
    TEST_ASSERT_FALSE(e.hasStepped(6));
}

void test_has_stepped_per_channel_with_different_rates() {
    SequencerEngine e;
    e.start();
    e.setTicksPerStep(1, STEP * 2); // channel 1 half as fast
    e.advance(STEP); // ch0 crosses, ch1 does not yet
    TEST_ASSERT_TRUE(e.hasStepped(0));
    TEST_ASSERT_FALSE(e.hasStepped(1));
    e.advance(STEP); // now ch1 crosses too
    TEST_ASSERT_TRUE(e.hasStepped(0));
    TEST_ASSERT_TRUE(e.hasStepped(1));
}

/*
 * Per-channel selected pattern
 */

void test_defaults_every_channel_to_pattern_zero() {
    SequencerEngine e;
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_EQUAL_INT8(0, e.getSelectedPattern(ch));
    }
}

void test_sets_and_reads_selected_pattern_independently() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setSelectedPattern(0, 3));
    TEST_ASSERT_TRUE(e.setSelectedPattern(1, 10));
    TEST_ASSERT_EQUAL_INT8(3, e.getSelectedPattern(0));
    TEST_ASSERT_EQUAL_INT8(10, e.getSelectedPattern(1));
    TEST_ASSERT_EQUAL_INT8(0, e.getSelectedPattern(2));
}

void test_rejects_out_of_range_pattern_and_channel() {
    SequencerEngine e;
    TEST_ASSERT_FALSE(e.setSelectedPattern(0, 16));
    TEST_ASSERT_FALSE(e.setSelectedPattern(6, 0));
    TEST_ASSERT_EQUAL_INT8(-1, e.getSelectedPattern(6));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_starts_stopped_at_phase_zero_with_defaults);
    RUN_TEST(test_does_not_advance_while_stopped);
    RUN_TEST(test_advances_by_ticks_only_while_running);
    RUN_TEST(test_stop_preserves_phase_and_advance_is_noop);
    RUN_TEST(test_reset_zeroes_phase_without_changing_running);
    RUN_TEST(test_advance_zero_is_noop);

    RUN_TEST(test_derives_step_from_phase_and_ticks_per_step);
    RUN_TEST(test_wraps_step_at_effective_length);
    RUN_TEST(test_masterphase_untouched_when_length_changes);
    RUN_TEST(test_no_jump_when_length_shrinks_within_range);
    RUN_TEST(test_folds_into_range_only_when_length_drops_below);
    RUN_TEST(test_keeps_step_when_length_grows);
    RUN_TEST(test_global_reset_realigns_all_channels);
    RUN_TEST(test_rejects_invalid_effective_length_without_mutation);
    RUN_TEST(test_isolates_execution_state_between_channels);
    RUN_TEST(test_supports_different_ticks_per_step_per_channel);
    RUN_TEST(test_rejects_invalid_channel_and_ticks_per_step);
    RUN_TEST(test_has_stepped_reports_boundary_crossings);
    RUN_TEST(test_has_stepped_is_false_while_stopped_and_for_invalid_channel);
    RUN_TEST(test_has_stepped_per_channel_with_different_rates);

    RUN_TEST(test_defaults_every_channel_to_pattern_zero);
    RUN_TEST(test_sets_and_reads_selected_pattern_independently);
    RUN_TEST(test_rejects_out_of_range_pattern_and_channel);

    return UNITY_END();
}
