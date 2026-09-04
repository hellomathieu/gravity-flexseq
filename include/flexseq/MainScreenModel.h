#ifndef FLEXSEQ_MAIN_SCREEN_MODEL_H
#define FLEXSEQ_MAIN_SCREEN_MODEL_H

#include <stdint.h>

namespace flexseq {

class SequencerEngine;
class UiController;

// Le parametre principal, dans le vocabulaire du RENDERER. La regle qui le
// choisit vit dans UiController::mainField(), et nulle part ailleurs :
// mainScreenModelOf() traduit sa reponse ici, en un seul endroit.
enum MainParameter : uint8_t {
    MAIN_NONE = 0,
    MAIN_TEMPO,
    MAIN_SUBDIV,
    MAIN_SKIP_CHANCE,
    MAIN_PATTERN
};

// Ce que l'ecran principal doit savoir pour se dessiner, et rien de plus. Les
// types sont des entiers nus, comme clockSource l'etait deja : le renderer
// n'inclut donc pas UiController, dont il ne connait que les valeurs.
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

    // Le mode du channel, et les deux valeurs qu'il rend principales. Le
    // renderer ne DEDUIT jamais lequel afficher : mainField le lui dit.
    uint8_t mode;
    uint8_t offset;
    uint8_t skipChance;
    uint16_t stepTicks;
    uint8_t mainParameter;

    // Vrai quand le mode prend les trois lignes de l'original. Le renderer ne
    // DEDUIT pas la regle : isLegacyModeTab() en est la seule source.
    bool legacyLayout;

    uint16_t tempo;
    uint8_t clockSource;

    uint8_t headlineWidth;
};

// Construit le modele depuis l'etat reel. Cette fonction existe pour que le
// remplissage soit TESTABLE : il vivait dans main.cpp, ou rien ne l'atteignait.
MainScreenModel mainScreenModelOf(const UiController& ui, const SequencerEngine& engine);

}  // namespace flexseq

#endif // FLEXSEQ_MAIN_SCREEN_MODEL_H
