#include <stdint.h>
#include <unity.h>

#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>

using flexseq::SequencerEngine;
using flexseq::Transport;

void setUp() {}
void tearDown() {}

static const uint16_t STEP = SequencerEngine::TICKS_PER_SIXTEENTH; // 24

void test_tick_advances_only_while_running() {
    SequencerEngine e;
    Transport t(e);

    t.tick(STEP);
    TEST_ASSERT_EQUAL_UINT32(0, e.masterPhase()); // not running yet

    t.resume(); // run without reset
    t.tick(STEP);
    TEST_ASSERT_EQUAL_UINT32(STEP, e.masterPhase());
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
}

void test_start_resets_then_runs() {
    SequencerEngine e;
    Transport t(e);

    t.resume();
    t.tick(STEP * 5);
    TEST_ASSERT_EQUAL_UINT32(STEP * 5, e.masterPhase());

    t.start(); // MIDI Start = reset + run
    TEST_ASSERT_EQUAL_UINT32(0, e.masterPhase());
    TEST_ASSERT_TRUE(e.isRunning());
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
}

void test_stop_preserves_phase_then_resume_continues() {
    SequencerEngine e;
    Transport t(e);

    t.resume();
    t.tick(STEP * 3);
    t.stop();
    TEST_ASSERT_FALSE(e.isRunning());
    TEST_ASSERT_EQUAL_UINT32(STEP * 3, e.masterPhase());

    t.tick(STEP); // ignored while stopped
    TEST_ASSERT_EQUAL_UINT32(STEP * 3, e.masterPhase());

    t.resume(); // continue at current phase (no reset)
    t.tick(STEP);
    TEST_ASSERT_EQUAL_UINT32(STEP * 4, e.masterPhase());
    TEST_ASSERT_EQUAL_INT8(4, e.effectiveStep(0));
}

void test_reset_zeroes_phase_without_stopping() {
    SequencerEngine e;
    Transport t(e);

    t.resume();
    t.tick(STEP * 7);
    t.reset(); // external reset (global)
    TEST_ASSERT_EQUAL_UINT32(0, e.masterPhase());
    TEST_ASSERT_TRUE(e.isRunning());

    t.tick(STEP); // still running -> progresses
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
}

void test_batched_ticks_cross_multiple_steps() {
    SequencerEngine e;
    Transport t(e);

    t.resume();
    t.tick(STEP * 4); // 4 output ticks worth in one drain
    TEST_ASSERT_EQUAL_INT8(4, e.effectiveStep(0));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_tick_advances_only_while_running);
    RUN_TEST(test_start_resets_then_runs);
    RUN_TEST(test_stop_preserves_phase_then_resume_continues);
    RUN_TEST(test_reset_zeroes_phase_without_stopping);
    RUN_TEST(test_batched_ticks_cross_multiple_steps);
    return UNITY_END();
}
