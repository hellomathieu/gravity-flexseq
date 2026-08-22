#include <Arduino.h>
#include <libGravity.h>

#include <flexseq/CvSampler.h>
#include <flexseq/EepromStorage.h>
#include <flexseq/InputAdapter.h>
#include <flexseq/Persistence.h>
#include <flexseq/PagedScreen.h>
#include <flexseq/PatternBank.h>
#include <flexseq/PatternScreen.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>
#include <flexseq/TriggerSequencer.h>
#include <flexseq/UiController.h>

namespace {

flexseq::PatternBank patternBank;
flexseq::SequencerEngine engine;
flexseq::Transport transport(engine);
flexseq::TriggerSequencer triggers(patternBank, engine);
flexseq::UiController ui(engine, patternBank, transport);
flexseq::Preferences preferences;
flexseq::PersistentImage persistentImage(patternBank, engine, ui, preferences);
flexseq::PersistenceScheduler persistence;
flexseq::EepromStorage eeprom;
char uiFooter[] = "CH1";
constexpr uint8_t UI_FOOTER_CHANNEL = 2;

// --- UI ---------------------------------------------------------------------
// Rendu ETALE : UNE BANDE PAR PASSAGE de loop() (ADR 0001). Le mode _1_ de
// libGravity n'alloue que 128 o de tampon pour un ecran de 1024 : U8g2 rend donc
// l'image en 8 bandes horizontales. Les enchainer dans un seul appel bloquerait
// la boucle le temps de l'image entiere (~25 ms de bus a 400 kHz), pendant
// lesquels les ticks s'accumulent et les onsets se tassent au drainage suivant.
// Une bande a la fois ramene le pire cas a ~3 ms.
//
// On ne redessine QUE lorsque l'affichage a reellement change, et on ne demarre
// jamais une image plus souvent que UI_MIN_INTERVAL_MS : le drainage des ticks
// et l'emission des triggers passent toujours en premier dans loop().
constexpr uint8_t UI_CHANNEL = 0;
constexpr int8_t UI_CURSOR = 0;
constexpr uint16_t UI_MIN_INTERVAL_MS = 40;

// "EDIT PATTERN A1" : les deux derniers caracteres suivent le pattern courant.
char uiTitle[16] = "EDIT PATTERN A1";
constexpr uint8_t UI_TITLE_BANK = 13;
constexpr uint8_t UI_TITLE_NUM = 14;

uint32_t uiLastDrawMs = 0;
int8_t uiLastStep = -2;
int8_t uiLastPattern = -2;

// L'image en cours. PagedScreen gele le modele et le contenu du pattern, puis
// rend une bande par appel. `decltype` evite de reecrire ici le type d'affichage
// de libGravity.
flexseq::PagedScreen<decltype(gravity.display)> uiScreen;

// Incremented in the uClock 96-PPQN output ISR (AttachIntHandler), drained in
// loop() so the engine is only mutated in main-loop context (no torn reads of
// its multi-byte state).
volatile uint16_t pendingTicks = 0;

void onOutputTick(uint32_t) {
    ++pendingTicks;
}

// Ouvre une image : releve l'etat a afficher et le confie a PagedScreen, qui le
// gele. Sans effet si rien n'est affichable.
void beginUiFrame() {
    const int8_t selected = engine.getSelectedPattern(UI_CHANNEL);
    if (selected < 0) {
        return;
    }
    uiTitle[UI_TITLE_BANK] = (selected < 8) ? 'A' : 'B';
    uiTitle[UI_TITLE_NUM] = static_cast<char>('1' + (selected % 8));

    flexseq::PatternScreenModel model;
    model.title = uiTitle;
    model.titleWidth = 0;  // PagedScreen la mesure une fois par image
    model.pattern = patternBank.getPattern(static_cast<uint8_t>(selected));
    model.length = engine.getEffectiveLength(UI_CHANNEL);
    model.cursor = UI_CURSOR;
    model.playhead = engine.effectiveStep(UI_CHANNEL);
    model.barLength = static_cast<uint8_t>(engine.getBarLength(UI_CHANNEL));
    uiFooter[UI_FOOTER_CHANNEL] = static_cast<char>('1' + UI_CHANNEL);
    model.footer = uiFooter;

    uiScreen.begin(gravity.display, model);
}

}  // namespace

void setup() {
    gravity.Init();

    // libGravity ne definit aucune police : police integree U8g2 (evite aussi
    // d'embarquer les donnees de police GPLv3 du firmware d'origine).
    gravity.display.setFont(u8g2_font_5x7_tf);

    // The engine reads the bank to shorten triplet steps (3 in one step's time).
    engine.setPatternBank(&patternBank);

    // Echantillonnage du CV SOUS INTERRUPTION. FlexSeq prend la propriete du
    // convertisseur : voir include/flexseq/CvSampler.h. La calibration est lue
    // sur les objets de libGravity, qui la detiennent.
    flexseq::cv::configure(flexseq::cv::CV1, gravity.cv1.GetCalibrationLow(),
                           gravity.cv1.GetCalibrationHigh(), gravity.cv1.GetOffset());
    flexseq::cv::configure(flexseq::cv::CV2, gravity.cv2.GetCalibrationLow(),
                           gravity.cv2.GetCalibrationHigh(), gravity.cv2.GetOffset());
    flexseq::cv::start();

    flexseq::input::begin(ui);

    // Persistance : on relit l'image, et si l'octet de version ne repond pas on
    // repart des defauts EN LES ECRIVANT — le format est ainsi materialise des
    // le premier demarrage, pas a la premiere edition. Voir PRD 11.1.
    if (!persistence.load(eeprom, persistentImage)) {
        persistence.markDirty(millis());
    }

    // Drive the master phase from the unified 96-PPQN output clock (internal
    // and external sources both surface here).
    gravity.clock.AttachIntHandler(onOutputTick);

    transport.start();  // global reset + run
}

void loop() {
    // PAS gravity.Process() : il appelle cv1/cv2.Process(), donc un analogRead
    // bloquant qui entrerait en collision avec les conversions de l'ISR
    // (CvSampler.h). On appelle ses morceaux ; les sorties etaient deja pilotees
    // explicitement plus bas, de sorte que FlexSeq ne depend plus du tout de
    // cette fonction — ni de son index de boucle non initialise.
    flexseq::input::process(millis());

    // Atomically drain the ticks accumulated by the ISR, then advance once.
    uint16_t ticks;
    noInterrupts();
    ticks = pendingTicks;
    pendingTicks = 0;
    interrupts();

    if (ticks > 0) {
        transport.tick(ticks);

        // Emit a pulse on every channel that owes one. A ratchet step owes
        // several onsets; the output can only be re-armed once per drain, so a
        // pulse is emitted here and the remaining onsets land on the following
        // drains (ticks arrive far more often than steps).
        for (uint8_t ch = 0; ch < flexseq::SequencerEngine::CHANNEL_COUNT; ++ch) {
            if (triggers.triggerCount(ch) > 0) {
                gravity.outputs[ch].Trigger();
            }
        }
    }

    // L'ecriture EEPROM prend ~3,4 ms pendant lesquelles la boucle attend : elle
    // n'a donc lieu QUE sur un passage sans onset, un octet a la fois, apres le
    // delai de calme. PRD 11.1.
    if (ticks == 0) {
        persistence.advance(eeprom, persistentImage, millis());
    }

    // Rendu de l'ecran : UNE bande par passage (ADR 0001). Une image en cours
    // se poursuit jusqu'a son terme ; on n'en ouvre une nouvelle que si
    // l'affichage a change, et jamais avant UI_MIN_INTERVAL_MS.
    if (uiScreen.busy()) {
        uiScreen.advance(gravity.display);
    } else {
        const int8_t step = engine.effectiveStep(UI_CHANNEL);
        const int8_t pat = engine.getSelectedPattern(UI_CHANNEL);
        if (step != uiLastStep || pat != uiLastPattern) {
            const uint32_t now = millis();
            if (now - uiLastDrawMs >= UI_MIN_INTERVAL_MS) {
                uiLastDrawMs = now;
                uiLastStep = step;
                uiLastPattern = pat;
                beginUiFrame();
            }
        }
    }

    // Auto-off safeguard. gravity.Process() also does this, but libGravity's
    // loop uses an uninitialised index (libGravity.cpp), so drive it explicitly.
    for (uint8_t ch = 0; ch < flexseq::SequencerEngine::CHANNEL_COUNT; ++ch) {
        gravity.outputs[ch].Process();
    }
}
