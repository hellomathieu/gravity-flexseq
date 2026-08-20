#include <Arduino.h>
#include <libGravity.h>

#include <flexseq/CvSampler.h>
#include <flexseq/PagedScreen.h>
#include <flexseq/PatternBank.h>
#include <flexseq/PatternScreen.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>
#include <flexseq/TriggerSequencer.h>

namespace {

flexseq::PatternBank patternBank;
flexseq::SequencerEngine engine;
flexseq::Transport transport(engine);
flexseq::TriggerSequencer triggers(patternBank, engine);

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
    model.pattern = patternBank.getPattern(static_cast<uint8_t>(selected));
    model.length = engine.getEffectiveLength(UI_CHANNEL);
    model.cursor = UI_CURSOR;
    model.playhead = engine.effectiveStep(UI_CHANNEL);
    model.barLength = static_cast<uint8_t>(engine.getBarLength(UI_CHANNEL));

    uiScreen.begin(gravity.display, model);
}

// --- Sonde de pile (build de mesure seulement) -------------------------------
// Le linker ne compte que la RAM statique : un debordement de pile ne se
// manifeste jamais comme une erreur de lien, seulement comme une corruption
// silencieuse. On mesure donc la pile a l'execution, par PEINTURE : la RAM
// libre est remplie d'un motif au demarrage, et le point le plus bas atteint se
// lit ensuite comme la premiere adresse qui ne porte plus ce motif.
//
// Le resultat sort en LARGEUR D'IMPULSION sur la sortie CH1 : 100 us par octet,
// ce qui le rend lisible dans le VCD de simavr (la seule trace fiable de cette
// version : `sram16` n'emet que des horodatages sans valeur). Une impulsion de
// mesure depasse 10 ms, un trigger musical en fait 5 : aucune confusion.
//
// Compile UNIQUEMENT dans env:stackprobe (-DFLEXSEQ_STACK_PROBE). Le firmware de
// production ne contient pas une instruction de tout ceci.
#ifdef FLEXSEQ_STACK_PROBE
// Symbole du linker : fin de .bss, donc premier octet de la RAM libre. Il n'a
// pas de nom C++ — d'ou le `extern "C"`, sans quoi il serait cherche dans
// l'espace de noms anonyme.
extern "C" uint8_t _end;
constexpr uint8_t PROBE_PATTERN = 0xC5;
constexpr uint16_t PROBE_PERIOD_MS = 1000;
constexpr uint8_t PROBE_US_PER_BYTE = 100;
constexpr uint8_t PROBE_CONFIRM = 8;   // octets peints consecutifs exiges
constexpr uint16_t PROBE_REF = 100;    // largeur d'etalonnage, en "octets"
uint32_t probeLastMs = 0;

// Remplit la RAM libre, de la fin de .bss jusqu'a un peu sous le pointeur de
// pile courant. Ce qui s'est passe AVANT cet appel (constructeurs globaux,
// init() d'Arduino) n'est donc pas couvert : la mesure porte sur la phase de
// fonctionnement.
void probePaint() {
    uint8_t* p = &_end;
    uint8_t* const top = reinterpret_cast<uint8_t*>(SP) - 16;
    while (p < top) {
        *p++ = PROBE_PATTERN;
    }
}

// Profondeur maximale atteinte, en octets, depuis RAMEND.
//
// On balaie du HAUT vers le bas, et non l'inverse : le bas de la RAM libre est
// le debut du tas (`__heap_start` == `_end`), et une allocation posterieure a la
// peinture y ecrirait, ce qui ferait conclure a tort que la pile est descendue
// jusque-la. Depuis le haut, la premiere zone encore peinte est la frontiere.
//
// PROBE_CONFIRM octets consecutifs sont exiges : un seul octet de pile valant
// par hasard 0xC5 arreterait sinon le balayage trop tot.
uint16_t probeStackUsed() {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(RAMEND);
    const uint8_t* const floor = &_end + PROBE_CONFIRM;
    while (p > floor) {
        bool clean = true;
        for (uint8_t i = 0; i < PROBE_CONFIRM; ++i) {
            if (p[-i] != PROBE_PATTERN) {
                clean = false;
                break;
            }
        }
        if (clean) {
            break;
        }
        --p;
    }
    return static_cast<uint16_t>(RAMEND - reinterpret_cast<uint16_t>(p));
}

// Une impulsion dure `n` iterations de delayMicroseconds(100) — donc un peu
// PLUS de 100 us par iteration, a cause du surcout de boucle. On emet donc
// d'abord une impulsion d'ETALONNAGE de PROBE_REF iterations : le rapport des
// deux largeurs donne le nombre d'octets sans que cette derive intervienne.
void probePulse(uint16_t iterations) {
    gravity.outputs[0].High();
    for (uint16_t i = 0; i < iterations; ++i) {
        delayMicroseconds(PROBE_US_PER_BYTE);
    }
    gravity.outputs[0].Low();
}

void probeReport() {
    const uint16_t used = probeStackUsed();
    probePulse(PROBE_REF);  // reference : PROBE_REF "octets"
    delay(2);
    probePulse(used);
}
#endif

}  // namespace

void setup() {
#ifdef FLEXSEQ_STACK_PROBE
    probePaint();  // avant tout appel profond
#endif
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
    gravity.shift_button.Process();
    gravity.play_button.Process();
    gravity.encoder.Process();

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

#ifdef FLEXSEQ_STACK_PROBE
    if (millis() - probeLastMs >= PROBE_PERIOD_MS) {
        probeLastMs = millis();
        probeReport();
    }
#endif

    // Auto-off safeguard. gravity.Process() also does this, but libGravity's
    // loop uses an uninitialised index (libGravity.cpp), so drive it explicitly.
    for (uint8_t ch = 0; ch < flexseq::SequencerEngine::CHANNEL_COUNT; ++ch) {
        gravity.outputs[ch].Process();
    }
}
