#include <Arduino.h>
#include <libGravity.h>

#include <flexseq/PatternBank.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>
#include <flexseq/TriggerSequencer.h>

namespace {

flexseq::PatternBank patternBank;
flexseq::SequencerEngine engine;
flexseq::Transport transport(engine);
flexseq::TriggerSequencer triggers(patternBank, engine);

// Incremented in the uClock 96-PPQN output ISR (AttachIntHandler), drained in
// loop() so the engine is only mutated in main-loop context (no torn reads of
// its multi-byte state).
volatile uint16_t pendingTicks = 0;

void onOutputTick(uint32_t) {
    ++pendingTicks;
}

}  // namespace

void setup() {
    gravity.Init();

    // The engine reads the bank to shorten triplet steps (3 in one step's time).
    engine.setPatternBank(&patternBank);

    // Drive the master phase from the unified 96-PPQN output clock (internal
    // and external sources both surface here).
    gravity.clock.AttachIntHandler(onOutputTick);

    transport.start();  // global reset + run
}

void loop() {
    gravity.Process();

    // Atomically drain the ticks accumulated by the ISR, then advance once.
    uint16_t ticks;
    noInterrupts();
    ticks = pendingTicks;
    pendingTicks = 0;
    interrupts();

    if (ticks > 0) {
        transport.tick(ticks);

        // Emit a pulse on every channel that owes one. A ratchet step owes
        // several onsets; the output can only be re-armed once per drain, so a
        // pulse is emitted here and the remaining onsets land on the following
        // drains (ticks arrive far more often than steps).
        for (uint8_t ch = 0; ch < flexseq::SequencerEngine::CHANNEL_COUNT; ++ch) {
            if (triggers.triggerCount(ch) > 0) {
                gravity.outputs[ch].Trigger();
            }
        }
    }

    // Auto-off safeguard. gravity.Process() also does this, but libGravity's
    // loop uses an uninitialised index (libGravity.cpp), so drive it explicitly.
    for (uint8_t ch = 0; ch < flexseq::SequencerEngine::CHANNEL_COUNT; ++ch) {
        gravity.outputs[ch].Process();
    }
}
