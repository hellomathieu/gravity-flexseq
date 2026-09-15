#ifndef FLEXSEQ_UI_FRAME_H
#define FLEXSEQ_UI_FRAME_H

#include <stdint.h>

#include <flexseq/SequencerEngine.h>
#include <flexseq/UiController.h>

namespace flexseq {

enum UiFrameKind : uint8_t {
    UI_FRAME_MAIN,
    UI_FRAME_CHANNEL_EDIT,
};

struct UiFrameChoice {
    uint8_t kind;
    // Le canal dont le playhead varie dans le temps, -1 quand l image n en
    // porte aucun. Il decide du redessin autant que du dessin.
    int8_t channel;
};

// L image que l etat d interface designe.
//
// ⚠️ CETTE FONCTION EXISTE POUR QUE DEUX LECTEURS NE DIVERGENT PAS. Le
// declencheur de redessin de main.cpp et le selecteur d ecran lisaient des
// etats differents, et le playhead ne redessinait pas toujours. Les deux lisent
// desormais ceci.
//
// Elle vit dans un en-tete et non dans main.cpp parce que main.cpp n est
// compile par aucun test natif — docs/open-risks.md, ligne 99.
inline UiFrameChoice uiFrameChoiceOf(const UiController& ui) {
    const int8_t channel = ui.selectedChannel();
    if (ui.level() == UiController::LEVEL_EDIT && channel >= 0) {
        return UiFrameChoice{UI_FRAME_CHANNEL_EDIT, channel};
    }
    return UiFrameChoice{UI_FRAME_MAIN, -1};
}

}  // namespace flexseq

#endif // FLEXSEQ_UI_FRAME_H
