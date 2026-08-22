#include <Arduino.h>
#include <libGravity.h>

#include <flexseq/PagedScreen.h>
#include <flexseq/PatternBank.h>
#include <flexseq/PatternScreen.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>

// -----------------------------------------------------------------------------
// Harnais de validation VISUELLE — Wokwi (extension VS Code)
//
// C'est du FIRMWARE REEL compile depuis l'arbre : meme domaine, meme renderer,
// meme objet display de libGravity. Aucune copie du code, donc aucune derive
// possible. Lance par l'extension Wokwi pour VS Code (voir wokwi.toml).
//
// Seule difference avec main.cpp : un CONTENU DE DEMONSTRATION est prechargé
// pour rendre visibles tous les cas de la legende (ratchets, triolet, points
// hors LENGTH, barres de mesure). main.cpp reste vierge de ce contenu.
//
// L'horloge interne de libGravity demarre seule (uClock, 120 BPM par defaut) :
// le playhead avance donc sans intervention.
//
// NE PAS FLASHER sur le Gravity physique.
// -----------------------------------------------------------------------------

namespace {

flexseq::PatternBank patternBank;
flexseq::SequencerEngine engine;
flexseq::Transport transport(engine);

constexpr uint8_t CH = 0;
constexpr int8_t CURSOR = 5; // volontairement colle a la barre du step 6

// On ne redessine que lorsque le playhead a bouge, et le rendu est ETALE — une
// bande par passage de loop(), comme dans main.cpp (ADR 0001). Ce harnais doit
// exercer le MEME chemin de rendu que le firmware, sinon il ne valide plus rien.
int8_t lastDrawnStep = -2;

flexseq::PagedScreen<decltype(gravity.display)> screen;

volatile uint16_t pendingTicks = 0;

void onOutputTick(uint32_t) {
    ++pendingTicks;
}

// Ouvre une image. PagedScreen gele le modele, puis en rend une bande par appel
// a advance() ; drawPatternScreen() doit rester pure.
void beginFrame() {
    flexseq::PatternScreenModel model;
    model.title = "EDIT PATTERN A1";
    model.titleWidth = 0;  // PagedScreen la mesure une fois par image
    model.pattern = patternBank.getPattern(0);
    model.length = engine.getEffectiveLength(CH);
    model.cursor = CURSOR;
    model.playhead = engine.effectiveStep(CH);
    model.barLength = static_cast<uint8_t>(engine.getBarLength(CH));
    model.footer = "CH1  120BPM";

    screen.begin(gravity.display, model);
}

}  // namespace

void setup() {
    gravity.Init();

    // libGravity ne definit aucune police.
    gravity.display.setFont(u8g2_font_5x7_tf);

    // Contenu de demonstration : couvre toute la legende du PRD.
    flexseq::Pattern* pattern = patternBank.getPattern(0);
    const uint8_t active[] = {0, 2, 5, 6, 7, 8, 13, 15, 16, 19};
    for (uint8_t i = 0; i < sizeof(active); ++i) {
        pattern->writeStep(active[i], true);
    }
    pattern->setRatchet(2, flexseq::RATCHET_2);
    pattern->setRatchet(6, flexseq::RATCHET_6);
    pattern->setRatchet(8, flexseq::RATCHET_3);
    pattern->setRatchet(16, flexseq::RATCHET_4);
    pattern->setRatchet(15, flexseq::RATCHET_TRIPLET); // tient 2 unites

    engine.setPatternBank(&patternBank); // sans ca, les ratchets sont ignores
    engine.setSelectedPattern(CH, 0);
    engine.setEffectiveLength(CH, 20);   // steps 20..23 -> simples points
    engine.setSubdiv(CH, 1);             // /1 : une unite par step
    engine.setBarLength(CH, 3);          // separation en 3/4

    gravity.clock.AttachIntHandler(onOutputTick);
    transport.start();
}

void loop() {
    gravity.shift_button.Process();
    gravity.play_button.Process();
    gravity.encoder.Process();

    uint16_t ticks;
    noInterrupts();
    ticks = pendingTicks;
    pendingTicks = 0;
    interrupts();

    if (ticks > 0) {
        transport.tick(ticks);
    }

    if (screen.busy()) {
        screen.advance(gravity.display);
    } else {
        const int8_t step = engine.effectiveStep(CH);
        if (step != lastDrawnStep) {
            lastDrawnStep = step;
            beginFrame();
        }
    }

    for (uint8_t ch = 0; ch < flexseq::SequencerEngine::CHANNEL_COUNT; ++ch) {
        gravity.outputs[ch].Process();
    }
}
