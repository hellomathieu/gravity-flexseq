#include <flexseq/SequencerEngine.h>

#include <flexseq/Pattern.h>
#include <flexseq/PatternBank.h>

namespace flexseq {

SequencerEngine::SequencerEngine()
    : bank_(nullptr),
      phase_(0),
      running_(false),
      stepped_(0) {
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        onsets_[ch] = 0;
        channels_[ch].selectedPattern = 0;
        channels_[ch].effectiveLength = DEFAULT_LENGTH;
        channels_[ch].subdiv = DEFAULT_SUBDIV;
        channels_[ch].ticksPerStep = subdivToTicks(DEFAULT_SUBDIV);
        channels_[ch].barLength = DEFAULT_BAR_LENGTH;
        channels_[ch].mode = DEFAULT_CHANNEL_MODE;
        channels_[ch].offset = 0;
        channels_[ch].skipChance = 0;
        channels_[ch].localStep = 0;
        channels_[ch].acc = 0;
        refreshStepTiming(ch);
    }
}

void SequencerEngine::setPatternBank(const PatternBank* bank) {
    bank_ = bank;
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        refreshStepTiming(ch);
    }
}

void SequencerEngine::refreshStepTiming(uint8_t channel, bool resetSubOnset) {
    ChannelState& c = channels_[channel];

    uint8_t code = RATCHET_NONE;
    if (bank_ != nullptr && c.mode == MODE_SEQ) {
        const Pattern* pattern = bank_->getPattern(c.selectedPattern);
        if (pattern != nullptr) {
            code = pattern->getRatchet(c.localStep);
        }
    }

    const uint8_t span = ratchetSpan(code);
    uint8_t triggers = ratchetTriggers(code);

    c.stepTicks = static_cast<uint16_t>(c.ticksPerStep * span);

    // A sub-slot must be a whole number of ticks; otherwise the ratchet is
    // silently ignored for this combination (documented fallback, no drift).
    if (triggers > 1 && c.stepTicks % triggers != 0) {
        triggers = 1;
    }

    c.triggers = triggers;
    c.slotTicks = static_cast<uint16_t>(c.stepTicks / triggers);
    if (resetSubOnset) {
        c.subOnset = 0;
    } else if (c.subOnset >= c.triggers) {
        // Keep the onsets already emitted in this step: an edit must not make
        // them fire again.
        c.subOnset = static_cast<uint8_t>(c.triggers - 1);
    }
}

void SequencerEngine::refreshTiming(uint8_t channel) {
    if (!validChannel(channel)) {
        return;
    }
    refreshStepTiming(channel, false);
}

void SequencerEngine::refreshTiming() {
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        refreshStepTiming(ch, false);
    }
}

uint32_t SequencerEngine::masterPhase() const {
    return phase_;
}

bool SequencerEngine::isRunning() const {
    return running_;
}

void SequencerEngine::start() {
    running_ = true;
}

void SequencerEngine::stop() {
    running_ = false;
}

void SequencerEngine::reset() {
    phase_ = 0;
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        channels_[ch].localStep = 0;
        channels_[ch].acc = 0;
        refreshStepTiming(ch);
    }
}

void SequencerEngine::advance(uint16_t ticks) {
    stepped_ = 0; // report only the crossings of THIS advance()
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        onsets_[ch] = 0;
    }
    if (!running_ || ticks == 0) {
        return;
    }

    // uint32_t wraps naturally at 2^32.
    phase_ += ticks;

    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        ChannelState& c = channels_[ch];
        c.acc = static_cast<uint16_t>(c.acc + ticks);

        while (true) {
            if (c.mode == MODE_CLOCK) {
                if (c.subOnset == 0 && c.offset > 0 && c.acc >= c.offset) {
                    c.subOnset = 1;
                    ++onsets_[ch];
                    continue;
                }
            } else if (c.subOnset + 1 < c.triggers) {
                const uint16_t nextAt =
                    static_cast<uint16_t>(c.slotTicks * (c.subOnset + 1));
                if (c.acc >= nextAt) {
                    ++c.subOnset;
                    ++onsets_[ch];
                    continue;
                }
            }

            if (c.stepTicks > 0 && c.acc >= c.stepTicks) {
                c.acc = static_cast<uint16_t>(c.acc - c.stepTicks);
                c.localStep = static_cast<uint8_t>((c.localStep + 1) % c.effectiveLength);
                stepped_ = static_cast<uint8_t>(stepped_ | (1u << ch));
                refreshStepTiming(ch); // new step -> new duration / trigger count
                if (c.mode != MODE_CLOCK || c.offset == 0) {
                    ++onsets_[ch];     // the step's own onset
                }
                continue;
            }

            break;
        }
    }
}

int8_t SequencerEngine::getSelectedPattern(uint8_t channel) const {
    if (!validChannel(channel)) {
        return -1;
    }
    return static_cast<int8_t>(channels_[channel].selectedPattern);
}

bool SequencerEngine::setSelectedPattern(uint8_t channel, uint8_t index) {
    if (!validChannel(channel) || index >= PATTERN_COUNT) {
        return false;
    }
    channels_[channel].selectedPattern = index;
    refreshStepTiming(channel);
    return true;
}

uint8_t SequencerEngine::getEffectiveLength(uint8_t channel) const {
    if (!validChannel(channel)) {
        return 0;
    }
    return channels_[channel].effectiveLength;
}

bool SequencerEngine::setEffectiveLength(uint8_t channel, uint8_t length) {
    if (!validChannel(channel) || length < MIN_LENGTH || length > MAX_LENGTH) {
        return false;
    }
    ChannelState& c = channels_[channel];
    c.effectiveLength = length;
    // Smoothed local phase: only fold when the position falls out of range.
    if (c.localStep >= length) {
        c.localStep = static_cast<uint8_t>(c.localStep % length);
        refreshStepTiming(channel);
    }
    return true;
}

uint16_t SequencerEngine::getTicksPerStep(uint8_t channel) const {
    if (!validChannel(channel)) {
        return 0;
    }
    return channels_[channel].ticksPerStep;
}

bool SequencerEngine::setTicksPerStep(uint8_t channel, uint16_t ticks) {
    if (!validChannel(channel) || ticks < 1) {
        return false;
    }
    ChannelState& c = channels_[channel];
    c.ticksPerStep = ticks;
    refreshStepTiming(channel);
    clampOffset(channel);
    if (c.acc >= c.stepTicks) {
        c.acc = static_cast<uint16_t>(c.acc % c.stepTicks);
    }
    return true;
}

int16_t SequencerEngine::getSubdiv(uint8_t channel) const {
    if (!validChannel(channel)) {
        return 0;
    }
    return channels_[channel].subdiv;
}

bool SequencerEngine::setSubdiv(uint8_t channel, int16_t subdiv) {
    if (!validChannel(channel)) {
        return false;
    }
    const uint16_t ticks = subdivToTicks(subdiv);
    if (ticks < 1) {
        return false;
    }
    ChannelState& c = channels_[channel];
    c.subdiv = subdiv;
    c.ticksPerStep = ticks;
    refreshStepTiming(channel);
    clampOffset(channel);
    if (c.acc >= c.stepTicks) {
        c.acc = static_cast<uint16_t>(c.acc % c.stepTicks);
    }
    return true;
}

void SequencerEngine::clampOffset(uint8_t channel) {
    ChannelState& c = channels_[channel];
    if (c.offset >= c.ticksPerStep) {
        c.offset = static_cast<uint16_t>(c.ticksPerStep - 1);
    }
    if (c.offset > MAX_OFFSET) {
        c.offset = MAX_OFFSET;
    }
}

ChannelMode SequencerEngine::getChannelMode(uint8_t channel) const {
    if (!validChannel(channel)) {
        return DEFAULT_CHANNEL_MODE;
    }
    return static_cast<ChannelMode>(channels_[channel].mode);
}

bool SequencerEngine::setChannelMode(uint8_t channel, ChannelMode mode) {
    if (!validChannel(channel) || static_cast<uint8_t>(mode) >= CHANNEL_MODE_COUNT) {
        return false;
    }
    ChannelState& c = channels_[channel];
    if (c.mode == static_cast<uint8_t>(mode)) {
        return true;
    }
    c.mode = static_cast<uint8_t>(mode);
    refreshStepTiming(channel);
    if (c.acc >= c.stepTicks) {
        c.acc = static_cast<uint16_t>(c.acc % c.stepTicks);
    }
    return true;
}

uint16_t SequencerEngine::getOffset(uint8_t channel) const {
    if (!validChannel(channel)) {
        return 0;
    }
    return channels_[channel].offset;
}

bool SequencerEngine::setOffset(uint8_t channel, uint16_t offset) {
    if (!validChannel(channel)) {
        return false;
    }
    ChannelState& c = channels_[channel];
    c.offset = offset;
    clampOffset(channel);
    return true;
}

uint8_t SequencerEngine::getSkipChance(uint8_t channel) const {
    if (!validChannel(channel)) {
        return 0;
    }
    return channels_[channel].skipChance;
}

bool SequencerEngine::setSkipChance(uint8_t channel, uint8_t tenths) {
    if (!validChannel(channel) || tenths > MAX_SKIP_CHANCE) {
        return false;
    }
    channels_[channel].skipChance = tenths;
    return true;
}

int8_t SequencerEngine::getBarLength(uint8_t channel) const {
    if (!validChannel(channel)) {
        return -1;
    }
    return static_cast<int8_t>(channels_[channel].barLength);
}

bool SequencerEngine::setBarLength(uint8_t channel, uint8_t steps) {
    if (!validChannel(channel)) {
        return false;
    }
    // Only separations that divide a row of 12 evenly, so a bar never lands in
    // the middle of a row wrap.
    if (steps != BAR_NONE && steps != 2 && steps != 3 && steps != 4 && steps != 6) {
        return false;
    }
    channels_[channel].barLength = steps;
    return true;
}

uint16_t SequencerEngine::currentStepTicks(uint8_t channel) const {
    if (!validChannel(channel)) {
        return 0;
    }
    return channels_[channel].stepTicks;
}

uint8_t SequencerEngine::currentStepTriggers(uint8_t channel) const {
    if (!validChannel(channel)) {
        return 0;
    }
    return channels_[channel].triggers;
}

int8_t SequencerEngine::effectiveStep(uint8_t channel) const {
    if (!validChannel(channel)) {
        return -1;
    }
    return static_cast<int8_t>(channels_[channel].localStep);
}

bool SequencerEngine::hasStepped(uint8_t channel) const {
    if (!validChannel(channel)) {
        return false;
    }
    return (stepped_ & (1u << channel)) != 0;
}

uint8_t SequencerEngine::onsetCount(uint8_t channel) const {
    if (!validChannel(channel)) {
        return 0;
    }
    return onsets_[channel];
}

}  // namespace flexseq
