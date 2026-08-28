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

uint8_t channelRecordByte(const SequencerEngine& engine, uint8_t channel, uint8_t offset) {
    switch (offset) {
        case 0: {
            const int8_t selected = engine.getSelectedPattern(channel);
            return selected < 0 ? 0 : static_cast<uint8_t>(selected);
        }
        case 1:
            return engine.getEffectiveLength(channel);
        case 2: {
            const int8_t index = subdivIndexOf(engine.getSubdiv(channel));
            return index < 0 ? DEFAULT_SUBDIV_INDEX : static_cast<uint8_t>(index);
        }
        case 3: {
            const int8_t bar = engine.getBarLength(channel);
            return bar < 0 ? 0 : static_cast<uint8_t>(bar);
        }
        case 4:
            return static_cast<uint8_t>(engine.getChannelMode(channel));
        case 5:
            return static_cast<uint8_t>(engine.getOffset(channel) & 0xFF);
        case 6:
            return engine.getSkipChance(channel);
        default:
            return 0;
    }
}

void applyChannelRecordByte(SequencerEngine& engine, uint8_t channel, uint8_t offset,
                            uint8_t value) {
    switch (offset) {
        case 0: engine.setSelectedPattern(channel, value); break;
        case 1: engine.setEffectiveLength(channel, value); break;
        case 2: engine.setSubdiv(channel, subdivAtIndex(value)); break;
        case 3: engine.setBarLength(channel, value); break;
        case 4: engine.setChannelMode(channel, static_cast<ChannelMode>(value)); break;
        case 5: engine.setOffset(channel, value); break;
        case 6: engine.setSkipChance(channel, value); break;
        default: break;
    }
}

uint8_t prefsRecordByte(const Preferences& prefs, uint16_t rel) {
    if (rel == 0) {
        return prefs.rotateScreen;
    }
    if (rel == 1) {
        return prefs.reverseEncoder;
    }
    const uint8_t channel = static_cast<uint8_t>((rel - 2) / 2);
    const int16_t offset = prefs.cvCalibration[channel];
    return ((rel - 2) % 2 == 0) ? static_cast<uint8_t>(offset & 0xFF)
                                : static_cast<uint8_t>((offset >> 8) & 0xFF);
}

void applyPrefsRecordByte(Preferences& prefs, uint16_t rel, uint8_t value) {
    if (rel == 0) {
        prefs.rotateScreen = value ? 1 : 0;
        return;
    }
    if (rel == 1) {
        prefs.reverseEncoder = value ? 1 : 0;
        return;
    }
    const uint8_t channel = static_cast<uint8_t>((rel - 2) / 2);
    int16_t& offset = prefs.cvCalibration[channel];
    if ((rel - 2) % 2 == 0) {
        offset = static_cast<int16_t>((offset & ~0x00FF) | value);
    } else {
        offset = static_cast<int16_t>((offset & 0x00FF)
                                      | (static_cast<int16_t>(value) << 8));
    }
}

}  // namespace

namespace persist {
namespace v3 {

uint8_t contentByte(const Pattern& pattern, uint8_t offset) {
    if (offset < STEP_BYTES) {
        const uint8_t raw = pattern.stepByte(offset);
        return (offset == STEP_BYTES - 1)
                   ? static_cast<uint8_t>(raw & LAST_STEP_BYTE_MASK)
                   : raw;
    }
    return pattern.ratchetByte(static_cast<uint8_t>(offset - STEP_BYTES));
}

void applyContentByte(Pattern& pattern, uint8_t offset, uint8_t value) {
    if (offset < STEP_BYTES) {
        const uint8_t kept = (offset == STEP_BYTES - 1)
                                 ? static_cast<uint8_t>(value & LAST_STEP_BYTE_MASK)
                                 : value;
        pattern.setStepByte(offset, kept);
        return;
    }
    pattern.setRatchetByte(static_cast<uint8_t>(offset - STEP_BYTES),
                           sanitisedRatchetNibbles(value));
}

uint8_t templateByte(const Pattern& pattern, uint8_t length, uint8_t offset) {
    if (offset == RECORD_LENGTH_AT) {
        if (length < MIN_TEMPLATE_LENGTH) {
            return MIN_TEMPLATE_LENGTH;
        }
        if (length > MAX_TEMPLATE_LENGTH) {
            return MAX_TEMPLATE_LENGTH;
        }
        return length;
    }
    return contentByte(pattern, offset);
}

bool applyTemplateByte(Pattern& pattern, uint8_t& length, uint8_t offset, uint8_t value) {
    if (offset == RECORD_LENGTH_AT) {
        if (value < MIN_TEMPLATE_LENGTH || value > MAX_TEMPLATE_LENGTH) {
            return false;
        }
        length = value;
        return true;
    }
    applyContentByte(pattern, offset, value);
    return true;
}

uint8_t factoryTemplateByte(uint8_t index, uint8_t offset) {
    if (index >= TEMPLATE_COUNT) {
        return 0;
    }
    if (offset == RECORD_LENGTH_AT) {
        return FACTORY_TEMPLATE_LENGTH;
    }
    if (offset >= RECORD_STEPS_AT + FACTORY_MASK_BYTES) {
        return 0;
    }
    const uint16_t mask = factoryStepMask(index);
    const uint8_t shift = static_cast<uint8_t>((offset - RECORD_STEPS_AT) * 8);
    return static_cast<uint8_t>((mask >> shift) & 0xFF);
}

}  // namespace v3
}  // namespace persist

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
    return channelRecordByte(engine_, channel, offset);
}

void PersistentImage::applyChannelByte(uint8_t channel, uint8_t offset, uint8_t value) {
    applyChannelRecordByte(engine_, channel, offset, value);
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
    return prefsRecordByte(prefs_, static_cast<uint16_t>(index - persist::PREFS_OFFSET));
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
    applyPrefsRecordByte(prefs_, static_cast<uint16_t>(index - persist::PREFS_OFFSET), value);
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
        engine_.setChannelMode(channel, DEFAULT_CHANNEL_MODE);
        engine_.setOffset(channel, 0);
        engine_.setSkipChance(channel, 0);
    }
    ui_.setTempo(UiController::DEFAULT_TEMPO);
    ui_.setClockSource(0);
    prefs_ = Preferences();
    engine_.refreshTiming();
}

uint16_t PersistentImageV3::addressAt(uint16_t index) const {
    using namespace persist::v3;
    uint16_t offset = 0;
    if (index == IMAGE_VERSION_AT) {
        offset = HEADER_OFFSET;
    } else if (index < IMAGE_CHANNELS_AT) {
        offset = static_cast<uint16_t>(INSTANCES_OFFSET + (index - IMAGE_INSTANCES_AT));
    } else if (index < IMAGE_GLOBAL_AT) {
        offset = static_cast<uint16_t>(CHANNELS_OFFSET + (index - IMAGE_CHANNELS_AT));
    } else if (index < IMAGE_PREFS_AT) {
        offset = static_cast<uint16_t>(GLOBAL_OFFSET + (index - IMAGE_GLOBAL_AT));
    } else {
        offset = static_cast<uint16_t>(PREFS_OFFSET + (index - IMAGE_PREFS_AT));
    }
    return static_cast<uint16_t>(persist::BASE_ADDRESS + offset);
}

uint8_t PersistentImageV3::byteAt(uint16_t index) const {
    using namespace persist::v3;
    if (index == IMAGE_VERSION_AT) {
        return FORMAT_VERSION;
    }
    if (index < IMAGE_CHANNELS_AT) {
        const uint16_t rel = static_cast<uint16_t>(index - IMAGE_INSTANCES_AT);
        const Pattern* instance =
            engine_.instanceForChannel(static_cast<uint8_t>(rel / INSTANCE_RECORD));
        if (instance == nullptr) {
            return 0;
        }
        return contentByte(*instance, static_cast<uint8_t>(rel % INSTANCE_RECORD));
    }
    if (index < IMAGE_GLOBAL_AT) {
        const uint16_t rel = static_cast<uint16_t>(index - IMAGE_CHANNELS_AT);
        return channelRecordByte(engine_, static_cast<uint8_t>(rel / CHANNEL_RECORD),
                                 static_cast<uint8_t>(rel % CHANNEL_RECORD));
    }
    if (index < IMAGE_PREFS_AT) {
        const uint16_t rel = static_cast<uint16_t>(index - IMAGE_GLOBAL_AT);
        if (rel == GLOBAL_TEMPO_LO_AT) {
            return static_cast<uint8_t>(ui_.tempo() & 0xFF);
        }
        if (rel == GLOBAL_TEMPO_HI_AT) {
            return static_cast<uint8_t>((ui_.tempo() >> 8) & 0xFF);
        }
        if (rel == GLOBAL_CLOCK_SOURCE_AT) {
            return ui_.clockSource();
        }
        return 0;
    }
    return prefsRecordByte(prefs_, static_cast<uint16_t>(index - IMAGE_PREFS_AT));
}

void PersistentImageV3::applyByte(uint16_t index, uint8_t value) {
    using namespace persist::v3;
    if (index == IMAGE_VERSION_AT) {
        return;
    }
    if (index < IMAGE_CHANNELS_AT) {
        const uint16_t rel = static_cast<uint16_t>(index - IMAGE_INSTANCES_AT);
        Pattern* instance =
            engine_.instanceForChannel(static_cast<uint8_t>(rel / INSTANCE_RECORD));
        if (instance != nullptr) {
            applyContentByte(*instance, static_cast<uint8_t>(rel % INSTANCE_RECORD), value);
        }
        return;
    }
    if (index < IMAGE_GLOBAL_AT) {
        const uint16_t rel = static_cast<uint16_t>(index - IMAGE_CHANNELS_AT);
        applyChannelRecordByte(engine_, static_cast<uint8_t>(rel / CHANNEL_RECORD),
                               static_cast<uint8_t>(rel % CHANNEL_RECORD), value);
        return;
    }
    if (index < IMAGE_PREFS_AT) {
        const uint16_t rel = static_cast<uint16_t>(index - IMAGE_GLOBAL_AT);
        if (rel == GLOBAL_TEMPO_LO_AT) {
            ui_.setTempo(static_cast<uint16_t>((ui_.tempo() & 0xFF00u) | value));
            return;
        }
        if (rel == GLOBAL_TEMPO_HI_AT) {
            ui_.setTempo(static_cast<uint16_t>((ui_.tempo() & 0x00FFu)
                                               | (static_cast<uint16_t>(value) << 8)));
            return;
        }
        if (rel == GLOBAL_CLOCK_SOURCE_AT) {
            ui_.setClockSource(value);
        }
        return;
    }
    applyPrefsRecordByte(prefs_, static_cast<uint16_t>(index - IMAGE_PREFS_AT), value);
}

void PersistentImageV3::resetToDefaults() {
    for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
        Pattern* instance = engine_.instanceForChannel(channel);
        if (instance != nullptr) {
            instance->clear();
        }
        engine_.setSelectedPattern(channel, 0);
        engine_.setEffectiveLength(channel, SequencerEngine::DEFAULT_LENGTH);
        engine_.setSubdiv(channel, DEFAULT_SUBDIV);
        engine_.setBarLength(channel, SequencerEngine::DEFAULT_BAR_LENGTH);
        engine_.setChannelMode(channel, DEFAULT_CHANNEL_MODE);
        engine_.setOffset(channel, 0);
        engine_.setSkipChance(channel, 0);
    }
    ui_.setTempo(UiController::DEFAULT_TEMPO);
    ui_.setClockSource(0);
    prefs_ = Preferences();
    engine_.refreshTiming();
}

}  // namespace flexseq
