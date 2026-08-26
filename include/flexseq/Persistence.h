#ifndef FLEXSEQ_PERSISTENCE_H
#define FLEXSEQ_PERSISTENCE_H

#include <stdint.h>

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

static_assert(MIN_TEMPLATE_LENGTH == 1, "a template plays at least one step");
static_assert(MAX_TEMPLATE_LENGTH == 36,
              "the format bound is the pattern capacity, never the engine's cap");

uint8_t contentByte(const Pattern& pattern, uint8_t offset);
void applyContentByte(Pattern& pattern, uint8_t offset, uint8_t value);

uint8_t templateByte(const Pattern& pattern, uint8_t length, uint8_t offset);
bool applyTemplateByte(Pattern& pattern, uint8_t& length, uint8_t offset, uint8_t value);

}  // namespace v3

}  // namespace persist

class PersistentImage {
public:
    static constexpr uint16_t SIZE = persist::TOTAL_SIZE;

    PersistentImage(PatternBank& bank, SequencerEngine& engine, UiController& ui,
                    Preferences& preferences)
        : bank_(bank), engine_(engine), ui_(ui), prefs_(preferences) {}

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

class PersistenceScheduler {
public:
    static constexpr uint16_t QUIET_MS = persist::QUIET_MS;

    PersistenceScheduler()
        : lastChangeMs_(0), cursor_(0), dirty_(false), writing_(false) {}

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

    template <typename Storage>
    bool advance(Storage& storage, const PersistentImage& image, uint32_t nowMs) {
        if (!writing_) {
            if (!quietElapsed(nowMs)) {
                return false;
            }
            writing_ = true;
            cursor_ = 0;
        }
        while (cursor_ < PersistentImage::SIZE) {
            const uint16_t address = static_cast<uint16_t>(persist::BASE_ADDRESS + cursor_);
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

    template <typename Storage>
    bool load(Storage& storage, PersistentImage& image) {
        if (storage.read(persist::BASE_ADDRESS + persist::HEADER_OFFSET)
            != persist::FORMAT_VERSION) {
            image.resetToDefaults();
            return false;
        }
        for (uint16_t index = persist::HEADER_SIZE; index < PersistentImage::SIZE; ++index) {
            image.applyByte(index,
                            storage.read(static_cast<uint16_t>(persist::BASE_ADDRESS + index)));
        }
        return true;
    }

private:
    uint32_t lastChangeMs_;
    uint16_t cursor_;
    bool dirty_;
    bool writing_;
};

}  // namespace flexseq

#endif // FLEXSEQ_PERSISTENCE_H
