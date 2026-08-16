#include <stdint.h>
#include <unity.h>

#include <flexseq/PatternBank.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/TriggerSequencer.h>

using flexseq::PatternBank;
using flexseq::Pattern;
using flexseq::SequencerEngine;
using flexseq::TriggerSequencer;

void setUp() {}
void tearDown() {}

static const uint16_t STEP = SequencerEngine::TICKS_PER_SIXTEENTH; // 24

void test_triggers_only_on_active_step_onset() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(bank, engine);

    // Channel 0 plays pattern 0 with steps 1 and 3 active, length 4.
    engine.setSelectedPattern(0, 0);
    engine.setEffectiveLength(0, 4);
    bank.getPattern(0)->writeStep(1, true);
    bank.getPattern(0)->writeStep(3, true);

    engine.start();

    engine.advance(STEP); // onto step 1 (active)
    TEST_ASSERT_TRUE(trig.triggered(0));

    engine.advance(STEP); // onto step 2 (inactive)
    TEST_ASSERT_FALSE(trig.triggered(0));

    engine.advance(STEP); // onto step 3 (active)
    TEST_ASSERT_TRUE(trig.triggered(0));

    engine.advance(STEP); // onto step 0 (inactive)
    TEST_ASSERT_FALSE(trig.triggered(0));
}

void test_no_trigger_without_a_step_onset() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(bank, engine);

    bank.getPattern(0)->writeStep(1, true);
    engine.setEffectiveLength(0, 4);
    engine.start();

    engine.advance(STEP - 1); // no boundary crossed
    TEST_ASSERT_FALSE(trig.triggered(0));
}

void test_shared_pattern_triggers_on_multiple_channels() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(bank, engine);

    // CH0 and CH1 both play pattern 0 (shared), step 1 active.
    engine.setSelectedPattern(0, 0);
    engine.setSelectedPattern(1, 0);
    engine.setEffectiveLength(0, 4);
    engine.setEffectiveLength(1, 4);
    bank.getPattern(0)->writeStep(1, true);

    engine.start();
    engine.advance(STEP); // both step onto step 1 (active)

    TEST_ASSERT_TRUE(trig.triggered(0));
    TEST_ASSERT_TRUE(trig.triggered(1));
}

void test_channels_with_different_patterns_are_independent() {
    PatternBank bank;
    SequencerEngine engine;
    TriggerSequencer trig(bank, engine);

    engine.setSelectedPattern(0, 0);
    engine.setSelectedPattern(1, 1);
    engine.setEffectiveLength(0, 4);
    engine.setEffectiveLength(1, 4);
    bank.getPattern(0)->writeStep(1, true); // only pattern 0 has step 1

    engine.start();
    engine.advance(STEP); // both onto step 1

    TEST_ASSERT_TRUE(trig.triggered(0));
    TEST_ASSERT_FALSE(trig.triggered(1));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_triggers_only_on_active_step_onset);
    RUN_TEST(test_no_trigger_without_a_step_onset);
    RUN_TEST(test_shared_pattern_triggers_on_multiple_channels);
    RUN_TEST(test_channels_with_different_patterns_are_independent);
    return UNITY_END();
}
