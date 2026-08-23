#include "flexseq/Pattern.h"

namespace flexseq {

bool isValidRatchet(uint8_t code) {
    return code == RATCHET_NONE || code == RATCHET_2 || code == RATCHET_3 ||
           code == RATCHET_4 || code == RATCHET_6 || code == RATCHET_TRIPLET;
}

uint8_t ratchetTriggers(uint8_t code) {
    if (code == RATCHET_TRIPLET) {
        return 3; // three triggers, spread over two step durations
    }
    if (code == RATCHET_2 || code == RATCHET_3 || code == RATCHET_4 ||
        code == RATCHET_6) {
        return code; // the code IS the trigger count
    }
    return 1;
}

uint8_t ratchetSpan(uint8_t code) {
    return (code == RATCHET_TRIPLET) ? 2 : 1;
}

bool ratchetFitsStep(uint8_t code, uint16_t ticksPerStep) {
    const uint8_t triggers = ratchetTriggers(code);
    if (triggers <= 1) {
        return true;
    }
    const uint32_t stepTicks =
        static_cast<uint32_t>(ticksPerStep) * ratchetSpan(code);
    return stepTicks / triggers >= MIN_SLOT_TICKS;
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

void Pattern::clear() {
    packedSteps[0] = 0;
    packedSteps[1] = 0;
    packedSteps[2] = 0;

    clearRatchets();
}

bool Pattern::setRatchet(uint8_t index, uint8_t code) {
    if (index >= DEFAULT_TOTAL_STEPS || !isValidRatchet(code)) {
        return false;
    }

    const uint8_t byteIndex = index >> 1;      // two nibbles per byte
    const bool highNibble = (index & 0x01) != 0;

    if (highNibble) {
        packedRatchets[byteIndex] =
            static_cast<uint8_t>((packedRatchets[byteIndex] & 0x0F) |
                                 static_cast<uint8_t>(code << 4));
    } else {
        packedRatchets[byteIndex] =
            static_cast<uint8_t>((packedRatchets[byteIndex] & 0xF0) | (code & 0x0F));
    }

    return true;
}

uint8_t Pattern::getRatchet(uint8_t index) const {
    if (index >= DEFAULT_TOTAL_STEPS) {
        return RATCHET_NONE;
    }

    const uint8_t byteIndex = index >> 1;
    const bool highNibble = (index & 0x01) != 0;

    return highNibble ? static_cast<uint8_t>(packedRatchets[byteIndex] >> 4)
                      : static_cast<uint8_t>(packedRatchets[byteIndex] & 0x0F);
}

void Pattern::clearRatchets() {
    for (uint8_t i = 0; i < 12; ++i) {
        packedRatchets[i] = 0;
    }
}

} // namespace flexseq
