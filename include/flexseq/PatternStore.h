#pragma once

#include <stdint.h>

#include <flexseq/Pattern.h>

namespace flexseq {

constexpr uint8_t CHANNEL_COUNT = 6;
constexpr uint8_t PATTERN_PER_CHANNEL = 16;

class PatternStore {
public:
    PatternStore();

    Pattern* getPattern(uint8_t channel, uint8_t patternIndex);
    const Pattern* getPattern(uint8_t channel, uint8_t patternIndex) const;

private:
    Pattern patterns[CHANNEL_COUNT][PATTERN_PER_CHANNEL];
};

static_assert(
    sizeof(PatternStore) == CHANNEL_COUNT * PATTERN_PER_CHANNEL * sizeof(Pattern),
    "PatternStore has an unexpected memory footprint"
);

}  // namespace flexseq