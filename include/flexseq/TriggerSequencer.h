#ifndef FLEXSEQ_TRIGGER_SEQUENCER_H
#define FLEXSEQ_TRIGGER_SEQUENCER_H

#include <stdint.h>

#include <flexseq/PatternBank.h>
#include <flexseq/SequencerEngine.h>

namespace flexseq {

// TriggerSequencer — pure decision layer: given the shared PatternBank and the
// SequencerEngine, decides which channels must emit a trigger RIGHT NOW.
//
// A channel triggers on every ONSET that lands on an active step of its
// selected pattern. A plain step yields one onset; a ratchet step yields N (see
// Pattern's ratchet codes). Call triggered()/triggerCount() immediately after
// SequencerEngine::advance() (that is when the onset count is valid). The
// firmware turns the result into real DigitalOutput pulses; this class touches
// no hardware, so it is unit testable.
class TriggerSequencer {
public:
    TriggerSequencer(const PatternBank& bank, const SequencerEngine& engine)
        : bank_(bank), engine_(engine) {}

    // Number of trigger pulses the channel owes for the last advance().
    uint8_t triggerCount(uint8_t channel) const {
        const uint8_t onsets = engine_.onsetCount(channel);
        if (onsets == 0) {
            return 0;
        }
        return activeStep(channel) ? onsets : 0;
    }

    bool triggered(uint8_t channel) const { return triggerCount(channel) > 0; }

private:
    bool activeStep(uint8_t channel) const {
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

    const PatternBank& bank_;
    const SequencerEngine& engine_;
};

}  // namespace flexseq

#endif // FLEXSEQ_TRIGGER_SEQUENCER_H
