#include <flexseq/SequencerEngine.h>

namespace flexseq {

SequencerEngine::SequencerEngine()
    : phase_(0),
      running_(false) {
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        channels_[ch].selectedPattern = 0;
        channels_[ch].effectiveLength = DEFAULT_LENGTH;
        channels_[ch].ticksPerStep = TICKS_PER_SIXTEENTH;
        channels_[ch].localStep = 0;
        channels_[ch].acc = 0;
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
    }
}

void SequencerEngine::advance(uint16_t ticks) {
    if (!running_ || ticks == 0) {
        return;
    }

    // uint32_t wraps naturally at 2^32.
    phase_ += ticks;

    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        ChannelState& c = channels_[ch];
        c.acc = static_cast<uint16_t>(c.acc + ticks);
        while (c.acc >= c.ticksPerStep) {
            c.acc = static_cast<uint16_t>(c.acc - c.ticksPerStep);
            c.localStep = static_cast<uint8_t>((c.localStep + 1) % c.effectiveLength);
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
    if (c.acc >= ticks) {
        c.acc = static_cast<uint16_t>(c.acc % ticks);
    }
    return true;
}

int8_t SequencerEngine::effectiveStep(uint8_t channel) const {
    if (!validChannel(channel)) {
        return -1;
    }
    return static_cast<int8_t>(channels_[channel].localStep);
}

}  // namespace flexseq
