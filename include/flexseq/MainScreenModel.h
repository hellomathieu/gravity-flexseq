#ifndef FLEXSEQ_MAIN_SCREEN_MODEL_H
#define FLEXSEQ_MAIN_SCREEN_MODEL_H

#include <stdint.h>

#include <flexseq/PatternAction.h>

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

    // Le pattern que l ecran NOMME : celui du canal sur un onglet de canal,
    // l emplacement parcouru sur l onglet PATTERNS.
    int8_t patternIndex;
    bool slotEmpty;      // rempli hors du domaine : le lire demande l EEPROM
    uint8_t length;
    int16_t subdiv;
    uint8_t barLength;

    uint8_t mode;
    uint8_t offset;
    uint8_t skipChance;
    uint16_t stepTicks;
    uint8_t mainParameter;
    bool patternAsk;
    bool patternYes;

    uint8_t cv1Target;
    uint8_t cv2Target;

    bool configPage;

    uint16_t tempo;
    uint8_t clockSource;
    bool running;


};

MainScreenModel mainScreenModelOf(const UiController& ui, const SequencerEngine& engine);

}  // namespace flexseq

#endif // FLEXSEQ_MAIN_SCREEN_MODEL_H
