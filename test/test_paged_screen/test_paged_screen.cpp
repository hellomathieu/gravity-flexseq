#include <stdint.h>
#include <string.h>
#include <unity.h>

#include <flexseq/PagedScreen.h>
#include <flexseq/Pattern.h>
#include <flexseq/PatternScreen.h>

using flexseq::PagedScreen;
using flexseq::Pattern;
using flexseq::PatternScreenModel;
namespace screen = flexseq::screen;

// Faux affichage : le canvas de PatternScreen (1 bit, setDrawColor(0) EFFACE)
// PLUS le contrat de pagination de U8g2 — firstPage() prepare, nextPage()
// transfere la bande courante et renvoie 0 quand l'image est complete.
//
// Il compte l'encre posee entre deux frontieres de bande : c'est ce qui permet
// de constater qu'une meme image est bien dessinee huit fois a l'identique.
struct FakeDisplay {
    static constexpr uint8_t PAGES = 8;

    bool px[screen::HEIGHT][screen::WIDTH];
    uint8_t color;

    uint8_t page;
    uint8_t firstPageCalls;
    uint8_t nextPageCalls;
    uint8_t bands;
    uint16_t bandInk[PAGES + 2];

    FakeDisplay() { reset(); }

    void reset() {
        memset(px, 0, sizeof(px));
        color = 1;
        page = 0;
        firstPageCalls = 0;
        nextPageCalls = 0;
        bands = 0;
        memset(bandInk, 0, sizeof(bandInk));
    }

    // --- pagination ---------------------------------------------------------

    void firstPage() {
        ++firstPageCalls;
        page = 0;
        openBand();
    }

    uint8_t nextPage() {
        ++nextPageCalls;
        closeBand();
        ++page;
        return (page < PAGES) ? 1 : 0;
    }

    void openBand() { memset(px, 0, sizeof(px)); }

    void closeBand() {
        if (bands < PAGES + 2) {
            bandInk[bands] = ink();
        }
        ++bands;
        openBand();
    }

    uint16_t ink() const {
        uint16_t n = 0;
        for (uint8_t y = 0; y < screen::HEIGHT; ++y) {
            for (uint8_t x = 0; x < screen::WIDTH; ++x) {
                if (px[y][x]) ++n;
            }
        }
        return n;
    }

    // --- canvas -------------------------------------------------------------

    void setDrawColor(uint8_t c) { color = c; }

    void drawPixel(uint8_t x, uint8_t y) {
        if (x < screen::WIDTH && y < screen::HEIGHT) {
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

    uint8_t drawStr(uint8_t, uint8_t, const char* s) { return getStrWidth(s); }

    uint8_t getStrWidth(const char* s) const {
        return static_cast<uint8_t>(5 * strlen(s));
    }
};

namespace {

Pattern source;
FakeDisplay display;
PagedScreen<FakeDisplay> paged;

PatternScreenModel modelOf(const Pattern& pattern) {
    PatternScreenModel model;
    model.title = "EDIT PATTERN A1";
    model.pattern = &pattern;
    model.length = 24;
    model.cursor = 0;
    model.playhead = 0;
    model.barLength = 4;
    return model;
}

// Mene l'image en cours a son terme. Renvoie le nombre d'appels a advance().
uint8_t finishFrame() {
    uint8_t calls = 0;
    while (true) {
        ++calls;
        if (!paged.advance(display)) {
            return calls;
        }
    }
}

void reset() {
    source.clear();
    source.clearRatchets();
    display.reset();
    paged = PagedScreen<FakeDisplay>();
}

}  // namespace

// begin() prepare et dessine, mais ne transfere rien : firstPage() seul.
void test_begin_draws_the_first_band_without_transferring(void) {
    reset();
    source.writeStep(0, true);

    paged.begin(display, modelOf(source));

    TEST_ASSERT_EQUAL_UINT8(1, display.firstPageCalls);
    TEST_ASSERT_EQUAL_UINT8(0, display.nextPageCalls);
    TEST_ASSERT_TRUE(paged.busy());
    TEST_ASSERT_GREATER_THAN_UINT16(0, display.ink());
}

// Une image = 8 transferts, donc 8 appels a nextPage(), le dernier terminant.
void test_frame_spans_exactly_eight_bands(void) {
    reset();
    source.writeStep(0, true);

    paged.begin(display, modelOf(source));
    const uint8_t calls = finishFrame();

    TEST_ASSERT_EQUAL_UINT8(FakeDisplay::PAGES, calls);
    TEST_ASSERT_EQUAL_UINT8(FakeDisplay::PAGES, display.nextPageCalls);
    TEST_ASSERT_EQUAL_UINT8(FakeDisplay::PAGES, display.bands);
    TEST_ASSERT_FALSE(paged.busy());
}

// L'ecran reste occupe tant que l'image n'est pas complete : c'est ce drapeau
// que la boucle principale lit pour ne pas ouvrir une image par-dessus l'autre.
void test_screen_stays_busy_until_the_last_band(void) {
    reset();
    paged.begin(display, modelOf(source));

    for (uint8_t i = 1; i < FakeDisplay::PAGES; ++i) {
        TEST_ASSERT_TRUE(paged.advance(display));
        TEST_ASSERT_TRUE(paged.busy());
    }

    TEST_ASSERT_FALSE(paged.advance(display));
    TEST_ASSERT_FALSE(paged.busy());
}

// Hors image, advance() ne fait rien du tout — aucun transfert parasite.
void test_advance_without_a_frame_does_nothing(void) {
    reset();

    TEST_ASSERT_FALSE(paged.advance(display));
    TEST_ASSERT_EQUAL_UINT8(0, display.nextPageCalls);
    TEST_ASSERT_EQUAL_UINT8(0, display.firstPageCalls);
}

// Le coeur de l'affaire : une edition survenue PENDANT l'image ne la dechire
// pas. Le contenu ayant ete copie, les 8 bandes montrent la meme chose.
void test_editing_during_a_frame_does_not_tear_it(void) {
    reset();
    source.writeStep(0, true);

    paged.begin(display, modelOf(source));

    // Edition en cours de lecture (PRD 6.3), entre la premiere bande et la suite.
    for (uint8_t i = 1; i < 24; ++i) {
        source.writeStep(i, true);
    }

    finishFrame();

    TEST_ASSERT_EQUAL_UINT8(FakeDisplay::PAGES, display.bands);
    for (uint8_t i = 1; i < FakeDisplay::PAGES; ++i) {
        TEST_ASSERT_EQUAL_UINT16(display.bandInk[0], display.bandInk[i]);
    }
}

// ... et la sonde ci-dessus n'est pas aveugle : la meme edition change bien
// l'image des lors qu'une NOUVELLE image la releve.
void test_the_edit_shows_up_on_the_next_frame(void) {
    reset();
    source.writeStep(0, true);

    paged.begin(display, modelOf(source));
    finishFrame();
    const uint16_t before = display.bandInk[0];

    for (uint8_t i = 1; i < 24; ++i) {
        source.writeStep(i, true);
    }

    display.bands = 0;
    paged.begin(display, modelOf(source));
    finishFrame();

    TEST_ASSERT_NOT_EQUAL_UINT16(before, display.bandInk[0]);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_draws_the_first_band_without_transferring);
    RUN_TEST(test_frame_spans_exactly_eight_bands);
    RUN_TEST(test_screen_stays_busy_until_the_last_band);
    RUN_TEST(test_advance_without_a_frame_does_nothing);
    RUN_TEST(test_editing_during_a_frame_does_not_tear_it);
    RUN_TEST(test_the_edit_shows_up_on_the_next_frame);
    return UNITY_END();
}
