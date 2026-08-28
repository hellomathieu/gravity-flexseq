#include <Arduino.h>
#include <libGravity.h>

#include <flexseq/CvSampler.h>
#include <flexseq/EepromStorage.h>
#include <flexseq/FactoryPatterns.h>
#if FLEXSEQ_ENCODER_PROBE
#include <flexseq/EncoderProbe.h>
#endif
#include <flexseq/InputAdapter.h>
#include <flexseq/Persistence.h>
#include <flexseq/PagedScreen.h>
#include <flexseq/PatternBank.h>
#include <flexseq/PatternScreen.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>
#include <flexseq/TransportAdapter.h>
#include <flexseq/TriggerSequencer.h>
#include <flexseq/UiController.h>

namespace {

flexseq::PatternBank patternBank;
flexseq::SequencerEngine engine;
flexseq::Transport transport(engine);
flexseq::TriggerSequencer triggers(engine);
flexseq::UiController ui(engine, transport);
flexseq::Preferences preferences;
flexseq::PersistentImage persistentImage(patternBank, engine, ui, preferences);
flexseq::PersistenceScheduler persistence;
flexseq::EepromStorage eeprom;
// "CH1  120BPM" : le channel et le tempo, reecrits a chaque image.
char uiFooter[13] = "CH1  120BPM";
constexpr uint8_t UI_FOOTER_CHANNEL = 2;
constexpr uint8_t UI_FOOTER_TEMPO = 5;

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
constexpr uint16_t UI_MIN_INTERVAL_MS = 40;

// "EDIT PATTERN A1" : les deux derniers caracteres suivent le pattern courant.
char uiTitle[16] = "EDIT PATTERN A1";
constexpr uint8_t UI_TITLE_BANK = 13;
constexpr uint8_t UI_TITLE_NUM = 14;

uint32_t uiLastDrawMs = 0;
int8_t uiLastStep = -2;
uint8_t uiLastRevision = 0xFF;
uint8_t savedRevision = 0;

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

void beginEditFrame(uint8_t channel) {
    const int8_t selected = engine.getSelectedPattern(channel);
    if (selected < 0) {
        return;
    }
    uiTitle[UI_TITLE_BANK] = (selected < 8) ? 'A' : 'B';
    uiTitle[UI_TITLE_NUM] = static_cast<char>('1' + (selected % 8));

    uiFooter[UI_FOOTER_CHANNEL] = static_cast<char>('1' + channel);
    const uint8_t written =
        flexseq::detail::writeUnsigned(uiFooter + UI_FOOTER_TEMPO, ui.tempo());
    uiFooter[UI_FOOTER_TEMPO + written] = 'B';
    uiFooter[UI_FOOTER_TEMPO + written + 1] = 'P';
    uiFooter[UI_FOOTER_TEMPO + written + 2] = 'M';
    uiFooter[UI_FOOTER_TEMPO + written + 3] = '\0';

#if FLEXSEQ_ENCODER_PROBE
    flexseq::probe::writeReport(uiTitle, uiFooter);
#endif

    flexseq::PatternScreenModel model;
    model.title = uiTitle;
    model.titleWidth = 0;  // PagedScreen la mesure une fois par image
    model.pattern = engine.patternForChannel(channel);
    model.length = engine.getEffectiveLength(channel);
    model.cursor = static_cast<int8_t>(ui.stepCursor());
    model.playhead = engine.effectiveStep(channel);
    model.barLength = static_cast<uint8_t>(engine.getBarLength(channel));
    model.footer = uiFooter;

    uiScreen.begin(gravity.display, model);
}

void beginMainFrame() {
    const int8_t channel = ui.selectedChannel();

    flexseq::MainScreenModel model;
    model.tab = ui.currentTab();
    model.insideTab = (ui.level() == flexseq::UiController::LEVEL_TAB);
    model.cursor = ui.cursor();
    model.fieldOpen = ui.fieldOpen();
    model.fieldCount = ui.fieldCount();
    model.patternIndex = channel >= 0
        ? engine.getSelectedPattern(static_cast<uint8_t>(channel))
        : -1;
    model.length = channel >= 0
        ? engine.getEffectiveLength(static_cast<uint8_t>(channel))
        : 0;
    model.subdiv = channel >= 0 ? engine.getSubdiv(static_cast<uint8_t>(channel)) : 0;
    model.barLength = channel >= 0
        ? static_cast<uint8_t>(engine.getBarLength(static_cast<uint8_t>(channel)))
        : 0;
    model.tempo = ui.tempo();
    model.clockSource = ui.clockSource();
    model.headlineWidth = 0;  // PagedScreen la mesure une fois par image

    uiScreen.begin(gravity.display, model);
}

// Ouvre une image sur l'ecran que l'etat d'interface designe : EDIT PATTERN quand
// on y est, l'ecran principal partout ailleurs.
void beginUiFrame() {
    const int8_t channel = ui.selectedChannel();
    if (ui.level() == flexseq::UiController::LEVEL_EDIT && channel >= 0) {
        beginEditFrame(static_cast<uint8_t>(channel));
    } else {
        beginMainFrame();
    }
}

}  // namespace

namespace flexseq {
namespace probe {
Pattern* volatile instanceBase = nullptr;
}  // namespace probe
}  // namespace flexseq

void setup() {
    gravity.Init();

    // libGravity ne definit aucune police : police integree U8g2 (evite aussi
    // d'embarquer les donnees de police GPLv3 du firmware d'origine).
    gravity.display.setFont(u8g2_font_5x7_tr);

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
        // Premier demarrage, ou format inconnu. L'original livre A1..A8 avec du
        // contenu et B1..B8 vides (Gravity.ino:83-98) : sans cela la regle qui
        // gele A1..A8 gelerait huit emplacements vides.
        flexseq::loadFactoryPatterns(patternBank);
        persistence.markDirty(millis());
    }

    flexseq::probe::instanceBase = engine.instanceForChannel(0);

    for (uint8_t ch = 0; ch < flexseq::SequencerEngine::CHANNEL_COUNT; ++ch) {
        flexseq::Pattern* instance = engine.instanceForChannel(ch);
        const int8_t selectedTemplate = engine.getSelectedPattern(ch);
        if (instance == nullptr || selectedTemplate < 0) {
            continue;
        }
        const flexseq::Pattern* source =
            patternBank.getPattern(static_cast<uint8_t>(selectedTemplate));
        if (source != nullptr) {
            *instance = *source;
        }
    }

    // Drive the master phase from the unified 96-PPQN output clock (internal
    // and external sources both surface here).
    gravity.clock.AttachIntHandler(onOutputTick);

    // L'horloge externe : libGravity attache l'ISR mais n'appelle jamais
    // uClock.clockMe() — c'est notre callback qui doit le faire. Le tempo et la
    // source chargés depuis l'EEPROM sont appliqués ici.
    flexseq::transport::begin(ui, transport);

#if FLEXSEQ_START_IN_EDIT
    // Un harnais a besoin d'une boucle LENTE qui emet des triggers, et aucun
    // binaire ne reunissait les deux : l'ecran principal ne redessine presque
    // jamais, et env:wokwi qui rend EDIT n'instancie pas de TriggerSequencer.
    // On entre donc dans EDIT par les GESTES publics, sans rien exposer de plus
    // dans le domaine. L'onglet par defaut est deja un channel.
    ui.handle(flexseq::UiController::EVENT_PRESS);
    for (uint8_t i = 0; i < ui.fieldCount(); ++i) {
        if (ui.field() == flexseq::UiController::FIELD_EDIT_ENTRY) {
            break;
        }
        ui.handle(flexseq::UiController::EVENT_ROTATE, 1);
    }
    ui.handle(flexseq::UiController::EVENT_PRESS);
    // Et le transport DEMARRE. Depuis que le module boote a l'arret, un ecran
    // EDIT sans playhead qui avance ne se redessine presque jamais : un harnais
    // de rendu y mesurerait quelques echantillons et les presenterait comme les
    // autres. Le drapeau met le firmware dans l'etat qu'on veut observer, et
    // l'etat observable inclut le mouvement.
    ui.handle(flexseq::UiController::EVENT_PLAY_PRESS);
#endif

    // Le module demarre A L'ARRET, comme l'original : `isPlaying` y est un
    // global a zero (Gravity.ino:110). PLAY le lance en horloge interne, et une
    // impulsion externe le lance dans les autres sources.
    transport.reset();
}

void loop() {
#if FLEXSEQ_ENCODER_PROBE
    const uint32_t probeStart = micros();
    flexseq::probe::advancePage(millis());
#endif
    // PAS gravity.Process() : il appelle cv1/cv2.Process(), donc un analogRead
    // bloquant qui entrerait en collision avec les conversions de l'ISR
    // (CvSampler.h). On appelle ses morceaux ; les sorties etaient deja pilotees
    // explicitement plus bas, de sorte que FlexSeq ne depend plus du tout de
    // cette fonction — ni de son index de boucle non initialise.
    flexseq::input::process(millis());
    flexseq::transport::apply(ui);

    // Toute edition rend l'etat a sauvegarder. Le compteur de revisions suffit :
    // il change des qu'un geste a ete traite, et une sauvegarde de trop ne coute
    // rien puisque seuls les octets reellement modifies sont ecrits (PRD 11.1).
    if (ui.revision() != savedRevision) {
        savedRevision = ui.revision();
        persistence.markDirty(millis());
    }

    // Atomically drain the ticks accumulated by the ISR, then advance once.
    uint16_t ticks;
    noInterrupts();
    ticks = pendingTicks;
    pendingTicks = 0;
    interrupts();

    if (ticks > 0) {
        transport.tick(ticks);
        triggers.update();
    }

    // Paiement de la dette d'onsets. Un pas a ratchet en doit plusieurs, et une
    // sortie ne se rearme qu'une fois par impulsion : declencher une sortie
    // deja haute prolongerait l'impulsion au lieu d'en creer une seconde. On
    // paie donc UN onset par passage et seulement sur une sortie basse, sur
    // TOUT passage — y compris ceux sans tick, sinon le surplus serait perdu.
    for (uint8_t ch = 0; ch < flexseq::SequencerEngine::CHANNEL_COUNT; ++ch) {
        if (!gravity.outputs[ch].On() && triggers.takeTrigger(ch)) {
            gravity.outputs[ch].Trigger();
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
        // Le playhead ne se lit QUE sur l'ecran EDIT : l'ecran principal ne
        // porte aucun element qui varie dans le temps. Le redessiner a chaque
        // step y coutait huit bandes sans rien changer a l'image, et privait la
        // persistance de ses passages sans tick.
        const int8_t channel = ui.selectedChannel();
        const bool editing =
            (ui.level() == flexseq::UiController::LEVEL_EDIT) && channel >= 0;
        const int8_t step = editing
            ? engine.effectiveStep(static_cast<uint8_t>(channel))
            : -1;
        const uint8_t revision = ui.revision();
        bool due = (step != uiLastStep || revision != uiLastRevision);
#if FLEXSEQ_ENCODER_PROBE
        due = due || flexseq::probe::pageChanged();
#endif
        if (due) {
            const uint32_t now = millis();
            if (now - uiLastDrawMs >= UI_MIN_INTERVAL_MS) {
                uiLastDrawMs = now;
                uiLastStep = step;
                uiLastRevision = revision;
                beginUiFrame();
            }
        }
    }

    // Auto-off safeguard. gravity.Process() also does this, but libGravity's
    // loop uses an uninitialised index (libGravity.cpp), so drive it explicitly.
    for (uint8_t ch = 0; ch < flexseq::SequencerEngine::CHANNEL_COUNT; ++ch) {
        gravity.outputs[ch].Process();
    }
#if FLEXSEQ_ENCODER_PROBE
    flexseq::probe::recordPass(micros() - probeStart);
#endif
}
