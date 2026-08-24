#ifndef FLEXSEQ_FACTORY_PATTERNS_H
#define FLEXSEQ_FACTORY_PATTERNS_H

#include <stdint.h>

#include <flexseq/PatternBank.h>

namespace flexseq {

constexpr uint8_t FACTORY_PATTERN_COUNT = 8;
constexpr uint8_t FACTORY_STEP_COUNT = 16;

void loadFactoryPatterns(PatternBank& bank);

}  // namespace flexseq

#endif // FLEXSEQ_FACTORY_PATTERNS_H
