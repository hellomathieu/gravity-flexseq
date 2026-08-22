#include <flexseq/Subdiv.h>

#ifdef __AVR__
#include <avr/pgmspace.h>
#endif

namespace flexseq {

namespace {

#ifdef __AVR__
const int16_t kSubdivChoices[SUBDIV_CHOICE_COUNT] PROGMEM = {
#else
const int16_t kSubdivChoices[SUBDIV_CHOICE_COUNT] = {
#endif
    -24, -16, -12, -8, -6, -4, -3, -2,
    1,
    2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 24, 32, 64, 128,
};

int16_t choiceAt(uint8_t index) {
#ifdef __AVR__
    return static_cast<int16_t>(pgm_read_word_near(&kSubdivChoices[index]));
#else
    return kSubdivChoices[index];
#endif
}

}  // namespace

int16_t subdivAtIndex(uint8_t index) {
    if (index >= SUBDIV_CHOICE_COUNT) {
        return 0;
    }
    return choiceAt(index);
}

int8_t subdivIndexOf(int16_t subdiv) {
    for (uint8_t index = 0; index < SUBDIV_CHOICE_COUNT; ++index) {
        if (choiceAt(index) == subdiv) {
            return static_cast<int8_t>(index);
        }
    }
    return -1;
}

}  // namespace flexseq
