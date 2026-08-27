#include <flexseq/FactoryPatterns.h>

#ifdef __AVR__
#include <avr/pgmspace.h>
#endif

namespace flexseq {

namespace {

#ifdef __AVR__
const uint16_t kFactory[FACTORY_PATTERN_COUNT] PROGMEM = {
#else
const uint16_t kFactory[FACTORY_PATTERN_COUNT] = {
#endif
    0x9111,
    0x0810,
    0x1249,
    0xCCCC,
    0xEEEE,
    0x5454,
    0x7FBF,
    0xB733,
};

}  // namespace

uint16_t factoryStepMask(uint8_t index) {
    if (index >= FACTORY_PATTERN_COUNT) {
        return 0;
    }
#ifdef __AVR__
    return static_cast<uint16_t>(pgm_read_word_near(&kFactory[index]));
#else
    return kFactory[index];
#endif
}

void loadFactoryPatterns(PatternBank& bank) {
    for (uint8_t index = 0; index < FACTORY_PATTERN_COUNT; ++index) {
        Pattern* pattern = bank.getPattern(index);
        if (pattern == nullptr) {
            continue;
        }
        pattern->clear();
        pattern->setLowStepMask(factoryStepMask(index));
    }
}

}  // namespace flexseq
