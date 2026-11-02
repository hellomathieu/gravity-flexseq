#include <stdint.h>
#include <unity.h>

#include <flexseq/PatternBank.h>
#include <flexseq/SequencerEngine.h>

using flexseq::SequencerEngine;
using flexseq::MODE_CLOCK;
using flexseq::MODE_RANDOM;
using flexseq::MODE_SEQ;
using flexseq::RATCHET_3;
using flexseq::RATCHET_4;
using flexseq::RATCHET_6;
using flexseq::RATCHET_TRIPLET;

static void allSeq(SequencerEngine& e) {
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        e.setChannelMode(ch, flexseq::MODE_SEQ);
    }
}

void setUp() {}
void tearDown() {}

static const uint16_t STEP = SequencerEngine::PPQN; // 96 = default ticksPerStep (/1)

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
    TEST_ASSERT_TRUE(e.setBaseLength(0, 16));
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
    TEST_ASSERT_TRUE(e.setBaseLength(0, 4));
    TEST_ASSERT_EQUAL_UINT32(before, e.masterPhase());
    TEST_ASSERT_TRUE(e.setBaseLength(0, 24));
    TEST_ASSERT_EQUAL_UINT32(before, e.masterPhase());
}

void test_no_jump_when_length_shrinks_within_range() {
    SequencerEngine e;
    e.start();
    e.advance(STEP * 5); // localStep = 5
    TEST_ASSERT_EQUAL_INT8(5, e.effectiveStep(0));
    TEST_ASSERT_TRUE(e.setBaseLength(0, 11));
    TEST_ASSERT_EQUAL_INT8(5, e.effectiveStep(0));
    TEST_ASSERT_TRUE(e.setBaseLength(0, 8));
    TEST_ASSERT_EQUAL_INT8(5, e.effectiveStep(0));
}

void test_folds_into_range_only_when_length_drops_below() {
    SequencerEngine e;
    e.start();
    e.advance(STEP * 13); // localStep = 13
    TEST_ASSERT_TRUE(e.setBaseLength(0, 11));
    TEST_ASSERT_EQUAL_INT8(13 % 11, e.effectiveStep(0)); // 2
}

void test_keeps_step_when_length_grows() {
    SequencerEngine e;
    e.start();
    e.setBaseLength(0, 8);
    e.advance(STEP * 3); // localStep = 3
    TEST_ASSERT_TRUE(e.setBaseLength(0, 24));
    TEST_ASSERT_EQUAL_INT8(3, e.effectiveStep(0));
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(4, e.effectiveStep(0));
}

void test_global_reset_realigns_all_channels() {
    SequencerEngine e;
    e.start();
    e.setBaseLength(1, 3);
    e.advance(STEP * 7);
    TEST_ASSERT_TRUE(e.effectiveStep(0) > 0);
    e.reset();
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(1));
    TEST_ASSERT_EQUAL_UINT32(0, e.masterPhase());
}

void test_rejects_invalid_effective_length_without_mutation() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setBaseLength(0, 12));
    TEST_ASSERT_FALSE(e.setBaseLength(0, 0));
    TEST_ASSERT_FALSE(e.setBaseLength(0, 37));
    TEST_ASSERT_EQUAL_UINT8(12, e.getEffectiveLength(0));
}

void test_isolates_execution_state_between_channels() {
    SequencerEngine e;
    e.start();
    e.setBaseLength(0, 16);
    e.setBaseLength(1, 3);
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
    TEST_ASSERT_EQUAL_INT8(4, e.effectiveStep(0)); // 384/96 = 4 steps
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(1)); // 384/192 = 2 steps
}

void test_rejects_invalid_channel_and_ticks_per_step() {
    SequencerEngine e;
    TEST_ASSERT_EQUAL_INT8(-1, e.effectiveStep(6));
    TEST_ASSERT_FALSE(e.setBaseLength(6, 8));
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

void test_default_subdiv_and_setSubdiv_rate() {
    SequencerEngine e;
    TEST_ASSERT_EQUAL_INT16(1, e.getSubdiv(0)); // /1 = quarter (noire), default
    TEST_ASSERT_EQUAL_UINT16(96, e.getTicksPerStep(0));

    e.start();
    TEST_ASSERT_TRUE(e.setSubdiv(0, -4)); // 1/16 = 24 ticks
    TEST_ASSERT_EQUAL_UINT16(24, e.getTicksPerStep(0));
    e.advance(24);
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
}

void test_subdiv_per_channel_gives_different_rates() {
    SequencerEngine e;
    e.start();
    e.setSubdiv(0, -4); // 1/16 -> 24 ticks
    e.setSubdiv(1, 1);  // 1/4  -> 96 ticks
    e.advance(96);
    TEST_ASSERT_EQUAL_INT8(4, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(1));
}

void test_rejects_invalid_subdiv() {
    SequencerEngine e;
    TEST_ASSERT_FALSE(e.setSubdiv(0, 0));
    TEST_ASSERT_FALSE(e.setSubdiv(6, 1));
    TEST_ASSERT_EQUAL_INT16(0, e.getSubdiv(6));
}

/*
 * Ratchets — N triggers inside one step; the triplet stretches time
 */

void test_plain_step_fires_one_onset_per_step() {
    SequencerEngine e;
    allSeq(e);
    e.start();
    TEST_ASSERT_EQUAL_UINT16(96, e.currentStepTicks(0));
    TEST_ASSERT_EQUAL_UINT8(1, e.currentStepTriggers(0));
    e.advance(96);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    TEST_ASSERT_TRUE(e.hasStepped(0));
}

void test_ratchet_fires_n_onsets_inside_one_step_duration() {
    SequencerEngine e;
    flexseq::PatternBank eBank;
    e.setPatternBank(&eBank);
    allSeq(e);
    e.setSelectedPattern(0, 0);
    e.patternForChannel(0)->setRatchet(1, RATCHET_3);
    e.start();

    e.advance(96); // -> step 1, the ratchet step
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_UINT8(3, e.currentStepTriggers(0));
    // The step duration is UNCHANGED: only the trigger density grows.
    TEST_ASSERT_EQUAL_UINT16(96, e.currentStepTicks(0));

    e.advance(32);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0)); // 2nd of three
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0)); // still the same step
    e.advance(32);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0)); // 3rd of three
    e.advance(32);
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0)); // now the next step
}

void test_ratchet_step_keeps_the_pattern_duration() {
    SequencerEngine e;
    flexseq::PatternBank eBank;
    e.setPatternBank(&eBank);
    allSeq(e);
    e.setSelectedPattern(0, 0);
    e.setBaseLength(0, 4);
    e.patternForChannel(0)->setRatchet(0, RATCHET_6);
    e.start();
    e.advance(96 * 4); // four plain step durations
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0)); // full loop, back to start
}

void test_ratchet_counts_all_onsets_in_a_batched_advance() {
    SequencerEngine e;
    flexseq::PatternBank eBank;
    e.setPatternBank(&eBank);
    allSeq(e);
    e.setSelectedPattern(0, 0);
    e.patternForChannel(0)->setRatchet(1, RATCHET_4);
    e.start();
    e.advance(96);       // -> step 1 (ratchet 4)
    e.advance(96);       // whole ratchet step in one drain: 3 sub + 1 step onset
    TEST_ASSERT_EQUAL_UINT8(4, e.onsetCount(0));
}

void test_triplet_stretches_the_step_to_two_units() {
    SequencerEngine e;
    flexseq::PatternBank eBank;
    e.setPatternBank(&eBank);
    allSeq(e);
    e.setSelectedPattern(0, 0);
    e.patternForChannel(0)->setRatchet(1, RATCHET_TRIPLET);
    e.start();

    e.advance(96); // -> step 1, the triplet
    TEST_ASSERT_EQUAL_UINT16(192, e.currentStepTicks(0)); // two units
    TEST_ASSERT_EQUAL_UINT8(3, e.currentStepTriggers(0));

    e.advance(64); // 192 / 3
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
    e.advance(64);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    e.advance(64); // end of the stretched step
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0));
}

void test_triplet_pushes_the_rest_of_the_pattern_later() {
    SequencerEngine plain;
    flexseq::PatternBank plainBank;
    plain.setPatternBank(&plainBank);
    SequencerEngine withTriplet;
    flexseq::PatternBank withTripletBank;
    withTriplet.setPatternBank(&withTripletBank);
    allSeq(plain);
    allSeq(withTriplet);

    withTriplet.patternForChannel(0)->setRatchet(0, RATCHET_TRIPLET);
    withTriplet.refreshTiming();

    plain.start();
    withTriplet.start();
    plain.advance(96 * 3);
    withTriplet.advance(96 * 3);
    // The triplet consumed two units, so this channel is one step behind.
    TEST_ASSERT_EQUAL_INT8(3, plain.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(2, withTriplet.effectiveStep(0));
}

// PRD 6.3.1 : un sous-slot n'a plus a tomber sur un tick entier, il doit valoir
// au moins MIN_SLOT_TICKS. A x3 un tiers de step vaut 10,67 ticks : le ratchet
// joue, et ses positions sont arrondies a moins d'un tick.
void test_a_ratchet_plays_when_its_slot_is_not_a_whole_tick() {
    SequencerEngine e;
    flexseq::PatternBank eBank;
    e.setPatternBank(&eBank);
    allSeq(e);
    e.setSelectedPattern(0, 0);
    e.patternForChannel(0)->setRatchet(0, RATCHET_3);
    TEST_ASSERT_TRUE(e.setSubdiv(0, -3));
    TEST_ASSERT_EQUAL_UINT8(3, e.currentStepTriggers(0));
}

// Le plancher, lui, refuse : a x12 un step vaut 8 ticks, donc un sixieme
// vaudrait 1 tick.
void test_a_ratchet_is_refused_when_its_slot_falls_under_two_ticks() {
    SequencerEngine e;
    flexseq::PatternBank eBank;
    e.setPatternBank(&eBank);
    allSeq(e);
    e.setSelectedPattern(0, 0);
    e.patternForChannel(0)->setRatchet(0, RATCHET_6);
    TEST_ASSERT_TRUE(e.setSubdiv(0, -12));
    TEST_ASSERT_EQUAL_UINT8(1, e.currentStepTriggers(0));
}

void test_a_new_engine_plays_plain_steps() {
    SequencerEngine e;
    allSeq(e);
    e.start();
    TEST_ASSERT_EQUAL_UINT8(1, e.currentStepTriggers(0));
    e.advance(96);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_ratchets_do_not_shift_masterphase() {
    SequencerEngine e;
    flexseq::PatternBank eBank;
    e.setPatternBank(&eBank);
    allSeq(e);
    e.patternForChannel(0)->setRatchet(0, RATCHET_TRIPLET);
    e.start();
    e.advance(192);
    TEST_ASSERT_EQUAL_UINT32(192, e.masterPhase());
}

/*
 * Measure separation — graphical only
 */

void test_bar_length_defaults_and_accepts_the_allowed_set() {
    SequencerEngine e;
    TEST_ASSERT_EQUAL_INT8(4, e.getBarLength(0)); // default 4/4
    TEST_ASSERT_TRUE(e.setBarLength(0, 0));       // none
    TEST_ASSERT_TRUE(e.setBarLength(0, 2));
    TEST_ASSERT_TRUE(e.setBarLength(0, 3));
    TEST_ASSERT_TRUE(e.setBarLength(0, 6));
    TEST_ASSERT_EQUAL_INT8(6, e.getBarLength(0));
}

void test_bar_length_rejects_values_that_do_not_divide_twelve() {
    SequencerEngine e;
    TEST_ASSERT_FALSE(e.setBarLength(0, 5));
    TEST_ASSERT_FALSE(e.setBarLength(0, 8));
    TEST_ASSERT_FALSE(e.setBarLength(6, 4)); // invalid channel
    TEST_ASSERT_EQUAL_INT8(-1, e.getBarLength(6));
}

void test_bar_length_never_affects_timing() {
    SequencerEngine a;
    SequencerEngine b;
    b.setBarLength(0, 3);
    a.start();
    b.start();
    a.advance(96 * 5);
    b.advance(96 * 5);
    TEST_ASSERT_EQUAL_INT8(a.effectiveStep(0), b.effectiveStep(0));
    TEST_ASSERT_EQUAL_UINT32(a.masterPhase(), b.masterPhase());
}


/*
 * Channel modes — CLOCK / RANDOM / SEQ (PRD 4.2)
 */

void test_every_channel_starts_in_clock_mode() {
    SequencerEngine e;
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_EQUAL_UINT8(MODE_CLOCK, e.getChannelMode(ch));
        TEST_ASSERT_EQUAL_UINT16(0, e.getOffset(ch));
        TEST_ASSERT_EQUAL_UINT8(0, e.getSkipChance(ch));
    }
}

void test_mode_setter_rejects_an_unknown_mode_and_an_unknown_channel() {
    SequencerEngine e;
    TEST_ASSERT_FALSE(e.setChannelMode(0, static_cast<flexseq::ChannelMode>(3)));
    TEST_ASSERT_FALSE(e.setChannelMode(SequencerEngine::CHANNEL_COUNT, MODE_SEQ));
    TEST_ASSERT_EQUAL_UINT8(MODE_CLOCK, e.getChannelMode(0));
}

void test_clock_fires_one_onset_per_step_at_offset_zero() {
    SequencerEngine e;
    e.start();
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
    e.advance(95);
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
    e.advance(1);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    e.advance(96);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_clock_fires_at_the_offset_pulse_not_at_the_boundary() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setOffset(0, 10));
    e.start();
    e.advance(9);
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
    e.advance(1); // tick 10
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    e.advance(86); // tick 96, the step boundary
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
    e.advance(10); // tick 106
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_clock_keeps_one_onset_per_step_whatever_the_offset() {
    for (uint16_t offset = 0; offset < 96; offset += 7) {
        SequencerEngine e;
        e.setOffset(0, offset);
        e.start();
        uint16_t total = 0;
        for (uint16_t tick = 0; tick < 96 * 10; ++tick) {
            e.advance(1);
            total = static_cast<uint16_t>(total + e.onsetCount(0));
        }
        TEST_ASSERT_EQUAL_UINT16(10, total);
    }
}

void test_clock_counts_every_offset_crossing_in_a_batched_advance() {
    SequencerEngine e;
    e.setOffset(0, 10);
    e.start();
    e.advance(200); // crosses tick 10 and tick 106
    TEST_ASSERT_EQUAL_UINT8(2, e.onsetCount(0));
}

void test_clock_and_random_ignore_ratchets() {
    SequencerEngine e;
    flexseq::PatternBank eBank;
    e.setPatternBank(&eBank);
    e.patternForChannel(0)->setRatchet(0, RATCHET_4);
    e.patternForChannel(0)->setRatchet(1, RATCHET_TRIPLET);
    e.setChannelMode(1, MODE_RANDOM);
    e.start();

    TEST_ASSERT_EQUAL_UINT8(1, e.currentStepTriggers(0));
    TEST_ASSERT_EQUAL_UINT8(1, e.currentStepTriggers(1));
    e.advance(96);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(1));
    TEST_ASSERT_EQUAL_UINT16(96, e.currentStepTicks(0)); // no triplet stretch
}

void test_random_fires_on_the_step_boundary() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_RANDOM);
    e.setOffset(0, 10); // offset belongs to CLOCK only
    e.start();
    e.advance(10);
    TEST_ASSERT_EQUAL_UINT8(0, e.onsetCount(0));
    e.advance(86);
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
}

void test_offset_is_clamped_to_the_step_and_follows_the_rate() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setOffset(0, 500));
    TEST_ASSERT_EQUAL_UINT16(95, e.getOffset(0)); // 96 ticks per step at /1

    e.setSubdiv(0, 2); // 192 ticks per step
    TEST_ASSERT_EQUAL_UINT16(95, e.getOffset(0)); // still valid, untouched
    e.setOffset(0, 150);
    e.setSubdiv(0, -4); // 24 ticks per step
    TEST_ASSERT_EQUAL_UINT16(23, e.getOffset(0));

    e.setTicksPerStep(0, 8);
    TEST_ASSERT_EQUAL_UINT16(7, e.getOffset(0));
}

void test_an_offset_equal_to_the_step_is_pulled_back_inside_it() {
    SequencerEngine e;
    e.setOffset(0, 96); // exactly ticksPerStep: never reached, the step would be silent
    TEST_ASSERT_EQUAL_UINT16(95, e.getOffset(0));
    e.start();
    uint16_t total = 0;
    for (uint16_t tick = 0; tick < 96 * 4; ++tick) {
        e.advance(1);
        total = static_cast<uint16_t>(total + e.onsetCount(0));
    }
    TEST_ASSERT_EQUAL_UINT16(4, total);
}

void test_skip_chance_is_bounded_to_ten_tenths() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setSkipChance(0, 10));
    TEST_ASSERT_EQUAL_UINT8(10, e.getSkipChance(0));
    TEST_ASSERT_FALSE(e.setSkipChance(0, 11));
    TEST_ASSERT_EQUAL_UINT8(10, e.getSkipChance(0));
    TEST_ASSERT_FALSE(e.setSkipChance(SequencerEngine::CHANNEL_COUNT, 3));
}

void test_switching_to_seq_starts_reading_the_pattern_again() {
    SequencerEngine e;
    flexseq::PatternBank eBank;
    e.setPatternBank(&eBank);
    e.patternForChannel(0)->setRatchet(0, RATCHET_4);
    TEST_ASSERT_EQUAL_UINT8(1, e.currentStepTriggers(0));
    e.setChannelMode(0, MODE_SEQ);
    TEST_ASSERT_EQUAL_UINT8(4, e.currentStepTriggers(0));
    e.setChannelMode(0, MODE_CLOCK);
    TEST_ASSERT_EQUAL_UINT8(1, e.currentStepTriggers(0));
}

struct Bench {
    flexseq::PatternBank bank;
    SequencerEngine e;
    Bench() { e.setPatternBank(&bank); }
};

static bool stepIsActive(const flexseq::Pattern* p, uint8_t step) {
    bool active = false;
    return p != nullptr && p->readStep(step, active) && active;
}

void test_a_fresh_engine_has_no_bank_and_plays_no_pattern() {
    SequencerEngine e;
    TEST_ASSERT_NULL(e.patternBank());
    for (uint8_t ch = 0; ch < 6; ++ch) {
        TEST_ASSERT_NULL(e.patternForChannel(ch));
    }
    const SequencerEngine& frozen = e;
    TEST_ASSERT_NULL(frozen.patternForChannel(0));
}

void test_the_engine_keeps_the_bank_it_is_given() {
    flexseq::PatternBank bank;
    SequencerEngine e;
    e.setPatternBank(&bank);
    TEST_ASSERT_EQUAL_PTR(&bank, e.patternBank());
    e.setPatternBank(nullptr);
    TEST_ASSERT_NULL(e.patternBank());
    TEST_ASSERT_NULL(e.patternForChannel(0));
}

void test_the_channel_plays_the_pattern_its_selector_names() {
    Bench b;
    for (uint8_t ch = 0; ch < 6; ++ch) {
        TEST_ASSERT_TRUE(b.e.setSelectedPattern(ch, static_cast<uint8_t>(ch + 3)));
        TEST_ASSERT_EQUAL_PTR(b.bank.getPattern(static_cast<uint8_t>(ch + 3)),
                              b.e.patternForChannel(ch));
    }
    TEST_ASSERT_EQUAL_UINT8(6, SequencerEngine::CHANNEL_COUNT);
}

void test_an_invalid_channel_has_no_pattern_even_with_a_bank() {
    Bench b;
    TEST_ASSERT_NULL(b.e.patternForChannel(6));
    TEST_ASSERT_NULL(b.e.patternForChannel(255));
    const SequencerEngine& frozen = b.e;
    TEST_ASSERT_NULL(frozen.patternForChannel(6));
}

void test_the_const_overload_names_the_same_pattern() {
    Bench b;
    const SequencerEngine& frozen = b.e;
    for (uint8_t ch = 0; ch < 6; ++ch) {
        TEST_ASSERT_TRUE(b.e.setSelectedPattern(ch, static_cast<uint8_t>(15 - ch)));
        TEST_ASSERT_TRUE(b.e.patternForChannel(ch) == frozen.patternForChannel(ch));
    }
}

void test_two_channels_on_one_pattern_share_it() {
    Bench b;
    TEST_ASSERT_TRUE(b.e.setSelectedPattern(0, 2));
    TEST_ASSERT_TRUE(b.e.setSelectedPattern(1, 2));
    TEST_ASSERT_EQUAL_PTR(b.e.patternForChannel(0), b.e.patternForChannel(1));

    TEST_ASSERT_TRUE(b.e.patternForChannel(0)->writeStep(5, true));
    TEST_ASSERT_TRUE_MESSAGE(stepIsActive(b.e.patternForChannel(1), 5),
                             "an edit on one channel must reach the other channel on the same pattern");
}

void test_an_edit_reaches_no_channel_on_another_pattern() {
    Bench b;
    for (uint8_t ch = 0; ch < 6; ++ch) {
        TEST_ASSERT_TRUE(b.e.setSelectedPattern(ch, ch));
    }
    TEST_ASSERT_TRUE(b.e.patternForChannel(0)->writeStep(3, true));
    TEST_ASSERT_TRUE(b.e.patternForChannel(0)->setRatchet(3, RATCHET_3));
    for (uint8_t ch = 1; ch < 6; ++ch) {
        const flexseq::Pattern* other = b.e.patternForChannel(ch);
        for (uint8_t step = 0; step < 36; ++step) {
            TEST_ASSERT_FALSE(stepIsActive(other, step));
            TEST_ASSERT_EQUAL_UINT8(0, other->getRatchet(step));
        }
    }
}

void test_a_load_is_a_change_of_selector_and_copies_nothing() {
    Bench b;
    TEST_ASSERT_TRUE(b.bank.getPattern(7)->writeStep(4, true));
    TEST_ASSERT_TRUE(b.e.setSelectedPattern(0, 7));
    TEST_ASSERT_TRUE(stepIsActive(b.e.patternForChannel(0), 4));

    TEST_ASSERT_TRUE(b.e.setSelectedPattern(0, 9));
    TEST_ASSERT_FALSE(stepIsActive(b.e.patternForChannel(0), 4));
    TEST_ASSERT_TRUE_MESSAGE(stepIsActive(b.bank.getPattern(7), 4),
                             "leaving a pattern must not alter it");
    TEST_ASSERT_FALSE_MESSAGE(stepIsActive(b.bank.getPattern(9), 4),
                              "arriving on a pattern must not copy the previous one into it");
}

static const int16_t CV_ZONE_PLUS_TEN = 330;

void test_without_a_pattern_routing_the_played_pattern_is_the_selected_one() {
    Bench b;
    b.e.setChannelMode(0, MODE_SEQ);
    TEST_ASSERT_TRUE(b.e.setSelectedPattern(0, 4));
    TEST_ASSERT_TRUE(b.e.setCvInput(flexseq::CV_SOURCE_1, CV_ZONE_PLUS_TEN));
    b.e.start();
    b.e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(4, b.e.patternCvIndex(0));
    TEST_ASSERT_EQUAL_PTR(b.bank.getPattern(4), b.e.patternForChannel(0));
}

void test_the_played_pattern_follows_the_cv_index_and_the_selector_does_not_move() {
    Bench b;
    b.e.setChannelMode(0, MODE_SEQ);
    TEST_ASSERT_TRUE(b.e.setSelectedPattern(0, 4));
    TEST_ASSERT_TRUE(b.e.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_PATTERN));
    TEST_ASSERT_TRUE(b.e.setCvInput(flexseq::CV_SOURCE_1, CV_ZONE_PLUS_TEN));
    b.e.start();
    TEST_ASSERT_EQUAL_PTR(b.bank.getPattern(4), b.e.patternForChannel(0));

    b.e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(14, b.e.patternCvIndex(0));
    TEST_ASSERT_EQUAL_PTR(b.bank.getPattern(14), b.e.patternForChannel(0));
    TEST_ASSERT_EQUAL_INT8(4, b.e.getSelectedPattern(0));
}

void test_the_const_overload_plays_the_cv_index_too() {
    Bench b;
    const SequencerEngine& frozen = b.e;
    b.e.setChannelMode(0, MODE_SEQ);
    TEST_ASSERT_TRUE(b.e.setSelectedPattern(0, 4));
    TEST_ASSERT_TRUE(b.e.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_PATTERN));
    TEST_ASSERT_TRUE(b.e.setCvInput(flexseq::CV_SOURCE_1, CV_ZONE_PLUS_TEN));
    b.e.start();
    b.e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(14, b.e.patternCvIndex(0));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(b.bank.getPattern(14), frozen.patternForChannel(0),
                                  "the trigger sequencer reads through a const engine: it must play the CV index too");
    TEST_ASSERT_EQUAL_PTR(b.e.patternForChannel(0), frozen.patternForChannel(0));
}

void test_the_ratchet_cached_at_the_boundary_comes_from_the_played_pattern() {
    Bench b;
    b.e.setChannelMode(0, MODE_SEQ);
    TEST_ASSERT_TRUE(b.e.setSelectedPattern(0, 4));
    TEST_ASSERT_TRUE(b.bank.getPattern(4)->setRatchet(1, RATCHET_TRIPLET));
    TEST_ASSERT_TRUE(b.bank.getPattern(14)->setRatchet(1, RATCHET_3));
    TEST_ASSERT_TRUE(b.e.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_PATTERN));
    TEST_ASSERT_TRUE(b.e.setCvInput(flexseq::CV_SOURCE_1, CV_ZONE_PLUS_TEN));
    b.e.start();

    b.e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(1, b.e.effectiveStep(0));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, b.e.currentStepTriggers(0),
                                    "the cache must read the ratchet of the pattern the index now names");
    TEST_ASSERT_EQUAL_UINT16(STEP, b.e.currentStepTicks(0));
}

void test_the_pattern_modulation_reads_no_pattern_outside_seq() {
    Bench b;
    TEST_ASSERT_TRUE(b.e.setSelectedPattern(0, 4));
    TEST_ASSERT_TRUE(b.e.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_PATTERN));
    TEST_ASSERT_TRUE(b.e.setCvInput(flexseq::CV_SOURCE_1, CV_ZONE_PLUS_TEN));
    b.e.start();
    b.e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(4, b.e.patternCvIndex(0));
    TEST_ASSERT_EQUAL_PTR(b.bank.getPattern(4), b.e.patternForChannel(0));
}

void test_the_manual_entry_point_stops_at_the_interface_ceiling() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setBaseLength(0, 1));
    TEST_ASSERT_TRUE(e.setBaseLength(0, 24));
    TEST_ASSERT_TRUE(e.setBaseLength(0, 25));
    TEST_ASSERT_EQUAL_UINT8(25, e.getEffectiveLength(0));
    TEST_ASSERT_TRUE(e.setBaseLength(0, 36));
    TEST_ASSERT_EQUAL_UINT8(36, e.getBaseLength(0));
    TEST_ASSERT_FALSE(e.setBaseLength(0, 37));
    TEST_ASSERT_EQUAL_UINT8(36, e.getBaseLength(0));
}

void test_the_manual_entry_point_stops_at_one() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setBaseLength(0, 1));
    TEST_ASSERT_FALSE(e.setBaseLength(0, 0));
    TEST_ASSERT_EQUAL_UINT8(1, e.getBaseLength(0));
}

void test_the_storage_entry_point_accepts_the_pattern_capacity() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setBaseLengthFromStorage(0, 36));
    TEST_ASSERT_EQUAL_UINT8(36, e.getBaseLength(0));
    TEST_ASSERT_EQUAL_UINT8(36, e.getEffectiveLength(0));
}

void test_storage_stops_at_the_pattern_capacity() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setBaseLengthFromStorage(0, 36));
    TEST_ASSERT_FALSE(e.setBaseLengthFromStorage(0, 37));
    TEST_ASSERT_EQUAL_UINT8(36, e.getBaseLength(0));
}

void test_a_base_length_below_the_ceiling_needs_no_clamp() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setBaseLengthFromStorage(0, 12));
    TEST_ASSERT_EQUAL_UINT8(12, e.getBaseLength(0));
    TEST_ASSERT_EQUAL_UINT8(12, e.getEffectiveLength(0));
}

void test_an_invalid_channel_refuses_both_entry_points() {
    SequencerEngine e;
    TEST_ASSERT_FALSE(e.setBaseLength(6, 8));
    TEST_ASSERT_FALSE(e.setBaseLengthFromStorage(6, 8));
    TEST_ASSERT_EQUAL_UINT8(0, e.getBaseLength(6));
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

    RUN_TEST(test_default_subdiv_and_setSubdiv_rate);
    RUN_TEST(test_subdiv_per_channel_gives_different_rates);
    RUN_TEST(test_rejects_invalid_subdiv);

    RUN_TEST(test_plain_step_fires_one_onset_per_step);
    RUN_TEST(test_ratchet_fires_n_onsets_inside_one_step_duration);
    RUN_TEST(test_ratchet_step_keeps_the_pattern_duration);
    RUN_TEST(test_ratchet_counts_all_onsets_in_a_batched_advance);
    RUN_TEST(test_triplet_stretches_the_step_to_two_units);
    RUN_TEST(test_triplet_pushes_the_rest_of_the_pattern_later);
    RUN_TEST(test_a_ratchet_plays_when_its_slot_is_not_a_whole_tick);
    RUN_TEST(test_a_ratchet_is_refused_when_its_slot_falls_under_two_ticks);
    RUN_TEST(test_a_new_engine_plays_plain_steps);
    RUN_TEST(test_ratchets_do_not_shift_masterphase);

    RUN_TEST(test_bar_length_defaults_and_accepts_the_allowed_set);
    RUN_TEST(test_bar_length_rejects_values_that_do_not_divide_twelve);
    RUN_TEST(test_bar_length_never_affects_timing);

    RUN_TEST(test_every_channel_starts_in_clock_mode);
    RUN_TEST(test_mode_setter_rejects_an_unknown_mode_and_an_unknown_channel);
    RUN_TEST(test_clock_fires_one_onset_per_step_at_offset_zero);
    RUN_TEST(test_clock_fires_at_the_offset_pulse_not_at_the_boundary);
    RUN_TEST(test_clock_keeps_one_onset_per_step_whatever_the_offset);
    RUN_TEST(test_clock_counts_every_offset_crossing_in_a_batched_advance);
    RUN_TEST(test_clock_and_random_ignore_ratchets);
    RUN_TEST(test_random_fires_on_the_step_boundary);
    RUN_TEST(test_offset_is_clamped_to_the_step_and_follows_the_rate);
    RUN_TEST(test_an_offset_equal_to_the_step_is_pulled_back_inside_it);
    RUN_TEST(test_skip_chance_is_bounded_to_ten_tenths);
    RUN_TEST(test_switching_to_seq_starts_reading_the_pattern_again);
    RUN_TEST(test_a_fresh_engine_has_no_bank_and_plays_no_pattern);
    RUN_TEST(test_the_engine_keeps_the_bank_it_is_given);
    RUN_TEST(test_the_channel_plays_the_pattern_its_selector_names);
    RUN_TEST(test_an_invalid_channel_has_no_pattern_even_with_a_bank);
    RUN_TEST(test_the_const_overload_names_the_same_pattern);
    RUN_TEST(test_two_channels_on_one_pattern_share_it);
    RUN_TEST(test_an_edit_reaches_no_channel_on_another_pattern);
    RUN_TEST(test_a_load_is_a_change_of_selector_and_copies_nothing);
    RUN_TEST(test_without_a_pattern_routing_the_played_pattern_is_the_selected_one);
    RUN_TEST(test_the_played_pattern_follows_the_cv_index_and_the_selector_does_not_move);
    RUN_TEST(test_the_const_overload_plays_the_cv_index_too);
    RUN_TEST(test_the_ratchet_cached_at_the_boundary_comes_from_the_played_pattern);
    RUN_TEST(test_the_pattern_modulation_reads_no_pattern_outside_seq);
    RUN_TEST(test_the_manual_entry_point_stops_at_the_interface_ceiling);
    RUN_TEST(test_the_manual_entry_point_stops_at_one);
    RUN_TEST(test_the_storage_entry_point_accepts_the_pattern_capacity);
    RUN_TEST(test_storage_stops_at_the_pattern_capacity);
    RUN_TEST(test_a_base_length_below_the_ceiling_needs_no_clamp);
    RUN_TEST(test_an_invalid_channel_refuses_both_entry_points);
    return UNITY_END();
}
