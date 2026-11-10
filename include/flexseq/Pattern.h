#ifndef FLEXSEQ_PATTERN_H
#define FLEXSEQ_PATTERN_H

#include <stdint.h>

namespace flexseq {

enum : uint8_t {
    RATCHET_NONE = 0,
    RATCHET_2 = 2,
    RATCHET_3 = 3,
    RATCHET_4 = 4,
    RATCHET_6 = 6,
    RATCHET_TRIPLET = 7,
    RATCHET_OFFBEAT = 8,
    RATCHET_PAUSE_2 = 9,
    RATCHET_PAUSE_3 = 10,
    RATCHET_PAUSE_4 = 11,
};

bool isValidRatchet(uint8_t code);
bool ratchetSkipsStepOnset(uint8_t code);

uint8_t ratchetTriggers(uint8_t code);

uint8_t ratchetSpan(uint8_t code);
uint8_t ratchetPauseUnits(uint8_t code);

constexpr uint8_t MIN_SLOT_TICKS = 2;

bool ratchetFitsStep(uint8_t code, uint16_t ticksPerStep);

constexpr uint8_t RATCHET_CHOICE_COUNT = 10;

inline uint8_t ratchetChoiceAt(uint8_t index) {
    if (index == 0) return RATCHET_NONE;
    if (index == 1) return RATCHET_OFFBEAT;
    if (index == 2) return RATCHET_2;
    if (index == 3) return RATCHET_3;
    if (index == 4) return RATCHET_4;
    if (index == 5) return RATCHET_6;
    if (index == 6) return RATCHET_TRIPLET;
    return static_cast<uint8_t>(RATCHET_PAUSE_2 + index - 7);
}

inline int8_t ratchetChoiceIndexOf(uint8_t code) {
    for (uint8_t index = 0; index < RATCHET_CHOICE_COUNT; ++index) {
        if (ratchetChoiceAt(index) == code) {
            return static_cast<int8_t>(index);
        }
    }
    return -1;
}

class Pattern {
public:
    static constexpr uint8_t DEFAULT_TOTAL_STEPS = 36;
    static constexpr uint8_t STEP_BYTES = (DEFAULT_TOTAL_STEPS + 7) / 8;
    static constexpr uint8_t RATCHET_BYTES = DEFAULT_TOTAL_STEPS / 2;

    constexpr Pattern() : packedSteps{}, packedRatchets{} {}

    bool readStep(uint8_t index, bool& active) const;
    bool writeStep(uint8_t index, bool active);

    void clear();

    void setLowStepMask(uint16_t bits);

    bool setRatchet(uint8_t index, uint8_t code);
    uint8_t getRatchet(uint8_t index) const;

    void clearRatchets();

    uint8_t stepByte(uint8_t index) const;
    void setStepByte(uint8_t index, uint8_t value);

    uint8_t ratchetByte(uint8_t index) const;
    void setRatchetByte(uint8_t index, uint8_t value);

private:
    uint8_t packedSteps[STEP_BYTES];
    uint8_t packedRatchets[RATCHET_BYTES];
};

static_assert(sizeof(Pattern) == 23, "Pattern must remain 23 bytes (content only)");

} // namespace flexseq

#endif // FLEXSEQ_PATTERN_H
