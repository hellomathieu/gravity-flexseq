#include <flexseq/OriginalFonts.h>
#include <stdint.h>
#include <string.h>
#include <unity.h>

#include <flexseq/MainScreen.h>
#include <flexseq/MainScreenModel.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>
#include <flexseq/UiController.h>

using flexseq::Band;
using flexseq::MainScreenModel;
using flexseq::drawMainScreen;
namespace screen = flexseq::screen;
namespace ms = flexseq::mainscreen;

void setUp() {}
void tearDown() {}

namespace {

struct Call {
    uint8_t x;
    uint8_t y;
    char text[16];   // SUBDIVISION et SKIP CHANCE font onze caracteres
};

struct RecordingCanvas {
    bool px[screen::HEIGHT][screen::WIDTH];
    uint8_t color;
    uint8_t clipY0;
    uint8_t clipY1;
    Call calls[16];
    uint8_t callCount;

    RecordingCanvas() { reset(); }

    void reset() {
        memset(px, 0, sizeof(px));
        color = 1;
        clipY0 = 0;
        clipY1 = screen::HEIGHT - 1;
        callCount = 0;
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

    void drawBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
        for (uint8_t i = 0; i < h; ++i) drawHLine(x, static_cast<uint8_t>(y + i), w);
    }

    uint8_t drawStr(uint8_t x, uint8_t y, const char* s) {
        if (callCount < 16) {
            Call& c = calls[callCount++];
            c.x = x;
            c.y = y;
            strncpy(c.text, s, sizeof(c.text) - 1);
            c.text[sizeof(c.text) - 1] = '\0';
        }
        return getStrWidth(s);
    }

    // Le double modelise DEUX polices : 5 px par caractere pour les etiquettes,
    // 13 pour le gros parametre. Sans cela un test de centrage mesurerait faux.
    void setFont(const uint8_t* font) { bigFont = (font == flexseq::FONT_STK_L); }

    uint8_t getStrWidth(const char* s) const {
        return static_cast<uint8_t>((bigFont ? 13 : 5) * strlen(s));
    }

    bool bigFont = false;

    bool at(uint8_t x, uint8_t y) const { return px[y][x]; }

    const Call* find(const char* text) const {
        for (uint8_t i = 0; i < callCount; ++i) {
            if (strcmp(calls[i].text, text) == 0) return &calls[i];
        }
        return nullptr;
    }

    // Le meme texte peut apparaitre deux fois — le « 4 » d'un onglet et le « 4 »
    // d'une separation de mesure. On leve l'ambiguite par la ligne de base.
    const Call* findOnBaseline(const char* text, uint8_t baseline) const {
        for (uint8_t i = 0; i < callCount; ++i) {
            if (calls[i].y == baseline && strcmp(calls[i].text, text) == 0) {
                return &calls[i];
            }
        }
        return nullptr;
    }

    uint16_t inkInRows(uint8_t y0, uint8_t y1) const {
        uint16_t n = 0;
        for (uint8_t y = y0; y <= y1; ++y)
            for (uint8_t x = 0; x < screen::WIDTH; ++x)
                if (px[y][x]) ++n;
        return n;
    }
};

RecordingCanvas canvas;

MainScreenModel channelTab(uint8_t tab = 1) {
    MainScreenModel m{};
    m.tab = tab;
    m.insideTab = false;
    m.cursor = 0;
    m.fieldOpen = false;
    m.fieldCount = 4;
    m.patternIndex = 0;
    m.length = 16;
    m.subdiv = 1;
    m.barLength = 4;
    m.tempo = 120;
    m.clockSource = 0;
    m.mode = static_cast<uint8_t>(flexseq::MODE_SEQ);
    m.offset = 0;
    m.skipChance = 0;
    m.mainParameter = flexseq::MAIN_PATTERN;
    return m;
}

// L'onglet d'un channel en CLOCK ou en RANDOM : les trois lignes de l'original.
MainScreenModel legacyTab(flexseq::ChannelMode mode, uint8_t cursor = 0,
                          bool insideTab = true, bool fieldOpen = false) {
    MainScreenModel m = channelTab();
    m.mode = static_cast<uint8_t>(mode);
    m.fieldCount = 3;
    m.cursor = cursor;
    m.insideTab = insideTab;
    m.fieldOpen = fieldOpen;
    m.mainParameter = mode == flexseq::MODE_CLOCK
        ? flexseq::MAIN_SUBDIV : flexseq::MAIN_SKIP_CHANCE;
    m.subdiv = -4;
    m.offset = 3;
    m.skipChance = 3;
    return m;
}

MainScreenModel clockTab() {
    MainScreenModel m = channelTab(0);
    m.fieldCount = 2;
    return m;
}

MainScreenModel settingsTab() {
    MainScreenModel m = channelTab(ms::TAB_COUNT - 1);
    m.fieldCount = 0;
    return m;
}

}  // namespace

/*
 * Barre d'onglets
 */

void test_the_tab_bar_has_nine_evenly_spaced_slots() {
    TEST_ASSERT_EQUAL_UINT8(9, ms::TAB_COUNT);
    TEST_ASSERT_EQUAL_UINT8(12, ms::TAB_SLOT_W);
    TEST_ASSERT_EQUAL_UINT8(6, ms::tabCentreX(0));
    TEST_ASSERT_EQUAL_UINT8(102, ms::tabCentreX(8));
    for (uint8_t tab = 1; tab < ms::TAB_COUNT; ++tab) {
        TEST_ASSERT_EQUAL_UINT8(12, ms::tabCentreX(tab) - ms::tabCentreX(tab - 1));
    }
}

void test_the_bar_no_longer_fills_the_width_of_the_screen() {
    TEST_ASSERT_EQUAL_UINT8(108, ms::TAB_SLOT_W * ms::TAB_COUNT);
    TEST_ASSERT_TRUE(ms::TAB_SLOT_W * ms::TAB_COUNT < screen::WIDTH);
}

void test_the_roles_of_the_nine_tabs_are_named() {
    TEST_ASSERT_EQUAL_UINT8(0, ms::TAB_CLOCK);
    TEST_ASSERT_EQUAL_UINT8(1, ms::TAB_FIRST_CHANNEL);
    TEST_ASSERT_EQUAL_UINT8(6, ms::TAB_LAST_CHANNEL);
    TEST_ASSERT_EQUAL_UINT8(7, ms::TAB_PATTERNS);
    TEST_ASSERT_EQUAL_UINT8(8, ms::TAB_SETTINGS);
}

void test_the_patterns_tab_is_not_a_channel() {
    MainScreenModel m = channelTab(7);
    TEST_ASSERT_FALSE(flexseq::detail::isChannelTab(m));
    m.tab = 8;
    TEST_ASSERT_FALSE(flexseq::detail::isChannelTab(m));
    for (uint8_t tab = 1; tab <= 6; ++tab) {
        m.tab = tab;
        TEST_ASSERT_TRUE(flexseq::detail::isChannelTab(m));
    }
}

// Les trois glyphes de la barre et les six chiffres doivent occuper LA MEME
// bande. Avant le 2026-09-09 chacun avait son ancrage : l'horloge etait
// dessinee sur 7 rangees dans une bande qui n'en porte que 5, donc rognee par
// le bas de l'ecran, et PATTERNS et les reglages flottaient deux pixels plus
// bas que les chiffres.
void test_the_drawn_glyphs_of_the_bar_share_the_band_of_the_digits() {
    canvas.reset();
    drawMainScreen(canvas, channelTab(2));
    const uint8_t glyphTabs[2] = {ms::TAB_PATTERNS, ms::TAB_SETTINGS};
    for (uint8_t i = 0; i < 2; ++i) {
        const uint8_t tab = glyphTabs[i];
        const uint8_t x0 = ms::tabSlotX(tab);
        int top = -1, bottom = -1;
        for (uint8_t y = ms::TAB_BOX_Y; y <= ms::TAB_BASELINE_Y; ++y) {
            for (uint8_t dx = 0; dx < ms::TAB_SLOT_W; ++dx) {
                if (canvas.at(static_cast<uint8_t>(x0 + dx), y)) {
                    if (top < 0) top = y;
                    bottom = y;
                }
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(top >= 0, "chaque creneau porte de l encre");
        TEST_ASSERT_EQUAL_INT_MESSAGE(58, top, "meme rangee du haut pour tous");
        TEST_ASSERT_EQUAL_INT_MESSAGE(62, bottom, "meme rangee du bas pour tous");
    }
}

void test_the_clock_tab_draws_the_glyph_of_the_original() {
    TEST_ASSERT_EQUAL_INT('w', ms::VELVETSCREEN_CLOCK);
    canvas.reset();
    drawMainScreen(canvas, channelTab());
    const Call* call = canvas.findOnBaseline("w", ms::TAB_BASELINE_Y);
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_EQUAL_UINT8(ms::tabCentreX(ms::TAB_CLOCK) - 2, call->x);
}

void test_the_patterns_glyph_is_two_rows_of_three_single_dots() {
    canvas.reset();
    drawMainScreen(canvas, channelTab());
    const uint8_t cx = ms::tabCentreX(ms::TAB_PATTERNS);
    const uint8_t dots[3] = {static_cast<uint8_t>(cx - 3), cx,
                             static_cast<uint8_t>(cx + 3)};
    for (uint8_t i = 0; i < 3; ++i) {
        TEST_ASSERT_TRUE_MESSAGE(canvas.at(dots[i], 58), "rangee du haut");
        TEST_ASSERT_TRUE_MESSAGE(canvas.at(dots[i], 62), "rangee du bas");
        TEST_ASSERT_FALSE_MESSAGE(canvas.at(dots[i], 59), "un point tient une rangee");
        TEST_ASSERT_FALSE_MESSAGE(canvas.at(dots[i], 60), "rien entre les rangees");
        TEST_ASSERT_FALSE_MESSAGE(canvas.at(dots[i], 61), "un point tient une rangee");
    }
    TEST_ASSERT_FALSE_MESSAGE(canvas.at(static_cast<uint8_t>(cx - 2), 58),
                              "les points sont espaces de trois pixels");
    TEST_ASSERT_FALSE_MESSAGE(canvas.at(static_cast<uint8_t>(cx - 1), 58),
                              "les points sont espaces de trois pixels");
}

void test_the_settings_glyph_is_two_sliders_seven_pixels_wide() {
    TEST_ASSERT_EQUAL_UINT8(7, ms::TAB_WIDE_GLYPH_W);
    canvas.reset();
    drawMainScreen(canvas, channelTab());
    const uint8_t cx = ms::tabCentreX(ms::TAB_SETTINGS);
    const uint8_t x = static_cast<uint8_t>(cx - 3);
    for (uint8_t dx = 0; dx < 7; ++dx) {
        const uint8_t px = static_cast<uint8_t>(x + dx);
        TEST_ASSERT_TRUE_MESSAGE(canvas.at(px, 59), "premiere glissiere");
        TEST_ASSERT_TRUE_MESSAGE(canvas.at(px, 61), "seconde glissiere");
        TEST_ASSERT_FALSE_MESSAGE(canvas.at(px, 60), "rien entre les glissieres");
    }
    TEST_ASSERT_TRUE_MESSAGE(canvas.at(cx, 58), "molette de la premiere");
    TEST_ASSERT_TRUE_MESSAGE(canvas.at(static_cast<uint8_t>(x + 1), 62),
                             "molette de la seconde");
    TEST_ASSERT_FALSE_MESSAGE(canvas.at(x, 58), "une seule molette par glissiere");
    TEST_ASSERT_FALSE_MESSAGE(canvas.at(x, 62), "une seule molette par glissiere");
    TEST_ASSERT_FALSE_MESSAGE(canvas.at(static_cast<uint8_t>(x + 7), 59),
                              "la glissiere ne depasse pas sept pixels");
}

void test_the_transport_indicator_shows_stop_when_the_transport_is_stopped() {
    canvas.reset();
    MainScreenModel m = channelTab();
    m.running = false;
    drawMainScreen(canvas, m);
    const Call* call = canvas.findOnBaseline("t", ms::TAB_BASELINE_Y);
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_EQUAL_UINT8(121, call->x);
    TEST_ASSERT_NULL(canvas.findOnBaseline("r", ms::TAB_BASELINE_Y));
}

void test_the_transport_indicator_shows_play_when_the_transport_runs() {
    canvas.reset();
    MainScreenModel m = channelTab();
    m.running = true;
    drawMainScreen(canvas, m);
    const Call* call = canvas.findOnBaseline("r", ms::TAB_BASELINE_Y);
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_EQUAL_UINT8(122, call->x);
    TEST_ASSERT_NULL(canvas.findOnBaseline("t", ms::TAB_BASELINE_Y));
}

void test_the_transport_indicator_is_drawn_on_the_internal_clock_only() {
    for (uint8_t source = 1; source <= 5; ++source) {
        canvas.reset();
        MainScreenModel m = channelTab();
        m.clockSource = source;
        m.running = true;
        drawMainScreen(canvas, m);
        TEST_ASSERT_NULL_MESSAGE(canvas.findOnBaseline("r", ms::TAB_BASELINE_Y),
                                 "aucun indicateur hors horloge interne");
        m.running = false;
        canvas.reset();
        drawMainScreen(canvas, m);
        TEST_ASSERT_NULL_MESSAGE(canvas.findOnBaseline("t", ms::TAB_BASELINE_Y),
                                 "aucun indicateur hors horloge interne");
    }
}

void test_the_transport_indicator_sits_outside_the_nine_slots() {
    TEST_ASSERT_EQUAL_UINT8(121, ms::TRANSPORT_STOP_X);
    TEST_ASSERT_EQUAL_UINT8(122, ms::TRANSPORT_PLAY_X);
    TEST_ASSERT_EQUAL_UINT8(108, ms::TAB_SLOT_W * ms::TAB_COUNT);
    TEST_ASSERT_EQUAL_INT('r', ms::VELVETSCREEN_PLAY);
    TEST_ASSERT_EQUAL_INT('t', ms::VELVETSCREEN_STOP);
}

void test_the_glyph_band_of_the_bar_is_never_clipped() {
    TEST_ASSERT_EQUAL_UINT8(58, ms::TAB_GLYPH_TOP_Y);
    TEST_ASSERT_EQUAL_UINT8(5, ms::TAB_GLYPH_H);
    TEST_ASSERT_EQUAL_UINT8(62, ms::TAB_GLYPH_TOP_Y + ms::TAB_GLYPH_H - 1);
    TEST_ASSERT_TRUE(ms::TAB_GLYPH_TOP_Y + ms::TAB_GLYPH_H - 1 <= screen::HEIGHT - 1);
}

void test_the_six_channel_digits_sit_at_their_slot_centres() {
    canvas.reset();
    drawMainScreen(canvas, channelTab());
    for (uint8_t channel = 1; channel <= 6; ++channel) {
        char expected[2] = {static_cast<char>('0' + channel), '\0'};
        const Call* call = canvas.findOnBaseline(expected, ms::TAB_BASELINE_Y);
        TEST_ASSERT_NOT_NULL(call);
        TEST_ASSERT_EQUAL_UINT8(ms::tabCentreX(channel) - 2, call->x);
    }
}

void test_the_selected_tab_is_inverted() {
    canvas.reset();
    MainScreenModel m = channelTab(3);
    drawMainScreen(canvas, m);
    const uint8_t x = ms::tabSlotX(3);
    TEST_ASSERT_TRUE(canvas.at(x, ms::TAB_BOX_Y));
    TEST_ASSERT_TRUE(canvas.at(static_cast<uint8_t>(x + ms::TAB_SLOT_W - 1), ms::TAB_BOX_Y));
    // La case voisine n'est pas remplie.
    TEST_ASSERT_FALSE(canvas.at(static_cast<uint8_t>(ms::tabSlotX(4) + 1), ms::TAB_BOX_Y));
}

void test_the_clock_and_settings_tabs_are_glyphs_not_digits() {
    canvas.reset();
    drawMainScreen(canvas, channelTab());
    TEST_ASSERT_NULL(canvas.find("0"));
    TEST_ASSERT_NULL(canvas.find("7"));
    TEST_ASSERT_NULL(canvas.find("8"));
    TEST_ASSERT_TRUE(canvas.inkInRows(ms::TAB_TOP_Y, ms::TAB_BASELINE_Y) > 0);
    const uint8_t roles[2] = {ms::TAB_PATTERNS, ms::TAB_SETTINGS};
    const char* names[2] = {"glyphe de patterns absent",
                            "glyphe de reglages absent"};
    for (uint8_t r = 0; r < 2; ++r) {
        bool ink = false;
        const uint8_t x0 = ms::tabSlotX(roles[r]);
        for (uint8_t y = ms::TAB_TOP_Y; y <= ms::TAB_BASELINE_Y; ++y) {
            for (uint8_t dx = 0; dx < ms::TAB_SLOT_W; ++dx) {
                if (canvas.at(static_cast<uint8_t>(x0 + dx), y)) ink = true;
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(ink, names[r]);
    }
}

/*
 * Contenu d'un onglet de channel
 */

void test_the_pattern_name_is_the_main_parameter_of_a_seq_tab() {
    canvas.reset();
    MainScreenModel m = channelTab();
    m.patternIndex = 9;
    drawMainScreen(canvas, m);
    const Call* call = canvas.findOnBaseline("B2", ms::MAIN_VALUE_BASELINE_Y);
    TEST_ASSERT_NOT_NULL_MESSAGE(call, "le nom du pattern est le gros parametre");
    canvas.setFont(flexseq::FONT_STK_L);
    const uint8_t w = canvas.getStrWidth("B2");
    canvas.setFont(flexseq::FONT_VELVETSCREEN);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::MAIN_CENTRE_X - w / 2, call->x,
        "il est centre sur la moitie gauche, comme dans l original");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("PATTERN"), "et son etiquette est PATTERN");
}

// Un seul jeu de glyphes : le renderer ne change JAMAIS de police, donc rien ne
// peut rester dans le mauvais etat pour l'element suivant.
void test_the_renderer_restores_the_label_font_after_the_main_parameter() {
    canvas.reset();
    drawMainScreen(canvas, channelTab());
    TEST_ASSERT_FALSE_MESSAGE(canvas.bigFont,
        "la grande police ne reste pas armee pour l element suivant");
    const Call* digit = canvas.findOnBaseline("1", ms::TAB_BASELINE_Y);
    TEST_ASSERT_NOT_NULL_MESSAGE(digit, "le chiffre de l onglet est dessine");
}

// L'espace laisse libre sous la seconde rangee est REEL : c'est la place des
// champs de source et destination CV du PRD 10.2.
void test_the_space_below_the_rows_is_reserved_and_empty() {
    canvas.reset();
    drawMainScreen(canvas, channelTab());
    const uint8_t top = ms::ROW_B_BOX_Y + ms::ROW_BOX_H;
    TEST_ASSERT_TRUE_MESSAGE(ms::RULE_Y - top >= 2 * ms::ROW_BOX_H,
        "il ne reste pas la place de deux rangees pour les champs CV");
    TEST_ASSERT_EQUAL_UINT16(0, canvas.inkInRows(top, ms::RULE_Y - 1));
}

void test_a_seq_tab_draws_its_three_lines_at_the_geometry_of_the_original() {
    canvas.reset();
    drawMainScreen(canvas, channelTab());

    const Call* mode = canvas.find("MODE:");
    TEST_ASSERT_NOT_NULL_MESSAGE(mode, "la ligne 1 porte MODE:");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::LINE_LABEL_X, mode->x, "etiquette a x=62");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::LINE_0_BASELINE_Y, mode->y, "ligne de base 8");

    const Call* edit = canvas.find("EDIT");
    TEST_ASSERT_NOT_NULL_MESSAGE(edit, "la ligne 2 porte EDIT");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::LINE_LABEL_X, edit->x, "etiquette a x=62");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::LINE_1_BASELINE_Y, edit->y, "ligne de base 19");

    const Call* config = canvas.find("CONFIG");
    TEST_ASSERT_NOT_NULL_MESSAGE(config, "la ligne 3 porte CONFIG");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::LINE_2_BASELINE_Y, config->y, "ligne de base 30");

    TEST_ASSERT_NULL_MESSAGE(canvas.find("OFF"),
        "EDIT et CONFIG sont des entrees, elles ne portent aucune valeur");
}

void test_subdiv_is_shown_the_gravity_way() {
    char text[6];
    flexseq::detail::subdivLabel(1, text);
    TEST_ASSERT_EQUAL_STRING("/1", text);
    flexseq::detail::subdivLabel(128, text);
    TEST_ASSERT_EQUAL_STRING("/128", text);
    flexseq::detail::subdivLabel(-24, text);
    TEST_ASSERT_EQUAL_STRING("x24", text);
    flexseq::detail::subdivLabel(-2, text);
    TEST_ASSERT_EQUAL_STRING("x2", text);
}

void test_a_separation_of_none_is_shown_as_a_dash() {
    char text[4];
    flexseq::detail::barLabel(0, text);
    TEST_ASSERT_EQUAL_STRING("-", text);
    flexseq::detail::barLabel(6, text);
    TEST_ASSERT_EQUAL_STRING("6", text);
}

void test_every_pattern_of_the_bank_has_a_distinct_name() {
    char seen[16][3];
    for (uint8_t index = 0; index < 16; ++index) {
        flexseq::detail::patternName(static_cast<int8_t>(index), seen[index]);
    }
    TEST_ASSERT_EQUAL_STRING("A1", seen[0]);
    TEST_ASSERT_EQUAL_STRING("A8", seen[7]);
    TEST_ASSERT_EQUAL_STRING("B1", seen[8]);
    TEST_ASSERT_EQUAL_STRING("B8", seen[15]);
    for (uint8_t a = 0; a < 16; ++a) {
        for (uint8_t b = static_cast<uint8_t>(a + 1); b < 16; ++b) {
            TEST_ASSERT_TRUE(strcmp(seen[a], seen[b]) != 0);
        }
    }
}

/*
 * Onglet horloge et onglet reglages
 */

void test_the_clock_tab_shows_the_tempo_big_and_the_source() {
    canvas.reset();
    MainScreenModel m = clockTab();
    m.tempo = 240;
    m.clockSource = 5;
    drawMainScreen(canvas, m);

    const Call* headline = canvas.findOnBaseline("240", ms::HEADLINE_BASELINE_Y);
    TEST_ASSERT_NOT_NULL(headline);
    TEST_ASSERT_NOT_NULL(canvas.find("SRC"));
    TEST_ASSERT_NOT_NULL(canvas.find("MIDI"));
    TEST_ASSERT_NULL_MESSAGE(canvas.find("LEN"), "aucun reglage de channel dans l'onglet horloge");
}

void test_the_six_clock_sources_have_distinct_labels() {
    for (uint8_t a = 0; a < 6; ++a) {
        for (uint8_t b = static_cast<uint8_t>(a + 1); b < 6; ++b) {
            TEST_ASSERT_TRUE(strcmp(flexseq::detail::sourceLabel(a),
                                    flexseq::detail::sourceLabel(b)) != 0);
        }
    }
    TEST_ASSERT_EQUAL_STRING("INT", flexseq::detail::sourceLabel(0));
    TEST_ASSERT_EQUAL_STRING("MIDI", flexseq::detail::sourceLabel(5));
}

void test_the_settings_tab_is_empty_while_it_is_deferred() {
    canvas.reset();
    drawMainScreen(canvas, settingsTab());
    TEST_ASSERT_NULL(canvas.find("LEN"));
    TEST_ASSERT_NULL(canvas.find("SRC"));
    // La barre d'onglets et le filet restent la : seul le contenu manque.
    TEST_ASSERT_EQUAL_UINT16(0, canvas.inkInRows(ms::HEADLINE_BOX_Y, ms::ROW_B_BOX_Y - 1));
    TEST_ASSERT_TRUE(canvas.at(ms::RULE_X, ms::RULE_Y));
    TEST_ASSERT_TRUE(canvas.inkInRows(ms::TAB_TOP_Y, ms::TAB_BASELINE_Y) > 0);
}

/*
 * Curseur et champ ouvert
 */

static uint16_t inkInBoxOf(const RecordingCanvas& c, uint8_t x0, uint8_t w,
                           uint8_t baseline) {
    uint16_t ink = 0;
    for (uint8_t y = static_cast<uint8_t>(baseline - 6); y <= baseline; ++y) {
        for (uint8_t x = x0; x < x0 + w; ++x) {
            if (c.at(x, y)) ++ink;
        }
    }
    return ink;
}

// La rangee du HAUT de la boite de surbrillance : les glyphes de velvetscreen
// occupent base-5 a base-1, donc seule l inversion encre base-6. C est le meme
// temoin que celui de la sonde de gestes, et il ne confond pas un pave avec du
// texte.
uint16_t cursorTopRowInk(const RecordingCanvas& c, uint8_t x0, uint8_t w,
                         uint8_t baseline) {
    const uint8_t y = static_cast<uint8_t>(baseline - flexseq::FONT_VELVETSCREEN_HEIGHT - 1);
    uint16_t ink = 0;
    for (uint8_t x = x0; x < x0 + w; ++x) {
        if (c.at(x, y)) ++ink;
    }
    return ink;
}

uint16_t lineTopRowInk(const RecordingCanvas& c, uint8_t line) {
    return cursorTopRowInk(c, ms::LINE_LABEL_X - 1, 40,
                           static_cast<uint8_t>(ms::LINE_0_BASELINE_Y
                                                + line * ms::LINE_SPACING_Y));
}

uint16_t bigLabelTopRowInk(const RecordingCanvas& c) {
    return cursorTopRowInk(c, 0, ms::LINE_LABEL_X - 2, ms::MAIN_LABEL_BASELINE_Y);
}

void test_the_cursor_marks_the_line_it_is_on_and_no_other() {
    canvas.reset();
    MainScreenModel m = channelTab();
    m.insideTab = true;
    // PRD 5.0 amendement 1ter : la position 0 nomme MODE, qui est la PREMIERE
    // ligne. La grande valeur ne prend plus le curseur.
    m.cursor = 0;
    drawMainScreen(canvas, m);
    TEST_ASSERT_TRUE_MESSAGE(lineTopRowInk(canvas, 0) > 0,
        "la ligne du champ nomme porte le pave");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, lineTopRowInk(canvas, 1), "et elle seule");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, lineTopRowInk(canvas, 2), "et elle seule");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, bigLabelTopRowInk(canvas),
        "la grande valeur ne porte rien : le curseur n y est pas");
}

// PRD 5.0 amendement 1ter : aucune position du curseur ne marque la grande
// valeur. Les trois positions marquent les trois lignes, et rien d autre.
void test_no_cursor_position_marks_the_big_value() {
    for (uint8_t position = 0; position < 3; ++position) {
        canvas.reset();
        MainScreenModel m = channelTab();
        m.insideTab = true;
        m.cursor = position;
        drawMainScreen(canvas, m);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, bigLabelTopRowInk(canvas),
            "la grande valeur ne porte jamais le pave");
        TEST_ASSERT_TRUE_MESSAGE(lineTopRowInk(canvas, position) > 0,
            "et la ligne du curseur le porte");
    }
}

// L ORACLE : le domaine nomme le champ, l ecran marque sa ligne. Le rendu
// derive la position de la grande valeur, donc rien ne garantit tout seul qu il
// suive le controleur. Ce test pilote un VRAI controleur et confronte les deux a
// chaque position, ce qu un modele ecrit a la main ne peut pas faire.
void test_the_highlight_marks_the_line_of_the_field_the_domain_names() {
    using flexseq::UiController;
    flexseq::SequencerEngine engine;
    flexseq::Transport transport(engine);
    UiController ui(engine, transport);

    engine.setChannelMode(0, flexseq::MODE_SEQ);
    for (uint8_t guard = 0; guard < 2 * UiController::TAB_COUNT; ++guard) {
        if (ui.currentTab() == UiController::TAB_FIRST_CHANNEL) break;
        ui.handle(UiController::EVENT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT8(UiController::TAB_FIRST_CHANNEL, ui.currentTab());
    ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(UiController::SEQ_CHANNEL_TAB_FIELDS,
        ui.fieldCount(), "un canal en SEQ porte trois positions");

    for (uint8_t position = 0; position < UiController::SEQ_CHANNEL_TAB_FIELDS;
         ++position) {
        for (uint8_t guard = 0; guard < 2 * UiController::SEQ_CHANNEL_TAB_FIELDS;
             ++guard) {
            if (ui.cursor() == position) break;
            ui.handle(UiController::EVENT_ROTATE, 1);
        }
        TEST_ASSERT_EQUAL_UINT8(position, ui.cursor());

        canvas.reset();
        drawMainScreen(canvas, flexseq::mainScreenModelOf(ui, engine));

        uint8_t marked = 0;
        int8_t line = -1;
        for (uint8_t n = 0; n < 3; ++n) {
            if (lineTopRowInk(canvas, n) > 0) { line = static_cast<int8_t>(n); ++marked; }
        }
        const bool bigValue = bigLabelTopRowInk(canvas) > 0;
        if (bigValue) ++marked;
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, marked,
            "une seule surbrillance, le curseur etant un index unique");

        switch (ui.field()) {
            case UiController::FIELD_PATTERN:
                TEST_ASSERT_TRUE_MESSAGE(bigValue, "PATTERN : la grande valeur");
                break;
            case UiController::FIELD_MODE:
                TEST_ASSERT_EQUAL_INT8_MESSAGE(0, line, "MODE : la premiere ligne");
                break;
            case UiController::FIELD_EDIT_ENTRY:
                TEST_ASSERT_EQUAL_INT8_MESSAGE(1, line, "EDIT : la deuxieme ligne");
                break;
            default:
                TEST_ASSERT_EQUAL_INT8_MESSAGE(2, line, "CONFIG : la troisieme ligne");
                break;
        }
    }
}

void test_opening_a_field_moves_the_mark_from_the_label_to_the_value() {
    canvas.reset();
    drawMainScreen(canvas, legacyTab(flexseq::MODE_CLOCK, 0, true, false));
    const uint16_t labelClosed = inkInBoxOf(canvas, ms::LINE_LABEL_X - 1, 36,
                                            ms::LINE_0_BASELINE_Y);
    const uint16_t valueClosed = inkInBoxOf(canvas, ms::LINE_VALUE_X - 2, 28,
                                            ms::LINE_0_BASELINE_Y);

    canvas.reset();
    drawMainScreen(canvas, legacyTab(flexseq::MODE_CLOCK, 0, true, true));
    const uint16_t labelOpen = inkInBoxOf(canvas, ms::LINE_LABEL_X - 1, 36,
                                          ms::LINE_0_BASELINE_Y);
    const uint16_t valueOpen = inkInBoxOf(canvas, ms::LINE_VALUE_X - 2, 28,
                                          ms::LINE_0_BASELINE_Y);

    TEST_ASSERT_TRUE_MESSAGE(labelClosed > labelOpen,
        "le pave quitte l etiquette quand le champ s ouvre");
    TEST_ASSERT_TRUE_MESSAGE(valueOpen > valueClosed,
        "et un cadre entoure la valeur");
}

void test_no_cursor_is_drawn_while_on_the_tab_bar() {
    canvas.reset();
    MainScreenModel m = channelTab();
    m.insideTab = false;
    m.cursor = 1;
    drawMainScreen(canvas, m);
    TEST_ASSERT_FALSE(canvas.at(ms::COL_LEFT_X, ms::ROW_A_BOX_Y));
    TEST_ASSERT_FALSE(canvas.at(ms::HEADLINE_BOX_X, ms::HEADLINE_BOX_Y));
}

/*
 * Ecartement par bande — la meme propriete qu'ADR 0001 exige de l'ecran EDIT
 */

void test_eight_bands_reunited_equal_the_whole_image() {
    MainScreenModel m = channelTab();
    m.insideTab = true;
    m.cursor = 4;

    static RecordingCanvas whole;
    whole.reset();
    drawMainScreen(whole, m);

    static RecordingCanvas banded;
    banded.reset();
    for (uint8_t row = 0; row < screen::HEIGHT / 8; ++row) {
        const Band band = {static_cast<uint8_t>(row * 8), static_cast<uint8_t>(row * 8 + 7)};
        banded.clipY0 = band.y0;
        banded.clipY1 = band.y1;
        drawMainScreen(banded, m, band);
    }

    uint16_t diff = 0;
    for (uint8_t y = 0; y < screen::HEIGHT; ++y)
        for (uint8_t x = 0; x < screen::WIDTH; ++x)
            if (whole.px[y][x] != banded.px[y][x]) ++diff;
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, diff,
        "la reunion des 8 bandes differe de l'image complete");
}

void test_the_tab_bar_is_drawn_in_exactly_one_band() {
    MainScreenModel m = channelTab();
    uint8_t bands = 0;
    for (uint8_t row = 0; row < screen::HEIGHT / 8; ++row) {
        canvas.reset();
        const Band band = {static_cast<uint8_t>(row * 8), static_cast<uint8_t>(row * 8 + 7)};
        canvas.clipY0 = band.y0;
        canvas.clipY1 = band.y1;
        drawMainScreen(canvas, m, band);
        if (canvas.findOnBaseline("1", ms::TAB_BASELINE_Y) != nullptr) {
            ++bands;
            TEST_ASSERT_EQUAL_UINT8(ms::TAB_TOP_Y / 8, row);
        }
    }
    TEST_ASSERT_EQUAL_UINT8(1, bands);
}

// Aucune bande de cet ecran n'est vide : la grande police en occupe quatre, les
// deux rangees une chacune, le filet et la barre d'onglets les deux dernieres.
// La bande du filet ne porte en revanche AUCUN texte — c'est ce qui la rend
// candidate a un saut ulterieur, comme le titre et le pied de l'ecran EDIT.
void test_the_rule_band_carries_the_rule_and_no_text() {
    MainScreenModel m = channelTab();
    canvas.reset();
    canvas.clipY0 = 48;
    canvas.clipY1 = 55;
    drawMainScreen(canvas, m, Band{48, 55});
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, canvas.callCount, "aucun texte dans la bande du filet");
    TEST_ASSERT_TRUE(canvas.at(ms::RULE_X, ms::RULE_Y));
}

// ----------------------------------------------------------------------------
// Les trois lignes de l'original — lot 11, etape 5b-ii
//
// ⚠️ u8g2 place l'encre ENTIEREMENT au-dessus de la ligne de base : un glyphe
// de 5 px occupe base - 5 .. base - 1. Mesure sur le tampon du panneau reel,
// 2026-09-04. Les positions ci-dessous suivent cette convention.
// ----------------------------------------------------------------------------

void test_a_clock_tab_draws_the_three_lines_at_the_geometry_of_the_original() {
    RecordingCanvas canvas;
    drawMainScreen(canvas, legacyTab(flexseq::MODE_CLOCK));
    const Call* mode = canvas.find("MODE:");
    TEST_ASSERT_NOT_NULL_MESSAGE(mode, "la ligne 1 porte MODE:");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::LINE_LABEL_X, mode->x, "etiquette a x=62");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::LINE_0_BASELINE_Y, mode->y, "ligne de base 8");

    const Call* value = canvas.find("CLOCK");
    TEST_ASSERT_NOT_NULL_MESSAGE(value, "sa valeur est CLOCK");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::LINE_VALUE_X, value->x, "valeur a x=99");

    const Call* offset = canvas.find("OFFSET:");
    TEST_ASSERT_NOT_NULL_MESSAGE(offset, "la ligne 2 porte OFFSET: en CLOCK");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::LINE_1_BASELINE_Y, offset->y, "ligne de base 19");

    const Call* mod = canvas.find("MOD:");
    TEST_ASSERT_NOT_NULL_MESSAGE(mod, "la ligne 3 porte MOD:");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::LINE_2_BASELINE_Y, mod->y, "ligne de base 30");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("OFF"),
        "et sa valeur lit OFF : le mecanisme est au lot 13");
}

void test_a_random_tab_puts_the_subdivision_on_the_second_line() {
    RecordingCanvas canvas;
    drawMainScreen(canvas, legacyTab(flexseq::MODE_RANDOM));
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("SUBDIV:"),
        "la ligne 2 porte SUBDIV: en RANDOM");
    TEST_ASSERT_NULL_MESSAGE(canvas.find("OFFSET:"),
        "et jamais OFFSET: : ce n est pas le mode CLOCK");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("RAND"), "sa valeur de mode est RAND");
}

void test_the_main_parameter_is_centred_on_its_box() {
    RecordingCanvas canvas;
    drawMainScreen(canvas, legacyTab(flexseq::MODE_CLOCK));
    const Call* label = canvas.find("SUBDIVISION");
    TEST_ASSERT_NOT_NULL_MESSAGE(label, "l etiquette du parametre principal");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::MAIN_LABEL_BASELINE_Y, label->y, "ligne de base 41");
    const uint8_t w = 5 * 11;   // SUBDIVISION, onze caracteres du double
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::MAIN_CENTRE_X - w / 2, label->x,
        "centree sur x=29");

    const Call* value = canvas.find("x4");
    TEST_ASSERT_NOT_NULL_MESSAGE(value, "la valeur en gros, subdiv -4 donne x4");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::MAIN_VALUE_BASELINE_Y, value->y, "ligne de base 28");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::MAIN_CENTRE_X - (13 * 2) / 2, value->x,
        "centree avec la LARGE police : 13 px par caractere dans le double");
}

MainScreenModel configTab() {
    MainScreenModel m = legacyTab(flexseq::MODE_SEQ);
    m.configPage = true;
    m.patternIndex = 9;
    m.length = 20;
    m.subdiv = -4;
    return m;
}

void test_the_config_page_shows_the_pattern_name_in_the_large_font() {
    RecordingCanvas canvas;
    drawMainScreen(canvas, configTab());
    const Call* value = canvas.find("B2");
    TEST_ASSERT_NOT_NULL_MESSAGE(value, "le pattern 9 s ecrit B2");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::MAIN_VALUE_BASELINE_Y, value->y,
        "en gros, sur la ligne de base du parametre principal");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("PATTERN"), "et son etiquette");
}

void test_the_config_page_carries_length_subdiv_and_mod() {
    RecordingCanvas canvas;
    drawMainScreen(canvas, configTab());
    const Call* len = canvas.find("LENGTH:");
    TEST_ASSERT_NOT_NULL_MESSAGE(len, "ligne 1 : LENGTH");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::LINE_0_BASELINE_Y, len->y, "ligne 1");
    const Call* sub = canvas.find("SUBDIV:");
    TEST_ASSERT_NOT_NULL_MESSAGE(sub, "ligne 2 : SUBDIV");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::LINE_1_BASELINE_Y, sub->y, "ligne 2");
    const Call* mod = canvas.find("MOD:");
    TEST_ASSERT_NOT_NULL_MESSAGE(mod, "ligne 3 : MOD");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ms::LINE_2_BASELINE_Y, mod->y, "ligne 3");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("20"), "la longueur du canal");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("x4"), "la subdivision du canal");
}

void test_the_config_page_shows_none_of_the_three_lines_of_a_mode_tab() {
    RecordingCanvas canvas;
    drawMainScreen(canvas, configTab());
    TEST_ASSERT_NULL_MESSAGE(canvas.find("MODE:"), "MODE n est pas sur CONFIG");
    TEST_ASSERT_NULL_MESSAGE(canvas.find("OFFSET:"), "ni OFFSET");
    TEST_ASSERT_NULL_MESSAGE(canvas.find("SUBDIVISION"),
        "ni l etiquette du parametre principal d un canal en CLOCK");
}

void test_the_mod_line_names_the_routing_of_both_inputs() {
    RecordingCanvas canvas;
    MainScreenModel m = configTab();
    m.cv1Target = flexseq::CV_DEST_PATTERN;
    m.cv2Target = flexseq::CV_DEST_LENGTH;
    drawMainScreen(canvas, m);
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("MOD:"), "la ligne 3 porte MOD");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("P/L"),
        "la position nomme l entree : CV1 avant la barre, CV2 apres");
    TEST_ASSERT_NULL_MESSAGE(canvas.find("OFF"), "et OFF a disparu");
}

void test_a_single_routing_shows_a_dash_for_the_free_input() {
    RecordingCanvas canvas;
    MainScreenModel m = configTab();
    m.cv1Target = flexseq::CV_DEST_NONE;
    m.cv2Target = flexseq::CV_DEST_STEP;
    drawMainScreen(canvas, m);
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("-/S"),
        "CV1 libre s ecrit avec un tiret, et CV2 porte STEP");
}

void test_no_routing_shows_off() {
    RecordingCanvas canvas;
    MainScreenModel m = configTab();
    m.cv1Target = flexseq::CV_DEST_NONE;
    m.cv2Target = flexseq::CV_DEST_NONE;
    drawMainScreen(canvas, m);
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("OFF"), "aucun routage se lit OFF");
}

// Le cycle ne peut pas produire deux entrees sur la meme destination, mais le
// format le peut. Le nommage doit donc l accepter, sinon l ecran mentirait.
void test_the_naming_accepts_a_routing_the_cycle_cannot_produce() {
    RecordingCanvas canvas;
    MainScreenModel m = configTab();
    m.cv1Target = flexseq::CV_DEST_PATTERN;
    m.cv2Target = flexseq::CV_DEST_PATTERN;
    drawMainScreen(canvas, m);
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("P/P"),
        "deux entrees sur PATTERN se nomment, elles ne se taisent pas");
    TEST_ASSERT_NULL_MESSAGE(canvas.find("OFF"), "et surtout elles ne se lisent pas OFF");
}

void test_a_clock_tab_names_the_routing_it_carries() {
    RecordingCanvas canvas;
    MainScreenModel m = legacyTab(flexseq::MODE_CLOCK);
    m.cv1Target = flexseq::CV_DEST_RESET;
    m.cv2Target = flexseq::CV_DEST_NONE;
    drawMainScreen(canvas, m);
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("R/-"),
        "un canal passe en CLOCK garde son routage, et l ecran le dit");
}

void test_a_seq_tab_without_the_config_flag_shows_its_own_lines() {
    RecordingCanvas canvas;
    MainScreenModel m = configTab();
    m.configPage = false;
    drawMainScreen(canvas, m);
    TEST_ASSERT_NULL_MESSAGE(canvas.find("LENGTH:"), "la page CONFIG ne fuit pas");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("EDIT"), "l onglet SEQ reprend EDIT");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("CONFIG"), "et CONFIG");
}

void test_random_shows_the_skip_chance_as_a_percentage() {
    RecordingCanvas canvas;
    drawMainScreen(canvas, legacyTab(flexseq::MODE_RANDOM));
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("30%"),
        "une chance de 3 s ecrit 30%, comme l original");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("SKIP CHANCE"), "et son etiquette");
}

void test_the_legacy_layout_draws_no_headline_and_no_old_field() {
    RecordingCanvas canvas;
    drawMainScreen(canvas, legacyTab(flexseq::MODE_CLOCK));
    TEST_ASSERT_NULL_MESSAGE(canvas.find("LEN"),
        "LEN quitte l ecran en CLOCK : il ne change rien d audible la");
    TEST_ASSERT_NULL_MESSAGE(canvas.find("SEP"), "SEP aussi");
    TEST_ASSERT_NULL_MESSAGE(canvas.find("EDIT"), "et l entree en edition aussi");
}

void test_a_seq_tab_takes_the_three_lines_of_the_original() {
    RecordingCanvas canvas;
    drawMainScreen(canvas, channelTab());
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("MODE:"), "la premiere ligne est MODE");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("EDIT"), "la deuxieme est EDIT");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("CONFIG"), "la troisieme est CONFIG");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("PATTERN"),
        "et le parametre principal est le nom du pattern");
    TEST_ASSERT_NULL_MESSAGE(canvas.find("LEN"), "LEN a quitte l onglet");
    TEST_ASSERT_NULL_MESSAGE(canvas.find("SUB"), "SUB aussi");
    TEST_ASSERT_NULL_MESSAGE(canvas.find("SEP"), "SEP aussi");
}

void test_the_cursor_inverts_the_label_of_its_line() {
    RecordingCanvas plain;
    drawMainScreen(plain, legacyTab(flexseq::MODE_CLOCK, 2, false));
    RecordingCanvas marked;
    drawMainScreen(marked, legacyTab(flexseq::MODE_CLOCK, 2, true));
    // Le pave d'inversion ajoute de l'encre autour de l etiquette de la ligne 3.
    uint16_t plainInk = 0, markedInk = 0;
    for (uint8_t y = ms::LINE_2_BASELINE_Y - 6; y <= ms::LINE_2_BASELINE_Y; ++y) {
        for (uint8_t x = ms::LINE_LABEL_X - 1; x < ms::LINE_LABEL_X + 20; ++x) {
            if (plain.at(x, y)) ++plainInk;
            if (marked.at(x, y)) ++markedInk;
        }
    }
    TEST_ASSERT_GREATER_THAN_MESSAGE(plainInk, markedInk,
        "le curseur doit ajouter de l encre : sans cela il serait invisible");
}

/*
 * PATTERNS tab — lot 16E step 4b
 */

void test_the_patterns_tab_draws_the_slot_its_state_and_the_editor_entry() {
    canvas.reset();
    flexseq::MainScreenModel m{};
    m.tab = ms::TAB_PATTERNS;
    m.patternIndex = 10;
    m.slotEmpty = true;
    m.mainParameter = flexseq::MAIN_PATTERN;
    drawMainScreen(canvas, m);
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("B3"), "le nom de l emplacement");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("PATTERN"), "l etiquette");
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("EDIT"), "l entree dans l editeur");
    // L etiquette SLOT repetait le nom que la grande valeur porte deja.
    TEST_ASSERT_NULL_MESSAGE(canvas.find("SLOT"), "l etat se lit seul");
}

void test_an_occupied_slot_reads_used() {
    canvas.reset();
    flexseq::MainScreenModel m{};
    m.tab = ms::TAB_PATTERNS;
    m.patternIndex = 15;
    m.slotEmpty = false;
    m.mainParameter = flexseq::MAIN_PATTERN;
    drawMainScreen(canvas, m);
    TEST_ASSERT_NOT_NULL(canvas.find("B8"));
    TEST_ASSERT_NOT_NULL(canvas.find("PATTERN"));
}

// L onglet PATTERNS prend la mise en page d un canal en SEQ : la grande valeur
// porte le nom de l emplacement, et il n y a donc PAS de titre centre.
void test_the_patterns_tab_carries_no_headline() {
    flexseq::MainScreenModel m{};
    m.tab = flexseq::mainscreen::TAB_PATTERNS;
    m.patternIndex = 10;
    char out[6];
    flexseq::detail::headlineOf(m, out);
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_the_patterns_tab_names_the_slot_in_the_main_value() {
    flexseq::MainScreenModel m{};
    m.tab = flexseq::mainscreen::TAB_PATTERNS;
    m.patternIndex = 10;
    m.mainParameter = flexseq::MAIN_PATTERN;
    char out[10];
    flexseq::detail::mainValueOf(m, out);
    TEST_ASSERT_EQUAL_STRING("B3", out);
    // L etat vit sous la grande valeur, contre le nom de l emplacement qu il
    // decrit, et il n est donc PAS une ligne selectionnable.
    TEST_ASSERT_EQUAL_STRING("PATTERN", flexseq::detail::mainLabelOf(m));
}

// L etat de l emplacement est un CARRE a gauche de l etiquette : plein quand
// l emplacement porte quelque chose, creux quand il est libre. C est le sens
// que la grille de l editeur donne deja a ces deux formes.
void test_the_slot_state_is_a_square_beside_the_label() {
    namespace ms = flexseq::mainscreen;
    const uint8_t side = ms::MAIN_LABEL_GLYPH_W;
    auto inkInGlyph = [&](bool empty) {
        canvas.reset();
        flexseq::MainScreenModel m{};
        m.tab = ms::TAB_PATTERNS;
        m.patternIndex = 10;
        m.mainParameter = flexseq::MAIN_PATTERN;
        m.slotEmpty = empty;
        drawMainScreen(canvas, m);
        const Call* c = canvas.find("PATTERN");
        TEST_ASSERT_NOT_NULL(c);
        const uint8_t x0 = static_cast<uint8_t>(c->x - ms::MAIN_LABEL_GLYPH_GAP - side);
        const uint8_t y0 = static_cast<uint8_t>(c->y - side);
        uint16_t ink = 0;
        for (uint8_t dy = 0; dy < side; ++dy) {
            for (uint8_t dx = 0; dx < side; ++dx) {
                if (canvas.at(static_cast<uint8_t>(x0 + dx),
                              static_cast<uint8_t>(y0 + dy))) ++ink;
            }
        }
        return ink;
    };
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(side * side, inkInGlyph(false),
                                     "occupe : le carre est PLEIN");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(4 * side - 4, inkInGlyph(true),
                                     "libre : le carre est CREUX");
}

void test_the_patterns_tab_takes_the_three_lines_of_the_original() {
    flexseq::MainScreenModel m{};
    m.tab = flexseq::mainscreen::TAB_PATTERNS;
    m.patternIndex = 10;
    m.slotEmpty = true;
    const char* flashLabel = nullptr;
    char value[10];

    // EDIT est la SEULE ligne, donc la seule chose que le curseur atteint.
    flexseq::detail::legacyLine(m, 0, &flashLabel, value);
    TEST_ASSERT_EQUAL_STRING("EDIT", flashLabel);
    TEST_ASSERT_EQUAL_STRING("", value);

    flexseq::detail::legacyLine(m, 1, &flashLabel, value);
    TEST_ASSERT_EQUAL_STRING("", flashLabel);

    flexseq::detail::legacyLine(m, 2, &flashLabel, value);
    TEST_ASSERT_EQUAL_STRING("", flashLabel);
}

// PRD 5.0 amendement 1ter : la grande valeur ne nomme plus d action. Elle porte
// la QUESTION tant qu un chargement destructeur attend sa reponse.
void test_the_question_replaces_the_label_by_its_own_word() {
    namespace ms = flexseq::mainscreen;
    auto draw = [](bool ask) {
        canvas.reset();
        flexseq::MainScreenModel m = channelTab();
        m.patternAsk = ask;
        drawMainScreen(canvas, m);
    };

    draw(false);
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("PATTERN"),
        "sans question, le champ nomme le parametre");
    TEST_ASSERT_NULL(canvas.find("SURE"));

    draw(true);
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("SURE"), "la question porte son mot");
    TEST_ASSERT_NULL_MESSAGE(canvas.find("PATTERN"),
        "et elle remplace le parametre");
}

// ⚠️ La question se lit SUR LA BARRE, et c est la ou le geste vit. La lier au
// curseur la rendrait invisible a l endroit meme ou elle est posee.
void test_the_question_shows_outside_the_tab() {
    canvas.reset();
    flexseq::MainScreenModel m = channelTab();
    m.insideTab = false;
    m.patternAsk = true;
    drawMainScreen(canvas, m);
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("SURE"),
        "le geste part de la barre, donc la question s y voit");
}

// La page CONFIG porte la meme etiquette et un autre champ : elle ne doit jamais
// afficher la question.
void test_the_question_never_replaces_the_label_on_the_config_page() {
    canvas.reset();
    flexseq::MainScreenModel m = channelTab();
    m.insideTab = true;
    m.configPage = true;
    m.patternAsk = true;
    drawMainScreen(canvas, m);
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("PATTERN"),
        "la page CONFIG garde son etiquette");
    TEST_ASSERT_NULL(canvas.find("SURE"));
}

// ⚠️ L onglet PATTERNS porte la MEME etiquette et un autre champ. Ce test
// manquait a la premiere redaction de 1ter, et c est le miroir TypeScript qui
// a trouve le defaut : le rendu ecrivait SURE sur cet onglet.
void test_the_question_never_shows_on_the_patterns_tab() {
    namespace ms = flexseq::mainscreen;
    canvas.reset();
    flexseq::MainScreenModel m = channelTab(ms::TAB_PATTERNS);
    m.insideTab = true;
    m.patternAsk = true;
    drawMainScreen(canvas, m);
    TEST_ASSERT_NOT_NULL_MESSAGE(canvas.find("PATTERN"),
        "l onglet PATTERNS garde son etiquette");
    TEST_ASSERT_NULL(canvas.find("SURE"));
}

// PRD 5.0 amendement 1ter : le curseur ne se pose plus sur la grande valeur,
// donc son etiquette ne s inverse jamais — ni dans l onglet, ni ailleurs.
void test_the_big_value_label_never_inverts() {
    namespace ms = flexseq::mainscreen;
    auto inkUnderLabel = [](bool insideTab) {
        canvas.reset();
        flexseq::MainScreenModel m{};
        m.tab = ms::TAB_FIRST_CHANNEL;
        m.mode = static_cast<uint8_t>(flexseq::MODE_SEQ);
        m.patternIndex = 0;
        m.mainParameter = flexseq::MAIN_PATTERN;
        m.insideTab = insideTab;
        m.cursor = 0;
        drawMainScreen(canvas, m);
        const Call* c = canvas.find("PATTERN");
        TEST_ASSERT_NOT_NULL(c);
        uint16_t ink = 0;
        for (uint8_t dy = 0; dy < flexseq::FONT_VELVETSCREEN_HEIGHT; ++dy) {
            for (uint8_t dx = 0; dx < 6; ++dx) {
                if (canvas.at(static_cast<uint8_t>(c->x + dx),
                              static_cast<uint8_t>(c->y - dy))) ++ink;
            }
        }
        return ink;
    };
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(
        inkUnderLabel(false), inkUnderLabel(true),
        "entrer dans l onglet ne change rien a la grande valeur");
}

void test_the_settings_tab_headline_stays_empty() {
    flexseq::MainScreenModel m{};
    m.tab = flexseq::mainscreen::TAB_SETTINGS;
    char out[6];
    flexseq::detail::headlineOf(m, out);
    TEST_ASSERT_EQUAL_STRING("", out);
}


int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_clock_tab_draws_the_three_lines_at_the_geometry_of_the_original);
    RUN_TEST(test_a_random_tab_puts_the_subdivision_on_the_second_line);
    RUN_TEST(test_the_main_parameter_is_centred_on_its_box);
    RUN_TEST(test_the_config_page_shows_the_pattern_name_in_the_large_font);
    RUN_TEST(test_the_config_page_carries_length_subdiv_and_mod);
    RUN_TEST(test_the_config_page_shows_none_of_the_three_lines_of_a_mode_tab);
    RUN_TEST(test_the_mod_line_names_the_routing_of_both_inputs);
    RUN_TEST(test_a_single_routing_shows_a_dash_for_the_free_input);
    RUN_TEST(test_no_routing_shows_off);
    RUN_TEST(test_the_naming_accepts_a_routing_the_cycle_cannot_produce);
    RUN_TEST(test_a_clock_tab_names_the_routing_it_carries);
    RUN_TEST(test_a_seq_tab_without_the_config_flag_shows_its_own_lines);
    RUN_TEST(test_random_shows_the_skip_chance_as_a_percentage);
    RUN_TEST(test_the_legacy_layout_draws_no_headline_and_no_old_field);
    RUN_TEST(test_a_seq_tab_takes_the_three_lines_of_the_original);
    RUN_TEST(test_the_cursor_inverts_the_label_of_its_line);

    RUN_TEST(test_the_tab_bar_has_nine_evenly_spaced_slots);
    RUN_TEST(test_the_bar_no_longer_fills_the_width_of_the_screen);
    RUN_TEST(test_the_roles_of_the_nine_tabs_are_named);
    RUN_TEST(test_the_patterns_tab_is_not_a_channel);
    RUN_TEST(test_the_drawn_glyphs_of_the_bar_share_the_band_of_the_digits);
    RUN_TEST(test_the_clock_tab_draws_the_glyph_of_the_original);
    RUN_TEST(test_the_patterns_glyph_is_two_rows_of_three_single_dots);
    RUN_TEST(test_the_settings_glyph_is_two_sliders_seven_pixels_wide);
    RUN_TEST(test_the_transport_indicator_shows_stop_when_the_transport_is_stopped);
    RUN_TEST(test_the_transport_indicator_shows_play_when_the_transport_runs);
    RUN_TEST(test_the_transport_indicator_is_drawn_on_the_internal_clock_only);
    RUN_TEST(test_the_transport_indicator_sits_outside_the_nine_slots);
    RUN_TEST(test_the_glyph_band_of_the_bar_is_never_clipped);
    RUN_TEST(test_the_six_channel_digits_sit_at_their_slot_centres);
    RUN_TEST(test_the_selected_tab_is_inverted);
    RUN_TEST(test_the_clock_and_settings_tabs_are_glyphs_not_digits);

    RUN_TEST(test_the_pattern_name_is_the_main_parameter_of_a_seq_tab);
    RUN_TEST(test_the_renderer_restores_the_label_font_after_the_main_parameter);
    RUN_TEST(test_the_space_below_the_rows_is_reserved_and_empty);
    RUN_TEST(test_a_seq_tab_draws_its_three_lines_at_the_geometry_of_the_original);
    RUN_TEST(test_subdiv_is_shown_the_gravity_way);
    RUN_TEST(test_a_separation_of_none_is_shown_as_a_dash);
    RUN_TEST(test_every_pattern_of_the_bank_has_a_distinct_name);

    RUN_TEST(test_the_clock_tab_shows_the_tempo_big_and_the_source);
    RUN_TEST(test_the_six_clock_sources_have_distinct_labels);
    RUN_TEST(test_the_settings_tab_is_empty_while_it_is_deferred);

    RUN_TEST(test_the_cursor_marks_the_line_it_is_on_and_no_other);
    RUN_TEST(test_no_cursor_position_marks_the_big_value);
    RUN_TEST(test_the_highlight_marks_the_line_of_the_field_the_domain_names);
    RUN_TEST(test_opening_a_field_moves_the_mark_from_the_label_to_the_value);
    RUN_TEST(test_no_cursor_is_drawn_while_on_the_tab_bar);

    RUN_TEST(test_eight_bands_reunited_equal_the_whole_image);
    RUN_TEST(test_the_tab_bar_is_drawn_in_exactly_one_band);
    RUN_TEST(test_the_rule_band_carries_the_rule_and_no_text);
    RUN_TEST(test_the_patterns_tab_draws_the_slot_its_state_and_the_editor_entry);
    RUN_TEST(test_an_occupied_slot_reads_used);
    RUN_TEST(test_the_patterns_tab_carries_no_headline);
    RUN_TEST(test_the_patterns_tab_names_the_slot_in_the_main_value);
    RUN_TEST(test_the_slot_state_is_a_square_beside_the_label);
    RUN_TEST(test_the_patterns_tab_takes_the_three_lines_of_the_original);
    RUN_TEST(test_the_big_value_label_never_inverts);
    RUN_TEST(test_the_question_replaces_the_label_by_its_own_word);
    RUN_TEST(test_the_question_shows_outside_the_tab);
        RUN_TEST(test_the_question_never_replaces_the_label_on_the_config_page);
    RUN_TEST(test_the_question_never_shows_on_the_patterns_tab);
    RUN_TEST(test_the_settings_tab_headline_stays_empty);

    return UNITY_END();
}
