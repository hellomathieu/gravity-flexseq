#include <stdint.h>
#include <string.h>
#include <flexseq/PatternScreen.h>
#include <unity.h>

#include <flexseq/PagedScreen.h>
#include <flexseq/PatternScreen.h>
#include <flexseq/Pattern.h>
#include <flexseq/PatternScreen.h>

using flexseq::PagedScreen;
using flexseq::Pattern;
using flexseq::MainScreenModel;
using flexseq::PatternScreenModel;
namespace screen = flexseq::screen;

// Faux affichage : le canvas de PatternScreen (1 bit, setDrawColor(0) EFFACE)
// PLUS le contrat de pagination de U8g2 — firstPage() prepare, nextPage()
// transfere la bande courante et renvoie 0 quand l'image est complete, et
// getBufferCurrTileRow()/getBufferTileHeight() disent quelle bande vient.
//
// Il DECOUPE a la bande courante, comme le mode page, et ACCUMULE l'image sur
// toute la trame : ce qu'on verifie ensuite est la REUNION des 8 bandes, seule
// chose que l'ecran finit par montrer.
struct FakeDisplay {
    static constexpr uint8_t PAGES = 8;

    bool buffer[screen::HEIGHT][screen::WIDTH];  /* le tampon de page */
    bool panel[screen::HEIGHT][screen::WIDTH];   /* ce que le panneau AFFICHE */
    uint8_t color;

    uint8_t page;
    uint8_t clearCalls;
    uint8_t sendCalls;
    uint8_t bands;
    bool sent[PAGES];
    bool clipped;   /* false : pas de decoupe, pour rendre une image de reference */
    uint16_t ops;   /* appels de dessin recus, decoupe ou non : "le rendu a tourne" */

    FakeDisplay() { reset(); }

    void reset() {
        memset(buffer, 0, sizeof(buffer));
        memset(panel, 0, sizeof(panel));
        memset(sent, 0, sizeof(sent));
        color = 1;
        page = 0;
        clearCalls = 0;
        sendCalls = 0;
        memset(opsPerPage, 0, sizeof(opsPerPage));
        bands = 0;
        clipped = true;
        ops = 0;
    }

    // --- pagination ---------------------------------------------------------

    // Le contrat de la boucle de pages MANUELLE : PagedScreen positionne la
    // ligne, efface, dessine, envoie. On enregistre les bandes ENVOYEES : ce sont
    // les seules qui atteignent le panneau.
    void setBufferCurrTileRow(uint8_t row) { page = row; }

    void clearBuffer() {
        ++clearCalls;
        // Efface la bande courante seulement, comme le tampon de 128 octets.
        for (uint8_t y = page * 8; y < page * 8 + 8 && y < screen::HEIGHT; ++y) {
            for (uint8_t x = 0; x < screen::WIDTH; ++x) buffer[y][x] = false;
        }
    }

    void sendBuffer() {
        ++sendCalls;
        sent[page] = true;
        ++bands;
        // Le panneau est a MEMOIRE : la bande envoyee remplace la precedente, les
        // autres restent telles quelles.
        for (uint8_t y = page * 8; y < page * 8 + 8 && y < screen::HEIGHT; ++y) {
            for (uint8_t x = 0; x < screen::WIDTH; ++x) panel[y][x] = buffer[y][x];
        }
    }

    uint8_t getBufferTileHeight() const { return 1; }

    // Pour un rendu de REFERENCE, non pagine : tout le tampon d'un coup.
    void commit() {
        for (uint8_t y = 0; y < screen::HEIGHT; ++y)
            for (uint8_t x = 0; x < screen::WIDTH; ++x) panel[y][x] = buffer[y][x];
    }

    // Appels de dessin recus par bande : c'est ainsi qu'on constate qu'une bande
    // n'a pas ete dessinee du tout, meme quand elle ne porte pas de pixels (le
    // texte n'est pas rasterise ici).
    uint16_t opsPerPage[PAGES];

    // Encre AFFICHEE par le panneau — la seule qui compte.
    uint16_t ink() const {
        uint16_t n = 0;
        for (uint8_t y = 0; y < screen::HEIGHT; ++y) {
            for (uint8_t x = 0; x < screen::WIDTH; ++x) {
                if (panel[y][x]) ++n;
            }
        }
        return n;
    }

    uint16_t inkInBand(uint8_t row) const {
        uint16_t n = 0;
        for (uint8_t y = row * 8; y < row * 8 + 8 && y < screen::HEIGHT; ++y) {
            for (uint8_t x = 0; x < screen::WIDTH; ++x) {
                if (panel[y][x]) ++n;
            }
        }
        return n;
    }

    // --- canvas -------------------------------------------------------------

    void setDrawColor(uint8_t c) { color = c; }

    // Le decoupage se fait en coordonnees d'AFFICHAGE, apres la rotation de 180
    // degres qu'applique U8G2_R2 — c'est l'ordre reel de U8g2. Modeliser cet
    // ordre est ce qui rend le test capable de voir une bande convertie a
    // l'envers ; sans lui il reste aveugle a l'erreur la plus facile a commettre.
    void drawPixel(uint8_t x, uint8_t y) {
        ++ops;
        if (page < PAGES) ++opsPerPage[page];
        if (x >= screen::WIDTH || y >= screen::HEIGHT) return;
        // Le tampon et le panneau sont en coordonnees d'AFFICHAGE, comme le
        // materiel : la rotation U8G2_R2 s'applique donc ICI, avant le decoupage
        // et avant l'ecriture. C'est l'ordre reel de U8g2.
        const uint8_t dy = static_cast<uint8_t>(screen::HEIGHT - 1 - y);
        if (clipped && (dy < page * 8 || dy > page * 8 + 7)) return;
        buffer[dy][x] = (color != 0);
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

    uint8_t drawStr(uint8_t, uint8_t, const char* s) {
        ++ops;
        if (page < PAGES) ++opsPerPage[page];
        return getStrWidth(s);
    }

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
    model.titleWidth = 0;
    model.pattern = &pattern;
    model.length = 24;
    model.cursor = 0;
    model.playhead = 0;
    model.barLength = 4;
    return model;
}

MainScreenModel mainModelOf() {
    MainScreenModel m;
    m.tab = 1;
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
    m.headlineWidth = 0;
    return m;
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

// begin() rend et ENVOIE deja la premiere bande : la boucle manuelle ne perd
// plus un passage a preparer sans transferer, comme le faisait firstPage().
void test_begin_renders_and_sends_the_first_band(void) {
    reset();
    source.writeStep(0, true);

    paged.begin(display, modelOf(source));

    TEST_ASSERT_EQUAL_UINT8(1, display.sendCalls);
    TEST_ASSERT_EQUAL_UINT8(1, display.clearCalls);
    TEST_ASSERT_TRUE(paged.busy());
}

// Une image FRAICHE touche les 8 bandes : rien n'est encore connu du panneau.
void test_a_first_frame_sends_every_band(void) {
    reset();
    source.writeStep(0, true);

    paged.begin(display, modelOf(source));
    finishFrame();

    TEST_ASSERT_EQUAL_UINT8(FakeDisplay::PAGES, display.sendCalls);
    for (uint8_t r = 0; r < FakeDisplay::PAGES; ++r) {
        TEST_ASSERT_TRUE_MESSAGE(display.sent[r], "une bande n'a pas ete envoyee");
    }
    TEST_ASSERT_FALSE(paged.busy());
}

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
    TEST_ASSERT_EQUAL_UINT8(0, display.sendCalls);
    TEST_ASSERT_EQUAL_UINT8(0, display.clearCalls);
}

// --- LE SAUT DE BANDE --------------------------------------------------------

// Le titre inchange : sa bande n'est NI effacee, NI dessinee, NI envoyee. Les
// trois ensemble : effacer sans redessiner puis envoyer ferait disparaitre le
// titre le temps d'une image, et c'est le seul defaut de cette optimisation qui
// serait visible.
//
// Que le panneau continue de l'AFFICHER est une propriete du materiel, verifiee
// sur le vrai modele SSD1306 par tools/run-screen-dump.sh — pas ici.
void test_an_unchanged_title_skips_its_band(void) {
    reset();
    source.writeStep(0, true);

    paged.begin(display, modelOf(source));   // premiere image : tout
    finishFrame();
    const uint8_t TITLE = FakeDisplay::PAGES - 1;
    TEST_ASSERT_TRUE(display.sent[TITLE]);
    TEST_ASSERT_GREATER_THAN_UINT16(0, display.opsPerPage[TITLE]);

    memset(display.sent, 0, sizeof(display.sent));
    memset(display.opsPerPage, 0, sizeof(display.opsPerPage));
    const uint8_t sendsBefore = display.sendCalls;
    const uint8_t clearsBefore = display.clearCalls;

    paged.begin(display, modelOf(source));   // seconde image : titre identique
    finishFrame();

    TEST_ASSERT_FALSE_MESSAGE(display.sent[TITLE], "bande du titre renvoyee sans raison");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, display.opsPerPage[TITLE],
                                     "bande du titre redessinee sans raison");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(FakeDisplay::PAGES - 1,
        static_cast<uint8_t>(display.sendCalls - sendsBefore), "un envoi de trop");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(FakeDisplay::PAGES - 1,
        static_cast<uint8_t>(display.clearCalls - clearsBefore),
        "le tampon de la bande a ete efface alors qu'elle est sautee");
    // Le pied a emporte la seconde voie de saut : les deux bandes sous la grille
    // sont desormais envoyees, vides, a chaque image.
    TEST_ASSERT_TRUE_MESSAGE(display.sent[0],
        "sans pied, aucune bande sous la grille n'est plus sautee");
    TEST_ASSERT_TRUE_MESSAGE(display.sent[1],
        "sans pied, aucune bande sous la grille n'est plus sautee");
}

// La bande du titre est la SEULE voie de saut : le pied a quitte l'ecran EDIT
// avec la sienne. Une image de routine envoie donc exactement une bande de moins
// que l'image complete.
void test_only_the_title_band_is_ever_skipped(void) {
    reset();
    PatternScreenModel m = modelOf(source);

    paged.begin(display, m);
    finishFrame();

    const uint8_t sendsBefore = display.sendCalls;
    paged.begin(display, m);
    finishFrame();

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(FakeDisplay::PAGES - 1,
        static_cast<uint8_t>(display.sendCalls - sendsBefore),
        "seule la bande du titre doit etre sautee");
}

// Titre change : sa bande revient.
void test_a_changed_title_redraws_its_band(void) {
    reset();
    static char title[] = "EDIT PATTERN A1";
    PatternScreenModel m = modelOf(source);
    m.title = title;

    paged.begin(display, m);
    finishFrame();

    memset(display.sent, 0, sizeof(display.sent));
    title[14] = '2';                          // A1 -> A2
    m.titleWidth = 0;
    paged.begin(display, m);
    finishFrame();

    TEST_ASSERT_TRUE_MESSAGE(display.sent[FakeDisplay::PAGES - 1],
                             "le titre a change et sa bande n'a pas ete refaite");
}

// « A2 » et « B1 » ont la meme somme de caracteres : l'empreinte ne doit pas s'y
// tromper, ce sont deux titres voisins.
void test_two_titles_with_the_same_character_sum_are_distinguished(void) {
    reset();
    static char title[] = "EDIT PATTERN A2";
    PatternScreenModel m = modelOf(source);
    m.title = title;

    paged.begin(display, m);
    finishFrame();

    memset(display.sent, 0, sizeof(display.sent));
    title[13] = 'B';
    title[14] = '1';
    m.titleWidth = 0;
    paged.begin(display, m);
    finishFrame();

    TEST_ASSERT_TRUE_MESSAGE(display.sent[FakeDisplay::PAGES - 1],
                             "A2 et B1 confondus par l'empreinte");
}

// Le filet : une image sur FULL_REFRESH_EVERY est rendue integralement, meme si
// rien n'a change. Un oubli de notre logique se repare donc tout seul.
void test_the_safety_net_forces_a_full_frame_periodically(void) {
    reset();
    source.writeStep(0, true);

    uint8_t fullFrames = 0;
    for (uint8_t f = 0; f < 2 * PagedScreen<FakeDisplay>::FULL_REFRESH_EVERY; ++f) {
        memset(display.sent, 0, sizeof(display.sent));
        paged.begin(display, modelOf(source));
        finishFrame();
        if (display.sent[FakeDisplay::PAGES - 1]) {
            ++fullFrames;
        }
    }

    // Deux periodes : deux images completes, ni plus ni moins.
    TEST_ASSERT_EQUAL_UINT8(2, fullFrames);
}

// Le coeur de l'affaire : une edition survenue PENDANT l'image ne la dechire
// pas. Le contenu ayant ete copie, l'image finale est celle du DEBUT de trame.
//
// On ne compare plus l'encre entre bandes — depuis que le renderer ecarte ce qui
// ne tombe pas dans la bande courante, elles ne dessinent plus la meme chose. On
// compare la REUNION des 8 bandes a une image de reference rendue d'un seul
// tenant depuis le contenu d'origine : c'est ce que l'ecran montre vraiment.
void test_editing_during_a_frame_does_not_tear_it(void) {
    reset();
    source.writeStep(0, true);

    // Reference : le contenu tel qu'il est AVANT l'edition, rendu sans decoupe.
    static FakeDisplay reference;
    reference.reset();
    reference.clipped = false;
    flexseq::drawPatternScreen(reference, modelOf(source));
    reference.commit();
    const uint16_t expected = reference.ink();

    paged.begin(display, modelOf(source));

    // Edition en cours de lecture (PRD 6.3), entre la premiere bande et la suite.
    for (uint8_t i = 1; i < 24; ++i) {
        source.writeStep(i, true);
    }

    finishFrame();

    TEST_ASSERT_EQUAL_UINT8(FakeDisplay::PAGES, display.bands);
    TEST_ASSERT_EQUAL_UINT16(expected, display.ink());
}

// ... et la sonde ci-dessus n'est pas aveugle : la meme edition change bien
// l'image des lors qu'une NOUVELLE image la releve.
void test_the_edit_shows_up_on_the_next_frame(void) {
    reset();
    source.writeStep(0, true);

    paged.begin(display, modelOf(source));
    finishFrame();
    const uint16_t before = display.ink();

    for (uint8_t i = 1; i < 24; ++i) {
        source.writeStep(i, true);
    }

    paged.begin(display, modelOf(source));
    finishFrame();

    TEST_ASSERT_NOT_EQUAL_UINT16(before, display.ink());
}

// LE SENS de la conversion de bande, et rien d'autre.
//
// U8G2_R2 fait tourner de 180 degres AVANT le decoupage : la bande d'AFFICHAGE 0
// (lignes 0..7) montre donc les lignes LOGIQUES 56..63, ou aucun element ne se
// trouve — le plus bas est le chiffre de ratchet de la seconde ligne, a y 47. Les
// deux premieres bandes d'affichage ne doivent donc recevoir aucun appel de
// dessin, et la DERNIERE doit en recevoir : elle porte le titre, en haut du
// canvas logique.
//
// Inverser la conversion echange ces deux constats. C'est exactement le defaut
// qui laissait l'ecran quasi blanc, et qu'aucun test ne voyait tant que le faux
// affichage ne modelisait pas la rotation.
void test_the_band_conversion_is_the_right_way_round(void) {
    reset();
    source.writeStep(0, true);
    source.setRatchet(0, flexseq::RATCHET_2);

    uint16_t opsPerBand[FakeDisplay::PAGES];

    paged.begin(display, modelOf(source));
    opsPerBand[0] = display.ops;
    for (uint8_t i = 1; i < FakeDisplay::PAGES; ++i) {
        const uint16_t before = display.ops;
        paged.advance(display);
        opsPerBand[i] = static_cast<uint16_t>(display.ops - before);
    }

    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, opsPerBand[0],
        "bande d'affichage 0 = logique 56..63 : sous la grille, et le pied est parti");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, opsPerBand[1],
        "bande d'affichage 1 = logique 48..55 : sous la grille");
    TEST_ASSERT_GREATER_THAN_UINT16_MESSAGE(0, opsPerBand[FakeDisplay::PAGES - 1],
        "la derniere bande porte le titre");
}

// Une image complete laisse de l'encre : la reunion des bandes montre quelque
// chose, quel que soit le detail du decoupage.
void test_a_whole_frame_leaves_ink(void) {
    reset();
    source.writeStep(0, true);
    source.writeStep(13, true);

    paged.begin(display, modelOf(source));
    finishFrame();

    TEST_ASSERT_EQUAL_UINT8(FakeDisplay::PAGES, display.bands);
    TEST_ASSERT_GREATER_THAN_UINT16(0, display.ink());
}

/*
 * L'ecran principal, rendu par le meme etalement (lot 7)
 */

void test_the_main_screen_sends_every_band(void) {
    reset();
    const uint8_t before = display.sendCalls;
    paged.begin(display, mainModelOf());
    finishFrame();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(FakeDisplay::PAGES,
        static_cast<uint8_t>(display.sendCalls - before),
        "l'ecran principal ne saute aucune bande");
    TEST_ASSERT_EQUAL(flexseq::PagedScreen<FakeDisplay>::MODE_MAIN, paged.mode());
}

void test_the_main_screen_never_skips_even_on_a_second_frame(void) {
    reset();
    paged.begin(display, mainModelOf());
    finishFrame();
    const uint8_t before = display.sendCalls;
    paged.begin(display, mainModelOf());
    finishFrame();
    TEST_ASSERT_EQUAL_UINT8(FakeDisplay::PAGES,
                            static_cast<uint8_t>(display.sendCalls - before));
}

// Changer d'ecran doit forcer une image COMPLETE : sinon une bande sautee
// garderait les pixels de l'ecran precedent, et le saut est justement conditionne
// par une empreinte qui ne connait pas le changement d'ecran.
void test_switching_screen_forces_a_full_frame(void) {
    reset();
    paged.begin(display, modelOf(source));
    finishFrame();
    paged.begin(display, modelOf(source));   // titre identique : bandes sautees
    finishFrame();

    paged.begin(display, mainModelOf());     // bascule vers l'ecran principal
    finishFrame();

    memset(display.sent, 0, sizeof(display.sent));
    const uint8_t before = display.sendCalls;
    paged.begin(display, modelOf(source));   // retour a EDIT
    finishFrame();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(FakeDisplay::PAGES,
        static_cast<uint8_t>(display.sendCalls - before),
        "le retour a EDIT doit tout redessiner, pas sauter le titre");
    TEST_ASSERT_TRUE(display.sent[FakeDisplay::PAGES - 1]);
    TEST_ASSERT_TRUE(display.sent[0]);
}

void test_the_main_screen_leaves_ink(void) {
    reset();
    paged.begin(display, mainModelOf());
    finishFrame();
    TEST_ASSERT_TRUE_MESSAGE(display.ops > 0, "l'ecran principal n'a rien dessine");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_renders_and_sends_the_first_band);
    RUN_TEST(test_a_first_frame_sends_every_band);
    RUN_TEST(test_screen_stays_busy_until_the_last_band);
    RUN_TEST(test_advance_without_a_frame_does_nothing);
    RUN_TEST(test_editing_during_a_frame_does_not_tear_it);
    RUN_TEST(test_the_edit_shows_up_on_the_next_frame);
    RUN_TEST(test_the_band_conversion_is_the_right_way_round);
    RUN_TEST(test_a_whole_frame_leaves_ink);
    RUN_TEST(test_an_unchanged_title_skips_its_band);
    RUN_TEST(test_a_changed_title_redraws_its_band);
    RUN_TEST(test_only_the_title_band_is_ever_skipped);
    RUN_TEST(test_two_titles_with_the_same_character_sum_are_distinguished);
    RUN_TEST(test_the_safety_net_forces_a_full_frame_periodically);

    RUN_TEST(test_the_main_screen_sends_every_band);
    RUN_TEST(test_the_main_screen_never_skips_even_on_a_second_frame);
    RUN_TEST(test_switching_screen_forces_a_full_frame);
    RUN_TEST(test_the_main_screen_leaves_ink);
    return UNITY_END();
}
