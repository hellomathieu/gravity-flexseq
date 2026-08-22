#include <flexseq/Persistence.h>

namespace flexseq {

namespace {

uint8_t sanitisedRatchetNibbles(uint8_t value) {
    const uint8_t low = static_cast<uint8_t>(value & 0x0F);
    const uint8_t high = static_cast<uint8_t>((value >> 4) & 0x0F);
    const uint8_t keptLow = isValidRatchet(low) ? low : RATCHET_NONE;
    const uint8_t keptHigh = isValidRatchet(high) ? high : RATCHET_NONE;
    return static_cast<uint8_t>((keptHigh << 4) | keptLow);
}

}  // namespace

uint8_t PersistentImage::patternByte(uint8_t pattern, uint8_t offset) const {
    const Pattern* p = bank_.getPattern(pattern);
    if (p == nullptr) {
        return 0;
    }
    if (offset < persist::PATTERN_STEP_BYTES) {
        uint8_t packed = 0;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            const uint8_t step = static_cast<uint8_t>(offset * 8 + bit);
            bool active = false;
            if (p->readStep(step, active) && active) {
                packed = static_cast<uint8_t>(packed | (1u << bit));
            }
        }
        return packed;
    }
    const uint8_t pair = static_cast<uint8_t>(offset - persist::PATTERN_STEP_BYTES);
    const uint8_t low = p->getRatchet(static_cast<uint8_t>(pair * 2));
    const uint8_t high = p->getRatchet(static_cast<uint8_t>(pair * 2 + 1));
    return static_cast<uint8_t>(((high & 0x0F) << 4) | (low & 0x0F));
}

void PersistentImage::applyPatternByte(uint8_t pattern, uint8_t offset, uint8_t value) {
    Pattern* p = bank_.getPattern(pattern);
    if (p == nullptr) {
        return;
    }
    if (offset < persist::PATTERN_STEP_BYTES) {
        for (uint8_t bit = 0; bit < 8; ++bit) {
            const uint8_t step = static_cast<uint8_t>(offset * 8 + bit);
            p->writeStep(step, (value & (1u << bit)) != 0);
        }
        return;
    }
    const uint8_t kept = sanitisedRatchetNibbles(value);
    const uint8_t pair = static_cast<uint8_t>(offset - persist::PATTERN_STEP_BYTES);
    p->setRatchet(static_cast<uint8_t>(pair * 2), static_cast<uint8_t>(kept & 0x0F));
    p->setRatchet(static_cast<uint8_t>(pair * 2 + 1), static_cast<uint8_t>((kept >> 4) & 0x0F));
}

uint8_t PersistentImage::channelByte(uint8_t channel, uint8_t offset) const {
    switch (offset) {
        case 0: {
            const int8_t selected = engine_.getSelectedPattern(channel);
            return selected < 0 ? 0 : static_cast<uint8_t>(selected);
        }
        case 1:
            return engine_.getEffectiveLength(channel);
        case 2: {
            const int8_t index = subdivIndexOf(engine_.getSubdiv(channel));
            return index < 0 ? DEFAULT_SUBDIV_INDEX : static_cast<uint8_t>(index);
        }
        case 3: {
            const int8_t bar = engine_.getBarLength(channel);
            return bar < 0 ? 0 : static_cast<uint8_t>(bar);
        }
        default:
            return 0;
    }
}

void PersistentImage::applyChannelByte(uint8_t channel, uint8_t offset, uint8_t value) {
    switch (offset) {
        case 0:
            engine_.setSelectedPattern(channel, value);
            break;
        case 1:
            engine_.setEffectiveLength(channel, value);
            break;
        case 2:
            engine_.setSubdiv(channel, subdivAtIndex(value));
            break;
        case 3:
            engine_.setBarLength(channel, value);
            break;
        default:
            break;
    }
}

uint8_t PersistentImage::byteAt(uint16_t index) const {
    if (index == persist::HEADER_OFFSET) {
        return persist::FORMAT_VERSION;
    }
    if (index < persist::CHANNELS_OFFSET) {
        const uint16_t rel = static_cast<uint16_t>(index - persist::PATTERNS_OFFSET);
        return patternByte(static_cast<uint8_t>(rel / persist::PATTERN_RECORD),
                           static_cast<uint8_t>(rel % persist::PATTERN_RECORD));
    }
    if (index < persist::GLOBAL_OFFSET) {
        const uint16_t rel = static_cast<uint16_t>(index - persist::CHANNELS_OFFSET);
        return channelByte(static_cast<uint8_t>(rel / persist::CHANNEL_RECORD),
                           static_cast<uint8_t>(rel % persist::CHANNEL_RECORD));
    }
    if (index < persist::PREFS_OFFSET) {
        const uint16_t rel = static_cast<uint16_t>(index - persist::GLOBAL_OFFSET);
        if (rel == 0) {
            return static_cast<uint8_t>(ui_.tempo() & 0xFF);
        }
        if (rel == 1) {
            return static_cast<uint8_t>((ui_.tempo() >> 8) & 0xFF);
        }
        return ui_.clockSource();
    }
    const uint16_t rel = static_cast<uint16_t>(index - persist::PREFS_OFFSET);
    if (rel == 0) {
        return prefs_.rotateScreen;
    }
    if (rel == 1) {
        return prefs_.reverseEncoder;
    }
    const uint8_t channel = static_cast<uint8_t>((rel - 2) / 2);
    const int16_t offset = prefs_.cvCalibration[channel];
    return ((rel - 2) % 2 == 0) ? static_cast<uint8_t>(offset & 0xFF)
                                : static_cast<uint8_t>((offset >> 8) & 0xFF);
}

void PersistentImage::applyByte(uint16_t index, uint8_t value) {
    if (index == persist::HEADER_OFFSET) {
        return;
    }
    if (index < persist::CHANNELS_OFFSET) {
        const uint16_t rel = static_cast<uint16_t>(index - persist::PATTERNS_OFFSET);
        applyPatternByte(static_cast<uint8_t>(rel / persist::PATTERN_RECORD),
                         static_cast<uint8_t>(rel % persist::PATTERN_RECORD), value);
        return;
    }
    if (index < persist::GLOBAL_OFFSET) {
        const uint16_t rel = static_cast<uint16_t>(index - persist::CHANNELS_OFFSET);
        applyChannelByte(static_cast<uint8_t>(rel / persist::CHANNEL_RECORD),
                         static_cast<uint8_t>(rel % persist::CHANNEL_RECORD), value);
        return;
    }
    if (index < persist::PREFS_OFFSET) {
        const uint16_t rel = static_cast<uint16_t>(index - persist::GLOBAL_OFFSET);
        if (rel == 0) {
            ui_.setTempo(static_cast<uint16_t>((ui_.tempo() & 0xFF00u) | value));
            return;
        }
        if (rel == 1) {
            ui_.setTempo(static_cast<uint16_t>((ui_.tempo() & 0x00FFu)
                                               | (static_cast<uint16_t>(value) << 8)));
            return;
        }
        ui_.setClockSource(value);
        return;
    }
    const uint16_t rel = static_cast<uint16_t>(index - persist::PREFS_OFFSET);
    if (rel == 0) {
        prefs_.rotateScreen = value ? 1 : 0;
        return;
    }
    if (rel == 1) {
        prefs_.reverseEncoder = value ? 1 : 0;
        return;
    }
    const uint8_t channel = static_cast<uint8_t>((rel - 2) / 2);
    int16_t& offset = prefs_.cvCalibration[channel];
    if ((rel - 2) % 2 == 0) {
        offset = static_cast<int16_t>((offset & ~0x00FF) | value);
    } else {
        offset = static_cast<int16_t>((offset & 0x00FF)
                                      | (static_cast<int16_t>(value) << 8));
    }
}

void PersistentImage::resetToDefaults() {
    for (uint8_t index = 0; index < PATTERN_COUNT; ++index) {
        Pattern* p = bank_.getPattern(index);
        if (p != nullptr) {
            p->clear();
        }
    }
    for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
        engine_.setSelectedPattern(channel, 0);
        engine_.setEffectiveLength(channel, SequencerEngine::DEFAULT_LENGTH);
        engine_.setSubdiv(channel, DEFAULT_SUBDIV);
        engine_.setBarLength(channel, SequencerEngine::DEFAULT_BAR_LENGTH);
    }
    ui_.setTempo(UiController::DEFAULT_TEMPO);
    ui_.setClockSource(0);
    prefs_ = Preferences();
    engine_.refreshTiming();
}

}  // namespace flexseq
