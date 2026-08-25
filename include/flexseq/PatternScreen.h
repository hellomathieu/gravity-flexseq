#ifndef FLEXSEQ_PATTERN_SCREEN_H
#define FLEXSEQ_PATTERN_SCREEN_H

#include <stdint.h>

#include <flexseq/Pattern.h>

namespace flexseq {

// PatternScreen — rendu PUR de l'ecran EDIT PATTERN (miroir de
// sim/src/sim/OledDisplay.ts). Aucune dependance materielle : il est templatise
// sur le canvas, dont l'interface est CELLE DE U8g2 (drawPixel / drawHLine /
// drawVLine / drawFrame / drawStr / setDrawColor / getStrWidth). Le firmware
// l'instancie donc directement sur `gravity.display` (mode _1_, buffer deja
// alloue par libGravity) : pas d'adaptateur, pas de vtable, 0 octet de RAM.
//
// Le canvas est appele PLUSIEURS FOIS par rafraichissement (boucle
// firstPage()/nextPage() du mode _1_) : cette fonction doit rester pure.
//
// Geometrie : POC Wokwi flexseq-oled-playground/sketch.ino (24 steps en 2
// lignes de 12, pas de 10 px, grille centree, glyphes 5x5, cadre 9x9).
namespace screen {

constexpr uint8_t WIDTH = 128;
constexpr uint8_t HEIGHT = 64;

constexpr uint8_t PER_ROW = 12;
constexpr uint8_t GRID_ROWS = 2;
constexpr uint8_t GRID_STEPS = PER_ROW * GRID_ROWS;
constexpr uint8_t COL_SPACING = 10;
constexpr uint8_t GRID_WIDTH = (PER_ROW - 1) * COL_SPACING;      // 110
constexpr uint8_t COL_X0 = (WIDTH - GRID_WIDTH + 1) / 2;         // 9

// Ecart vertical superieur a celui du sketch (22/35) : le sketch precede les
// ratchets, dont le chiffre se loge sous le step.
constexpr uint8_t ROW_CY_0 = 20;
constexpr uint8_t ROW_CY_1 = 38;

constexpr uint8_t GLYPH_HALF = 2;   // glyphe 5x5
constexpr uint8_t SELECT_HALF = 4;  // cadre 9x9
constexpr uint8_t SELECT_SIZE = 9;
// Chiffre de ratchet : dessine a la main en 3x5 px (et non via la police 5x7,
// deux fois plus encombrante). Loge sous le glyphe, SOUS le cadre du curseur.
constexpr uint8_t DIGIT_W = 3;
constexpr uint8_t DIGIT_H = 5;
constexpr uint8_t DIGIT_DY = 5; // premiere ligne du chiffre, depuis cy

// La barre de mesure DEPASSE le cadre 9x9 du curseur (+/-4) : sans cela, un
// curseur voisin d'une barre l'absorbe visuellement et on ne la distingue plus.
constexpr uint8_t BAR_HALF_H = 6;
constexpr uint8_t BAR_HEIGHT = 2 * BAR_HALF_H + 1; // 13 px

// Ligne de base du titre. 7 et non 8 : les glyphes 5x7 occupent alors y 1..7,
// donc UNE SEULE bande de 8 pixels. A 8 ils debordaient sur la bande voisine, qui
// redessinait tout le titre pour une seule ligne de pixels — 2,1 ms par image
// mesurees (voir tools/run-blocking-probe.sh). Le titre monte d'un pixel ; l'ecart
// au filet passe de 1 a 2 px.
constexpr uint8_t TITLE_BASELINE_Y = 7; // drawStr() aligne sur la LIGNE DE BASE
constexpr uint8_t HEADER_LINE_X = 4;
constexpr uint8_t HEADER_LINE_Y = 10;
constexpr uint8_t HEADER_LINE_W = 120;

constexpr uint8_t GLYPH_ASCENT = 6;
constexpr uint8_t GRID_BOTTOM_Y = ROW_CY_1 + DIGIT_DY + DIGIT_H - 1;
constexpr uint8_t FOOTER_BASELINE_Y = HEIGHT - 1;
constexpr uint8_t FOOTER_TOP_Y = FOOTER_BASELINE_Y - GLYPH_ASCENT;
constexpr uint8_t FOOTER_X = HEADER_LINE_X;

static_assert(
    FOOTER_TOP_Y > GRID_BOTTOM_Y,
    "the footer must sit strictly below every pixel the grid can draw"
);
static_assert(
    FOOTER_TOP_Y / 8 == FOOTER_BASELINE_Y / 8,
    "the footer must fit in a single 8-pixel band for the band skip to apply"
);

inline uint8_t colX(uint8_t index) {
    return static_cast<uint8_t>(COL_X0 + (index % PER_ROW) * COL_SPACING);
}

inline uint8_t rowOf(uint8_t index) {
    return static_cast<uint8_t>(index / PER_ROW);
}

inline uint8_t rowCY(uint8_t index) {
    return rowOf(index) == 0 ? ROW_CY_0 : ROW_CY_1;
}

}  // namespace screen

// La BANDE en cours de rendu, en ordonnees incluses.
//
// Le mode _1_ rend l'image en 8 bandes de 8 pixels, et U8g2 decoupe lui-meme ce
// qu'on lui envoie — mais l'APPEL a lieu quand meme, avec son cout. Or dessiner
// les 24 steps, leurs chiffres et le titre huit fois coutait ~3,9 ms par passage
// de boucle, autant que le transfert I2C lui-meme (mesure : ADR 0001). On ecarte
// donc en amont ce qui ne touche pas la bande.
//
// La bande est passee en PARAMETRE plutot que demandee au canvas : le renderer
// reste pur et sans dependance, et un test peut lui donner n'importe quelle
// bande. Par defaut, tout l'ecran — le rendu complet reste donc possible.
struct Band {
    uint8_t y0;
    uint8_t y1;
};

// Vrai si [top, bottom] rencontre la bande. En int16_t : `cy - BAR_HALF_H`
// deborderait par le bas sur un uint8_t.
inline bool touches(const Band& band, int16_t top, int16_t bottom) {
    return bottom >= static_cast<int16_t>(band.y0) && top <= static_cast<int16_t>(band.y1);
}

// Ce qu'il faut afficher. Rien n'est copie : les cellules sont derivees du
// Pattern a la volee (aucun tableau intermediaire, donc aucune RAM).
struct PatternScreenModel {
    const char* title;
    // Largeur du titre en pixels, pour le centrer. 0 = la demander au canvas.
    //
    // Pourquoi la porter dans le modele : `getStrWidth()` parcourt la chaine et
    // decode la police, ce qui coute ~1 ms par appel sur ce MCU. En mode page, le
    // renderer tourne une fois par bande : la mesurer a chaque fois est du travail
    // repete pour un resultat constant. `PagedScreen` la calcule une fois par
    // image, au moment du gel.
    uint8_t titleWidth;
    const Pattern* pattern;
    uint8_t length;    // LENGTH du channel : au-dela -> simple point
    int8_t cursor;     // step en cours d'edition, -1 pour masquer
    int8_t playhead;   // step joue, -1 pour masquer
    uint8_t barLength; // separation de mesure (0 = aucune) : GRAPHIQUE seule
    const char* footer;
};

namespace detail {

// Chiffres 3x5 des seuls ratchets affichables : 2, 3, 4, 6.
//
// Les 5 lignes de 3 pixels sont empilees dans un uint16 (15 bits), ligne 0 en
// tete. Elles sont ainsi des IMMEDIATS dans le code — donc en Flash, PAS en RAM
// (une table `constexpr` aurait coute 24 o de RAM, la ressource critique).
//
//   2 : ### / ..# / ### / #.. / ###
//   3 : ### / ..# / ### / ..# / ###
//   4 : #.# / #.# / ### / ..# / ..#
//   6 : ### / #.. / ### / #.# / ###
inline uint16_t digitBits(uint8_t code) {
    // Comparaisons plutot qu'un `switch` : avr-gcc emettait pour ce dernier une
    // table de saut de 10 o en RAM (mesure au avr-nm).
    if (code == RATCHET_2) return 0b111001111100111;
    if (code == RATCHET_3) return 0b111001111001111;
    if (code == RATCHET_4) return 0b101101111001001;
    if (code == RATCHET_6) return 0b111100111101111;
    return 0;
}

// Dessine le chiffre du ratchet centre sous le step. Sans effet si le code n'a
// pas de chiffre (aucun ratchet, ou TRIOLET qui a deja son triangle).
template <typename Canvas>
void drawRatchetDigit(Canvas& c, uint8_t cx, uint8_t cy, uint8_t code) {
    const uint16_t bits = digitBits(code);
    if (bits == 0) {
        return;
    }
    const uint8_t x0 = static_cast<uint8_t>(cx - 1); // 3 px centres sur cx
    const uint8_t y0 = static_cast<uint8_t>(cy + screen::DIGIT_DY);
    for (uint8_t r = 0; r < screen::DIGIT_H; ++r) {
        const uint8_t row =
            static_cast<uint8_t>((bits >> (screen::DIGIT_W * (screen::DIGIT_H - 1 - r))) & 0b111);
        for (uint8_t col = 0; col < screen::DIGIT_W; ++col) {
            if (row & (1u << (screen::DIGIT_W - 1 - col))) {
                c.drawPixel(static_cast<uint8_t>(x0 + col), static_cast<uint8_t>(y0 + r));
            }
        }
    }
}

template <typename Canvas>
void drawRing(Canvas& c, uint8_t cx, uint8_t cy) {
    const uint8_t x = static_cast<uint8_t>(cx - screen::GLYPH_HALF);
    const uint8_t y = static_cast<uint8_t>(cy - screen::GLYPH_HALF);
    c.drawHLine(x + 1, y, 3);
    c.drawPixel(x, y + 1);
    c.drawPixel(x + 4, y + 1);
    c.drawPixel(x, y + 2);
    c.drawPixel(x + 4, y + 2);
    c.drawPixel(x, y + 3);
    c.drawPixel(x + 4, y + 3);
    c.drawHLine(x + 1, y + 4, 3);
}

template <typename Canvas>
void drawDisc(Canvas& c, uint8_t cx, uint8_t cy) {
    const uint8_t x = static_cast<uint8_t>(cx - screen::GLYPH_HALF);
    const uint8_t y = static_cast<uint8_t>(cy - screen::GLYPH_HALF);
    c.drawHLine(x + 1, y, 3);
    c.drawHLine(x, y + 1, 5);
    c.drawHLine(x, y + 2, 5);
    c.drawHLine(x, y + 3, 5);
    c.drawHLine(x + 1, y + 4, 3);
}

// Triangle plein : step actif portant le TRIOLET (3 declenchements sur 2 unites).
template <typename Canvas>
void drawTriangle(Canvas& c, uint8_t cx, uint8_t cy) {
    const uint8_t y = static_cast<uint8_t>(cy - screen::GLYPH_HALF);
    c.drawPixel(cx, y);
    c.drawHLine(cx - 1, y + 1, 3);
    c.drawHLine(cx - 1, y + 2, 3);
    c.drawHLine(cx - 2, y + 3, 5);
    c.drawHLine(cx - 2, y + 4, 5);
}

}  // namespace detail

template <typename Canvas>
void drawPatternScreen(Canvas& canvas, const PatternScreenModel& model,
                       Band band = Band{0, screen::HEIGHT - 1}) {
    // En-tete : titre centre + filet. getStrWidth() parcourt la chaine, donc on
    // l'evite aussi quand la bande ne porte pas le titre.
    if (model.title != nullptr && touches(band, 0, screen::TITLE_BASELINE_Y)) {
        const uint8_t w = model.titleWidth != 0
                              ? model.titleWidth
                              : static_cast<uint8_t>(canvas.getStrWidth(model.title));
        canvas.drawStr(static_cast<uint8_t>((screen::WIDTH - w) / 2),
                       screen::TITLE_BASELINE_Y, model.title);
    }
    if (touches(band, screen::HEADER_LINE_Y, screen::HEADER_LINE_Y)) {
        canvas.drawHLine(screen::HEADER_LINE_X, screen::HEADER_LINE_Y, screen::HEADER_LINE_W);
    }

    // Separations de mesure : verticale dans la gouttiere, jamais en bord de ligne.
    if (model.barLength > 0) {
        for (uint8_t k = model.barLength; k < screen::GRID_STEPS;
             k = static_cast<uint8_t>(k + model.barLength)) {
            if (k % screen::PER_ROW == 0) {
                continue;
            }
            const int16_t cy = screen::rowCY(k);
            if (!touches(band, cy - screen::BAR_HALF_H, cy + screen::BAR_HALF_H)) {
                continue;
            }
            const uint8_t bx =
                static_cast<uint8_t>(screen::colX(k) - screen::COL_SPACING / 2);
            canvas.drawVLine(bx, static_cast<uint8_t>(cy - screen::BAR_HALF_H),
                             screen::BAR_HEIGHT);
        }
    }

    // Les steps, ligne par ligne : une ligne hors bande fait sauter ses 12
    // positions d'un coup. C'est l'economie principale — sur les 8 bandes, six
    // ne portent aucune ligne de steps.
    for (uint8_t row = 0; row < 2; ++row) {
        const int16_t cy = row == 0 ? screen::ROW_CY_0 : screen::ROW_CY_1;
        // Extension verticale d'une ligne : du haut du cadre de curseur au bas
        // du chiffre de ratchet.
        if (!touches(band, cy - screen::SELECT_HALF, cy + screen::DIGIT_DY + screen::DIGIT_H)) {
            continue;
        }
        const bool glyphs = touches(band, cy - screen::GLYPH_HALF, cy + screen::GLYPH_HALF);
        const bool digits = touches(band, cy + screen::DIGIT_DY,
                                   cy + screen::DIGIT_DY + screen::DIGIT_H - 1);

        for (uint8_t c = 0; c < screen::PER_ROW; ++c) {
            const uint8_t i = static_cast<uint8_t>(row * screen::PER_ROW + c);
            const uint8_t cx = screen::colX(i);

            if (i >= model.length) {
                if (glyphs) {
                    canvas.drawPixel(cx, static_cast<uint8_t>(cy)); // au-dela de LENGTH
                }
                continue;
            }

            bool active = false;
            uint8_t ratchet = RATCHET_NONE;
            if (model.pattern != nullptr) {
                model.pattern->readStep(i, active);
                ratchet = model.pattern->getRatchet(i);
            }

            if (glyphs) {
                if (active) {
                    if (ratchet == RATCHET_TRIPLET) {
                        detail::drawTriangle(canvas, cx, static_cast<uint8_t>(cy));
                    } else {
                        detail::drawDisc(canvas, cx, static_cast<uint8_t>(cy));
                    }
                } else {
                    detail::drawRing(canvas, cx, static_cast<uint8_t>(cy));
                }
            }

            // Chiffre de ratchet sous le step (le triolet a deja son triangle).
            if (digits && ratchet != RATCHET_NONE && ratchet != RATCHET_TRIPLET) {
                detail::drawRatchetDigit(canvas, cx, static_cast<uint8_t>(cy), ratchet);
            }
        }
    }

    // Cadre d'edition autour du step courant.
    if (model.cursor >= 0 && model.cursor < screen::GRID_STEPS) {
        const uint8_t c = static_cast<uint8_t>(model.cursor);
        const int16_t cy = screen::rowCY(c);
        if (touches(band, cy - screen::SELECT_HALF, cy + screen::SELECT_HALF)) {
            canvas.drawFrame(static_cast<uint8_t>(screen::colX(c) - screen::SELECT_HALF),
                             static_cast<uint8_t>(cy - screen::SELECT_HALF),
                             screen::SELECT_SIZE, screen::SELECT_SIZE);
        }
    }

    if (model.footer != nullptr
        && touches(band, screen::FOOTER_TOP_Y, screen::FOOTER_BASELINE_Y)) {
        canvas.drawStr(screen::FOOTER_X, screen::FOOTER_BASELINE_Y, model.footer);
    }

    // Step joue : pixel central inverse (efface sur un step actif, encre sinon).
    if (model.playhead >= 0 && model.playhead < screen::GRID_STEPS) {
        const uint8_t h = static_cast<uint8_t>(model.playhead);
        const int16_t cy = screen::rowCY(h);
        if (h < model.length && touches(band, cy, cy)) {
            bool active = false;
            if (model.pattern != nullptr) {
                model.pattern->readStep(h, active);
            }
            canvas.setDrawColor(active ? 0 : 1);
            canvas.drawPixel(screen::colX(h), static_cast<uint8_t>(cy));
            canvas.setDrawColor(1);
        }
    }
}

}  // namespace flexseq

#endif // FLEXSEQ_PATTERN_SCREEN_H
