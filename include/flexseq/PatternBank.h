#ifndef FLEXSEQ_PATTERN_BANK_H
#define FLEXSEQ_PATTERN_BANK_H

#include <stdint.h>

#include <flexseq/Pattern.h>

namespace flexseq {

// A shared bank of 16 patterns (A1..A8, B1..B8), matching the original Sitka
// firmware (seqA1..seqB8 shared globally). Each channel selects which pattern
// it plays via its own selectedPattern; editing a pattern is therefore visible
// to every channel that references it.
//
// Memory: 16 * 6 = 96 bytes, versus 672 bytes for the previous per-channel
// 6x16 store (~560 bytes of SRAM reclaimed on the ATmega328P).
constexpr uint8_t PATTERN_COUNT = 16;

class PatternBank {
public:
    constexpr PatternBank() : patterns{} {}

    Pattern* getPattern(uint8_t index);
    const Pattern* getPattern(uint8_t index) const;

private:
    Pattern patterns[PATTERN_COUNT];
};

static_assert(
    sizeof(PatternBank) == PATTERN_COUNT * sizeof(Pattern),
    "PatternBank has an unexpected memory footprint"
);

}  // namespace flexseq

#endif // FLEXSEQ_PATTERN_BANK_H
