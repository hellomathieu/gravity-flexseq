#ifndef FLEXSEQ_CLOCK_SOURCE_H
#define FLEXSEQ_CLOCK_SOURCE_H

#include <stdint.h>

namespace flexseq {

enum ClockMode : uint8_t {
    CLOCK_MODE_INT = 0,
    CLOCK_MODE_EXT = 1,
    CLOCK_MODE_MIDI = 2
};

constexpr uint8_t CLOCK_MODE_COUNT = 3;
constexpr uint8_t PPQN_CHOICE_COUNT = 4;

constexpr uint8_t CLOCK_SOURCE_INT = 0;
constexpr uint8_t CLOCK_SOURCE_FIRST_EXT = 1;
constexpr uint8_t CLOCK_SOURCE_MIDI = CLOCK_SOURCE_FIRST_EXT + PPQN_CHOICE_COUNT;
constexpr uint8_t CLOCK_SOURCE_VALUE_COUNT = CLOCK_SOURCE_MIDI + 1;

inline ClockMode clockModeOf(uint8_t source) {
    if (source < CLOCK_SOURCE_FIRST_EXT) {
        return CLOCK_MODE_INT;
    }
    return source < CLOCK_SOURCE_MIDI ? CLOCK_MODE_EXT : CLOCK_MODE_MIDI;
}

inline uint8_t ppqnIndexOf(uint8_t source) {
    return clockModeOf(source) == CLOCK_MODE_EXT
        ? static_cast<uint8_t>(source - CLOCK_SOURCE_FIRST_EXT)
        : 0;
}

inline uint8_t ppqnAtIndex(uint8_t index) {
    if (index == 0) return 24;
    if (index == 1) return 4;
    if (index == 2) return 2;
    return 1;
}

inline uint8_t clockSourceOf(ClockMode mode, uint8_t ppqnIndex) {
    if (mode == CLOCK_MODE_INT) {
        return CLOCK_SOURCE_INT;
    }
    if (mode == CLOCK_MODE_MIDI) {
        return CLOCK_SOURCE_MIDI;
    }
    return static_cast<uint8_t>(CLOCK_SOURCE_FIRST_EXT
        + (ppqnIndex < PPQN_CHOICE_COUNT ? ppqnIndex : 0));
}

}  // namespace flexseq

#endif // FLEXSEQ_CLOCK_SOURCE_H
