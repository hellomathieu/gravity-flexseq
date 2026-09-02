#include <flexseq/SequencerEngine.h>

#include <flexseq/LengthCv.h>

#include <flexseq/Pattern.h>

namespace flexseq {


SequencerEngine::SequencerEngine()
    : phase_(0),
      beatTick_(0),
      running_(false),
      stepped_(0),
      armed_(0),
      modulated_(nullptr) {
    for (uint8_t s = 0; s < CV_SOURCE_COUNT; ++s) {
        cvInput_[s] = 0;
    }
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        onsets_[ch] = 0;
        for (uint8_t s = 0; s < CV_SOURCE_COUNT; ++s) {
            channels_[ch].cvTarget[s] = DEFAULT_CV_DESTINATION;
            channels_[ch].cvZone[s] = 0;
        }
        channels_[ch].selectedPattern = 0;
        channels_[ch].baseLength = DEFAULT_LENGTH;
        channels_[ch].subdiv = DEFAULT_SUBDIV;
        channels_[ch].ticksPerStep = subdivToTicks(DEFAULT_SUBDIV);
        channels_[ch].barLength = DEFAULT_BAR_LENGTH;
        channels_[ch].mode = DEFAULT_CHANNEL_MODE;
        channels_[ch].offset = 0;
        channels_[ch].skipChance = 0;
        channels_[ch].localStep = 0;
        channels_[ch].acc = 0;
        channels_[ch].pendingTicks = 0;
        refreshEffectiveLength(ch);
        refreshStepTiming(ch);
    }
}

uint16_t SequencerEngine::subOnsetTick(uint16_t stepTicks, uint8_t triggers,
                                      uint8_t k) {
    if (triggers <= 1) {
        return stepTicks;
    }
    return static_cast<uint16_t>(
        (static_cast<uint32_t>(stepTicks) * k) / triggers);
}

void SequencerEngine::refreshStepTiming(uint8_t channel, bool resetSubOnset) {
    ChannelState& c = channels_[channel];

    uint8_t code = RATCHET_NONE;
    if (c.mode == MODE_SEQ) {
        const Pattern* pattern = patternForChannel(channel);
        if (pattern != nullptr) {
            code = pattern->getRatchet(c.localStep);
        }
    }

    const uint8_t span = ratchetSpan(code);
    uint8_t triggers = ratchetTriggers(code);

    c.stepTicks = static_cast<uint16_t>(c.ticksPerStep * span);

    if (!ratchetFitsStep(code, c.ticksPerStep)) {
        triggers = 1;
    }

    c.triggers = triggers;
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
    beatTick_ = 0;
    armed_ = static_cast<uint8_t>((1u << CHANNEL_COUNT) - 1u);
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        channels_[ch].localStep = 0;
        channels_[ch].acc = 0;
        if (channels_[ch].pendingTicks > 0) {
            const uint16_t ticksPerStep = channels_[ch].pendingTicks;
            channels_[ch].pendingTicks = 0;
            applyTicks(ch, ticksPerStep);
        }
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
    uint16_t beat = static_cast<uint16_t>(beatTick_ + ticks);
    const bool beatCrossed = beat >= PPQN;
    while (beat >= PPQN) {
        beat = static_cast<uint16_t>(beat - PPQN);
    }
    beatTick_ = static_cast<uint8_t>(beat);

    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        ChannelState& c = channels_[ch];
        const uint8_t channelBit = static_cast<uint8_t>(1u << ch);
        if ((armed_ & channelBit) != 0) {
            armed_ = static_cast<uint8_t>(armed_ & ~channelBit);
            if (c.mode != MODE_CLOCK || c.offset == 0) {
                ++onsets_[ch];
            }
        }
        if (beatCrossed && c.pendingTicks > 0) {
            const uint16_t ticksPerStep = c.pendingTicks;
            c.pendingTicks = 0;
            applyTicks(ch, ticksPerStep);
            c.acc = alignedAcc(c.stepTicks, ticks);
        }
        c.acc = static_cast<uint16_t>(c.acc + ticks);

        while (true) {
            if (c.mode == MODE_CLOCK) {
                if (c.subOnset == 0 && c.offset > 0 && c.acc >= c.offset) {
                    c.subOnset = 1;
                    ++onsets_[ch];
                    continue;
                }
            } else if (c.subOnset + 1 < c.triggers) {
                const uint16_t nextAt = subOnsetTick(
                    c.stepTicks, c.triggers, static_cast<uint8_t>(c.subOnset + 1));
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
                applyCvAtStepBoundary(ch);
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

Pattern* SequencerEngine::instanceForChannel(uint8_t channel) {
    if (!validChannel(channel)) {
        return nullptr;
    }
    return &instances_[channel];
}

const Pattern* SequencerEngine::instanceForChannel(uint8_t channel) const {
    if (!validChannel(channel)) {
        return nullptr;
    }
    return &instances_[channel];
}

Pattern* SequencerEngine::patternForChannel(uint8_t channel) {
    if (!validChannel(channel)) {
        return nullptr;
    }
    if (modulated_ != nullptr
        && modulated_->loaded[channel] != ModulatedPatternState::NOT_MODULATED) {
        return &modulated_->pattern[channel];
    }
    return instanceForChannel(channel);
}

const Pattern* SequencerEngine::patternForChannel(uint8_t channel) const {
    if (!validChannel(channel)) {
        return nullptr;
    }
    if (modulated_ != nullptr
        && modulated_->loaded[channel] != ModulatedPatternState::NOT_MODULATED) {
        return &modulated_->pattern[channel];
    }
    return instanceForChannel(channel);
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

bool SequencerEngine::setBaseLength(uint8_t channel, uint8_t length) {
    if (!validChannel(channel) || length < MIN_LENGTH || length > MAX_LENGTH) {
        return false;
    }
    channels_[channel].baseLength = length;
    refreshEffectiveLength(channel);
    return true;
}

bool SequencerEngine::setBaseLengthFromStorage(uint8_t channel, uint8_t length) {
    if (!validChannel(channel) || length < MIN_LENGTH || length > MAX_STORED_LENGTH) {
        return false;
    }
    channels_[channel].baseLength = length;
    refreshEffectiveLength(channel);
    return true;
}

uint8_t SequencerEngine::getBaseLength(uint8_t channel) const {
    if (!validChannel(channel)) {
        return 0;
    }
    return channels_[channel].baseLength;
}

bool SequencerEngine::setCvInput(uint8_t source, int16_t value) {
    if (source >= CV_SOURCE_COUNT) {
        return false;
    }
    cvInput_[source] = value;
    return true;
}

int16_t SequencerEngine::getCvInput(uint8_t source) const {
    return source < CV_SOURCE_COUNT ? cvInput_[source] : 0;
}

bool SequencerEngine::setCvDestination(uint8_t channel, uint8_t source,
                                       CvDestination destination) {
    if (!validChannel(channel) || source >= CV_SOURCE_COUNT
        || static_cast<uint8_t>(destination) >= CV_DESTINATION_COUNT) {
        return false;
    }
    ChannelState& c = channels_[channel];
    if (c.cvTarget[source] == static_cast<uint8_t>(destination)) {
        return true;
    }
    c.cvTarget[source] = static_cast<uint8_t>(destination);
    c.cvZone[source] = 0;
    return true;
}

CvDestination SequencerEngine::getCvDestination(uint8_t channel, uint8_t source) const {
    if (!validChannel(channel) || source >= CV_SOURCE_COUNT) {
        return CV_DEST_NONE;
    }
    return static_cast<CvDestination>(channels_[channel].cvTarget[source]);
}

int8_t SequencerEngine::cvZoneSum(uint8_t channel, uint8_t destination) const {
    const ChannelState& c = channels_[channel];
    int16_t sum = 0;
    for (uint8_t source = 0; source < CV_SOURCE_COUNT; ++source) {
        if (c.cvTarget[source] == destination) {
            sum = static_cast<int16_t>(sum + c.cvZone[source]);
        }
    }
    return static_cast<int8_t>(sum);
}

int8_t SequencerEngine::lengthCvOffset(uint8_t channel) const {
    if (!validChannel(channel)) {
        return 0;
    }
    return cvZoneSum(channel, CV_DEST_LENGTH);
}

int8_t SequencerEngine::patternCvIndex(uint8_t channel) const {
    if (!validChannel(channel)) {
        return -1;
    }
    return static_cast<int8_t>(lengthcv::patternIndexFor(
        channels_[channel].selectedPattern, cvZoneSum(channel, CV_DEST_PATTERN)));
}

static_assert(CV_SOURCE_COUNT <= 8, "the source mask is one byte");
static_assert(SequencerEngine::CHANNEL_COUNT <= 8, "armed_ is one byte");

void SequencerEngine::applyCvResetEvents(uint8_t sourceMask) {
    sourceMask =
        static_cast<uint8_t>(sourceMask & ((1u << CV_SOURCE_COUNT) - 1u));
    if (sourceMask == 0) {
        return;
    }
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        ChannelState& c = channels_[ch];
        if (c.mode != MODE_SEQ && c.mode != MODE_CLOCK) {
            continue;
        }
        bool routed = false;
        for (uint8_t source = 0; source < CV_SOURCE_COUNT; ++source) {
            if ((sourceMask & (1u << source)) != 0 &&
                c.cvTarget[source] == CV_DEST_RESET) {
                routed = true;
            }
        }
        if (!routed) {
            continue;
        }
        c.localStep = 0;
        c.acc = 0;
        refreshStepTiming(ch);
        armed_ = static_cast<uint8_t>(armed_ | (1u << ch));
    }
}

void SequencerEngine::applyCvAtStepBoundary(uint8_t channel) {
    ChannelState& c = channels_[channel];
    if (c.mode != MODE_SEQ) {
        refreshEffectiveLength(channel);
        return;
    }
    for (uint8_t source = 0; source < CV_SOURCE_COUNT; ++source) {
        const uint8_t target = c.cvTarget[source];
        if (target != CV_DEST_LENGTH && target != CV_DEST_PATTERN) {
            continue;
        }
        c.cvZone[source] =
            lengthcv::zoneWithHysteresis(cvInput_[source], c.cvZone[source]);
    }
    refreshEffectiveLength(channel);
}

void SequencerEngine::refreshEffectiveLength(uint8_t channel) {
    ChannelState& c = channels_[channel];
    int16_t wanted = static_cast<int16_t>(c.baseLength) + lengthCvOffset(channel);
    if (wanted < static_cast<int16_t>(MIN_LENGTH)) {
        wanted = static_cast<int16_t>(MIN_LENGTH);
    }
    if (wanted > static_cast<int16_t>(MAX_LENGTH)) {
        wanted = static_cast<int16_t>(MAX_LENGTH);
    }
    c.effectiveLength = static_cast<uint8_t>(wanted);
    // Smoothed local phase: only fold when the position falls out of range.
    if (c.localStep >= c.effectiveLength) {
        c.localStep = static_cast<uint8_t>(c.localStep % c.effectiveLength);
        refreshStepTiming(channel);
    }
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
    scheduleTicks(channel, ticks);
    return true;
}

void SequencerEngine::scheduleTicks(uint8_t channel, uint16_t ticks) {
    ChannelState& c = channels_[channel];
    if (!running_ || onBeat()) {
        c.pendingTicks = 0;
        applyTicks(channel, ticks);
        return;
    }
    c.pendingTicks = ticks;
}

void SequencerEngine::applyTicks(uint8_t channel, uint16_t ticks) {
    ChannelState& c = channels_[channel];
    c.ticksPerStep = ticks;
    refreshStepTiming(channel);
    clampOffset(channel);
    if (c.acc >= c.stepTicks) {
        c.acc = static_cast<uint16_t>(c.acc % c.stepTicks);
    }
}

uint16_t SequencerEngine::alignedAcc(uint16_t stepTicks, uint16_t ticks) const {
    const uint16_t target = static_cast<uint16_t>(beatTick_ % stepTicks);
    const uint16_t back = static_cast<uint16_t>(ticks % stepTicks);
    return static_cast<uint16_t>((target + stepTicks - back) % stepTicks);
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
    scheduleTicks(channel, ticks);
    return true;
}

void SequencerEngine::clampOffset(uint8_t channel) {
    ChannelState& c = channels_[channel];
    const uint16_t limit = static_cast<uint16_t>(c.ticksPerStep - 1);
    if (c.offset > limit) {
        c.offset = static_cast<uint8_t>(limit);
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
    for (uint8_t source = 0; source < CV_SOURCE_COUNT; ++source) {
        c.cvZone[source] = 0;
    }
    refreshStepTiming(channel);
    if (c.acc >= c.stepTicks) {
        c.acc = static_cast<uint16_t>(c.acc % c.stepTicks);
    }
    refreshEffectiveLength(channel);
    return true;
}

uint8_t SequencerEngine::getOffset(uint8_t channel) const {
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
    c.offset = offset > MAX_OFFSET ? MAX_OFFSET : static_cast<uint8_t>(offset);
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

void SequencerEngine::setModulatedPatterns(ModulatedPatternState* state) {
    modulated_ = state;
}

ModulatedPatternState* SequencerEngine::modulatedPatterns() const {
    return modulated_;
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
