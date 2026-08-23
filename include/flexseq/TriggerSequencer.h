#ifndef FLEXSEQ_TRIGGER_SEQUENCER_H
#define FLEXSEQ_TRIGGER_SEQUENCER_H

#include <stdint.h>

#include <flexseq/ChannelMode.h>
#include <flexseq/PatternBank.h>
#include <flexseq/Prng.h>
#include <flexseq/SequencerEngine.h>

namespace flexseq {

class TriggerSequencer {
public:
    static constexpr uint8_t SKIP_DRAW_BOUND = 10;

    TriggerSequencer(const PatternBank& bank, const SequencerEngine& engine)
        : bank_(bank), engine_(engine), prng_() {
        for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
            counts_[ch] = 0;
        }
    }

    void seed(uint16_t value) { prng_.seed(value); }

    void update() {
        for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
            counts_[ch] = decide(ch);
        }
    }

    uint8_t triggerCount(uint8_t channel) const {
        if (channel >= SequencerEngine::CHANNEL_COUNT) {
            return 0;
        }
        return counts_[channel];
    }

    bool triggered(uint8_t channel) const { return triggerCount(channel) > 0; }

private:
    uint8_t decide(uint8_t channel) {
        const uint8_t onsets = engine_.onsetCount(channel);
        if (onsets == 0) {
            return 0;
        }
        switch (engine_.getChannelMode(channel)) {
            case MODE_CLOCK:
                return onsets;
            case MODE_RANDOM:
                return keptByChance(channel) ? onsets : 0;
            default:
                return activeStep(channel) ? onsets : 0;
        }
    }

    bool keptByChance(uint8_t channel) {
        const uint8_t draw = static_cast<uint8_t>(prng_.below(SKIP_DRAW_BOUND) + 1);
        return draw > engine_.getSkipChance(channel);
    }

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
    Prng prng_;
    uint8_t counts_[SequencerEngine::CHANNEL_COUNT];
};

}  // namespace flexseq

#endif // FLEXSEQ_TRIGGER_SEQUENCER_H
