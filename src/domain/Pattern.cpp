#include "flexseq/Pattern.h"

namespace flexseq {

Pattern::Pattern()
    : packedSteps{0, 0, 0},
      baseLength(DEFAULT_PATTERN_LENGTH) {
}

bool Pattern::readStep(uint8_t index, bool& active) const {
    if (index >= STEP_COUNT) {
        return false;
    }

    const uint8_t byteIndex = index >> 3;
    const uint8_t bitIndex = index & 0x07;
    const uint8_t mask = static_cast<uint8_t>(1u << bitIndex);

    active = (packedSteps[byteIndex] & mask) != 0;
    return true;
}

bool Pattern::writeStep(uint8_t index, bool active) {
    if (index >= STEP_COUNT) {
        return false;
    }

    const uint8_t byteIndex = index >> 3;
    const uint8_t bitIndex = index & 0x07;
    const uint8_t mask = static_cast<uint8_t>(1u << bitIndex);

    if (active) {
        packedSteps[byteIndex] |= mask;
    } else {
        packedSteps[byteIndex] &= static_cast<uint8_t>(~mask);
    }

    return true;
}

uint8_t Pattern::getBaseLength() const {
    return baseLength;
}

bool Pattern::setBaseLength(uint8_t length) {
    if (length < MIN_PATTERN_LENGTH || length > MAX_PATTERN_LENGTH) {
        return false;
    }

    baseLength = length;
    return true;
}

void Pattern::clear() {
    packedSteps[0] = 0;
    packedSteps[1] = 0;
    packedSteps[2] = 0;
}

} // namespace flexseq
