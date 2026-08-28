#include <stdint.h>
#include <unity.h>

#include <flexseq/PatternBank.h>
#include <flexseq/Prng.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/TriggerSequencer.h>

using flexseq::PatternBank;
using flexseq::Pattern;
using flexseq::SequencerEngine;
using flexseq::TriggerSequencer;
using flexseq::MODE_CLOCK;
using flexseq::MODE_RANDOM;
using flexseq::MODE_SEQ;
using flexseq::Prng;

void setUp() {}
void tearDown() {}

static void seqMode(SequencerEngine& engine) {
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        engine.setChannelMode(ch, MODE_SEQ);
    }
}

static const uint16_t STEP = SequencerEngine::PPQN; // 96 = default ticksPerStep (/1)

void test_triggers_only_on_active_step_onset() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(engine);
    engine.setPatternBank(&bank);
    seqMode(engine);

    // Channel 0 plays pattern 0 with steps 1 and 3 active, length 4.
    engine.setSelectedPattern(0, 0);
    engine.setEffectiveLength(0, 4);
    engine.instanceForChannel(0)->writeStep(1, true);
    engine.instanceForChannel(0)->writeStep(3, true);

    engine.start();

    engine.advance(STEP); // onto step 1 (active)
    trig.update();
    TEST_ASSERT_TRUE(trig.triggered(0));

    engine.advance(STEP); // onto step 2 (inactive)
    trig.update();
    TEST_ASSERT_FALSE(trig.triggered(0));

    engine.advance(STEP); // onto step 3 (active)
    trig.update();
    TEST_ASSERT_TRUE(trig.triggered(0));

    engine.advance(STEP); // onto step 0 (inactive)
    trig.update();
    TEST_ASSERT_FALSE(trig.triggered(0));
}

void test_no_trigger_without_a_step_onset() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(engine);
    engine.setPatternBank(&bank);
    seqMode(engine);

    engine.instanceForChannel(0)->writeStep(1, true);
    engine.setEffectiveLength(0, 4);
    engine.start();

    engine.advance(STEP - 1); // no boundary crossed
    trig.update();
    TEST_ASSERT_FALSE(trig.triggered(0));
}

void test_channels_with_different_patterns_are_independent() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(engine);
    engine.setPatternBank(&bank);
    seqMode(engine);

    engine.setSelectedPattern(0, 0);
    engine.setSelectedPattern(1, 1);
    engine.setEffectiveLength(0, 4);
    engine.setEffectiveLength(1, 4);
    engine.instanceForChannel(0)->writeStep(1, true);

    engine.start();
    engine.advance(STEP); // both onto step 1
    trig.update();

    TEST_ASSERT_TRUE(trig.triggered(0));
    TEST_ASSERT_FALSE(trig.triggered(1));
}


/*
 * Modes — CLOCK ignores the pattern, RANDOM draws, SEQ reads it (PRD 4.2)
 */

static uint16_t countTriggers(TriggerSequencer& trig, SequencerEngine& engine,
                              uint8_t channel, uint16_t steps) {
    uint16_t kept = 0;
    for (uint16_t i = 0; i < steps; ++i) {
        engine.advance(STEP);
        trig.update();
        kept = static_cast<uint16_t>(kept + trig.triggerCount(channel));
    }
    return kept;
}

void test_clock_triggers_on_every_step_whatever_the_pattern() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(engine);
    engine.setPatternBank(&bank);

    engine.setEffectiveLength(0, 4);
    engine.start();
    TEST_ASSERT_EQUAL_UINT16(8, countTriggers(trig, engine, 0, 8));
}

void test_random_never_skips_at_zero() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(engine);
    engine.setPatternBank(&bank);

    engine.setChannelMode(0, MODE_RANDOM);
    engine.setSkipChance(0, 0);
    engine.start();
    TEST_ASSERT_EQUAL_UINT16(64, countTriggers(trig, engine, 0, 64));
}

void test_random_always_skips_at_ten() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(engine);
    engine.setPatternBank(&bank);

    engine.setChannelMode(0, MODE_RANDOM);
    engine.setSkipChance(0, 10);
    engine.start();
    TEST_ASSERT_EQUAL_UINT16(0, countTriggers(trig, engine, 0, 64));
}

void test_random_at_five_keeps_about_half() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(engine);
    engine.setPatternBank(&bank);

    engine.setChannelMode(0, MODE_RANDOM);
    engine.setSkipChance(0, 5);
    engine.start();
    const uint16_t kept = countTriggers(trig, engine, 0, 1000);
    TEST_ASSERT_GREATER_THAN_UINT16(400, kept);
    TEST_ASSERT_LESS_THAN_UINT16(600, kept);
}

void test_random_is_reproducible_from_one_run_to_the_next() {
    PatternBank bank;
    SequencerEngine a;
    SequencerEngine b;
    TriggerSequencer ta(a);
    a.setPatternBank(&bank);
    TriggerSequencer tb(b);
    b.setPatternBank(&bank);

    a.setChannelMode(0, MODE_RANDOM);
    b.setChannelMode(0, MODE_RANDOM);
    a.setSkipChance(0, 5);
    b.setSkipChance(0, 5);
    a.start();
    b.start();
    TEST_ASSERT_EQUAL_UINT16(countTriggers(ta, a, 0, 200), countTriggers(tb, b, 0, 200));
}

void test_a_different_seed_gives_a_different_run() {
    PatternBank bank;
    SequencerEngine a;
    SequencerEngine b;
    TriggerSequencer ta(a);
    a.setPatternBank(&bank);
    TriggerSequencer tb(b);
    b.setPatternBank(&bank);
    tb.seed(0x1234u);

    a.setChannelMode(0, MODE_RANDOM);
    b.setChannelMode(0, MODE_RANDOM);
    a.setSkipChance(0, 5);
    b.setSkipChance(0, 5);
    a.start();
    b.start();

    bool differed = false;
    for (uint16_t i = 0; i < 200 && !differed; ++i) {
        a.advance(STEP);
        b.advance(STEP);
        ta.update();
        tb.update();
        differed = ta.triggered(0) != tb.triggered(0);
    }
    TEST_ASSERT_TRUE(differed);
}

void test_the_draw_is_spent_only_when_a_step_actually_lands() {
    PatternBank bank;
    SequencerEngine withIdle;
    SequencerEngine backToBack;
    TriggerSequencer ti(withIdle);
    withIdle.setPatternBank(&bank);
    TriggerSequencer tb(backToBack);
    backToBack.setPatternBank(&bank);

    withIdle.setChannelMode(0, MODE_RANDOM);
    backToBack.setChannelMode(0, MODE_RANDOM);
    withIdle.setSkipChance(0, 5);
    backToBack.setSkipChance(0, 5);
    withIdle.start();
    backToBack.start();

    for (uint16_t i = 0; i < 50; ++i) {
        withIdle.advance(STEP / 2); // no boundary, so no draw
        ti.update();
        TEST_ASSERT_EQUAL_UINT8(0, ti.triggerCount(0));
        withIdle.advance(STEP / 2);
        ti.update();

        backToBack.advance(STEP);
        tb.update();
        TEST_ASSERT_EQUAL_UINT8(tb.triggerCount(0), ti.triggerCount(0));
    }
}

void test_counts_hold_still_until_the_next_update() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(engine);
    engine.setPatternBank(&bank);

    engine.start();
    engine.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(0, trig.triggerCount(0)); // no update() yet
    trig.update();
    TEST_ASSERT_EQUAL_UINT8(1, trig.triggerCount(0));
    TEST_ASSERT_EQUAL_UINT8(1, trig.triggerCount(0)); // reading twice changes nothing
}

// A ratchet 6 at SUBDIV x8 gives 12 ticks per step and a sub-onset every 2
// ticks. A drain wider than one slot therefore carries several onsets, and the
// output can only be re-armed once per pass: the surplus must WAIT, not vanish.
void test_an_onset_the_output_could_not_emit_is_not_lost() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(engine);
    engine.setPatternBank(&bank);
    seqMode(engine);
    engine.setPatternBank(&bank);

    engine.setSelectedPattern(0, 0);
    engine.instanceForChannel(0)->writeStep(0, true);
    engine.instanceForChannel(0)->setRatchet(0, flexseq::RATCHET_6);
    engine.setSubdiv(0, -8);
    engine.start();

    // Sub-onsets land at ticks 2, 4, 6, 8, 10. Four ticks cross two of them.
    engine.advance(4);
    trig.update();
    TEST_ASSERT_EQUAL_UINT8(2, trig.triggerCount(0));
    TEST_ASSERT_EQUAL_UINT8(2, trig.owedTriggers(0));

    TEST_ASSERT_TRUE(trig.takeTrigger(0));

    // One more tick crosses NOTHING: the next sub-onset is at 6. So this drain
    // owes nothing, and a debt of one can only be the onset never emitted.
    engine.advance(1);
    trig.update();
    TEST_ASSERT_EQUAL_UINT8(0, trig.triggerCount(0));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, trig.owedTriggers(0),
        "l onset non emis doit survivre au drainage suivant");
}

// The debt stops at one whole step's worth. Every step must be active: the
// boundary onset belongs to the NEXT step, so a pattern with one active step
// dries up after the first one.
void test_the_debt_stops_at_one_whole_step() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(engine);
    engine.setPatternBank(&bank);
    seqMode(engine);
    engine.setPatternBank(&bank);

    engine.setSelectedPattern(0, 0);
    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        engine.instanceForChannel(0)->writeStep(i, true);
        engine.instanceForChannel(0)->setRatchet(i, flexseq::RATCHET_6);
    }
    engine.setSubdiv(0, -8);
    engine.start();

    for (uint8_t i = 0; i < 40; ++i) {
        engine.advance(1);
        trig.update();
    }
    TEST_ASSERT_EQUAL_UINT8(6, trig.owedTriggers(0));
}

void test_an_out_of_range_channel_never_triggers() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(engine);
    engine.setPatternBank(&bank);
    engine.start();
    engine.advance(STEP);
    trig.update();
    TEST_ASSERT_EQUAL_UINT8(0, trig.triggerCount(SequencerEngine::CHANNEL_COUNT));
}

void test_the_generator_never_settles_and_never_yields_zero() {
    Prng prng;
    uint16_t seen = 0;
    uint16_t previous = prng.next();
    for (uint16_t i = 0; i < 2000; ++i) {
        const uint16_t value = prng.next();
        TEST_ASSERT_NOT_EQUAL_UINT16(0, value);
        if (value != previous) {
            ++seen;
        }
        previous = value;
    }
    TEST_ASSERT_EQUAL_UINT16(2000, seen);
}

void test_the_generator_matches_the_typescript_reference() {
    static const uint16_t golden[5] = {54031u, 61861u, 5940u, 65394u, 5969u};
    Prng prng;
    for (uint8_t i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_UINT16(golden[i], prng.next());
    }
}

void test_the_generator_covers_the_whole_draw_range() {
    Prng prng;
    bool hit[10] = {false, false, false, false, false, false, false, false, false, false};
    for (uint16_t i = 0; i < 500; ++i) {
        const uint8_t value = prng.below(10);
        TEST_ASSERT_LESS_THAN_UINT8(10, value);
        hit[value] = true;
    }
    for (uint8_t i = 0; i < 10; ++i) {
        TEST_ASSERT_TRUE(hit[i]);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_triggers_only_on_active_step_onset);
    RUN_TEST(test_no_trigger_without_a_step_onset);
    RUN_TEST(test_channels_with_different_patterns_are_independent);
    RUN_TEST(test_clock_triggers_on_every_step_whatever_the_pattern);
    RUN_TEST(test_random_never_skips_at_zero);
    RUN_TEST(test_random_always_skips_at_ten);
    RUN_TEST(test_random_at_five_keeps_about_half);
    RUN_TEST(test_random_is_reproducible_from_one_run_to_the_next);
    RUN_TEST(test_a_different_seed_gives_a_different_run);
    RUN_TEST(test_the_draw_is_spent_only_when_a_step_actually_lands);
    RUN_TEST(test_counts_hold_still_until_the_next_update);
    RUN_TEST(test_an_onset_the_output_could_not_emit_is_not_lost);
    RUN_TEST(test_the_debt_stops_at_one_whole_step);
    RUN_TEST(test_an_out_of_range_channel_never_triggers);
    RUN_TEST(test_the_generator_never_settles_and_never_yields_zero);
    RUN_TEST(test_the_generator_matches_the_typescript_reference);
    RUN_TEST(test_the_generator_covers_the_whole_draw_range);
    return UNITY_END();
}
