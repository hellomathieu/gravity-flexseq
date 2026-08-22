#include <Arduino.h>
#include <libGravity.h>

#include <flexseq/CvGate.h>
#include <flexseq/CvSampler.h>

// -----------------------------------------------------------------------------
// Firmware de MISE EN ROUTE — un diagnostic, pas un sequenceur.
//
// Pourquoi il existe. Tout FlexSeq a ete valide en tests natifs et en simulation ;
// rien n'a jamais tourne sur le module. Le premier flash est donc le moment ou
// une dizaine d'hypotheses se verifient d'un coup, et un ecran vide n'apprend
// rien sur laquelle a echoue. Ce firmware exerce chaque peripherique SEPAREMENT
// et montre le resultat a l'ecran : le premier flash devient un diagnostic.
//
// Aucun instrument requis. Tout ce qui est observable l'est sur l'OLED, sauf les
// sorties — qui sont des jacks : le firmware en active UNE a la fois, en tournant
// lentement, et affiche laquelle. On suit avec un cable.
//
// Ce que chaque ligne prouve :
//   CV1/CV2  la chaine ADC-sous-interruption ET la calibration (valeur en +/-512)
//   EDGE     la detection de front de CvGate sur un VRAI signal (compteur)
//   ENC      l'encodeur et son bouton, via les rappels de libGravity
//   SHIFT/PLAY  les deux boutons, etat direct ET rappel d'appui
//   CLK      la source d'horloge, le tempo, et le compteur de ticks a 96 PPQN
//   OUT      les 6 sorties et la sortie PULSE du MIDI Expander, une par une
//
// NE PAS confondre avec le firmware de production : ce fichier ne contient ni
// sequenceur, ni pattern, ni persistance. Il est compile par env:bringup seul.
//
// Le rendu est ici une image ENTIERE d'un seul coup, contrairement a main.cpp qui
// l'etale (ADR 0001). C'est deliberé : aucune contrainte de chronometrage
// n'existe dans un diagnostic, et la capture du CV — la seule chose qui pourrait
// en souffrir — passe par l'ISR, donc ne depend pas de la boucle. C'est meme une
// demonstration : les fronts CV doivent etre comptes correctement alors que la
// boucle est bloquee la moitie du temps.
// -----------------------------------------------------------------------------

namespace {

// 250 ms et non 120 : une image de texte entiere, non etalee, bloque la boucle
// ~100 ms. A 120 ms de cadence la moitie du temps passait a redessiner un ecran
// qu'un humain lit — et la consommation du verrou CV s'en trouvait retardee
// jusqu'a 170 ms. Un diagnostic n'a pas besoin de 8 images par seconde.
constexpr uint16_t UI_INTERVAL_MS = 250;
constexpr uint16_t OUTPUT_DWELL_MS = 800;  // temps sur chaque sortie
constexpr uint8_t OUT_COUNT = Gravity::OUTPUT_COUNT;      // 6
constexpr uint8_t OUTPUT_STEPS = OUT_COUNT + 2;          // 6 sorties + PULSE + rien

uint32_t uiLastMs = 0;
uint32_t outLastMs = 0;
uint8_t outStep = 0;  // 0..5 = CH1..CH6, 6 = PULSE, 7 = rien

volatile uint32_t ticks = 0;
uint16_t cvEdges[flexseq::cv::COUNT] = {0, 0};
int16_t encoderPos = 0;
uint16_t encoderPresses = 0;

// Mesure du seuil d'anti-rebond de l'encodeur. libGravity n'en a aucun, et deux
// crans rapides s'annulent (+1 puis -1). Le seuil doit se poser ENTRE le pire
// rebond et la plus rapide des inversions volontaires : ces deux distributions
// sont donc mesurees separement, et non supposees.
uint32_t rotateLastMs = 0;
int8_t rotateLastSign = 0;
uint16_t reversals = 0;
uint16_t reversalMinMs = 0xFFFF;
uint16_t reversalMaxMs = 0;
uint16_t sameDirMinMs = 0xFFFF;
uint16_t shiftPresses = 0;
uint16_t playPresses = 0;
uint16_t shiftLongPresses = 0;
uint16_t playLongPresses = 0;

// Calibration relevee une fois, pour afficher le CV en unites du domaine.
int16_t calLow[flexseq::cv::COUNT];
int16_t calHigh[flexseq::cv::COUNT];
int16_t calOffset[flexseq::cv::COUNT];

void onTick(uint32_t) { ++ticks; }
void onEncoderRotate(int change) {
    encoderPos = static_cast<int16_t>(encoderPos + change);
    const uint32_t now = millis();
    const int8_t sign = change < 0 ? -1 : 1;
    if (rotateLastSign != 0) {
        const uint32_t elapsed = now - rotateLastMs;
        const uint16_t dt = elapsed > 0xFFFFu ? 0xFFFFu : static_cast<uint16_t>(elapsed);
        if (sign != rotateLastSign) {
            ++reversals;
            if (dt < reversalMinMs) reversalMinMs = dt;
            if (dt > reversalMaxMs) reversalMaxMs = dt;
        } else if (dt < sameDirMinMs) {
            sameDirMinMs = dt;
        }
    }
    rotateLastMs = now;
    rotateLastSign = sign;
}
void onEncoderPress() { ++encoderPresses; }
void onShiftPress() {
    // Remet les compteurs a zero : on peut donc refaire une passe sans reflasher.
    ++shiftPresses;
    ticks = 0;
    cvEdges[0] = 0;
    cvEdges[1] = 0;
    encoderPos = 0;
    encoderPresses = 0;
    rotateLastSign = 0;
    reversals = 0;
    reversalMinMs = 0xFFFF;
    reversalMaxMs = 0;
    sameDirMinMs = 0xFFFF;
    playPresses = 0;
    shiftLongPresses = 0;
    playLongPresses = 0;
}

void onShiftLongPress() { ++shiftLongPresses; }
void onPlayLongPress() { ++playLongPresses; }
void onPlayPress() {
    ++playPresses;
    if (gravity.clock.IsPaused()) {
        gravity.clock.Start();
    } else {
        gravity.clock.Stop();
    }
}

// --- formatage sans printf ---------------------------------------------------
// snprintf coute ~1,5 ko de Flash sur AVR pour ce seul usage.

char* putStr(char* p, const char* s) {
    while (*s) {
        *p++ = *s++;
    }
    return p;
}

// Entier non signe, cale a droite sur `width` (0 = largeur libre).
char* putUint(char* p, uint32_t v, uint8_t width) {
    char tmp[11];
    uint8_t n = 0;
    do {
        tmp[n++] = static_cast<char>('0' + (v % 10));
        v /= 10;
    } while (v > 0 && n < sizeof(tmp));
    while (width > n) {
        *p++ = ' ';
        --width;
    }
    while (n > 0) {
        *p++ = tmp[--n];
    }
    return p;
}

char* putInt(char* p, int32_t v, uint8_t width) {
    if (v < 0) {
        *p++ = '-';
        v = -v;
    } else {
        *p++ = '+';
    }
    return putUint(p, static_cast<uint32_t>(v), width);
}

// Ecrit une ligne SEULEMENT si elle tombe dans la bande en cours.
//
// Meme raison qu'au renderer de production (ADR 0001) : U8g2 decoupe ce qu'on lui
// envoie, mais l'appel a lieu quand meme. Sept lignes redessinees huit fois, ce
// sont 56 rendus de chaine par image — l'ecran mettait ~250 ms, et la
// consommation du verrou CV attendait d'autant. Chaque ligne ne touche qu'une
// bande ou deux : les autres passes n'ont rien a faire.
void bandLine(uint8_t baseline, const char* text) {
    const uint8_t row = gravity.display.getBufferCurrTileRow();
    const int16_t d0 = static_cast<int16_t>(row) * 8;
    const int16_t d1 = d0 + static_cast<int16_t>(gravity.display.getBufferTileHeight()) * 8 - 1;
    const int16_t last = static_cast<int16_t>(gravity.display.getDisplayHeight()) - 1;
    const int16_t y0 = last - d1;
    const int16_t y1 = last - d0;
    // Boite d'un glyphe 5x7 : de la ligne de base moins l'ascendante au bas.
    if (static_cast<int16_t>(baseline) < y0 || static_cast<int16_t>(baseline) - 7 > y1) {
        return;
    }
    gravity.display.drawStr(0, baseline, text);
}

void drawScreen() {
    char line[40];
    char* p;

    gravity.display.firstPage();
    do {
        bandLine(7, "FLEXSEQ BRINGUP");
        gravity.display.drawHLine(0, 9, 128);

        // CV : valeur du domaine (+/-512), depuis l'echantillon de l'ISR.
        p = line;
        p = putStr(p, "CV1");
        p = putInt(p, flexseq::calibratedFromRaw(flexseq::cv::latestRaw(flexseq::cv::CV1),
                                                calLow[0], calHigh[0], calOffset[0]), 4);
        p = putStr(p, " CV2");
        p = putInt(p, flexseq::calibratedFromRaw(flexseq::cv::latestRaw(flexseq::cv::CV2),
                                                calLow[1], calHigh[1], calOffset[1]), 4);
        *p = '\0';
        bandLine(18, line);

        // Fronts CV : ce compteur prouve la capture, boucle bloquee ou non.
        p = line;
        p = putStr(p, "EDGE 1:");
        p = putUint(p, cvEdges[0], 0);
        p = putStr(p, " 2:");
        p = putUint(p, cvEdges[1], 0);
        *p = '\0';
        bandLine(27, line);

        // R = inversions : combien, et la fourchette de leurs intervalles. Un
        // rebond est une inversion tres rapide ; une inversion voulue par la main
        // est bien plus lente. Le seuil se pose entre les deux.
        // S = le plus court intervalle entre deux crans de MEME sens.
        p = line;
        p = putStr(p, "E");
        p = putInt(p, encoderPos, 3);
        p = putStr(p, "/");
        p = putUint(p, encoderPresses, 0);
        p = putStr(p, " R");
        p = putUint(p, reversals, 0);
        p = putStr(p, " ");
        p = putUint(p, reversals ? reversalMinMs : 0, 0);
        p = putStr(p, "-");
        p = putUint(p, reversalMaxMs, 0);
        p = putStr(p, " S");
        p = putUint(p, sameDirMinMs == 0xFFFF ? 0 : sameDirMinMs, 0);
        *p = '\0';
        bandLine(36, line);

        // Etat DIRECT des boutons (On() lit la broche) et nombre d'appuis vus
        // par les rappels : les deux ensemble separent un probleme de cablage
        // d'une anomalie de la bibliotheque (Button perd un relachement dans sa
        // fenetre d'anti-rebond — anomalie auditee).
        p = line;
        p = putStr(p, "SFT ");
        p = putUint(p, gravity.shift_button.On() ? 1 : 0, 0);
        p = putStr(p, "/");
        p = putUint(p, shiftPresses, 0);
        p = putStr(p, "/");
        p = putUint(p, shiftLongPresses, 0);
        p = putStr(p, " PLY ");
        p = putUint(p, gravity.play_button.On() ? 1 : 0, 0);
        p = putStr(p, "/");
        p = putUint(p, playPresses, 0);
        p = putStr(p, "/");
        p = putUint(p, playLongPresses, 0);
        *p = '\0';
        bandLine(45, line);

        p = line;
        p = putStr(p, gravity.clock.ExternalSource() ? "CLK ext " : "CLK int ");
        p = putUint(p, static_cast<uint32_t>(gravity.clock.Tempo()), 0);
        p = putStr(p, gravity.clock.IsPaused() ? " stop " : " run ");
        p = putUint(p, ticks, 0);
        *p = '\0';
        bandLine(54, line);

        // Sortie active : un chiffre a sa place, un point ailleurs.
        p = line;
        p = putStr(p, "OUT ");
        for (uint8_t i = 0; i < OUT_COUNT; ++i) {
            *p++ = (outStep == i) ? static_cast<char>('1' + i) : '.';
        }
        p = putStr(p, "  PULSE ");
        *p++ = (outStep == OUT_COUNT) ? 'P' : '.';
        *p = '\0';
        bandLine(63, line);
    } while (gravity.display.nextPage());
}

}  // namespace

void setup() {
    gravity.Init();
    gravity.display.setFont(u8g2_font_5x7_tr);

    calLow[0] = gravity.cv1.GetCalibrationLow();
    calHigh[0] = gravity.cv1.GetCalibrationHigh();
    calOffset[0] = gravity.cv1.GetOffset();
    calLow[1] = gravity.cv2.GetCalibrationLow();
    calHigh[1] = gravity.cv2.GetCalibrationHigh();
    calOffset[1] = gravity.cv2.GetOffset();

    // Le meme chemin que la production : ADC sous interruption.
    flexseq::cv::configure(flexseq::cv::CV1, calLow[0], calHigh[0], calOffset[0]);
    flexseq::cv::configure(flexseq::cv::CV2, calLow[1], calHigh[1], calOffset[1]);
    flexseq::cv::start();

    gravity.encoder.AttachRotateHandler(onEncoderRotate);
    gravity.encoder.AttachPressHandler(onEncoderPress);
    gravity.shift_button.AttachPressHandler(onShiftPress);
    gravity.play_button.AttachPressHandler(onPlayPress);
    gravity.shift_button.AttachLongPressHandler(onShiftLongPress);
    gravity.play_button.AttachLongPressHandler(onPlayLongPress);

    gravity.clock.AttachIntHandler(onTick);

    for (uint8_t i = 0; i < OUT_COUNT; ++i) {
        gravity.outputs[i].Low();
    }
    gravity.pulse.Low();
}

void loop() {
    // PAS gravity.Process() : il ferait un analogRead bloquant, en collision avec
    // les conversions de l'ISR (CvSampler.h).
    gravity.shift_button.Process();
    gravity.play_button.Process();
    gravity.encoder.Process();

    // Les fronts CV sont VERROUILLES par l'ISR : ce compteur ne peut pas rater
    // une impulsion, meme pendant un rendu qui bloque la boucle.
    for (uint8_t ch = 0; ch < flexseq::cv::COUNT; ++ch) {
        if (flexseq::cv::takeEdge(ch)) {
            ++cvEdges[ch];
        }
    }

    const uint32_t now = millis();

    // Une sortie a la fois, en tournant : on suit au cable.
    if (now - outLastMs >= OUTPUT_DWELL_MS) {
        outLastMs = now;
        for (uint8_t i = 0; i < OUT_COUNT; ++i) {
            gravity.outputs[i].Low();
        }
        gravity.pulse.Low();

        outStep = static_cast<uint8_t>((outStep + 1) % OUTPUT_STEPS);
        if (outStep < OUT_COUNT) {
            gravity.outputs[outStep].High();
        } else if (outStep == OUT_COUNT) {
            gravity.pulse.High();
        }
    }

    if (now - uiLastMs >= UI_INTERVAL_MS) {
        uiLastMs = now;
        drawScreen();
    }
}
