#include <Arduino.h>
#include <libGravity.h>

// -----------------------------------------------------------------------------
// SimAVR integration firmware
//
// This firmware is used only to validate the integration between the real
// libGravity output abstraction and the SimAVR VCD harness.
//
// It deliberately generates a deterministic CH1 trigger sequence.
// It does not access AVR GPIO registers directly.
//
// DO NOT upload this firmware to the physical Gravity module.
// -----------------------------------------------------------------------------

static constexpr uint8_t TEST_CHANNEL = 0;
static constexpr uint16_t HIGH_TIME_MS = 5;
static constexpr uint16_t LOW_TIME_MS = 95;

void setup() {
    gravity.Init();

    // Establish a known-safe initial state explicitly.
    gravity.outputs[TEST_CHANNEL].Low();
}

void loop() {
    // Generate a deterministic 5 ms trigger on CH1.
    gravity.outputs[TEST_CHANNEL].Trigger();

    delay(HIGH_TIME_MS);

    // Process the real DigitalOutput object. This uses libGravity's
    // trigger timing implementation.
    gravity.outputs[TEST_CHANNEL].Process();

    // Explicitly return to LOW so the test remains deterministic even if
    // the trigger-duration implementation changes.
    gravity.outputs[TEST_CHANNEL].Low();

    delay(LOW_TIME_MS);
}