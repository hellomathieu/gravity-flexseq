#ifndef FLEXSEQ_TRIGGER_SEQUENCER_H
#define FLEXSEQ_TRIGGER_SEQUENCER_H

#include <stdint.h>

#include <flexseq/PatternBank.h>
#include <flexseq/SequencerEngine.h>

namespace flexseq {

// TriggerSequencer — pure decision layer: given the shared PatternBank and the
// SequencerEngine, decides which channels must emit a trigger RIGHT NOW.
//
// A channel triggers when it just stepped onto an active step of its selected
// pattern. Call triggered() immediately after SequencerEngine::advance() (that
// is when hasStepped() is valid). The firmware turns a true result into a real
// DigitalOutput pulse; this class touches no hardware, so it is unit testable.
class TriggerSequencer {
public:
    TriggerSequencer(const PatternBank& bank, const SequencerEngine& engine)
        : bank_(bank), engine_(engine) {}

    bool triggered(uint8_t channel) const {
        if (!engine_.hasStepped(channel)) {
            return false;
        }
        const int8_t patternIndex = engine_.getSelectedPattern(channel);
        if (patternIndex < 0) {
            return false;
        }
        const Pattern* pattern = bank_.getPattern(static_cast<uint8_t>(patternIndex));
        if (pattern == nullptr) {
            return false;
        }
        const int8_t step = engine_.effectiveStep(channel);
        if (step < 0) {
            return false;
        }
        bool active = false;
        if (!pattern->readStep(static_cast<uint8_t>(step), active)) {
            return false;
        }
        return active;
    }

private:
    const PatternBank& bank_;
    const SequencerEngine& engine_;
};

}  // namespace flexseq

#endif // FLEXSEQ_TRIGGER_SEQUENCER_H
