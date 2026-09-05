#include <flexseq/OriginalFonts.h>
#include <stdint.h>
#include <string.h>
#include <unity.h>

#include <flexseq/MainScreen.h>

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
    m.fieldCount = 3;
    m.patternIndex = 0;
    m.length = 16;
    m.subdiv = 1;
    m.barLength = 4;
    m.tempo = 120;
    m.clockSource = 0;
    m.headlineWidth = 0;
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

void test_the_tab_bar_has_eight_evenly_spaced_slots() {
    TEST_ASSERT_EQUAL_UINT8(8, ms::TAB_COUNT);
    TEST_ASSERT_EQUAL_UINT8(16, ms::TAB_SLOT_W);
    TEST_ASSERT_EQUAL_UINT8(8, ms::tabCentreX(0));
    TEST_ASSERT_EQUAL_UINT8(120, ms::tabCentreX(7));
    for (uint8_t tab = 1; tab < ms::TAB_COUNT; ++tab) {
        TEST_ASSERT_EQUAL_UINT8(16, ms::tabCentreX(tab) - ms::tabCentreX(tab - 1));
    }
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
    // Encre presente aux deux extremites : les deux glyphes sont dessines.
    TEST_ASSERT_TRUE(canvas.inkInRows(ms::TAB_TOP_Y, ms::TAB_BASELINE_Y) > 0);
    bool leftInk = false;
    bool rightInk = false;
    for (uint8_t y = ms::TAB_TOP_Y; y <= ms::TAB_BASELINE_Y; ++y) {
        for (uint8_t x = 0; x < ms::TAB_SLOT_W; ++x) {
            if (canvas.at(x, y)) leftInk = true;
            if (canvas.at(static_cast<uint8_t>(screen::WIDTH - 1 - x), y)) rightInk = true;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(leftInk, "glyphe d'horloge absent");
    TEST_ASSERT_TRUE_MESSAGE(rightInk, "glyphe de reglages absent");
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

void test_the_cursor_marks_the_line_it_is_on_and_no_other() {
    canvas.reset();
    MainScreenModel m = channelTab();
    m.insideTab = true;
    m.cursor = 1;
    drawMainScreen(canvas, m);
    const uint16_t line0 = inkInBoxOf(canvas, ms::LINE_LABEL_X - 1, 40,
                                      ms::LINE_0_BASELINE_Y);
    const uint16_t line1 = inkInBoxOf(canvas, ms::LINE_LABEL_X - 1, 40,
                                      ms::LINE_1_BASELINE_Y);
    const uint16_t line2 = inkInBoxOf(canvas, ms::LINE_LABEL_X - 1, 40,
                                      ms::LINE_2_BASELINE_Y);
    TEST_ASSERT_TRUE_MESSAGE(line1 > line0, "la ligne du curseur porte le pave");
    TEST_ASSERT_TRUE_MESSAGE(line1 > line2, "et elle seule");
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

    RUN_TEST(test_the_tab_bar_has_eight_evenly_spaced_slots);
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
    RUN_TEST(test_opening_a_field_moves_the_mark_from_the_label_to_the_value);
    RUN_TEST(test_no_cursor_is_drawn_while_on_the_tab_bar);

    RUN_TEST(test_eight_bands_reunited_equal_the_whole_image);
    RUN_TEST(test_the_tab_bar_is_drawn_in_exactly_one_band);
    RUN_TEST(test_the_rule_band_carries_the_rule_and_no_text);

    return UNITY_END();
}
