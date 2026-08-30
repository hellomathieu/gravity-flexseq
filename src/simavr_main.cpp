#include <Arduino.h>
#include <libGravity.h>

#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>
#include <flexseq/TriggerSequencer.h>

// -----------------------------------------------------------------------------
// SimAVR integration firmware
//
// Exercises the REAL FlexSeq domain (SequencerEngine + Transport +
// TriggerSequencer) driving the REAL libGravity DigitalOutput, so the whole
// engine -> effectiveStep -> pattern read -> trigger -> GPIO chain is observable
// in the SimAVR VCD harness.
//
// The clock is synthetic here (a fixed-period step loop) instead of uClock, so
// the CH1 waveform is deterministic. main.cpp uses the identical domain code but
// is driven by the real 96-PPQN clock.
//
// Pattern 0 has steps 0/4/8/12 active over a length of 16, played on CH1. With
// one 1/16 step per STEP_MS, CH1 therefore pulses once every 4 steps.
//
// DO NOT upload this firmware to the physical Gravity module.
// -----------------------------------------------------------------------------

namespace {

flexseq::SequencerEngine engine;
flexseq::Transport transport(engine);
flexseq::TriggerSequencer triggers(engine);

constexpr uint8_t TEST_CHANNEL = 0;
constexpr uint16_t STEP_TICKS = flexseq::SequencerEngine::TICKS_PER_SIXTEENTH; // 24
constexpr uint16_t HIGH_MS = 5;  // trigger pulse width
constexpr uint16_t STEP_MS = 10; // duration of one 1/16 step (=> 40 ms per 4 steps)

}  // namespace

void setup() {
    gravity.Init();
    gravity.outputs[TEST_CHANNEL].Low();

    // Preload a deterministic pattern: active steps 0, 4, 8, 12 (length 16).
    flexseq::Pattern* pattern = engine.instanceForChannel(TEST_CHANNEL);
    pattern->writeStep(0, true);
    pattern->writeStep(4, true);
    pattern->writeStep(8, true);
    pattern->writeStep(12, true);

    engine.setSelectedPattern(TEST_CHANNEL, 0);
    engine.setBaseLength(TEST_CHANNEL, 16);
    engine.setSubdiv(TEST_CHANNEL, -4);  // 1/16 steps (24 ticks) for this harness

    transport.start();  // global reset + run
}

void loop() {
    // Advance exactly one 1/16 step, then emit a trigger if we landed on an
    // active step of the channel's selected pattern.
    transport.tick(STEP_TICKS);
    if (triggers.triggered(TEST_CHANNEL)) {
        gravity.outputs[TEST_CHANNEL].Trigger();  // HIGH
    }

    delay(HIGH_MS);
    gravity.outputs[TEST_CHANNEL].Process();  // real trigger timing -> LOW after HIGH_MS
    gravity.outputs[TEST_CHANNEL].Low();      // deterministic LOW
    delay(STEP_MS - HIGH_MS);
}
