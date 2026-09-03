#ifndef FLEXSEQ_TRIGGER_SEQUENCER_H
#define FLEXSEQ_TRIGGER_SEQUENCER_H

#include <stdint.h>

#include <flexseq/ChannelMode.h>
#include <flexseq/Pattern.h>
#include <flexseq/Prng.h>
#include <flexseq/SequencerEngine.h>

namespace flexseq {

class TriggerSequencer {
public:
    static constexpr uint8_t SKIP_DRAW_BOUND = 10;

    // A step carries at most six onsets: RATCHET_6 gives five sub-onsets plus
    // the step's own. A debt above that means the loop is more than one whole
    // step behind, and the older onsets have lost their musical meaning.
    static constexpr uint8_t MAX_OWED = 6;

    explicit TriggerSequencer(const SequencerEngine& engine)
        : engine_(engine), prng_() {
        for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
            counts_[ch] = 0;
            owed_[ch] = 0;
        }
    }

    void seed(uint16_t value) { prng_.seed(value); }

    void update() {
        for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
            counts_[ch] = decide(ch);
            const uint16_t total =
                static_cast<uint16_t>(owed_[ch]) + counts_[ch];
            owed_[ch] = total > MAX_OWED ? MAX_OWED : static_cast<uint8_t>(total);
        }
    }

    uint8_t triggerCount(uint8_t channel) const {
        if (channel >= SequencerEngine::CHANNEL_COUNT) {
            return 0;
        }
        return counts_[channel];
    }

    bool triggered(uint8_t channel) const { return triggerCount(channel) > 0; }

    // What is still owed to the output. An output can only be re-armed once per
    // main-loop pass, so a drain that carries several onsets leaves a debt.
    uint8_t owedTriggers(uint8_t channel) const {
        if (channel >= SequencerEngine::CHANNEL_COUNT) {
            return 0;
        }
        return owed_[channel];
    }

    bool takeTrigger(uint8_t channel) {
        if (channel >= SequencerEngine::CHANNEL_COUNT || owed_[channel] == 0) {
            return false;
        }
        --owed_[channel];
        return true;
    }

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
        const Pattern* pattern = engine_.patternForChannel(channel);
        if (pattern == nullptr) {
            return false;
        }
        const int8_t step = engine_.currentReadStep(channel);
        if (step < 0) {
            return false;
        }
        bool active = false;
        if (!pattern->readStep(static_cast<uint8_t>(step), active)) {
            return false;
        }
        return active;
    }

    const SequencerEngine& engine_;
    Prng prng_;
    uint8_t counts_[SequencerEngine::CHANNEL_COUNT];
    uint8_t owed_[SequencerEngine::CHANNEL_COUNT];
};

}  // namespace flexseq

#endif // FLEXSEQ_TRIGGER_SEQUENCER_H
