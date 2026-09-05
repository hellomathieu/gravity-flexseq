#ifndef FLEXSEQ_MAIN_SCREEN_MODEL_H
#define FLEXSEQ_MAIN_SCREEN_MODEL_H

#include <stdint.h>

namespace flexseq {

class SequencerEngine;
class UiController;

enum MainParameter : uint8_t {
    MAIN_NONE = 0,
    MAIN_TEMPO,
    MAIN_SUBDIV,
    MAIN_SKIP_CHANCE,
    MAIN_PATTERN
};

struct MainScreenModel {
    uint8_t tab;
    bool insideTab;
    uint8_t cursor;
    bool fieldOpen;
    uint8_t fieldCount;

    int8_t patternIndex;
    uint8_t length;
    int16_t subdiv;
    uint8_t barLength;

    uint8_t mode;
    uint8_t offset;
    uint8_t skipChance;
    uint16_t stepTicks;
    uint8_t mainParameter;

    bool configPage;

    uint16_t tempo;
    uint8_t clockSource;

    uint8_t headlineWidth;

    uint8_t mainValueWidth;
    uint8_t mainLabelWidth;
    uint8_t lineLabelWidth[3];
    uint8_t lineValueWidth;
};

MainScreenModel mainScreenModelOf(const UiController& ui, const SequencerEngine& engine);

}  // namespace flexseq

#endif // FLEXSEQ_MAIN_SCREEN_MODEL_H
