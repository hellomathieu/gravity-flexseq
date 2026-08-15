#include <flexseq/PatternStore.h>

namespace flexseq {

PatternStore::PatternStore()
    : patterns{} {
}

Pattern* PatternStore::getPattern(
    uint8_t channel,
    uint8_t patternIndex
) {
    if (channel >= CHANNEL_COUNT) {
        return nullptr;
    }

    if (patternIndex >= PATTERN_PER_CHANNEL) {
        return nullptr;
    }

    return &patterns[channel][patternIndex];
}

const Pattern* PatternStore::getPattern(
    uint8_t channel,
    uint8_t patternIndex
) const {
    if (channel >= CHANNEL_COUNT) {
        return nullptr;
    }

    if (patternIndex >= PATTERN_PER_CHANNEL) {
        return nullptr;
    }

    return &patterns[channel][patternIndex];
}

}  // namespace flexseq