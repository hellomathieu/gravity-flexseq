#ifndef FLEXSEQ_TRANSPORT_H
#define FLEXSEQ_TRANSPORT_H

#include <stdint.h>

#include <flexseq/SequencerEngine.h>

namespace flexseq {

// Transport — maps clock / transport EVENTS to the SequencerEngine, per the PRD
// (section Transport). It is the ONLY place that translates "what the clock
// source did" into engine progression, so it stays hardware-agnostic and unit
// testable. The libGravity glue (ISR callbacks, MIDI/EXT wiring) lives in the
// firmware entry point and only calls these methods.
//
// PRD mapping:
//   MIDI Start        -> start()   : global reset, then run
//   MIDI Continue     -> resume()  : run without reset (resume at current phase)
//   MIDI Stop         -> stop()    : stop without reset (phase preserved)
//   External Reset    -> reset()   : global reset (phase to 0), running unchanged
//   MIDI / Ext Clock  -> tick(n)   : advance the master phase by n 96-PPQN ticks
class Transport {
public:
    explicit Transport(SequencerEngine& engine) : engine_(engine) {}

    void start() {
        engine_.reset();
        engine_.start();
    }

    void resume() { engine_.start(); }

    void stop() { engine_.stop(); }

    void reset() { engine_.reset(); }

    // One 96-PPQN output tick per call by default. Batched draining is allowed
    // (n > 1) when several ticks accumulated between polls.
    void tick(uint16_t ticks = 1) { engine_.advance(ticks); }

private:
    SequencerEngine& engine_;
};

}  // namespace flexseq

#endif // FLEXSEQ_TRANSPORT_H
