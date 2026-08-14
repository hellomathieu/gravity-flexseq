#define private public

#include <clock.h>

#undef private

#include <unity.h>
#include <stdint.h>

#include "NeoHWSerial.h"

// -----------------------------------------------------------------------------
// Test state
// -----------------------------------------------------------------------------

static uint32_t external_callback_count = 0;

// -----------------------------------------------------------------------------
// Callbacks
// -----------------------------------------------------------------------------

static void on_external_clock() {
    external_callback_count++;
}

// -----------------------------------------------------------------------------
// Unity setup / teardown
// -----------------------------------------------------------------------------

void setUp() {
    external_callback_count = 0;
    NeoSerial.reset();
}

void tearDown() {
}

// -----------------------------------------------------------------------------
// Source selection
// -----------------------------------------------------------------------------

void test_clock_default_source_is_internal() {
    Clock clock;

    TEST_ASSERT_TRUE(clock.InternalSource());
    TEST_ASSERT_FALSE(clock.ExternalSource());
}

void test_clock_can_select_external_midi() {
    Clock clock;

    clock.SetSource(Clock::SOURCE_EXTERNAL_MIDI);

    TEST_ASSERT_TRUE(clock.ExternalSource());
    TEST_ASSERT_FALSE(clock.InternalSource());
}

void test_clock_can_switch_back_to_internal() {
    Clock clock;

    clock.SetSource(Clock::SOURCE_EXTERNAL_MIDI);

    TEST_ASSERT_TRUE(clock.ExternalSource());

    clock.SetSource(Clock::SOURCE_INTERNAL);

    TEST_ASSERT_TRUE(clock.InternalSource());
    TEST_ASSERT_FALSE(clock.ExternalSource());
}

// -----------------------------------------------------------------------------
// Investigation — SOURCE_LAST
//
// SOURCE_LAST is an enum sentinel, not a valid clock source.
//
// These tests document the current behaviour when the public SetSource()
// API nevertheless receives SOURCE_LAST.
// -----------------------------------------------------------------------------

void test_source_last_from_internal_preserves_internal_clock_mode() {
    Clock clock;

    // Establish a deterministic valid source first.
    clock.SetSource(Clock::SOURCE_INTERNAL);

    TEST_ASSERT_TRUE(clock.InternalSource());
    TEST_ASSERT_FALSE(clock.ExternalSource());

    clock.SetSource(Clock::SOURCE_LAST);

    // SOURCE_LAST must not silently turn the clock into an external source.
    TEST_ASSERT_TRUE(clock.InternalSource());
    TEST_ASSERT_FALSE(clock.ExternalSource());
}

void test_source_last_from_external_midi_preserves_external_clock_mode() {
    Clock clock;

    clock.SetSource(Clock::SOURCE_EXTERNAL_MIDI);

    TEST_ASSERT_TRUE(clock.ExternalSource());

    clock.SetSource(Clock::SOURCE_LAST);

    TEST_ASSERT_TRUE(clock.ExternalSource());
}

// -----------------------------------------------------------------------------
// MIDI Clock 0xF8
// -----------------------------------------------------------------------------

void test_midi_clock_invokes_external_callback() {
    Clock clock;

    clock.AttachExtHandler(on_external_clock);
    clock.SetSource(Clock::SOURCE_EXTERNAL_MIDI);

    NeoSerial.dispatch(MIDI_CLOCK);

    TEST_ASSERT_EQUAL_UINT32(1, external_callback_count);
}

void test_multiple_midi_clocks_invoke_callback_once_per_clock() {
    Clock clock;

    clock.AttachExtHandler(on_external_clock);
    clock.SetSource(Clock::SOURCE_EXTERNAL_MIDI);

    NeoSerial.dispatch(MIDI_CLOCK);
    NeoSerial.dispatch(MIDI_CLOCK);
    NeoSerial.dispatch(MIDI_CLOCK);
    NeoSerial.dispatch(MIDI_CLOCK);

    TEST_ASSERT_EQUAL_UINT32(4, external_callback_count);
}

// -----------------------------------------------------------------------------
// MIDI output
// -----------------------------------------------------------------------------

void test_send_midi_start_writes_midi_start() {
    Clock clock;

    Clock::sendMIDIStart();

    TEST_ASSERT_EQUAL_UINT8(
        MIDI_START,
        NeoSerial.lastWritten()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1,
        NeoSerial.writeCount()
    );
}

void test_send_midi_stop_writes_midi_stop() {
    Clock clock;

    Clock::sendMIDIStop();

    TEST_ASSERT_EQUAL_UINT8(
        MIDI_STOP,
        NeoSerial.lastWritten()
    );

    TEST_ASSERT_EQUAL_UINT32(
        1,
        NeoSerial.writeCount()
    );
}

// -----------------------------------------------------------------------------
// Clock Tick
// -----------------------------------------------------------------------------

void test_tick_is_callable() {
    Clock clock;

    clock.Tick();

    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Unity
// -----------------------------------------------------------------------------

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_clock_default_source_is_internal);
    RUN_TEST(test_clock_can_select_external_midi);
    RUN_TEST(test_clock_can_switch_back_to_internal);

    RUN_TEST(test_midi_clock_invokes_external_callback);
    RUN_TEST(test_multiple_midi_clocks_invoke_callback_once_per_clock);

    RUN_TEST(test_source_last_from_internal_preserves_internal_clock_mode);
    RUN_TEST(test_source_last_from_external_midi_preserves_external_clock_mode);

    RUN_TEST(test_send_midi_start_writes_midi_start);
    RUN_TEST(test_send_midi_stop_writes_midi_stop);

    RUN_TEST(test_tick_is_callable);

    return UNITY_END();
}