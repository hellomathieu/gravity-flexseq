#include <stdint.h>
#include <unity.h>

#include <flexseq/CvDestination.h>
#include <flexseq/Pattern.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/TriggerSequencer.h>

using flexseq::Pattern;
using flexseq::SequencerEngine;
using flexseq::TriggerSequencer;
using flexseq::CV_DEST_LENGTH;
using flexseq::CV_DEST_RESET;
using flexseq::CV_SOURCE_1;
using flexseq::CV_SOURCE_2;
using flexseq::MODE_CLOCK;
using flexseq::MODE_RANDOM;
using flexseq::MODE_SEQ;
using flexseq::RATCHET_4;
using flexseq::RATCHET_TRIPLET;

static const uint16_t STEP = 96;

void setUp() {}
void tearDown() {}

static void routeReset(SequencerEngine& e, uint8_t channel, uint8_t source) {
    TEST_ASSERT_TRUE(e.setCvDestination(channel, source, CV_DEST_RESET));
}

static void seqChannel(SequencerEngine& e, uint8_t channel) {
    TEST_ASSERT_TRUE(e.setChannelMode(channel, MODE_SEQ));
}

void test_a_cv1_edge_resets_a_seq_channel_routed_to_cv1_immediately() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP);
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0));
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
}

void test_a_cv2_edge_resets_a_channel_routed_to_cv2() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_2);
    e.start();
    e.advance(2 * STEP);
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0));
    e.applyCvResetEvents(1u << CV_SOURCE_2);
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
}

void test_an_edge_on_the_unrouted_source_does_nothing() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_2);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
}

void test_a_channel_routing_both_sources_resets_on_either_edge() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    routeReset(e, 0, CV_SOURCE_2);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1u << CV_SOURCE_2);
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
}

void test_a_zero_mask_is_a_strict_no_op() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP + 40);
    e.applyCvResetEvents(0);
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0));
    e.advance(56);
    TEST_ASSERT_EQUAL_INT8(3, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_bits_beyond_the_source_count_are_masked() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    routeReset(e, 0, CV_SOURCE_2);
    e.start();
    e.advance(2 * STEP + 40);
    e.applyCvResetEvents(0xFC);
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0));
    e.advance(56);
    TEST_ASSERT_EQUAL_INT8(3, e.effectiveStep(0));
}

void test_no_onset_is_emitted_between_the_call_and_the_next_advance() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(STEP + 40);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_the_first_advance_emits_one_onset_on_step_zero() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
}

void test_the_armament_does_not_fire_twice() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
}

void test_an_inactive_step_zero_gives_an_onset_and_no_trigger() {
    SequencerEngine e;
    TriggerSequencer t(e);
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP);
    t.update();
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    e.advance(1);
    t.update();
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    TEST_ASSERT_FALSE(t.triggered(0));
}

void test_an_active_step_zero_gives_a_trigger() {
    SequencerEngine e;
    TriggerSequencer t(e);
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    TEST_ASSERT_TRUE(e.instanceForChannel(0)->writeStep(0, true));
    e.start();
    e.advance(2 * STEP);
    t.update();
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    e.advance(1);
    t.update();
    TEST_ASSERT_EQUAL_UINT8(1, t.triggerCount(0));
}

void test_two_windows_give_two_resets() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    e.advance(3 * STEP - 1);
    TEST_ASSERT_EQUAL_INT8(3, e.effectiveStep(0));
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_both_bits_in_one_mask_give_one_onset() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    routeReset(e, 0, CV_SOURCE_2);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(0x03);
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_a_reset_leaves_the_pattern_and_the_selection_intact() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    Pattern* p = e.instanceForChannel(0);
    TEST_ASSERT_TRUE(p->writeStep(0, true));
    TEST_ASSERT_TRUE(p->writeStep(5, true));
    TEST_ASSERT_TRUE(p->setRatchet(3, RATCHET_4));
    TEST_ASSERT_TRUE(e.setSelectedPattern(0, 7));
    TEST_ASSERT_TRUE(e.setBaseLength(0, 18));
    e.start();
    e.advance(2 * STEP + 40);
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    TEST_ASSERT_EQUAL_UINT32(232, e.masterPhase());
    TEST_ASSERT_EQUAL_INT8(7, e.getSelectedPattern(0));
    TEST_ASSERT_EQUAL_UINT8(18, e.getBaseLength(0));
    bool active = false;
    TEST_ASSERT_TRUE(p->readStep(0, active));
    TEST_ASSERT_TRUE(active);
    TEST_ASSERT_TRUE(p->readStep(5, active));
    TEST_ASSERT_TRUE(active);
    TEST_ASSERT_TRUE(p->readStep(1, active));
    TEST_ASSERT_FALSE(active);
    TEST_ASSERT_EQUAL_UINT8(RATCHET_4, p->getRatchet(3));
}

void test_the_next_boundary_falls_exactly_step_ticks_after_the_reset() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP + 40);
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    e.advance(95);
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
    e.advance(1);
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
}

void test_the_length_cv_offset_survives_a_reset() {
    SequencerEngine e;
    seqChannel(e, 0);
    TEST_ASSERT_TRUE(e.setBaseLength(0, 18));
    TEST_ASSERT_TRUE(e.setCvDestination(0, CV_SOURCE_1, CV_DEST_LENGTH));
    routeReset(e, 0, CV_SOURCE_2);
    TEST_ASSERT_TRUE(e.setCvInput(CV_SOURCE_1, 330));
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(10, e.lengthCvOffset(0));
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));
    e.applyCvResetEvents(1u << CV_SOURCE_2);
    TEST_ASSERT_EQUAL_INT8(10, e.lengthCvOffset(0));
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
}

void test_a_pending_rate_survives_a_reset_and_applies_on_the_beat() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(40);
    TEST_ASSERT_TRUE(e.setSubdiv(0, -4));
    TEST_ASSERT_EQUAL_UINT16(96, e.getTicksPerStep(0));
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    TEST_ASSERT_EQUAL_UINT16(96, e.getTicksPerStep(0));
    e.advance(56);
    TEST_ASSERT_EQUAL_UINT16(24, e.getTicksPerStep(0));
}

void test_clock_with_zero_offset_fires_on_the_first_tick() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_CLOCK));
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_clock_with_an_offset_fires_at_the_offset_and_not_at_the_armament() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_CLOCK));
    TEST_ASSERT_TRUE(e.setOffset(0, 10));
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
    e.advance(8);
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_random_ignores_the_reset_and_the_return_to_seq_replays_nothing() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_RANDOM));
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0));
    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_SEQ));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0));
}

void test_seq_resets_while_random_stays_on_the_shared_source() {
    SequencerEngine e;
    seqChannel(e, 0);
    TEST_ASSERT_TRUE(e.setChannelMode(1, MODE_RANDOM));
    routeReset(e, 0, CV_SOURCE_1);
    routeReset(e, 1, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(1));
}

void test_a_reset_while_stopped_repositions_and_fires_only_after_start() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP + 40);
    e.stop();
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
    e.start();
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_a_global_reset_subsumes_a_cv_armament_into_one_onset() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    e.reset();
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_a_global_reset_arms_the_six_channels() {
    SequencerEngine e;
    for (uint8_t ch = 0; ch < 6; ++ch) {
        TEST_ASSERT_TRUE(e.setChannelMode(ch, MODE_SEQ));
    }
    e.start();
    e.advance(3 * STEP);
    e.reset();
    e.advance(1);
    for (uint8_t ch = 0; ch < 6; ++ch) {
        TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(ch));
        TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(ch));
    }
}

void test_a_reset_while_stopped_arms_until_the_first_start() {
    SequencerEngine e;
    seqChannel(e, 0);
    e.start();
    e.advance(2 * STEP);
    e.stop();
    e.reset();
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
    e.start();
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_a_global_reset_makes_clock_zero_offset_fire_on_the_first_tick() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_CLOCK));
    e.start();
    e.advance(2 * STEP);
    e.reset();
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_a_global_armament_in_clock_waits_for_the_offset() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_CLOCK));
    TEST_ASSERT_TRUE(e.setOffset(0, 10));
    e.start();
    e.advance(2 * STEP);
    e.reset();
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
    e.advance(8);
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_a_global_armament_in_random_emits_the_onset_through_the_draw() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_RANDOM));
    e.start();
    e.advance(2 * STEP);
    e.reset();
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_the_armament_survives_a_change_of_mode_to_random() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_RANDOM));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_the_armament_survives_a_change_of_mode_to_clock() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_CLOCK));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_a_global_armament_survives_a_change_of_mode_to_random() {
    SequencerEngine e;
    seqChannel(e, 0);
    e.start();
    e.advance(2 * STEP);
    e.reset();
    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_RANDOM));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_a_reset_in_the_middle_of_a_ratchet_abandons_the_remaining_sub_onsets() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    TEST_ASSERT_TRUE(e.instanceForChannel(0)->setRatchet(2, RATCHET_4));
    e.start();
    e.advance(2 * STEP);
    e.advance(24);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    e.advance(24);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    e.advance(71);
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_a_triplet_on_step_zero_doubles_the_recached_step_ticks() {
    SequencerEngine e;
    seqChannel(e, 0);
    routeReset(e, 0, CV_SOURCE_1);
    TEST_ASSERT_TRUE(e.instanceForChannel(0)->setRatchet(0, RATCHET_TRIPLET));
    e.refreshTiming(0);
    e.start();
    e.advance(192);
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
    e.advance(40);
    TEST_ASSERT_EQUAL_UINT16(96, e.currentStepTicks(0));
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    TEST_ASSERT_EQUAL_UINT16(192, e.currentStepTicks(0));
    TEST_ASSERT_EQUAL_UINT8(3, e.currentStepTriggers(0));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    e.advance(63);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_a_reset_of_one_channel_leaves_the_five_others_intact() {
    SequencerEngine e;
    for (uint8_t ch = 0; ch < 6; ++ch) {
        seqChannel(e, ch);
    }
    routeReset(e, 2, CV_SOURCE_1);
    e.start();
    e.advance(3 * STEP);
    e.applyCvResetEvents(1u << CV_SOURCE_1);
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(2));
    TEST_ASSERT_EQUAL_INT8(3, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(3, e.effectiveStep(1));
    TEST_ASSERT_EQUAL_INT8(3, e.effectiveStep(3));
    TEST_ASSERT_EQUAL_INT8(3, e.effectiveStep(4));
    TEST_ASSERT_EQUAL_INT8(3, e.effectiveStep(5));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_cv1_edge_resets_a_seq_channel_routed_to_cv1_immediately);
    RUN_TEST(test_a_cv2_edge_resets_a_channel_routed_to_cv2);
    RUN_TEST(test_an_edge_on_the_unrouted_source_does_nothing);
    RUN_TEST(test_a_channel_routing_both_sources_resets_on_either_edge);
    RUN_TEST(test_a_zero_mask_is_a_strict_no_op);
    RUN_TEST(test_bits_beyond_the_source_count_are_masked);
    RUN_TEST(test_no_onset_is_emitted_between_the_call_and_the_next_advance);
    RUN_TEST(test_the_first_advance_emits_one_onset_on_step_zero);
    RUN_TEST(test_the_armament_does_not_fire_twice);
    RUN_TEST(test_an_inactive_step_zero_gives_an_onset_and_no_trigger);
    RUN_TEST(test_an_active_step_zero_gives_a_trigger);
    RUN_TEST(test_two_windows_give_two_resets);
    RUN_TEST(test_both_bits_in_one_mask_give_one_onset);
    RUN_TEST(test_a_reset_leaves_the_pattern_and_the_selection_intact);
    RUN_TEST(test_the_next_boundary_falls_exactly_step_ticks_after_the_reset);
    RUN_TEST(test_the_length_cv_offset_survives_a_reset);
    RUN_TEST(test_a_pending_rate_survives_a_reset_and_applies_on_the_beat);
    RUN_TEST(test_clock_with_zero_offset_fires_on_the_first_tick);
    RUN_TEST(test_clock_with_an_offset_fires_at_the_offset_and_not_at_the_armament);
    RUN_TEST(test_random_ignores_the_reset_and_the_return_to_seq_replays_nothing);
    RUN_TEST(test_seq_resets_while_random_stays_on_the_shared_source);
    RUN_TEST(test_a_reset_while_stopped_repositions_and_fires_only_after_start);
    RUN_TEST(test_a_global_reset_subsumes_a_cv_armament_into_one_onset);
    RUN_TEST(test_a_global_reset_arms_the_six_channels);
    RUN_TEST(test_a_reset_while_stopped_arms_until_the_first_start);
    RUN_TEST(test_a_global_reset_makes_clock_zero_offset_fire_on_the_first_tick);
    RUN_TEST(test_a_global_armament_in_clock_waits_for_the_offset);
    RUN_TEST(test_a_global_armament_in_random_emits_the_onset_through_the_draw);
    RUN_TEST(test_the_armament_survives_a_change_of_mode_to_random);
    RUN_TEST(test_the_armament_survives_a_change_of_mode_to_clock);
    RUN_TEST(test_a_global_armament_survives_a_change_of_mode_to_random);
    RUN_TEST(test_a_reset_in_the_middle_of_a_ratchet_abandons_the_remaining_sub_onsets);
    RUN_TEST(test_a_triplet_on_step_zero_doubles_the_recached_step_ticks);
    RUN_TEST(test_a_reset_of_one_channel_leaves_the_five_others_intact);
    return UNITY_END();
}
