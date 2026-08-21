/*
 * screen_dump — lit la MEMOIRE D'ECRAN que le panneau SSD1306 recoit reellement,
 * et verifie la geometrie de bout en bout.
 *
 * Pourquoi. Le rendu OLED n'avait jamais ete constate autrement qu'a l'oeil dans
 * Wokwi — et la prise en charge de la rotation par sa piece `board-ssd1306`
 * n'avait jamais ete verifiee du tout (PRD 14). Or simavr modelise un vrai
 * esclave SSD1306 qui expose sa `vram` : on peut donc lire l'image que le
 * panneau affiche, sans Wokwi, sans jeton, et en faire des ASSERTIONS.
 *
 * Ce qui est verifie, et pourquoi c'est fort. Le firmware dessine dans un canvas
 * LOGIQUE ; U8g2 applique `U8G2_R2` (180 degres) avant d'ecrire dans la memoire du
 * panneau. Donc un point logique (x, y) atterrit en (127-x, 63-y). Le harnais
 * connait la geometrie par `flexseq/PatternScreen.h` — la meme source que le
 * firmware, aucune constante recopiee — et verifie que l'encre se trouve la ou
 * cette transformation la place. Un rendu sans rotation, ou une geometrie
 * derivee, echoue.
 *
 * WATCH=<ms> — SURVEILLANCE CONTINUE. Echantillonne la memoire du panneau a
 * intervalle regulier pendant toute la simulation et verifie qu'aucune bande,
 * UNE FOIS QU'ELLE A PORTE DE L'ENCRE, ne se retrouve vide. C'est la seule facon
 * dont un scintillement pourrait apparaitre : PagedScreen saute les bandes
 * inchangees, et si le cycle etait coupe au mauvais endroit — tampon efface puis
 * envoye sans avoir ete redessine — la bande disparaitrait le temps d'une image.
 * Le SSD1306 etant a memoire, ne rien envoyer ne peut rien effacer ; c'est
 * l'erreur d'implementation qu'on cherche, pas le comportement de l'ecran.
 *
 * La rotation se lit alors d'elle-meme : le titre, en haut du canvas logique,
 * doit apparaitre EN BAS du panneau. C'est ce qui justifie le montage physique
 * de l'OLED a 180 degres, et donc le `"rotate": 180` de diagram.json.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <sim_avr.h>
#include <sim_elf.h>
#include <sim_hex.h>
#include <sim_irq.h>
#include <avr_twi.h>
#include <parts/ssd1306_virt.h>
}

#include <flexseq/PatternScreen.h>

#include "simavr_uart_quiet.h"

namespace scr = flexseq::screen;

#define MCU      "atmega328p"
#define F_CPU_HZ 16000000UL
#define PANEL_W  128
#define PANEL_H  64

static ssd1306_t oled;
static uint32_t twi_starts, twi_data_bytes;

static void twi_watch(struct avr_irq_t*, uint32_t value, void*)
{
    avr_twi_msg_irq_t v;
    memcpy(&v, &value, sizeof(v) < sizeof(value) ? sizeof(v) : sizeof(value));
    if (v.u.twi.msg & TWI_COND_START) ++twi_starts;
    if (v.u.twi.msg & TWI_COND_WRITE) ++twi_data_bytes;
}

/* Un pixel de la memoire du panneau. Une page = 8 pixels VERTICAUX, bit 0 en
 * haut de la page. */
static inline int panel(uint8_t x, uint8_t y)
{
    if (x >= PANEL_W || y >= PANEL_H) return 0;
    return (oled.vram[y / 8][x] >> (y % 8)) & 1;
}

/* La transformation de U8G2_R2 : 180 degres. */
static inline uint8_t rotX(uint8_t x) { return (uint8_t)(PANEL_W - 1 - x); }
static inline uint8_t rotY(uint8_t y) { return (uint8_t)(PANEL_H - 1 - y); }

static int inkInBox(uint8_t cx, uint8_t cy, uint8_t half)
{
    int n = 0;
    for (int dy = -(int)half; dy <= (int)half; ++dy)
        for (int dx = -(int)half; dx <= (int)half; ++dx) {
            const int x = (int)cx + dx, y = (int)cy + dy;
            if (x >= 0 && y >= 0) n += panel((uint8_t)x, (uint8_t)y);
        }
    return n;
}

static int inkInRows(uint8_t y0, uint8_t y1)
{
    int n = 0;
    for (uint8_t y = y0; y <= y1 && y < PANEL_H; ++y)
        for (uint8_t x = 0; x < PANEL_W; ++x) n += panel(x, y);
    return n;
}

static void dumpAscii(const char* title, int flip)
{
    printf("%s\n   +", title);
    for (int i = 0; i < PANEL_W; ++i) putchar('-');
    printf("+\n");
    for (uint8_t r = 0; r < PANEL_H; ++r) {
        const uint8_t y = flip ? rotY(r) : r;
        printf("%2u |", r);
        for (uint8_t c = 0; c < PANEL_W; ++c) {
            const uint8_t x = flip ? rotX(c) : c;
            putchar(panel(x, y) ? '#' : ' ');
        }
        printf("|\n");
    }
    printf("   +");
    for (int i = 0; i < PANEL_W; ++i) putchar('-');
    printf("+\n");
}

static void writePgm(const char* path)
{
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P5\n%d %d\n255\n", PANEL_W, PANEL_H);
    for (uint8_t y = 0; y < PANEL_H; ++y)
        for (uint8_t x = 0; x < PANEL_W; ++x) fputc(panel(x, y) ? 255 : 0, f);
    fclose(f);
    printf("image du panneau ecrite : %s\n", path);
}

int main(int argc, char** argv)
{
    const char* fw = (argc > 1) ? argv[1] : ".pio/build/wokwi/firmware.hex";
    const double seconds = (argc > 2) ? atof(argv[2]) : 3.0;
    const uint8_t length = (argc > 3) ? (uint8_t)atoi(argv[3]) : 20;  /* LENGTH du contenu */
    const char* pgm = (argc > 4) ? argv[4] : NULL;

    setvbuf(stdout, NULL, _IONBF, 0);  /* voir blocking_probe.c */

    elf_firmware_t f = {{0}};
    /* AVANT le chargement : voir blocking_probe.c. */
    strcpy(f.mmcu, MCU);
    f.frequency = F_CPU_HZ;
    f.vcc = f.avcc = f.aref = 5000;
    sim_setup_firmware(fw, 0, &f, "screen_dump");

    avr_t* avr = avr_make_mcu_by_name(MCU);
    if (!avr) { fprintf(stderr, "MCU inconnu\n"); return 1; }
    avr_init(avr);
    avr_load_firmware(avr, &f);

    /* Journal de console de l'UART desarme : le firmware emet du MIDI, et ce
     * chemin de simavr lit un octet hors bornes. Voir simavr_uart_quiet.h. */
    uart_quiet(avr, '0');

    ssd1306_init(avr, &oled, PANEL_W, PANEL_H);
    avr_irq_t* twi_out = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_OUTPUT);
    avr_irq_t* twi_in = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT);
    avr_connect_irq(twi_out, oled.irq + IRQ_SSD1306_TWI_OUT);
    avr_connect_irq(oled.irq + IRQ_SSD1306_TWI_IN, twi_in);
    avr_irq_register_notify(twi_out, twi_watch, NULL);

    const uint64_t target = (uint64_t)(seconds * (double)F_CPU_HZ);

    /* Surveillance : encre par bande a chaque echantillon. */
    const double watch_ms = getenv("WATCH") ? atof(getenv("WATCH")) : 0.0;
    const uint64_t watch_period = (uint64_t)(watch_ms / 1000.0 * F_CPU_HZ);
    uint64_t next_watch = watch_period;
    int inked[8] = {0}, blanked[8] = {0};
    uint16_t low[8], high[8];
    uint32_t samples = 0;
    for (int b = 0; b < 8; ++b) { low[b] = 0xFFFF; high[b] = 0; }

    while (avr->cycle < target) {
        const int state = avr_run(avr);
        if (state == cpu_Done || state == cpu_Crashed) break;

        if (watch_period && avr->cycle >= next_watch) {
            next_watch += watch_period;
            ++samples;
            for (int b = 0; b < 8; ++b) {
                uint16_t n = 0;
                for (int c = 0; c < PANEL_W; ++c) {
                    uint8_t v = oled.vram[b][c];
                    while (v) { n += (v & 1); v >>= 1; }
                }
                /* Le minimum ne compte qu'APRES la premiere encre : avant, la
                 * bande est vide parce que rien n'y a encore ete ecrit, ce qui
                 * n'est pas une disparition. */
                if (n > 0) {
                    inked[b] = 1;
                    if (n < low[b]) low[b] = n;
                } else if (inked[b]) {
                    low[b] = 0;
                    blanked[b] = 1;   /* elle a PERDU son contenu */
                }
                if (n > high[b]) high[b] = n;
            }
        }
    }

    printf("firmware  %s\nsimulation %.1f s\n", fw, seconds);
    printf("bus I2C   %u transactions, %u octets ecrits\n"
           "curseur   page %u colonne %u, mode %u, drapeaux 0x%04x\n\n",
           twi_starts, twi_data_bytes, oled.cursor.page, oled.cursor.column,
           (unsigned)oled.addr_mode, oled.flags);
    printf("\n");

    const int total = inkInRows(0, PANEL_H - 1);
    if (total == 0) {
        printf("La memoire du panneau est VIDE : rien n'a ete transfere.\n");
        return 1;
    }

    if (getenv("ASCII")) {
        dumpAscii("--- memoire du panneau, telle quelle ---", 0);
        dumpAscii("--- la meme, retournee de 180 degres : ce que voit l'oeil ---", 1);
    }
    if (pgm) writePgm(pgm);

    /* --- 1. geometrie : chaque step a sa place, APRES rotation --------------
     * SKIP_GEOMETRY=1 pour un firmware qui n'affiche pas l'ecran EDIT PATTERN
     * (env:bringup, par exemple) : seule la rotation reste verifiable. */
    const int skip_geometry = getenv("SKIP_GEOMETRY") != NULL;
    int placed = 0, missing = 0;
    for (uint8_t i = 0; !skip_geometry && i < flexseq::Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        const uint8_t px = rotX(scr::colX(i));
        const uint8_t py = rotY(scr::rowCY(i));
        /* Un step au-dela de LENGTH n'est qu'un point : on tolere 1 pixel. */
        const int need = (i < length) ? 3 : 1;
        if (inkInBox(px, py, scr::GLYPH_HALF) >= need) ++placed; else ++missing;
    }

    /* --- 2. rotation : le titre est en BAS du panneau ----------------------- */
    /* Titre logique : ligne de base y=8, glyphes de y=2 a 8 -> panneau 55..61. */
    const int inkTitleBand = inkInRows(rotY(scr::TITLE_BASELINE_Y) - 6,
                                      (uint8_t)(rotY(scr::TITLE_BASELINE_Y) + 2));
    /* Rien ne doit se trouver au-dessus de la ligne 1 des chiffres de ratchet,
     * qui atterrit en panneau ~16 : les deux premieres bandes sont vides. */
    const int inkTop = inkInRows(0, 15);

    int watch_ok = 1;
    if (watch_period) {
        printf("=== SURVEILLANCE (%u echantillons, un toutes les %.1f ms) ===\n",
               samples, watch_ms);
        for (int b = 0; b < 8; ++b) {
            if (!inked[b]) { printf("    bande %d : jamais d'encre\n", b); continue; }
            printf("    bande %d : encre de %u a %u pixels%s\n", b, low[b], high[b],
                   blanked[b] ? "   <-- S'EST VIDEE" : "");
            if (blanked[b]) watch_ok = 0;
        }
        printf("  %s\n\n", watch_ok
               ? "aucune bande ne s'est jamais videe : pas de scintillement possible"
               : "UNE BANDE S'EST VIDEE : un cycle a ete coupe au mauvais endroit");
    }

    printf("=== GEOMETRIE (apres U8G2_R2) ===\n");
    if (skip_geometry) {
        printf("  ignoree (SKIP_GEOMETRY)\n");
    } else {
        printf("  %d / %d steps a leur place attendue%s\n", placed,
               (int)flexseq::Pattern::DEFAULT_TOTAL_STEPS, missing ? "  <-- INCOHERENT" : "");
    }
    printf("  encre totale %d pixels\n\n", total);
    printf("=== ROTATION ===\n");
    printf("  bande du titre (panneau y %u..%u) : %d pixels\n",
           rotY(scr::TITLE_BASELINE_Y) - 6, rotY(scr::TITLE_BASELINE_Y) + 2, inkTitleBand);
    printf("  haut du panneau (y 0..15)         : %d pixels\n", inkTop);

    const int geometry_ok = skip_geometry || (missing == 0);
    const int rotation_ok = (inkTitleBand > 0 && inkTop == 0);
    printf("\n  geometrie %s   rotation %s\n",
           geometry_ok ? "OK" : "KO", rotation_ok ? "OK" : "KO");
    return (geometry_ok && rotation_ok && watch_ok) ? 0 : 1;
}
