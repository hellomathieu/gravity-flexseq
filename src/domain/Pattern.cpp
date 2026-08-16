#include "flexseq/Pattern.h"

namespace flexseq {

Pattern::Pattern()
    : packedSteps{0, 0, 0},
      tripletStarts{0, 0, 0},
      baseLength(DEFAULT_PATTERN_LENGTH) {
}

bool Pattern::readStep(uint8_t index, bool& active) const {
    if (index >= DEFAULT_TOTAL_STEPS) {
        return false;
    }

    const uint8_t byteIndex = index >> 3;
    const uint8_t bitIndex = index & 0x07;
    const uint8_t mask = static_cast<uint8_t>(1u << bitIndex);

    active = (packedSteps[byteIndex] & mask) != 0;
    return true;
}

bool Pattern::writeStep(uint8_t index, bool active) {
    if (index >= DEFAULT_TOTAL_STEPS) {
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

    clearTriplets();
}

bool Pattern::addTriplet(uint8_t startIndex) {
    // A triplet occupies startIndex, startIndex + 1 and startIndex + 2.
    // Therefore the latest possible start is DEFAULT_TOTAL_STEPS - 3.
    if (startIndex > DEFAULT_TOTAL_STEPS - 3) {
        return false;
    }

    // A triplet must fit inside the active base length.
    if (startIndex + 3 > baseLength) {
        return false;
    }

    // A new triplet cannot overlap an existing triplet.
    //
    // An existing triplet starting at S occupies:
    //   S, S + 1, S + 2
    //
    // Therefore an existing start can conflict with startIndex
    // when it is in the range:
    //   startIndex - 2 ... startIndex + 2
    //
    // Avoid unsigned underflow by checking the lower bound separately.
    const uint8_t firstCandidate =
        (startIndex >= 2) ? static_cast<uint8_t>(startIndex - 2) : 0;

    const uint8_t lastCandidate =
        static_cast<uint8_t>(startIndex + 2);

    for (uint8_t existingStart = firstCandidate;
         existingStart <= lastCandidate;
         ++existingStart) {

        if (existingStart >= DEFAULT_TOTAL_STEPS - 2) {
            break;
        }

        if (isTripletStart(existingStart)) {
            return false;
        }
    }

    const uint8_t byteIndex = startIndex >> 3;
    const uint8_t bitIndex = startIndex & 0x07;
    const uint8_t mask = static_cast<uint8_t>(1u << bitIndex);

    tripletStarts[byteIndex] |= mask;

    return true;
}

bool Pattern::removeTriplet(uint8_t startIndex) {
    if (startIndex >= DEFAULT_TOTAL_STEPS) {
        return false;
    }

    if (!isTripletStart(startIndex)) {
        return false;
    }

    const uint8_t byteIndex = startIndex >> 3;
    const uint8_t bitIndex = startIndex & 0x07;
    const uint8_t mask = static_cast<uint8_t>(1u << bitIndex);

    tripletStarts[byteIndex] &= static_cast<uint8_t>(~mask);

    return true;
}

bool Pattern::isTripletStart(uint8_t index) const {
    if (index >= DEFAULT_TOTAL_STEPS) {
        return false;
    }

    const uint8_t byteIndex = index >> 3;
    const uint8_t bitIndex = index & 0x07;
    const uint8_t mask = static_cast<uint8_t>(1u << bitIndex);

    return (tripletStarts[byteIndex] & mask) != 0;
}

bool Pattern::isTripletStep(uint8_t index) const {
    if (index >= DEFAULT_TOTAL_STEPS) {
        return false;
    }

    for (uint8_t start = 0; start <= DEFAULT_TOTAL_STEPS - 3; ++start) {
        if (!isTripletStart(start)) {
            continue;
        }

        if (index >= start && index < start + 3) {
            return true;
        }
    }

    return false;
}

void Pattern::clearTriplets() {
    tripletStarts[0] = 0;
    tripletStarts[1] = 0;
    tripletStarts[2] = 0;
}

} // namespace flexseq