#ifndef FLEXSEQ_FACTORY_PATTERNS_H
#define FLEXSEQ_FACTORY_PATTERNS_H

#include <stdint.h>

#include <flexseq/PatternBank.h>

namespace flexseq {

constexpr uint8_t FACTORY_PATTERN_COUNT = 8;
constexpr uint8_t FACTORY_STEP_COUNT = 16;
constexpr uint8_t FACTORY_MASK_BYTES = 2;

static_assert(FACTORY_STEP_COUNT == FACTORY_MASK_BYTES * 8,
              "the factory mask carries exactly one bit per factory step");

uint16_t factoryStepMask(uint8_t index);

void loadFactoryPatterns(PatternBank& bank);

}  // namespace flexseq

#endif // FLEXSEQ_FACTORY_PATTERNS_H
