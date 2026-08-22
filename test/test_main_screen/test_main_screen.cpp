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

const uint8_t SMALL_FONT = 0x11;
const uint8_t BIG_FONT = 0x22;

struct Call {
    uint8_t x;
    uint8_t y;
    char text[8];
    bool big;
};

struct RecordingCanvas {
    bool px[screen::HEIGHT][screen::WIDTH];
    uint8_t color;
    uint8_t clipY0;
    uint8_t clipY1;
    const uint8_t* font;
    Call calls[16];
    uint8_t callCount;

    RecordingCanvas() { reset(); }

    void reset() {
        memset(px, 0, sizeof(px));
        color = 1;
        clipY0 = 0;
        clipY1 = screen::HEIGHT - 1;
        font = &SMALL_FONT;
        callCount = 0;
    }

    void setDrawColor(uint8_t c) { color = c; }
    void setFont(const uint8_t* f) { font = f; }

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
            c.big = (font == &BIG_FONT);
            strncpy(c.text, s, sizeof(c.text) - 1);
            c.text[sizeof(c.text) - 1] = '\0';
        }
        return getStrWidth(s);
    }

    uint8_t getStrWidth(const char* s) const {
        const uint8_t per = (font == &BIG_FONT) ? 20 : 5;
        return static_cast<uint8_t>(per * strlen(s));
    }

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
    MainScreenModel m;
    m.tab = tab;
    m.insideTab = false;
    m.cursor = 0;
    m.fieldOpen = false;
    m.fieldCount = 5;
    m.patternIndex = 0;
    m.length = 16;
    m.subdiv = 1;
    m.barLength = 4;
    m.tempo = 120;
    m.clockSource = 0;
    m.smallFont = &SMALL_FONT;
    m.bigFont = &BIG_FONT;
    m.headlineWidth = 0;
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
        TEST_ASSERT_FALSE(call->big);
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

void test_the_headline_is_the_pattern_name_in_the_big_font() {
    canvas.reset();
    MainScreenModel m = channelTab();
    m.patternIndex = 9; // B2
    drawMainScreen(canvas, m);
    const Call* call = canvas.find("B2");
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_TRUE_MESSAGE(call->big, "le nom du pattern doit etre en grande police");
    TEST_ASSERT_EQUAL_UINT8(ms::HEADLINE_BASELINE_Y, call->y);
    // Centre sur la largeur de la GRANDE police (20 px par glyphe dans ce faux),
    // pas sur celle qui est active apres restauration.
    TEST_ASSERT_EQUAL_UINT8((screen::WIDTH - 2 * 20) / 2, call->x);
}

void test_the_font_is_restored_after_the_headline() {
    canvas.reset();
    drawMainScreen(canvas, channelTab());
    const Call* digit = canvas.findOnBaseline("1", ms::TAB_BASELINE_Y);
    TEST_ASSERT_NOT_NULL(digit);
    TEST_ASSERT_FALSE_MESSAGE(digit->big, "la petite police n'a pas ete rendue");
    TEST_ASSERT_EQUAL_PTR(&SMALL_FONT, canvas.font);
}

void test_a_channel_tab_shows_length_subdiv_separation_and_the_edit_entry() {
    canvas.reset();
    MainScreenModel m = channelTab();
    m.length = 20;
    m.subdiv = -4;
    m.barLength = 3;
    drawMainScreen(canvas, m);

    TEST_ASSERT_NOT_NULL(canvas.find("LEN"));
    TEST_ASSERT_NOT_NULL(canvas.find("20"));
    TEST_ASSERT_NOT_NULL(canvas.find("SUB"));
    TEST_ASSERT_NOT_NULL(canvas.find("x4"));
    TEST_ASSERT_NOT_NULL(canvas.find("SEP"));
    TEST_ASSERT_NOT_NULL(canvas.find("3"));
    TEST_ASSERT_NOT_NULL(canvas.find("EDIT"));
}

void test_the_five_fields_of_a_channel_tab_fit_without_scrolling() {
    canvas.reset();
    drawMainScreen(canvas, channelTab());
    const uint8_t rowA = canvas.find("LEN")->y;
    const uint8_t rowB = canvas.find("SEP")->y;

    // Les positions sont verifiees les unes PAR RAPPORT aux autres, jamais contre
    // la constante qu'elles devraient controler : une assertion ecrite ainsi
    // passe quelle que soit la valeur de cette constante.
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(rowA, canvas.find("SUB")->y,
                                    "LEN et SUB doivent partager une rangee");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(rowB, canvas.find("EDIT")->y,
                                    "SEP et EDIT doivent partager une rangee");
    TEST_ASSERT_TRUE_MESSAGE(rowB >= rowA + ms::ROW_BOX_H,
                             "les deux rangees se chevauchent");
    TEST_ASSERT_TRUE(canvas.find("LEN")->x < canvas.find("SUB")->x);
    TEST_ASSERT_TRUE(canvas.find("SEP")->x < canvas.find("EDIT")->x);
    TEST_ASSERT_TRUE_MESSAGE(rowA > ms::HEADLINE_BOX_Y + ms::HEADLINE_BOX_H - 1,
                             "la premiere rangee empiete sur la grande police");
    TEST_ASSERT_TRUE_MESSAGE(rowB < ms::RULE_Y, "la seconde rangee empiete sur le filet");
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

    const Call* headline = canvas.find("240");
    TEST_ASSERT_NOT_NULL(headline);
    TEST_ASSERT_TRUE(headline->big);
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

void test_the_cursor_frames_the_field_it_is_on() {
    canvas.reset();
    MainScreenModel m = channelTab();
    m.insideTab = true;
    m.cursor = 2; // SUB, colonne de droite
    drawMainScreen(canvas, m);
    TEST_ASSERT_TRUE(canvas.at(ms::COL_RIGHT_X, ms::ROW_A_BOX_Y));
    TEST_ASSERT_TRUE(canvas.at(static_cast<uint8_t>(ms::COL_RIGHT_X + ms::COL_W - 1),
                               ms::ROW_A_BOX_Y));
    TEST_ASSERT_FALSE_MESSAGE(canvas.at(ms::COL_LEFT_X, ms::ROW_A_BOX_Y),
                              "la colonne de gauche est encadree a tort");
}

void test_an_open_field_is_inverted_and_the_frame_alone_is_not() {
    MainScreenModel m = channelTab();
    m.insideTab = true;
    m.cursor = 1; // LEN

    canvas.reset();
    m.fieldOpen = false;
    drawMainScreen(canvas, m);
    const uint16_t framed = canvas.inkInRows(ms::ROW_A_BOX_Y,
        static_cast<uint8_t>(ms::ROW_A_BOX_Y + ms::ROW_BOX_H - 1));

    canvas.reset();
    m.fieldOpen = true;
    drawMainScreen(canvas, m);
    const uint16_t inverted = canvas.inkInRows(ms::ROW_A_BOX_Y,
        static_cast<uint8_t>(ms::ROW_A_BOX_Y + ms::ROW_BOX_H - 1));

    TEST_ASSERT_TRUE_MESSAGE(inverted > framed,
                             "un champ ouvert doit poser plus d'encre qu'un simple cadre");
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

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_the_tab_bar_has_eight_evenly_spaced_slots);
    RUN_TEST(test_the_six_channel_digits_sit_at_their_slot_centres);
    RUN_TEST(test_the_selected_tab_is_inverted);
    RUN_TEST(test_the_clock_and_settings_tabs_are_glyphs_not_digits);

    RUN_TEST(test_the_headline_is_the_pattern_name_in_the_big_font);
    RUN_TEST(test_the_font_is_restored_after_the_headline);
    RUN_TEST(test_a_channel_tab_shows_length_subdiv_separation_and_the_edit_entry);
    RUN_TEST(test_the_five_fields_of_a_channel_tab_fit_without_scrolling);
    RUN_TEST(test_subdiv_is_shown_the_gravity_way);
    RUN_TEST(test_a_separation_of_none_is_shown_as_a_dash);
    RUN_TEST(test_every_pattern_of_the_bank_has_a_distinct_name);

    RUN_TEST(test_the_clock_tab_shows_the_tempo_big_and_the_source);
    RUN_TEST(test_the_six_clock_sources_have_distinct_labels);
    RUN_TEST(test_the_settings_tab_is_empty_while_it_is_deferred);

    RUN_TEST(test_the_cursor_frames_the_field_it_is_on);
    RUN_TEST(test_an_open_field_is_inverted_and_the_frame_alone_is_not);
    RUN_TEST(test_no_cursor_is_drawn_while_on_the_tab_bar);

    RUN_TEST(test_eight_bands_reunited_equal_the_whole_image);
    RUN_TEST(test_the_tab_bar_is_drawn_in_exactly_one_band);
    RUN_TEST(test_the_rule_band_carries_the_rule_and_no_text);

    return UNITY_END();
}
