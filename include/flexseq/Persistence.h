#ifndef FLEXSEQ_PERSISTENCE_H
#define FLEXSEQ_PERSISTENCE_H

#include <stdint.h>

#include <flexseq/FactoryPatterns.h>
#include <flexseq/Pattern.h>
#include <flexseq/PatternBank.h>
#include <flexseq/Preferences.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Subdiv.h>
#include <flexseq/UiController.h>

namespace flexseq {

namespace persist {

constexpr uint16_t BASE_ADDRESS = 384;
constexpr uint8_t FORMAT_VERSION = 2;

constexpr uint16_t HEADER_OFFSET = 0;
constexpr uint16_t HEADER_SIZE = 1;

constexpr uint8_t PATTERN_STEP_BYTES = 3;
constexpr uint8_t PATTERN_RATCHET_BYTES = 12;
constexpr uint8_t PATTERN_RECORD = PATTERN_STEP_BYTES + PATTERN_RATCHET_BYTES;
constexpr uint16_t PATTERNS_OFFSET = HEADER_OFFSET + HEADER_SIZE;
constexpr uint16_t PATTERNS_SIZE = PATTERN_COUNT * PATTERN_RECORD;

constexpr uint8_t CHANNEL_RECORD = 9;
constexpr uint16_t CHANNELS_OFFSET = PATTERNS_OFFSET + PATTERNS_SIZE;
constexpr uint16_t CHANNELS_SIZE = SequencerEngine::CHANNEL_COUNT * CHANNEL_RECORD;

constexpr uint16_t GLOBAL_OFFSET = CHANNELS_OFFSET + CHANNELS_SIZE;
constexpr uint16_t GLOBAL_SIZE = 3;

constexpr uint16_t PREFS_OFFSET = GLOBAL_OFFSET + GLOBAL_SIZE;
constexpr uint16_t PREFS_SIZE = 6;

constexpr uint16_t TOTAL_SIZE = HEADER_SIZE + PATTERNS_SIZE + CHANNELS_SIZE
                             + GLOBAL_SIZE + PREFS_SIZE;

constexpr uint16_t QUIET_MS = 3000;
constexpr uint16_t ORIGINAL_FIRMWARE_LAST = 320;
constexpr uint16_t EEPROM_SIZE = 1024;

static_assert(TOTAL_SIZE == 304, "PRD 11.1 fixes the version 2 image at 304 bytes");
static_assert(BASE_ADDRESS > ORIGINAL_FIRMWARE_LAST,
              "FlexSeq must never write over the original firmware's settings");
static_assert(BASE_ADDRESS + TOTAL_SIZE <= EEPROM_SIZE - 1,
              "the image must fit below the original firmware's memCode at 1023");
static_assert(PREFS_SIZE == sizeof(Preferences), "the prefs zone must match the struct");

namespace v3 {

constexpr uint8_t FORMAT_VERSION = 3;

constexpr uint8_t STEP_BYTES = Pattern::STEP_BYTES;
constexpr uint8_t RATCHET_BYTES = Pattern::RATCHET_BYTES;
constexpr uint8_t CONTENT_BYTES = STEP_BYTES + RATCHET_BYTES;
constexpr uint8_t LENGTH_BYTES = 1;

constexpr uint8_t RECORD_STEPS_AT = 0;
constexpr uint8_t RECORD_RATCHETS_AT = RECORD_STEPS_AT + STEP_BYTES;
constexpr uint8_t RECORD_LENGTH_AT = RECORD_RATCHETS_AT + RATCHET_BYTES;

constexpr uint8_t TEMPLATE_RECORD = CONTENT_BYTES + LENGTH_BYTES;
constexpr uint8_t INSTANCE_RECORD = CONTENT_BYTES;
constexpr uint8_t TEMPLATE_COUNT = PATTERN_COUNT;
constexpr uint8_t INSTANCE_COUNT = SequencerEngine::CHANNEL_COUNT;

constexpr uint16_t HEADER_OFFSET = 0;
constexpr uint16_t HEADER_SIZE = 1;

constexpr uint16_t TEMPLATES_OFFSET = HEADER_OFFSET + HEADER_SIZE;
constexpr uint16_t TEMPLATES_SIZE = TEMPLATE_COUNT * TEMPLATE_RECORD;

constexpr uint16_t INSTANCES_OFFSET = TEMPLATES_OFFSET + TEMPLATES_SIZE;
constexpr uint16_t INSTANCES_SIZE = INSTANCE_COUNT * INSTANCE_RECORD;

constexpr uint8_t CHANNEL_RECORD = 9;
constexpr uint16_t CHANNELS_OFFSET = INSTANCES_OFFSET + INSTANCES_SIZE;
constexpr uint16_t CHANNELS_SIZE = SequencerEngine::CHANNEL_COUNT * CHANNEL_RECORD;

constexpr uint16_t GLOBAL_OFFSET = CHANNELS_OFFSET + CHANNELS_SIZE;
constexpr uint16_t GLOBAL_SIZE = 5;
constexpr uint8_t GLOBAL_TEMPO_LO_AT = 0;
constexpr uint8_t GLOBAL_TEMPO_HI_AT = 1;
constexpr uint8_t GLOBAL_CLOCK_SOURCE_AT = 2;
constexpr uint8_t GLOBAL_MOD_AT = 3;
constexpr uint8_t GLOBAL_RANGE_AT = 4;

constexpr uint16_t PREFS_OFFSET = GLOBAL_OFFSET + GLOBAL_SIZE;
constexpr uint16_t PREFS_SIZE = 6;

constexpr uint16_t TOTAL_SIZE = PREFS_OFFSET + PREFS_SIZE;
constexpr uint16_t LAST_ADDRESS = BASE_ADDRESS + TOTAL_SIZE - 1;

static_assert(STEP_BYTES * 8 >= Pattern::DEFAULT_TOTAL_STEPS,
              "the step bytes must round up, or the last steps have no storage");
static_assert(STEP_BYTES * 8 - Pattern::DEFAULT_TOTAL_STEPS == 4,
              "ADR 0007 makes the four bits above the last step canonical zeros");
static_assert(RATCHET_BYTES * 2 == Pattern::DEFAULT_TOTAL_STEPS,
              "ADR 0007 gives one ratchet nibble per step");
static_assert(CONTENT_BYTES == sizeof(Pattern),
              "the record content must match the pattern structure");
static_assert(TEMPLATE_RECORD == CONTENT_BYTES + 1,
              "ADR 0006 adds one length byte to a template record");
static_assert(INSTANCE_RECORD == CONTENT_BYTES,
              "an instance stores content only; its length lives in the channel record");
static_assert(RECORD_LENGTH_AT == CONTENT_BYTES,
              "the length byte follows the content, with no gap");
static_assert(RECORD_LENGTH_AT + LENGTH_BYTES == TEMPLATE_RECORD,
              "the template record ends after its length byte");
static_assert(TEMPLATE_COUNT == PATTERN_COUNT,
              "PRD 11.1 stores sixteen templates");
static_assert(INSTANCE_COUNT == SequencerEngine::CHANNEL_COUNT,
              "PRD 11.1 stores one instance per channel");
static_assert(TOTAL_SIZE == HEADER_SIZE + TEMPLATES_SIZE + INSTANCES_SIZE + CHANNELS_SIZE
                          + GLOBAL_SIZE + PREFS_SIZE,
              "the offset chain must leave no gap and no overlap");
static_assert(TOTAL_SIZE == 588, "PRD 11.1 fixes the version 3 image at 588 bytes");
static_assert(BASE_ADDRESS + TOTAL_SIZE <= EEPROM_SIZE - 1,
              "the image must fit below the original firmware's memCode at 1023");
static_assert(BASE_ADDRESS > ORIGINAL_FIRMWARE_LAST,
              "FlexSeq must never write over the original firmware's settings");
static_assert(PREFS_SIZE == sizeof(Preferences), "the prefs zone must match the struct");
static_assert(Pattern::DEFAULT_TOTAL_STEPS <= 255,
              "the target length must fit the record's length byte");

constexpr uint8_t LAST_STEP_BYTE_BITS =
    Pattern::DEFAULT_TOTAL_STEPS - (STEP_BYTES - 1) * 8;
constexpr uint8_t LAST_STEP_BYTE_MASK =
    static_cast<uint8_t>((1u << LAST_STEP_BYTE_BITS) - 1u);

static_assert(LAST_STEP_BYTE_BITS == 4,
              "the last step byte carries four steps and four bits that carry none");
static_assert(LAST_STEP_BYTE_MASK == 0x0F,
              "the mask must keep the four low bits and drop the four high ones");

constexpr uint8_t MIN_TEMPLATE_LENGTH = 1;
constexpr uint8_t MAX_TEMPLATE_LENGTH = Pattern::DEFAULT_TOTAL_STEPS;

// PRD 5.0: A1 to A8 carry the original's factory content and never change.
constexpr uint8_t FROZEN_TEMPLATE_COUNT = 8;
static_assert(FROZEN_TEMPLATE_COUNT * 2 == PATTERN_COUNT,
              "PRD 5.0 splits the sixteen templates into eight frozen and eight writable");

static_assert(MIN_TEMPLATE_LENGTH == 1, "a template plays at least one step");
static_assert(MAX_TEMPLATE_LENGTH == 36,
              "the format bound is the pattern capacity, never the engine's cap");

constexpr uint8_t FACTORY_TEMPLATE_LENGTH = FACTORY_STEP_COUNT;

static_assert(FACTORY_TEMPLATE_LENGTH >= MIN_TEMPLATE_LENGTH
                  && FACTORY_TEMPLATE_LENGTH <= MAX_TEMPLATE_LENGTH,
              "a factory template length must be storable in a template record");
static_assert(FACTORY_TEMPLATE_LENGTH == 16,
              "the original firmware plays sixteen steps, and the B slots follow it");
static_assert(FACTORY_MASK_BYTES <= STEP_BYTES,
              "the factory mask must fit inside the record's step bytes");

uint8_t contentByte(const Pattern& pattern, uint8_t offset);
void applyContentByte(Pattern& pattern, uint8_t offset, uint8_t value);

uint8_t templateByte(const Pattern& pattern, uint8_t length, uint8_t offset);
bool isValidTemplateLength(uint8_t value);
bool applyTemplateByte(Pattern& pattern, uint8_t& length, uint8_t offset, uint8_t value);

uint8_t factoryTemplateByte(uint8_t index, uint8_t offset);

constexpr uint16_t templateAddress(uint8_t index, uint8_t offset) {
    return static_cast<uint16_t>(BASE_ADDRESS + TEMPLATES_OFFSET
                                 + index * TEMPLATE_RECORD + offset);
}

constexpr uint16_t IMAGE_INSTANCES_AT = 0;
constexpr uint16_t IMAGE_CHANNELS_AT = IMAGE_INSTANCES_AT + INSTANCES_SIZE;
constexpr uint16_t IMAGE_GLOBAL_AT = IMAGE_CHANNELS_AT + CHANNELS_SIZE;
constexpr uint16_t IMAGE_PREFS_AT = IMAGE_GLOBAL_AT + GLOBAL_SIZE;
constexpr uint16_t IMAGE_VERSION_AT = IMAGE_PREFS_AT + PREFS_SIZE;
constexpr uint16_t IMAGE_SIZE = IMAGE_VERSION_AT + HEADER_SIZE;

static_assert(IMAGE_SIZE + TEMPLATES_SIZE == TOTAL_SIZE,
              "the scanned image plus the template zone must cover the whole format");
static_assert(IMAGE_VERSION_AT == IMAGE_SIZE - 1,
              "the version must hold the last logical index, so the scan writes it last");

}  // namespace v3

}  // namespace persist

class PersistentImage {
public:
    static constexpr uint16_t SIZE = persist::TOTAL_SIZE;
    static constexpr uint16_t VERSION_INDEX = persist::HEADER_OFFSET;

    PersistentImage(PatternBank& bank, SequencerEngine& engine, UiController& ui,
                    Preferences& preferences)
        : bank_(bank), engine_(engine), ui_(ui), prefs_(preferences) {}

    uint16_t addressAt(uint16_t index) const {
        return static_cast<uint16_t>(persist::BASE_ADDRESS + index);
    }

    static constexpr uint8_t TEMPLATE_RECORD_SIZE = 1;
    bool canWriteTemplate(uint8_t, uint8_t) const { return false; }
    uint16_t templateAddressAt(uint8_t, uint8_t) const { return persist::BASE_ADDRESS; }
    uint8_t templateByteAt(uint8_t, uint8_t, uint8_t) const { return 0; }

    uint8_t byteAt(uint16_t index) const;
    void applyByte(uint16_t index, uint8_t value);
    void resetToDefaults();

private:
    uint8_t patternByte(uint8_t pattern, uint8_t offset) const;
    void applyPatternByte(uint8_t pattern, uint8_t offset, uint8_t value);
    uint8_t channelByte(uint8_t channel, uint8_t offset) const;
    void applyChannelByte(uint8_t channel, uint8_t offset, uint8_t value);

    PatternBank& bank_;
    SequencerEngine& engine_;
    UiController& ui_;
    Preferences& prefs_;
};

class PersistentImageV3 {
public:
    static constexpr uint16_t SIZE = persist::v3::IMAGE_SIZE;
    static constexpr uint16_t VERSION_INDEX = persist::v3::IMAGE_VERSION_AT;

    PersistentImageV3(SequencerEngine& engine, UiController& ui, Preferences& preferences)
        : engine_(engine), ui_(ui), prefs_(preferences) {}

    uint16_t addressAt(uint16_t index) const;
    uint8_t byteAt(uint16_t index) const;
    void applyByte(uint16_t index, uint8_t value);
    void resetToDefaults();

    template <typename Storage>
    void seedFactoryTemplates(Storage& storage) const {
        for (uint8_t index = 0; index < persist::v3::TEMPLATE_COUNT; ++index) {
            for (uint8_t offset = 0; offset < persist::v3::TEMPLATE_RECORD; ++offset) {
                storage.write(persist::v3::templateAddress(index, offset),
                              persist::v3::factoryTemplateByte(index, offset));
            }
        }
    }

    static constexpr uint8_t TEMPLATE_RECORD_SIZE = persist::v3::TEMPLATE_RECORD;

    template <typename Storage>
    bool isTemplateEmpty(Storage& storage, uint8_t index) const {
        if (index >= persist::v3::TEMPLATE_COUNT) {
            return false;
        }
        for (uint8_t offset = 0; offset < persist::v3::STEP_BYTES; ++offset) {
            uint8_t byte = storage.read(persist::v3::templateAddress(
                index, static_cast<uint8_t>(persist::v3::RECORD_STEPS_AT + offset)));
            if (offset == persist::v3::STEP_BYTES - 1) {
                byte = static_cast<uint8_t>(byte & persist::v3::LAST_STEP_BYTE_MASK);
            }
            if (byte != 0) {
                return false;
            }
        }
        return true;
    }

    bool canWriteTemplate(uint8_t channel, uint8_t index) const {
        if (index < persist::v3::FROZEN_TEMPLATE_COUNT
            || index >= persist::v3::TEMPLATE_COUNT) {
            return false;
        }
        return engine_.instanceForChannel(channel) != nullptr;
    }

    uint16_t templateAddressAt(uint8_t index, uint8_t offset) const {
        return persist::v3::templateAddress(index, offset);
    }

    uint8_t templateByteAt(uint8_t channel, uint8_t index, uint8_t offset) const {
        (void)index;
        const Pattern* instance = engine_.instanceForChannel(channel);
        if (instance == nullptr) {
            return 0;
        }
        return persist::v3::templateByte(*instance, engine_.getBaseLength(channel), offset);
    }

    template <typename Storage>
    bool saveTemplate(Storage& storage, uint8_t channel, uint8_t index) {
        if (index < persist::v3::FROZEN_TEMPLATE_COUNT
            || index >= persist::v3::TEMPLATE_COUNT) {
            return false;
        }
        const Pattern* instance = engine_.instanceForChannel(channel);
        if (instance == nullptr) {
            return false;
        }
        const uint8_t length = engine_.getBaseLength(channel);
        for (uint8_t offset = 0; offset < persist::v3::TEMPLATE_RECORD; ++offset) {
            storage.write(persist::v3::templateAddress(index, offset),
                          persist::v3::templateByte(*instance, length, offset));
        }
        return true;
    }

    template <typename Storage>
    bool loadTemplate(Storage& storage, uint8_t channel, uint8_t index) {
        if (index >= persist::v3::TEMPLATE_COUNT) {
            return false;
        }
        Pattern* instance = engine_.instanceForChannel(channel);
        if (instance == nullptr) {
            return false;
        }
        for (uint8_t offset = 0; offset < persist::v3::CONTENT_BYTES; ++offset) {
            persist::v3::applyContentByte(
                *instance, offset,
                storage.read(persist::v3::templateAddress(index, offset)));
        }
        // PRD 11.1: an out-of-range length is refused, the content already read stays.
        (void)engine_.setBaseLengthFromStorage(
            channel,
            storage.read(persist::v3::templateAddress(index,
                                                      persist::v3::RECORD_LENGTH_AT)));
        engine_.setSelectedPattern(channel, index);
        return true;
    }

    template <typename Storage>
    void loadTemplatesIntoInstances(Storage& storage) {
        for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
            Pattern* instance = engine_.instanceForChannel(channel);
            const int8_t selected = engine_.getSelectedPattern(channel);
            if (instance == nullptr || selected < 0) {
                continue;
            }
            for (uint8_t offset = 0; offset < persist::v3::CONTENT_BYTES; ++offset) {
                persist::v3::applyContentByte(
                    *instance, offset,
                    storage.read(persist::v3::templateAddress(
                        static_cast<uint8_t>(selected), offset)));
            }
        }
    }

private:
    SequencerEngine& engine_;
    UiController& ui_;
    Preferences& prefs_;
};

class PersistenceScheduler {
public:
    static constexpr uint16_t QUIET_MS = persist::QUIET_MS;

    static constexpr uint8_t NO_TEMPLATE = 0xFF;

    PersistenceScheduler()
        : lastChangeMs_(0), cursor_(0), dirty_(false), writing_(false),
          templateChannel_(0), templateIndex_(NO_TEMPLATE), templateCursor_(0) {}

    template <typename Image>
    bool requestTemplateWrite(const Image& image, uint8_t channel, uint8_t index) {
        if (templateIndex_ != NO_TEMPLATE) {
            return false;
        }
        if (!image.canWriteTemplate(channel, index)) {
            return false;
        }
        templateChannel_ = channel;
        templateIndex_ = index;
        templateCursor_ = 0;
        return true;
    }

    bool isWritingTemplate() const { return templateIndex_ != NO_TEMPLATE; }

    void markDirty(uint32_t nowMs) {
        dirty_ = true;
        lastChangeMs_ = nowMs;
        writing_ = false;
        cursor_ = 0;
    }

    bool isDirty() const { return dirty_; }
    bool isWriting() const { return writing_; }
    uint16_t cursor() const { return cursor_; }

    bool quietElapsed(uint32_t nowMs) const {
        return dirty_ && static_cast<uint32_t>(nowMs - lastChangeMs_) >= QUIET_MS;
    }

    template <typename Storage, typename Image>
    bool advance(Storage& storage, const Image& image, uint32_t nowMs) {
        // A template write is a command, not a debounce: it takes priority over
        // the image scan and waits for no quiet delay. One byte per call.
        if (templateIndex_ != NO_TEMPLATE) {
            storage.write(image.templateAddressAt(templateIndex_, templateCursor_),
                          image.templateByteAt(templateChannel_, templateIndex_,
                                               templateCursor_));
            ++templateCursor_;
            if (templateCursor_ >= Image::TEMPLATE_RECORD_SIZE) {
                templateIndex_ = NO_TEMPLATE;
            }
            return true;
        }
        if (!writing_) {
            if (!quietElapsed(nowMs)) {
                return false;
            }
            writing_ = true;
            cursor_ = 0;
        }
        while (cursor_ < Image::SIZE) {
            const uint16_t address = image.addressAt(cursor_);
            const uint8_t wanted = image.byteAt(cursor_);
            ++cursor_;
            if (storage.read(address) != wanted) {
                storage.write(address, wanted);
                return true;
            }
        }
        writing_ = false;
        dirty_ = false;
        return false;
    }

    template <typename Storage, typename Image>
    bool load(Storage& storage, Image& image) {
        if (storage.read(image.addressAt(Image::VERSION_INDEX))
            != image.byteAt(Image::VERSION_INDEX)) {
            image.resetToDefaults();
            return false;
        }
        for (uint16_t index = 0; index < Image::SIZE; ++index) {
            if (index == Image::VERSION_INDEX) {
                continue;
            }
            image.applyByte(index, storage.read(image.addressAt(index)));
        }
        return true;
    }

private:
    uint32_t lastChangeMs_;
    uint16_t cursor_;
    bool dirty_;
    bool writing_;
    uint8_t templateChannel_;
    uint8_t templateIndex_;
    uint8_t templateCursor_;
};

template <typename Storage>
bool loadTemplateIntoModulationBuffer(Storage& storage, ModulatedPatternState& state,
                                      uint8_t channel, uint8_t index) {
    if (channel >= SequencerEngine::CHANNEL_COUNT
        || index >= persist::v3::TEMPLATE_COUNT) {
        return false;
    }
    if (!persist::v3::isValidTemplateLength(storage.read(
            persist::v3::templateAddress(index, persist::v3::RECORD_LENGTH_AT)))) {
        return false;
    }
    for (uint8_t offset = 0; offset < persist::v3::TEMPLATE_RECORD; ++offset) {
        persist::v3::applyTemplateByte(
            state.pattern[channel], state.length[channel], offset,
            storage.read(persist::v3::templateAddress(index, offset)));
    }
    return true;
}

template <typename Storage>
bool loadTemplateIntoModulationBufferIfStorageIsFree(Storage& storage,
                                                     ModulatedPatternState& state,
                                                     uint8_t channel, uint8_t index) {
    if (storage.busy()) {
        return false;
    }
    return loadTemplateIntoModulationBuffer(storage, state, channel, index);
}

inline bool isRoutedToPattern(const SequencerEngine& engine, uint8_t channel) {
    for (uint8_t source = 0; source < CV_SOURCE_COUNT; ++source) {
        if (engine.getCvDestination(channel, source) == CV_DEST_PATTERN) {
            return true;
        }
    }
    return false;
}

inline bool isEligibleForPatternModulation(const SequencerEngine& engine,
                                           uint8_t channel) {
    return isRoutedToPattern(engine, channel)
        && engine.getChannelMode(channel) == MODE_SEQ;
}

template <typename Storage>
int8_t serviceOneModulationTemplateLoad(Storage& storage, const SequencerEngine& engine,
                                        ModulatedPatternState& state) {
    for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
        if (!isEligibleForPatternModulation(engine, channel)) {
            state.loaded[channel] = ModulatedPatternState::NOT_MODULATED;
        }
    }
    if (storage.busy()) {
        return -1;
    }
    for (uint8_t step = 0; step < SequencerEngine::CHANNEL_COUNT; ++step) {
        const uint8_t channel = static_cast<uint8_t>(
            (state.cursor + step) % SequencerEngine::CHANNEL_COUNT);
        if (!isEligibleForPatternModulation(engine, channel)) {
            continue;
        }
        const uint8_t wanted = static_cast<uint8_t>(engine.patternCvIndex(channel));
        if (wanted == state.loaded[channel]) {
            continue;
        }
        state.cursor =
            static_cast<uint8_t>((channel + 1) % SequencerEngine::CHANNEL_COUNT);
        if (loadTemplateIntoModulationBufferIfStorageIsFree(storage, state, channel,
                                                            wanted)) {
            state.loaded[channel] = wanted;
        }
        return static_cast<int8_t>(channel);
    }
    return -1;
}

template <typename Storage>
bool bootstrap(Storage& storage, PersistentImageV3& image,
               PersistenceScheduler& scheduler, uint32_t nowMs) {
    if (scheduler.load(storage, image)) {
        return true;
    }
    image.seedFactoryTemplates(storage);
    image.loadTemplatesIntoInstances(storage);
    scheduler.markDirty(nowMs);
    return false;
}

}  // namespace flexseq

#endif // FLEXSEQ_PERSISTENCE_H
