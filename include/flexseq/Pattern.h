#ifndef FLEXSEQ_PATTERN_H
#define FLEXSEQ_PATTERN_H

#include <stdint.h>

namespace flexseq {

class Pattern {
public:
    static constexpr uint8_t MIN_PATTERN_LENGTH = 1;
    static constexpr uint8_t MAX_PATTERN_LENGTH = 24;
    static constexpr uint8_t DEFAULT_PATTERN_LENGTH = 16;
    static constexpr uint8_t STEP_COUNT = 24;

    Pattern();

    bool readStep(uint8_t index, bool& active) const;
    bool writeStep(uint8_t index, bool active);

    uint8_t getBaseLength() const;
    bool setBaseLength(uint8_t length);

    void clear();

private:
    uint8_t packedSteps[3];
    uint8_t baseLength;
};

static_assert(sizeof(Pattern) == 4, "Pattern must remain 4 bytes");

} // namespace flexseq

#endif // FLEXSEQ_PATTERN_H
