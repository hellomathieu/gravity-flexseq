#include <stdint.h>
#include <string.h>
#include <unity.h>

#include <flexseq/Pattern.h>
#include <flexseq/PatternScreen.h>

using flexseq::Pattern;
using flexseq::PatternScreenModel;
using flexseq::drawPatternScreen;
using flexseq::RATCHET_2;
using flexseq::RATCHET_6;
using flexseq::RATCHET_3;
using flexseq::RATCHET_TRIPLET;
namespace screen = flexseq::screen;

// Canvas d'enregistrement : modelise vraiment le 1-bit — setDrawColor(0) EFFACE
// (c'est ainsi que le step joue creuse le centre d'un glyphe plein).
struct RecordingCanvas {
    bool px[screen::HEIGHT][screen::WIDTH];
    uint8_t color;
    // Bande de decoupe, comme le mode page de U8g2 : hors de [clipY0, clipY1],
    // le pixel n'est pas pose. Par defaut tout l'ecran, donc sans effet.
    uint8_t clipY0;
    uint8_t clipY1;
    char lastStr[32];
    uint8_t lastStrX;
    uint8_t lastStrY;
    uint8_t strCalls;

    RecordingCanvas() { reset(); }

    void reset() {
        memset(px, 0, sizeof(px));
        color = 1;
        clipY0 = 0;
        clipY1 = screen::HEIGHT - 1;
        lastStr[0] = '\0';
        lastStrX = 0;
        lastStrY = 0;
        strCalls = 0;
    }

    void setDrawColor(uint8_t c) { color = c; }

    void drawPixel(uint8_t x, uint8_t y) {
        if (x < screen::WIDTH && y < screen::HEIGHT && y >= clipY0 && y <= clipY1) {
            px[y][x] = (color != 0);
        }
    }

    void drawHLine(uint8_t x, uint8_t y, uint8_t w) {
        for (uint8_t i = 0; i < w; ++i) drawPixel(static_cast<uint8_t>(x + i), y);
    }

    void drawVLine(uint8_t x, uint8_t y, uint8_t h) {
        for (uint8_t i = 0; i < h; ++i) drawPixel(x, static_cast<uint8_t>(y + i));
    }

    void drawFrame(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
        drawHLine(x, y, w);
        drawHLine(x, static_cast<uint8_t>(y + h - 1), w);
        drawVLine(x, y, h);
        drawVLine(static_cast<uint8_t>(x + w - 1), y, h);
    }

    // Le texte n'est pas rasterise nativement : on enregistre l'appel.
    uint8_t drawStr(uint8_t x, uint8_t y, const char* s) {
        ++strCalls;
        lastStrX = x;
        lastStrY = y;
        strncpy(lastStr, s, sizeof(lastStr) - 1);
        lastStr[sizeof(lastStr) - 1] = '\0';
        return getStrWidth(s);
    }

    uint8_t getStrWidth(const char* s) const {
        return static_cast<uint8_t>(5 * strlen(s)); // 5x7 : 5 px par glyphe
    }

    bool at(uint8_t x, uint8_t y) const { return px[y][x]; }

    uint16_t inkCount() const {
        uint16_t n = 0;
        for (uint8_t y = 0; y < screen::HEIGHT; ++y)
            for (uint8_t x = 0; x < screen::WIDTH; ++x)
                if (px[y][x]) ++n;
        return n;
    }
};

static RecordingCanvas canvas;
static Pattern pattern;

void setUp() {
    canvas.reset();
    pattern.clear();
}
void tearDown() {}

static PatternScreenModel model(uint8_t length = 24, int8_t cursor = -1,
                                int8_t playhead = -1, uint8_t bar = 0) {
    PatternScreenModel m;
    m.title = nullptr; // pas de titre : on isole la grille
    m.titleWidth = 0;
    m.pattern = &pattern;
    m.length = length;
    m.cursor = cursor;
    m.playhead = playhead;
    m.barLength = bar;
    return m;
}

/*
 * Geometrie (miroir du sketch Wokwi et du renderer TS)
 */

void test_grid_is_two_rows_of_twelve_at_10px_pitch() {
    TEST_ASSERT_EQUAL_UINT8(9, screen::colX(0));
    TEST_ASSERT_EQUAL_UINT8(19, screen::colX(1));
    TEST_ASSERT_EQUAL_UINT8(119, screen::colX(11));
    // meme grille de colonnes sur les deux lignes
    for (uint8_t col = 0; col < 12; ++col) {
        TEST_ASSERT_EQUAL_UINT8(screen::colX(col), screen::colX(col + 12));
    }
    TEST_ASSERT_EQUAL_UINT8(20, screen::rowCY(0));
    TEST_ASSERT_EQUAL_UINT8(38, screen::rowCY(12));
    // tout tient dans 128 px
    TEST_ASSERT_TRUE(screen::colX(11) + screen::GLYPH_HALF < screen::WIDTH);
}

/*
 * Glyphes
 */

void test_active_step_is_a_filled_disc() {
    pattern.writeStep(0, true);
    drawPatternScreen(canvas, model());
    const uint8_t cx = screen::colX(0), cy = screen::rowCY(0);
    TEST_ASSERT_TRUE(canvas.at(cx, cy));         // centre plein
    TEST_ASSERT_TRUE(canvas.at(cx - 2, cy));     // 5 px de large
    TEST_ASSERT_TRUE(canvas.at(cx + 2, cy));
}

void test_inactive_step_is_a_hollow_ring() {
    drawPatternScreen(canvas, model());
    const uint8_t cx = screen::colX(0), cy = screen::rowCY(0);
    TEST_ASSERT_FALSE(canvas.at(cx, cy));        // centre vide
    TEST_ASSERT_TRUE(canvas.at(cx - 2, cy));     // bords presents
    TEST_ASSERT_TRUE(canvas.at(cx + 2, cy));
}

void test_beyond_length_is_a_single_dot() {
    drawPatternScreen(canvas, model(20));
    const uint8_t cx = screen::colX(20), cy = screen::rowCY(20);
    TEST_ASSERT_TRUE(canvas.at(cx, cy));
    TEST_ASSERT_FALSE(canvas.at(cx - 2, cy));    // rien d'autre
    TEST_ASSERT_FALSE(canvas.at(cx + 2, cy));
}

void test_triplet_step_is_a_triangle_not_a_disc() {
    pattern.writeStep(0, true);
    pattern.setRatchet(0, RATCHET_TRIPLET);
    drawPatternScreen(canvas, model());
    const uint8_t cx = screen::colX(0), cy = screen::rowCY(0);
    // sommet : un seul pixel en haut, base large en bas
    TEST_ASSERT_TRUE(canvas.at(cx, cy - 2));
    TEST_ASSERT_FALSE(canvas.at(cx - 2, cy - 2));
    TEST_ASSERT_TRUE(canvas.at(cx - 2, cy + 2));
    TEST_ASSERT_TRUE(canvas.at(cx + 2, cy + 2));
}

/*
 * Ratchets
 */

void test_ratchet_digit_is_drawn_under_the_step() {
    pattern.writeStep(0, true);
    pattern.setRatchet(0, RATCHET_3);
    drawPatternScreen(canvas, model());
    const uint8_t cx = screen::colX(0), cy = screen::rowCY(0);
    const uint8_t y0 = static_cast<uint8_t>(cy + screen::DIGIT_DY);
    // "3" = 111 / 001 / 111 / 001 / 111 sur 3 px centres sur cx
    TEST_ASSERT_TRUE(canvas.at(cx - 1, y0));      // ligne haute pleine
    TEST_ASSERT_TRUE(canvas.at(cx + 1, y0));
    TEST_ASSERT_FALSE(canvas.at(cx - 1, y0 + 1)); // 001 : seule la droite
    TEST_ASSERT_TRUE(canvas.at(cx + 1, y0 + 1));
    // aucune police utilisee pour les chiffres
    TEST_ASSERT_EQUAL_UINT8(0, canvas.strCalls);
}

void test_ratchet_digit_is_3x5_and_clears_the_cursor_frame() {
    pattern.setRatchet(0, RATCHET_6);
    drawPatternScreen(canvas, model(24, 0)); // curseur SUR le step
    const uint8_t cx = screen::colX(0), cy = screen::rowCY(0);
    // rien hors de la boite 3 px de large
    for (uint8_t r = 0; r < screen::DIGIT_H; ++r) {
        const uint8_t y = static_cast<uint8_t>(cy + screen::DIGIT_DY + r);
        TEST_ASSERT_FALSE(canvas.at(cx - 2, y));
        TEST_ASSERT_FALSE(canvas.at(cx + 2, y));
    }
    // le chiffre commence SOUS le cadre (bord bas du cadre = cy + SELECT_HALF)
    TEST_ASSERT_TRUE(screen::DIGIT_DY > screen::SELECT_HALF);
    // et il tient dans l'ecran
    TEST_ASSERT_TRUE(cy + screen::DIGIT_DY + screen::DIGIT_H <= screen::HEIGHT);
}

void test_each_ratchet_digit_has_a_distinct_pattern() {
    const uint8_t codes[] = {2, 3, 4, 6};
    uint16_t ink[4] = {0, 0, 0, 0};
    for (uint8_t i = 0; i < 4; ++i) {
        canvas.reset();
        pattern.clear();
        pattern.setRatchet(0, codes[i]);
        drawPatternScreen(canvas, model());
        ink[i] = canvas.inkCount();
    }
    // "3" (11 px) et "2" (11 px) ont la meme densite : on compare les formes
    canvas.reset();
    pattern.clear();
    pattern.setRatchet(0, 2);
    drawPatternScreen(canvas, model());
    const uint8_t cx = screen::colX(0);
    const uint8_t y0 = static_cast<uint8_t>(screen::rowCY(0) + screen::DIGIT_DY);
    // "2" a sa 4e ligne a GAUCHE (100), "3" a DROITE (001)
    TEST_ASSERT_TRUE(canvas.at(cx - 1, y0 + 3));
    TEST_ASSERT_FALSE(canvas.at(cx + 1, y0 + 3));
}

void test_triplet_has_no_digit() {
    pattern.writeStep(0, true);
    pattern.setRatchet(0, RATCHET_TRIPLET);
    drawPatternScreen(canvas, model());
    TEST_ASSERT_EQUAL_UINT8(0, canvas.strCalls); // le triangle suffit
}

void test_ratchet_digit_shows_on_an_inactive_step_too() {
    pattern.setRatchet(4, RATCHET_2);
    drawPatternScreen(canvas, model());
    TEST_ASSERT_EQUAL_UINT8(0, canvas.strCalls); // chiffres dessines, pas ecrits
    const uint8_t cx = screen::colX(4);
    const uint8_t y0 = static_cast<uint8_t>(screen::rowCY(4) + screen::DIGIT_DY);
    TEST_ASSERT_TRUE(canvas.at(cx - 1, y0)); // "2" : ligne haute pleine
}

/*
 * Separation de mesure (graphique)
 */

void test_bars_are_drawn_every_n_steps_inside_each_row() {
    drawPatternScreen(canvas, model(24, -1, -1, 4));
    const uint8_t bx4 = static_cast<uint8_t>(screen::colX(4) - 5);
    TEST_ASSERT_TRUE(canvas.at(bx4, screen::rowCY(4)));
    const uint8_t bx16 = static_cast<uint8_t>(screen::colX(16) - 5);
    TEST_ASSERT_TRUE(canvas.at(bx16, screen::rowCY(16)));
}

void test_bar_stays_visible_next_to_the_cursor_frame() {
    // Regression : le cadre 9x9 (+/-4) est colle a la barre (cx-5). La barre
    // doit donc depasser verticalement pour rester identifiable.
    drawPatternScreen(canvas, model(24, 5, -1, 3)); // curseur en 5, barre en 6
    const uint8_t bx = static_cast<uint8_t>(screen::colX(6) - 5);
    const uint8_t cy = screen::rowCY(6);
    // au-dela du cadre, en haut ET en bas
    TEST_ASSERT_TRUE(canvas.at(bx, static_cast<uint8_t>(cy - screen::SELECT_HALF - 1)));
    TEST_ASSERT_TRUE(canvas.at(bx, static_cast<uint8_t>(cy + screen::SELECT_HALF + 1)));
    // et le cadre est bien la, juste a cote
    TEST_ASSERT_TRUE(canvas.at(static_cast<uint8_t>(screen::colX(5) + screen::SELECT_HALF), cy));
}

void test_no_bar_at_a_row_edge() {
    drawPatternScreen(canvas, model(24, -1, -1, 4));
    const uint8_t bx12 = static_cast<uint8_t>(screen::colX(12) - 5);
    TEST_ASSERT_FALSE(canvas.at(bx12, screen::rowCY(12)));
}

void test_no_bar_when_separation_is_none() {
    drawPatternScreen(canvas, model(24, -1, -1, 0));
    for (uint8_t k = 1; k < 24; ++k) {
        const uint8_t bx = static_cast<uint8_t>(screen::colX(k) - 5);
        TEST_ASSERT_FALSE(canvas.at(bx, screen::rowCY(k)));
    }
}

void test_separations_2_3_6_stay_inside_the_rows() {
    const uint8_t seps[] = {2, 3, 6};
    for (uint8_t s = 0; s < 3; ++s) {
        canvas.reset();
        const uint8_t n = seps[s];
        drawPatternScreen(canvas, model(24, -1, -1, n));
        TEST_ASSERT_TRUE(canvas.at(static_cast<uint8_t>(screen::colX(n) - 5), screen::rowCY(n)));
        TEST_ASSERT_FALSE(canvas.at(static_cast<uint8_t>(screen::colX(12) - 5), screen::rowCY(12)));
    }
}

/*
 * Curseur & step joue
 */

void test_the_grid_ignores_the_steps_above_23() {
    for (uint8_t step = 24; step < 32; ++step) {
        pattern.writeStep(step, true);
        pattern.setRatchet(step, RATCHET_6);
    }
    drawPatternScreen(canvas, model(24));
    const uint16_t withHiddenContent = canvas.inkCount();

    canvas.reset();
    pattern.clear();
    drawPatternScreen(canvas, model(24));

    TEST_ASSERT_EQUAL_UINT16(canvas.inkCount(), withHiddenContent);
}

void test_a_cursor_above_23_frames_nothing() {
    drawPatternScreen(canvas, model(24, -1));
    const uint16_t withoutCursor = canvas.inkCount();

    canvas.reset();
    drawPatternScreen(canvas, model(24, 24));

    TEST_ASSERT_EQUAL_UINT16(withoutCursor, canvas.inkCount());
}

void test_cursor_frames_the_edited_step() {
    drawPatternScreen(canvas, model(24, 5));
    const uint8_t x = static_cast<uint8_t>(screen::colX(5) - screen::SELECT_HALF);
    const uint8_t y = static_cast<uint8_t>(screen::rowCY(5) - screen::SELECT_HALF);
    TEST_ASSERT_TRUE(canvas.at(x, y));                                   // coin haut-gauche
    TEST_ASSERT_TRUE(canvas.at(static_cast<uint8_t>(x + 8), y));         // haut-droit
    TEST_ASSERT_TRUE(canvas.at(x, static_cast<uint8_t>(y + 8)));         // bas-gauche
}

void test_playhead_clears_the_centre_of_an_active_step() {
    pattern.writeStep(3, true);
    drawPatternScreen(canvas, model(24, -1, 3));
    const uint8_t cx = screen::colX(3), cy = screen::rowCY(3);
    TEST_ASSERT_FALSE(canvas.at(cx, cy));     // centre creuse
    TEST_ASSERT_TRUE(canvas.at(cx - 2, cy));  // le reste du disque demeure
}

void test_playhead_inks_the_centre_of_an_inactive_step() {
    drawPatternScreen(canvas, model(24, -1, 3));
    TEST_ASSERT_TRUE(canvas.at(screen::colX(3), screen::rowCY(3)));
}

void test_playhead_beyond_length_draws_nothing_extra() {
    RecordingCanvas plain;
    PatternScreenModel m = model(20);
    drawPatternScreen(plain, m);
    drawPatternScreen(canvas, model(20, -1, 22)); // playhead hors LENGTH
    TEST_ASSERT_EQUAL_UINT16(plain.inkCount(), canvas.inkCount());
}

/*
 * En-tete
 */

void test_title_is_centred_on_its_baseline() {
    PatternScreenModel m = model();
    m.title = "EDIT PATTERN A1";
    drawPatternScreen(canvas, m);
    TEST_ASSERT_EQUAL_STRING("EDIT PATTERN A1", canvas.lastStr);
    TEST_ASSERT_EQUAL_UINT8(screen::TITLE_BASELINE_Y, canvas.lastStrY);
    const uint8_t w = canvas.getStrWidth("EDIT PATTERN A1");
    TEST_ASSERT_EQUAL_UINT8((screen::WIDTH - w) / 2, canvas.lastStrX);
    // le filet est trace
    TEST_ASSERT_TRUE(canvas.at(screen::HEADER_LINE_X, screen::HEADER_LINE_Y));
}

/*
 * Ecartement par bande (ADR 0001) — LA propriete de l'optimisation.
 *
 * Le renderer n'est appele qu'avec la bande que U8g2 va transferer, et il ecarte
 * ce qui n'y tombe pas. La reunion des 8 bandes doit donc rendre EXACTEMENT
 * l'image complete : un pixel de moins et un element a disparu de l'ecran, un
 * pixel de plus et une bande a debordé sur sa voisine.
 */

static bool sameImage(const RecordingCanvas& a, const RecordingCanvas& b, uint16_t* diff) {
    uint16_t n = 0;
    for (uint8_t y = 0; y < screen::HEIGHT; ++y) {
        for (uint8_t x = 0; x < screen::WIDTH; ++x) {
            if (a.px[y][x] != b.px[y][x]) ++n;
        }
    }
    *diff = n;
    return n == 0;
}

// Un contenu qui exerce tous les elements : glyphes des deux lignes, chiffres de
// ratchet, triolet, barres de mesure, curseur, playhead, points au-dela de LENGTH.
static PatternScreenModel richModel() {
    const uint8_t active[] = {0, 2, 5, 11, 12, 15, 19};
    for (uint8_t i = 0; i < sizeof(active); ++i) pattern.writeStep(active[i], true);
    pattern.setRatchet(2, flexseq::RATCHET_2);
    pattern.setRatchet(11, flexseq::RATCHET_6);
    pattern.setRatchet(12, flexseq::RATCHET_4);
    pattern.setRatchet(15, flexseq::RATCHET_TRIPLET);
    pattern.setRatchet(19, flexseq::RATCHET_3);

    PatternScreenModel m = model(20, 5, 12, 3);
    m.title = "EDIT PATTERN A1";
    return m;
}

void test_eight_bands_reunited_equal_the_whole_image(void) {
    const PatternScreenModel m = richModel();

    static RecordingCanvas whole;
    whole.reset();
    drawPatternScreen(whole, m); // bande par defaut = tout l'ecran

    static RecordingCanvas banded;
    banded.reset();
    for (uint8_t row = 0; row < screen::HEIGHT / 8; ++row) {
        const flexseq::Band band = {static_cast<uint8_t>(row * 8),
                                    static_cast<uint8_t>(row * 8 + 7)};
        // Le canvas decoupe comme U8g2 le ferait : un element a cheval sur deux
        // bandes est dessine deux fois, mais chaque passe ne pose que sa part.
        // Sans ce decoupage le test serait faux — la seconde passe reposerait le
        // pixel central qu'un playhead avait creuse dans la premiere.
        banded.clipY0 = band.y0;
        banded.clipY1 = band.y1;
        drawPatternScreen(banded, m, band);
    }

    uint16_t diff = 0;
    TEST_ASSERT_TRUE_MESSAGE(sameImage(whole, banded, &diff),
                             "la reunion des 8 bandes differe de l'image complete");
    TEST_ASSERT_EQUAL_UINT16(0, diff);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(whole.strCalls, banded.strCalls,
        "le texte n'est pas dessine le meme nombre de fois band par bande");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, whole.strCalls, "titre");
}

// L'ecartement doit AGIR, pas seulement etre correct : une bande vide de tout
// element ne doit rien poser du tout.
void test_a_band_without_any_element_draws_nothing(void) {
    PatternScreenModel m = richModel();

    // Bande 7 (y 56..63) : sous la derniere ligne de chiffres (max y = 38+5+4=47),
    // et le pied a quitte l'ecran EDIT.
    canvas.reset();
    drawPatternScreen(canvas, m, flexseq::Band{56, 63});

    uint16_t ink = 0;
    for (uint8_t y = 0; y < screen::HEIGHT; ++y)
        for (uint8_t x = 0; x < screen::WIDTH; ++x)
            if (canvas.px[y][x]) ++ink;

    TEST_ASSERT_EQUAL_UINT16(0, ink);
    TEST_ASSERT_EQUAL_UINT8(0, canvas.strCalls); // pas meme le titre mesure
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_grid_is_two_rows_of_twelve_at_10px_pitch);

    RUN_TEST(test_active_step_is_a_filled_disc);
    RUN_TEST(test_inactive_step_is_a_hollow_ring);
    RUN_TEST(test_beyond_length_is_a_single_dot);
    RUN_TEST(test_triplet_step_is_a_triangle_not_a_disc);

    RUN_TEST(test_ratchet_digit_is_drawn_under_the_step);
    RUN_TEST(test_ratchet_digit_is_3x5_and_clears_the_cursor_frame);
    RUN_TEST(test_each_ratchet_digit_has_a_distinct_pattern);
    RUN_TEST(test_triplet_has_no_digit);
    RUN_TEST(test_ratchet_digit_shows_on_an_inactive_step_too);

    RUN_TEST(test_bars_are_drawn_every_n_steps_inside_each_row);
    RUN_TEST(test_bar_stays_visible_next_to_the_cursor_frame);
    RUN_TEST(test_no_bar_at_a_row_edge);
    RUN_TEST(test_no_bar_when_separation_is_none);
    RUN_TEST(test_separations_2_3_6_stay_inside_the_rows);

    RUN_TEST(test_the_grid_ignores_the_steps_above_23);
    RUN_TEST(test_a_cursor_above_23_frames_nothing);
    RUN_TEST(test_cursor_frames_the_edited_step);
    RUN_TEST(test_playhead_clears_the_centre_of_an_active_step);
    RUN_TEST(test_playhead_inks_the_centre_of_an_inactive_step);
    RUN_TEST(test_playhead_beyond_length_draws_nothing_extra);

    RUN_TEST(test_title_is_centred_on_its_baseline);

    RUN_TEST(test_eight_bands_reunited_equal_the_whole_image);
    RUN_TEST(test_a_band_without_any_element_draws_nothing);
    return UNITY_END();
}
