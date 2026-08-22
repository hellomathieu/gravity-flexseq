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
constexpr uint8_t FORMAT_VERSION = 1;

constexpr uint16_t HEADER_OFFSET = 0;
constexpr uint16_t HEADER_SIZE = 1;

constexpr uint8_t PATTERN_STEP_BYTES = 3;
constexpr uint8_t PATTERN_RATCHET_BYTES = 12;
constexpr uint8_t PATTERN_RECORD = PATTERN_STEP_BYTES + PATTERN_RATCHET_BYTES;
constexpr uint16_t PATTERNS_OFFSET = HEADER_OFFSET + HEADER_SIZE;
constexpr uint16_t PATTERNS_SIZE = PATTERN_COUNT * PATTERN_RECORD;

constexpr uint8_t CHANNEL_RECORD = 6;
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

static_assert(TOTAL_SIZE == 286, "PRD 11.1 fixes the image at 286 bytes");
static_assert(BASE_ADDRESS > ORIGINAL_FIRMWARE_LAST,
              "FlexSeq must never write over the original firmware's settings");
static_assert(BASE_ADDRESS + TOTAL_SIZE <= EEPROM_SIZE - 1,
              "the image must fit below the original firmware's memCode at 1023");
static_assert(PREFS_SIZE == sizeof(Preferences), "the prefs zone must match the struct");

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
