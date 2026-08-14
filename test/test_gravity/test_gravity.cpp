#include <stdint.h>
#include <unity.h>

#include <uClock/uClock.h>
#include "NeoHWSerial.h"

// -----------------------------------------------------------------------------
// NeoHWSerial mock
// -----------------------------------------------------------------------------

MockNeoHWSerial NeoSerial;

// -----------------------------------------------------------------------------
// uClock mock
// -----------------------------------------------------------------------------

namespace umodular {
namespace clock {

uClockClass::uClockClass()
    : clock_state(PAUSED) {
}

void uClockClass::init() {
    clock_state = PAUSED;
}

void uClockClass::setOutputPPQN(PPQNResolution resolution) {
    (void)resolution;
}

void uClockClass::setInputPPQN(PPQNResolution resolution) {
    (void)resolution;
}

void uClockClass::setClockMode(ClockMode mode) {
    clock_mode = mode;
}

uClockClass::ClockMode uClockClass::getClockMode() {
    return clock_mode;
}

void uClockClass::setTempo(float tempo) {
    (void)tempo;
}

void uClockClass::start() {
    clock_state = STARTING;
}

void uClockClass::stop() {
    clock_state = PAUSED;
}

void uClockClass::clockMe() {
    if (clock_state == STARTING) {
        clock_state = STARTED;
    }
}

} // namespace clock
} // namespace umodular

// -----------------------------------------------------------------------------
// Global uClock instance
// -----------------------------------------------------------------------------

umodular::clock::uClockClass uClock;

// -----------------------------------------------------------------------------
// Include REAL libGravity implementation
// -----------------------------------------------------------------------------

#include "../../.pio/libdeps/nanoatmega328/libGravity/src/libGravity.cpp"

// -----------------------------------------------------------------------------
// Test lifecycle
// -----------------------------------------------------------------------------

void setUp() {
    ArduinoMock::reset();
}

void tearDown() {
}

// -----------------------------------------------------------------------------
// Gravity::Init()
// -----------------------------------------------------------------------------

void test_gravity_init_configures_encoder_interrupt_registers() {
    Gravity gravityUnderTest;

    gravityUnderTest.Init();

    TEST_ASSERT_EQUAL_UINT8(
        0x06,
        ArduinoMock::pcicr()
    );

    TEST_ASSERT_EQUAL_UINT8(
        0x10,
        ArduinoMock::pcmsk2()
    );

    TEST_ASSERT_EQUAL_UINT8(
        0x08,
        ArduinoMock::pcmsk1()
    );
}

// -----------------------------------------------------------------------------
// G1 investigation
// -----------------------------------------------------------------------------
//
// IMPORTANT:
//
// We deliberately DO NOT call Gravity::Process() here.
//
// Gravity::Process() currently crashes in Encoder::Process() because of the
// independently confirmed Encoder initialization defect.
//
// This test isolates only the output-processing loop found in libGravity:
//
//     for (int i; i < OUTPUT_COUNT; i++) {
//         outputs[i].Process();
//     }
//
// The purpose is to investigate the uninitialized loop variable `i`.
//
// This is an INVESTIGATION TEST, not a functional test.
// -----------------------------------------------------------------------------

void test_gravity_uninitialized_output_loop_is_investigated() {
    Gravity gravityUnderTest;

    gravityUnderTest.Init();

    // Start a trigger on every output.
    for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; ++i) {
        gravityUnderTest.outputs[i].Trigger();

        TEST_ASSERT_TRUE(
            gravityUnderTest.outputs[i].On()
        );
    }

    ArduinoMock::advanceMillis(
        DEFAULT_TRIGGER_DURATION_MS
    );

    // Deliberately reproduce the production loop exactly.
    //
    // DO NOT "fix" i here. The purpose of this test is to observe the
    // consequences of the production implementation.
    for (int i; i < Gravity::OUTPUT_COUNT; ++i) {
        gravityUnderTest.outputs[i].Process();
    }

    // If execution reaches this point, inspect the output states.
    //
    // We do not make this a functional assertion yet because the loop itself
    // contains undefined behaviour. The test is currently intended to
    // characterize the runtime behaviour.
    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Control experiment
// -----------------------------------------------------------------------------
//
// Same operation, but with a correctly initialized loop variable.
//
// This is NOT a test of the production implementation.
// It is a control experiment proving that DigitalOutput processing itself
// behaves correctly when addressed with valid indices.
// -----------------------------------------------------------------------------

void test_gravity_initialized_output_loop_control() {
    Gravity gravityUnderTest;

    gravityUnderTest.Init();

    for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; ++i) {
        gravityUnderTest.outputs[i].Trigger();

        TEST_ASSERT_TRUE(
            gravityUnderTest.outputs[i].On()
        );
    }

    ArduinoMock::advanceMillis(
        DEFAULT_TRIGGER_DURATION_MS
    );

    for (int i = 0; i < Gravity::OUTPUT_COUNT; ++i) {
        gravityUnderTest.outputs[i].Process();
    }

    for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; ++i) {
        TEST_ASSERT_FALSE(
            gravityUnderTest.outputs[i].On()
        );
    }
}

// -----------------------------------------------------------------------------
// Unity
// -----------------------------------------------------------------------------

int main() {
    UNITY_BEGIN();

    RUN_TEST(
        test_gravity_init_configures_encoder_interrupt_registers
    );

    RUN_TEST(
        test_gravity_uninitialized_output_loop_is_investigated
    );

    RUN_TEST(
        test_gravity_initialized_output_loop_control
    );

    return UNITY_END();
}