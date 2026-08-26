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
};

bool isValidRatchet(uint8_t code);

uint8_t ratchetTriggers(uint8_t code);

uint8_t ratchetSpan(uint8_t code);

constexpr uint8_t MIN_SLOT_TICKS = 2;

bool ratchetFitsStep(uint8_t code, uint16_t ticksPerStep);

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
