#include <flexseq/PatternBank.h>

namespace flexseq {

PatternBank::PatternBank()
    : patterns{} {
}

Pattern* PatternBank::getPattern(uint8_t index) {
    if (index >= PATTERN_COUNT) {
        return nullptr;
    }

    return &patterns[index];
}

const Pattern* PatternBank::getPattern(uint8_t index) const {
    if (index >= PATTERN_COUNT) {
        return nullptr;
    }

    return &patterns[index];
}

}  // namespace flexseq
