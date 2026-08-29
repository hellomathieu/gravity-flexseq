#include <stdint.h>
#include <unity.h>

#include <flexseq/Pattern.h>
#include <flexseq/PatternBank.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Subdiv.h>

using flexseq::PatternBank;
using flexseq::SequencerEngine;

void setUp() {}
void tearDown() {}

namespace {

constexpr int16_t MULTIPLICATIONS[] = {-24, -16, -12, -8, -6, -4, -3, -2};
constexpr int16_t DIVISIONS[] = {2, 3, 4, 6, 8};

void advanceBy(SequencerEngine& e, uint16_t ticks) {
    for (uint16_t i = 0; i < ticks; ++i) {
        e.advance(1);
    }
}

// First step onset of each channel strictly after `from`, advancing tick by tick.
void firstOnsets(SequencerEngine& e, uint32_t from, uint16_t window,
                 int32_t& first0, int32_t& first1) {
    first0 = -1;
    first1 = -1;
    for (uint16_t i = 1; i <= window; ++i) {
        e.advance(1);
        const uint32_t now = from + i;
        if (first0 < 0 && e.hasStepped(0)) {
            first0 = static_cast<int32_t>(now);
        }
        if (first1 < 0 && e.hasStepped(1)) {
            first1 = static_cast<int32_t>(now);
        }
    }
}

}  // namespace

void test_a_rate_change_waits_for_the_next_beat_while_running() {
    SequencerEngine e;
    e.start();
    advanceBy(e, 50);

    TEST_ASSERT_TRUE(e.setSubdiv(0, -2));
    TEST_ASSERT_EQUAL_INT16(-2, e.getSubdiv(0));
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(96, e.getTicksPerStep(0),
        "la cadence choisie ne joue pas encore");

    advanceBy(e, 45);
    TEST_ASSERT_EQUAL_UINT16(96, e.getTicksPerStep(0));

    e.advance(1);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(48, e.getTicksPerStep(0),
        "elle prend effet sur le temps");
}

void test_a_rate_change_applies_at_once_while_stopped() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setSubdiv(0, -2));
    TEST_ASSERT_EQUAL_UINT16(48, e.getTicksPerStep(0));
}

void test_a_rate_change_applies_at_once_when_already_on_a_beat() {
    SequencerEngine e;
    e.start();
    TEST_ASSERT_TRUE(e.setSubdiv(0, -2));
    TEST_ASSERT_EQUAL_UINT16(48, e.getTicksPerStep(0));
    advanceBy(e, 96);
    TEST_ASSERT_TRUE(e.setSubdiv(0, 1));
    TEST_ASSERT_EQUAL_UINT16(96, e.getTicksPerStep(0));
}

void test_the_last_selection_before_the_beat_is_the_one_that_applies() {
    SequencerEngine e;
    e.start();
    advanceBy(e, 40);
    e.setSubdiv(0, -2);
    e.setSubdiv(0, -4);
    TEST_ASSERT_EQUAL_UINT16(96, e.getTicksPerStep(0));
    advanceBy(e, 56);
    TEST_ASSERT_EQUAL_UINT16(24, e.getTicksPerStep(0));
    TEST_ASSERT_EQUAL_INT16(-4, e.getSubdiv(0));
}

void test_two_channels_agree_after_a_round_trip_through_x3() {
    SequencerEngine e;
    e.start();
    advanceBy(e, 150);
    e.setSubdiv(0, -3);
    advanceBy(e, 110);
    e.setSubdiv(0, 1);
    advanceBy(e, 140);

    int32_t first0 = -1;
    int32_t first1 = -1;
    firstOnsets(e, 400, 200, first0, first1);
    TEST_ASSERT_TRUE(first0 > 0);
    TEST_ASSERT_TRUE(first1 > 0);
    TEST_ASSERT_EQUAL_INT32_MESSAGE(first1, first0,
        "deux channels de meme cadence tombent ensemble");
    TEST_ASSERT_EQUAL_INT32(0, first0 % 96);
}

void test_every_multiplication_returns_to_the_grid() {
    for (uint8_t i = 0; i < sizeof(MULTIPLICATIONS) / sizeof(int16_t); ++i) {
        SequencerEngine e;
        e.start();
        advanceBy(e, 137);
        TEST_ASSERT_TRUE(e.setSubdiv(0, MULTIPLICATIONS[i]));
        advanceBy(e, 474);
        TEST_ASSERT_TRUE(e.setSubdiv(0, 1));
        advanceBy(e, 189);

        int32_t first0 = -1;
        int32_t first1 = -1;
        firstOnsets(e, 800, 200, first0, first1);
        TEST_ASSERT_TRUE(first0 > 0);
        TEST_ASSERT_EQUAL_INT32(first1, first0);
    }
}

void test_every_division_returns_to_the_grid() {
    for (uint8_t i = 0; i < sizeof(DIVISIONS) / sizeof(int16_t); ++i) {
        SequencerEngine e;
        e.start();
        advanceBy(e, 137);
        TEST_ASSERT_TRUE(e.setSubdiv(0, DIVISIONS[i]));
        advanceBy(e, 1434);
        TEST_ASSERT_TRUE(e.setSubdiv(0, 1));
        advanceBy(e, 129);

        int32_t first0 = -1;
        int32_t first1 = -1;
        firstOnsets(e, 1700, 300, first0, first1);
        TEST_ASSERT_TRUE(first0 > 0);
        TEST_ASSERT_EQUAL_INT32(first1, first0);
    }
}

void test_a_global_reset_applies_the_pending_rate_instead_of_dropping_it() {
    SequencerEngine e;
    e.start();
    advanceBy(e, 40);
    TEST_ASSERT_TRUE(e.setSubdiv(0, -2));
    TEST_ASSERT_EQUAL_UINT16(96, e.getTicksPerStep(0));

    e.reset();
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(48, e.getTicksPerStep(0),
        "un reset global applique la cadence en attente");
    TEST_ASSERT_EQUAL_INT16(-2, e.getSubdiv(0));

    for (uint16_t i = 1; i <= 200; ++i) {
        e.advance(1);
        if (e.hasStepped(0)) {
            TEST_ASSERT_EQUAL_UINT32(0, i % 48);
        }
    }
}

void test_a_drained_burst_of_ticks_does_not_lose_the_pending_rate() {
    SequencerEngine e;
    e.start();
    e.advance(90);
    TEST_ASSERT_TRUE(e.setSubdiv(0, -2));
    TEST_ASSERT_EQUAL_UINT16(96, e.getTicksPerStep(0));
    e.advance(9);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(48, e.getTicksPerStep(0),
        "un passage qui draine plusieurs ticks ne perd pas la cadence");

    for (uint16_t i = 1; i <= 300; ++i) {
        e.advance(1);
        const uint32_t now = 99 + i;
        if (now > 150 && e.hasStepped(0)) {
            TEST_ASSERT_EQUAL_UINT32(0, now % 48);
        }
    }
}

/*
 * No other edit shifts a channel
 */

namespace {

struct DriftRig {
    PatternBank bank;
    SequencerEngine engine;

    DriftRig() {
        engine.setPatternBank(&bank);
        for (uint8_t ch = 0; ch < 2; ++ch) {
            engine.setChannelMode(ch, flexseq::MODE_SEQ);
        }
        engine.setSelectedPattern(0, 0);
        engine.setSelectedPattern(1, 1);
        engine.start();
        advanceBy(engine, 137);
    }

    int32_t drift() {
        int32_t first0 = -1;
        int32_t first1 = -1;
        for (uint16_t i = 1; i <= 763; ++i) {
            engine.advance(1);
            const uint32_t now = 137 + i;
            if (now <= 600) {
                continue;
            }
            if (first0 < 0 && engine.hasStepped(0)) {
                first0 = static_cast<int32_t>(now);
            }
            if (first1 < 0 && engine.hasStepped(1)) {
                first1 = static_cast<int32_t>(now);
            }
        }
        TEST_ASSERT_TRUE(first0 > 0);
        TEST_ASSERT_TRUE(first1 > 0);
        return first0 - first1;
    }
};

}  // namespace

void test_a_length_edit_does_not_shift_the_channel() {
    {
        DriftRig r;
        r.engine.setBaseLength(0, 8);
        TEST_ASSERT_EQUAL_INT32(0, r.drift());
    }
    {
        DriftRig r;
        r.engine.setBaseLength(0, 1);
        TEST_ASSERT_EQUAL_INT32(0, r.drift());
    }
}

void test_selecting_another_pattern_does_not_shift_the_channel() {
    DriftRig r;
    r.engine.setSelectedPattern(0, 5);
    TEST_ASSERT_EQUAL_INT32(0, r.drift());
}

void test_a_mode_change_does_not_shift_the_channel() {
    DriftRig r;
    r.engine.setChannelMode(0, flexseq::MODE_CLOCK);
    TEST_ASSERT_EQUAL_INT32(0, r.drift());
}

void test_editing_the_ratchet_of_the_current_step_does_not_shift_the_channel() {
    {
        DriftRig r;
        const int8_t step = r.engine.effectiveStep(0);
        r.engine.instanceForChannel(0)->setRatchet(static_cast<uint8_t>(step), flexseq::RATCHET_TRIPLET);
        r.engine.refreshTiming(0);
        TEST_ASSERT_EQUAL_INT32(0, r.drift());
    }
    {
        DriftRig r;
        const int8_t step = r.engine.effectiveStep(0);
        r.engine.instanceForChannel(0)->setRatchet(static_cast<uint8_t>(step), flexseq::RATCHET_NONE);
        r.engine.refreshTiming(0);
        TEST_ASSERT_EQUAL_INT32(0, r.drift());
    }
}

void test_a_set_ticks_per_step_round_trip_returns_to_the_grid() {
    const uint16_t mids[] = {24, 32, 48, 192, 384};
    for (uint8_t i = 0; i < sizeof(mids) / sizeof(uint16_t); ++i) {
        SequencerEngine e;
        e.start();
        advanceBy(e, 137);
        TEST_ASSERT_TRUE(e.setTicksPerStep(0, mids[i]));
        advanceBy(e, 474);
        TEST_ASSERT_TRUE(e.setTicksPerStep(0, 96));
        advanceBy(e, 289);

        int32_t first0 = -1;
        int32_t first1 = -1;
        firstOnsets(e, 900, 300, first0, first1);
        TEST_ASSERT_TRUE(first0 > 0);
        TEST_ASSERT_EQUAL_INT32(first1, first0);
    }
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_a_rate_change_waits_for_the_next_beat_while_running);
    RUN_TEST(test_a_rate_change_applies_at_once_while_stopped);
    RUN_TEST(test_a_rate_change_applies_at_once_when_already_on_a_beat);
    RUN_TEST(test_the_last_selection_before_the_beat_is_the_one_that_applies);
    RUN_TEST(test_two_channels_agree_after_a_round_trip_through_x3);
    RUN_TEST(test_every_multiplication_returns_to_the_grid);
    RUN_TEST(test_every_division_returns_to_the_grid);
    RUN_TEST(test_a_global_reset_applies_the_pending_rate_instead_of_dropping_it);
    RUN_TEST(test_a_drained_burst_of_ticks_does_not_lose_the_pending_rate);

    RUN_TEST(test_a_length_edit_does_not_shift_the_channel);
    RUN_TEST(test_selecting_another_pattern_does_not_shift_the_channel);
    RUN_TEST(test_a_mode_change_does_not_shift_the_channel);
    RUN_TEST(test_editing_the_ratchet_of_the_current_step_does_not_shift_the_channel);
    RUN_TEST(test_a_set_ticks_per_step_round_trip_returns_to_the_grid);

    return UNITY_END();
}
