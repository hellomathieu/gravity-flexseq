/*
 * trigger_probe — observe les SIX SORTIES du firmware de production pendant
 * qu'il sequence, un vrai esclave SSD1306 sur le bus.
 *
 * POURQUOI CETTE SONDE EXISTE. Le chemin complet « contenu du pattern -> onset
 * -> impulsion sur une broche » n'avait jamais ete exerce, dans aucun binaire :
 * `main.cpp` emet les triggers mais sa banque est vide (`patterns{}`) et aucune
 * UI ne permet encore d'y ecrire, tandis que `wokwi_main.cpp` porte du contenu
 * de demonstration mais n'instancie pas de TriggerSequencer. Les tests natifs
 * valident le domaine, la sonde d'ecran valide le rendu, la sonde de blocage
 * valide le temps — et la fonction musicale du module n'etait observee nulle
 * part. C'est ce trou que ce fichier ferme, AVANT le premier flash.
 *
 * LE FIRMWARE N'EST PAS INSTRUMENTE. Le contenu du pattern est ecrit
 * DIRECTEMENT dans la RAM simulee, a l'adresse du symbole `patternBank` lue par
 * `avr-nm`, exactement comme la sonde CV consomme le verrou de front. Le binaire
 * mesure est donc celui qui sera flashe, sans une ligne de plus. `Pattern` etant
 * du stockage brut (`packedSteps[3]` + `packedRatchets[12]`, taille figee par
 * static_assert) et le moteur relisant la banque a chaque step, l'ecriture prend
 * effet immediatement, sans redemarrage.
 *
 * CE QUE L'ON SAIT AVANT DE MESURER, donc ce que la mesure met a l'epreuve :
 *   - les 6 channels selectionnent le pattern 0 par defaut (constructeur de
 *     SequencerEngine), donc les six sorties doivent battre a l'identique ;
 *   - LENGTH = 16 et SUBDIV = /1 par defaut, l'horloge interne demarre a 120 BPM
 *     des `Clock::Init()` : un step dure donc 500 ms et la boucle 8 s ;
 *   - libGravity eteint ses sorties apres DEFAULT_TRIGGER_DURATION_MS = 5 ms,
 *     mais l'extinction a lieu dans `outputs[ch].Process()`, EN FIN DE loop().
 *     Un passage durant 8,44 ms au p90 pendant le rendu, l'impulsion ne peut pas
 *     mesurer 5 ms : elle vaut 5 ms arrondis au passage suivant. C'est une
 *     PREDICTION, et la sonde la verifie au lieu de la supposer.
 *
 * LA GIGUE EST LE CHIFFRE QUI MANQUAIT. Les triggers sont emis au drainage des
 * ticks, en tete de loop(), donc un trigger arrive en retard d'au plus un
 * passage. A 120 BPM en /1 c'est 1,7 % d'un step ; a SUBDIV rapide la meme
 * absolue devient une fraction bien plus grande. On la mesure ici comme l'ecart
 * a la grille ideale reconstruite sur la premiere impulsion observee.
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <sim_avr.h>
#include <sim_elf.h>
#include <sim_hex.h>
#include <sim_irq.h>
#include <avr_adc.h>
#include <avr_twi.h>
#include <avr_ioport.h>
#include <avr_eeprom.h>
#include <parts/ssd1306_virt.h>

#include "simavr_uart_quiet.h"

#define MCU        "atmega328p"
#define F_CPU_HZ   16000000UL

/* Broches de sortie, telles que peripherials.h les definit et telles que le
 * Nano les cable : D0..D7 -> PD0..PD7, D8..D13 -> PB0..PB5. */
#define OUT_COUNT 6
#define LINE_COUNT (OUT_COUNT + 1)   /* 6 sorties + PULSE */

static const struct { char port; uint8_t bit; const char *name; } LINES[LINE_COUNT] = {
    {'D', 7, "OUT1 (D7)"},
    {'B', 0, "OUT2 (D8)"},
    {'B', 2, "OUT3 (D10)"},
    {'D', 6, "OUT4 (D6)"},
    {'B', 1, "OUT5 (D9)"},
    {'B', 3, "OUT6 (D11)"},
    {'D', 3, "PULSE (D3)"},
};

/* Le motif ATTENDU. Il arrive par la ligne de commande, la meme liste servant a
 * fabriquer l'image EEPROM : le harnais ne porte plus de copie du contenu.
 * Positions VOLONTAIREMENT IRREGULIERES cote appelant : un motif regulier
 * (0,4,8,12) serait indistinguable d'un compteur qui derive. */
#define MAX_ACTIVE 36                /* Pattern::DEFAULT_TOTAL_STEPS */
static uint8_t g_expected[MAX_ACTIVE];
static int g_expected_count;
/* La longueur jouee arrive par la ligne de commande, comme le motif. Le
 * harnais ne la deduit pas de l'image : l'appelant la donne, et il donne la
 * meme valeur au generateur. Defaut SequencerEngine::DEFAULT_LENGTH. */
static int g_pattern_length = 16;
#define TICKS_PER_STEP 96            /* SUBDIV = /1 a 96 PPQN */
#ifndef BPM
#define BPM 120                      /* UiController::DEFAULT_TEMPO */
#endif

/* Injection CV. La voie 1 est ADC7, la voie 2 ADC6 (peripherials.h de
 * libGravity). On ne s'abonne QUE si l'appelant fournit une tension : sans
 * cela les trois courses nominales ne changent pas de regime. */
static avr_irq_t *g_adc_irq[2];
static int g_cv_mv[2];
static int g_inject;
static uint32_t g_adc_replies[2];

/* Impulsion RESET datee, par les leviers d'environnement RESET_PULSE_MV,
 * RESET_PULSE_MS, RESET_PULSE_WIDTH_MS et RESET_PULSE_SOURCE (1 ou 2). Sans
 * RESET_PULSE_MV les courses existantes ne changent pas de regime. */
static int g_pulse_mv;
static int g_pulse_idx;
static int g_pulse_base;
static double g_pulse_at_ms = 8137.0;
static double g_pulse_width_ms = 50.0;

#define MAX_EDGES 4096

typedef struct {
    uint64_t rise[MAX_EDGES];
    uint64_t fall[MAX_EDGES];
    int nrise, nfall;
    int last;                 /* dernier niveau vu, -1 = inconnu */
} line_t;

static line_t g_lines[LINE_COUNT];
static double g_play_ms = 0.0;

/* Lot XCLK : l'horloge EXTERNE, injectee sur PD2 (EXT_PIN vaut 2). libGravity
 * y attache une interruption sur FRONT MONTANT, donc un creneau carre produit
 * exactement un front par periode, quel que soit le niveau de repos de la
 * broche. La sonde pilote les deux niveaux elle-meme, donc elle ne depend
 * d'aucun pull-up.
 *
 * PLAY reste presse : il est INERTE hors horloge interne, et le firmware
 * demarre le transport a la premiere impulsion externe
 * (src/hal/TransportAdapter.cpp, fidele a Gravity.ino:321-322). */
static long g_ext_period_us;      /* 0 = pas d'injection */
static long g_ext_pulse_us = 1000;
static double g_ext_start_ms;
static long g_ext_edges;          /* fronts montants REELLEMENT injectes */

/* Lot XCLK.3 : le TEMOIN du timer. uClock programme OCR1A et le prediviseur de
 * TCCR1B a chaque changement de tempo (uClock/platforms/avr.h, setTimer). La
 * periode d'un tick de sortie se lit donc DIRECTEMENT dans le materiel simule,
 * sans dependre de la disposition d'une classe :
 *
 *     periode = (OCR1A + 1) x prediviseur / F_CPU
 *
 * C'est plus fort qu'une lecture de membre prive : le registre est ce que le
 * timer applique VRAIMENT. */
#define REG_TCCR1B 0x81
#define REG_OCR1AL 0x88
#define REG_OCR1AH 0x89
static double g_trace_ms;         /* 0 = pas de trace */
#define TRACE_MAX 512
static double g_trace_at[TRACE_MAX];
static double g_trace_period_us[TRACE_MAX];
static int g_trace_n;

static int prescaler_of(uint8_t tccr1b) {
    switch (tccr1b & 0x07) {
        case 1: return 1;
        case 2: return 8;
        case 3: return 64;
        case 4: return 256;
        case 5: return 1024;
        default: return 0;   /* 0 ou 6/7 : horloge arretee ou source externe */
    }
}
static avr_t *g_avr;

static double us(uint64_t cycles) { return (double)cycles * 1e6 / (double)F_CPU_HZ; }
static double ms(uint64_t cycles) { return (double)cycles * 1e3 / (double)F_CPU_HZ; }

static void pin_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
    (void)irq;
    line_t *l = (line_t *)param;
    const int level = value ? 1 : 0;
    if (level == l->last) {
        return;                       /* pas un front */
    }
    l->last = level;
    if (level) {
        if (l->nrise < MAX_EDGES) l->rise[l->nrise++] = g_avr->cycle;
    } else {
        if (l->nfall < MAX_EDGES) l->fall[l->nfall++] = g_avr->cycle;
    }
}

static int cmp_d(const void *a, const void *b)
{
    const double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* Ecarts attendus, en steps, deduits du motif : entre deux steps actifs
 * consecutifs, puis le retour au debut du cycle. */
static int expected_gaps(int *gaps)
{
    for (int i = 0; i < g_expected_count - 1; ++i) {
        gaps[i] = g_expected[i + 1] - g_expected[i];
    }
    gaps[g_expected_count - 1] =
        g_pattern_length - g_expected[g_expected_count - 1] + g_expected[0];
    return g_expected_count;
}

static int parse_steps(const char *list)
{
    g_expected_count = 0;
    const char *p = list;
    while (*p && g_expected_count < MAX_ACTIVE) {
        char *end = NULL;
        const long v = strtol(p, &end, 10);
        if (end == p || v < 0 || v >= MAX_ACTIVE) return 0;
        g_expected[g_expected_count++] = (uint8_t)v;
        p = end;
        if (*p == ',') ++p;
    }
    return g_expected_count > 0;
}

/* Precharge l'image de persistance dans l'EEPROM SIMULEE, avant le premier
 * cycle. Le firmware la lit dans son setup() : le mode et le contenu du pattern
 * arrivent donc par le format documente, jamais par un decalage dans une
 * structure privee. */
static int load_eeprom(avr_t *avr, const char *path, uint16_t base)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    static uint8_t image[1024];
    const size_t n = fread(image, 1, sizeof(image), f);
    fclose(f);
    if (n == 0) return 0;
    avr_eeprom_desc_t ee = { .ee = image, .offset = base, .size = (uint32_t)n };
    if (avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &ee) != 0) return 0;
    printf("EEPROM prechargee : %zu octets a %u, version %u\n", n, base, image[0]);
    return 1;
}

static void adc_trigger_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
    (void)irq; (void)param;
    avr_adc_mux_t mux;
    memcpy(&mux, &value, sizeof(mux) < sizeof(value) ? sizeof(mux) : sizeof(value));
    if (mux.src == 7) { ++g_adc_replies[0]; avr_raise_irq(g_adc_irq[0], g_cv_mv[0]); }
    else if (mux.src == 6) { ++g_adc_replies[1]; avr_raise_irq(g_adc_irq[1], g_cv_mv[1]); }
}

int main(int argc, char **argv)
{
    const char *fw = (argc > 1) ? argv[1] : ".pio/build/nanoatmega328/firmware.hex";
    const double seconds = (argc > 2) ? atof(argv[2]) : 20.0;
    const char *ee_path = (argc > 3) ? argv[3] : NULL;
    const uint16_t ee_base = (argc > 4) ? (uint16_t)strtol(argv[4], NULL, 0) : 384;
    const char *mode = (argc > 5) ? argv[5] : "clock";
    const char *steps = (argc > 6) ? argv[6] : "0,3,4,9,15";
    if (argc > 7) g_pattern_length = (int)strtol(argv[7], NULL, 10);
    if (argc > 9) {
        g_cv_mv[0] = (int)strtol(argv[8], NULL, 10);
        g_cv_mv[1] = (int)strtol(argv[9], NULL, 10);
        g_inject = 1;
    }
    const int seq = strcmp(mode, "seq") == 0;
    /* Course RATCHET : les onsets ne sont plus sur la grille des steps, donc
     * ni l ecart entre impulsions ni la gigue par rapport a cette grille ne
     * veulent dire quoi que ce soit. Le critere est le NOMBRE d impulsions,
     * et il est evalue par le script contre la course SEQ. */
    const int ratchet = strcmp(mode, "ratchet") == 0;
    /* Course CVRESET : un front injecte en cours de lecture re-origine la
     * grille du canal route. Le harnais emet les temps de front bruts, et le
     * script porte les trois criteres — rotation avant, latence, re-origine. */
    const int cvreset = strcmp(mode, "cvreset") == 0;
    /* Course CVPATTERN : la fixture P35 du lot STEP. Le harnais emet les temps
     * de front bruts ; le script compte les onsets du step ou l'index PATTERN
     * change, et rien d'autre n'est suppose. */
    const int cvpattern = strcmp(mode, "cvpattern") == 0;
    /* Course CVSTEP : six instances distinctes et CV1 -> STEP. Le harnais emet
     * les temps de front bruts des SIX sorties ; le script compare chaque flux
     * a son horaire litteral, a phase connue par l'onset arme de PLAY. */
    const int cvstep = strcmp(mode, "cvstep") == 0;
    const int raw_edges = cvreset || cvpattern || cvstep;

    setvbuf(stdout, NULL, _IONBF, 0);

    const char *pulse_env = getenv("RESET_PULSE_MV");
    if (pulse_env != NULL && atoi(pulse_env) > 0) {
        g_pulse_mv = atoi(pulse_env);
        const char *src = getenv("RESET_PULSE_SOURCE");
        g_pulse_idx = (src != NULL && atoi(src) == 2) ? 1 : 0;
        const char *at = getenv("RESET_PULSE_MS");
        if (at != NULL) g_pulse_at_ms = atof(at);
        const char *w = getenv("RESET_PULSE_WIDTH_MS");
        if (w != NULL) g_pulse_width_ms = atof(w);
    }

    const char *trace_env = getenv("EXT_TRACE_MS");
    if (trace_env != NULL && atof(trace_env) > 0.0) g_trace_ms = atof(trace_env);

    const char *ext_env = getenv("EXT_PERIOD_US");
    if (ext_env != NULL && atol(ext_env) > 0) {
        g_ext_period_us = atol(ext_env);
        const char *w = getenv("EXT_PULSE_US");
        if (w != NULL && atol(w) > 0) g_ext_pulse_us = atol(w);
        const char *at = getenv("EXT_START_MS");
        if (at != NULL) g_ext_start_ms = atof(at);
        if (g_ext_pulse_us >= g_ext_period_us) {
            fprintf(stderr, "EXT_PULSE_US (%ld) doit rester sous EXT_PERIOD_US "
                            "(%ld) : sinon la broche ne redescend jamais et "
                            "aucun front montant ne part.\n",
                    g_ext_pulse_us, g_ext_period_us);
            return 2;
        }
    }

    if (!ee_path) {
        fprintf(stderr, "image EEPROM requise : sans elle le mode et le contenu du "
                        "pattern ne peuvent pas atteindre le firmware.\n");
        return 2;
    }
    if (!parse_steps(steps)) {
        fprintf(stderr, "liste de steps refusee : %s\n", steps);
        return 2;
    }

    elf_firmware_t f = {{0}};
    strcpy(f.mmcu, MCU);
    f.frequency = F_CPU_HZ;
    /* La reference d'analogique doit etre renseignee comme pour la sonde CV :
     * un .hex ne porte pas de section .mmcu, et sans cela l'ADC de simavr garde
     * ~3,3 V — l'ISR de CvSampler tourne pendant toute cette mesure. */
    f.vcc = 5000; f.avcc = 5000; f.aref = 5000;
    sim_setup_firmware(fw, 0, &f, "trigger_probe");

    avr_t *avr = avr_make_mcu_by_name(MCU);
    if (!avr) { fprintf(stderr, "MCU inconnu : %s\n", MCU); return 1; }
    g_avr = avr;
    avr_init(avr);
    avr_load_firmware(avr, &f);

    /* Journal de console de l'UART desarme : le firmware emet du MIDI, et ce
     * chemin de simavr lit un octet hors bornes. Voir simavr_uart_quiet.h. */
    uart_quiet(avr, '0');

    if (!load_eeprom(avr, ee_path, ee_base)) {
        fprintf(stderr, "image EEPROM illisible ou refusee : %s\n", ee_path);
        return 2;
    }

    /* L'esclave SSD1306, dans les deux sens : la charge de rendu fait partie de
     * ce qu'on mesure. Sans lui les transferts avortent sur NACK et la gigue
     * mesuree serait celle d'une boucle qui ne dessine pas. */
    ssd1306_t oled;
    ssd1306_init(avr, &oled, 128, 64);
    avr_irq_t *twi_out = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_OUTPUT);
    avr_irq_t *twi_in  = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT);
    avr_connect_irq(twi_out, oled.irq + IRQ_SSD1306_TWI_OUT);
    avr_connect_irq(oled.irq + IRQ_SSD1306_TWI_IN, twi_in);

    if (g_inject) {
        g_adc_irq[0] = avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_ADC7);
        g_adc_irq[1] = avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_ADC6);
        avr_irq_register_notify(
            avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_OUT_TRIGGER),
            adc_trigger_hook, NULL);
    }

    for (int i = 0; i < LINE_COUNT; ++i) {
        g_lines[i].last = -1;
        avr_irq_t *pin = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(LINES[i].port),
                                      LINES[i].bit);
        if (!pin) { fprintf(stderr, "broche introuvable : %s\n", LINES[i].name); return 1; }
        avr_irq_register_notify(pin, pin_hook, &g_lines[i]);
    }

    const uint64_t target = (uint64_t)(seconds * (double)F_CPU_HZ);

    printf("firmware   %s\n", fw);
    printf("simulation %.1f s ; mode %s ; steps attendus", seconds, mode);
    for (int i = 0; i < g_expected_count; ++i) printf(" %u", g_expected[i]);
    printf(" sur %d\n\n", g_pattern_length);
    if (g_inject) {
        printf("injection CV   CV1 %d mV ; CV2 %d mV\n\n", g_cv_mv[0], g_cv_mv[1]);
    }
    if (g_pulse_mv > 0) {
        g_pulse_base = g_cv_mv[g_pulse_idx];
        printf("impulsion RESET  CV%d a %d mV, de %.1f a %.1f ms\n\n",
               g_pulse_idx + 1, g_pulse_mv, g_pulse_at_ms,
               g_pulse_at_ms + g_pulse_width_ms);
    }
    const uint64_t pulse_from = (uint64_t)(g_pulse_at_ms * 1e-3 * (double)F_CPU_HZ);
    const uint64_t pulse_to =
        (uint64_t)((g_pulse_at_ms + g_pulse_width_ms) * 1e-3 * (double)F_CPU_HZ);

    /* Le firmware demarre A L'ARRET depuis le 2026-08-25, comme l'original. La
     * sonde doit donc APPUYER SUR PLAY, sinon elle mesurerait le silence et le
     * prendrait pour un defaut. Le bouton est actif a l'etat bas (INPUT_PULLUP)
     * et libGravity ne declenche que sur le RELACHEMENT, sous 750 ms : une
     * impulsion basse de 60 ms passe le debounce de 10 ms sans atteindre le
     * seuil d'appui long. */
    const uint64_t play_down = (uint64_t)(0.60 * (double)F_CPU_HZ);
    g_play_ms = 0.66 * 1000.0;  /* le relachement, pas l appui */
    const uint64_t play_up   = (uint64_t)(0.66 * (double)F_CPU_HZ);
    avr_irq_t *play = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 5);
    if (!play) { fprintf(stderr, "broche PLAY introuvable\n"); return 1; }
    int play_state = 2;  /* ni 0 ni 1 : force la premiere ecriture */

    avr_irq_t *ext = NULL;
    uint64_t ext_from = 0, ext_period_cy = 0, ext_pulse_cy = 0;
    int ext_state = 0;
    if (g_ext_period_us > 0) {
        ext = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 2);
        if (!ext) { fprintf(stderr, "broche EXT introuvable\n"); return 1; }
        ext_from = (uint64_t)(g_ext_start_ms * 1e-3 * (double)F_CPU_HZ);
        ext_period_cy = (uint64_t)((double)g_ext_period_us * 1e-6 * (double)F_CPU_HZ);
        ext_pulse_cy = (uint64_t)((double)g_ext_pulse_us * 1e-6 * (double)F_CPU_HZ);
        if (ext_period_cy == 0 || ext_pulse_cy == 0) {
            fprintf(stderr, "periode externe trop courte pour la resolution du "
                            "cycle : %ld us\n", g_ext_period_us);
            return 2;
        }
        avr_raise_irq(ext, 0);
        printf("  horloge EXTERNE : PD2, periode %ld us, impulsion %ld us, "
               "depart %.1f ms\n",
               g_ext_period_us, g_ext_pulse_us, g_ext_start_ms);
    }

    while (avr->cycle < target) {
        const int want = (avr->cycle >= play_down && avr->cycle < play_up) ? 0 : 1;
        if (want != play_state) {
            play_state = want;
            avr_raise_irq(play, want);
        }
        if (g_pulse_mv > 0) {
            g_cv_mv[g_pulse_idx] =
                (avr->cycle >= pulse_from && avr->cycle < pulse_to)
                    ? g_pulse_mv : g_pulse_base;
        }
        if (g_trace_ms > 0.0 && g_trace_n < TRACE_MAX) {
            const double now_ms = 1000.0 * (double)avr->cycle / (double)F_CPU_HZ;
            if (g_trace_n == 0 || now_ms >= g_trace_at[g_trace_n - 1] + g_trace_ms) {
                const uint16_t ocr = (uint16_t)avr->data[REG_OCR1AL]
                                   | ((uint16_t)avr->data[REG_OCR1AH] << 8);
                const int pre = prescaler_of(avr->data[REG_TCCR1B]);
                g_trace_at[g_trace_n] = now_ms;
                g_trace_period_us[g_trace_n] =
                    (pre == 0) ? -1.0
                               : 1e6 * ((double)ocr + 1.0) * (double)pre
                                     / (double)F_CPU_HZ;
                ++g_trace_n;
            }
        }
        if (ext != NULL && avr->cycle >= ext_from) {
            const uint64_t phase = (avr->cycle - ext_from) % ext_period_cy;
            const int level = (phase < ext_pulse_cy) ? 1 : 0;
            if (level != ext_state) {
                ext_state = level;
                avr_raise_irq(ext, level);
                if (level == 1) ++g_ext_edges;
            }
        }
        int state = avr_run(avr);
        if (state == cpu_Done || state == cpu_Crashed) {
            printf("!! CPU arrete (state=%d) a %" PRIu64 " cycles\n", state, avr->cycle);
            break;
        }
    }

    /* --- Analyse ---------------------------------------------------------- */
    const double step_ms = 60000.0 / (double)BPM;   /* /1 : un step = une noire */

    printf("=== SORTIES ===\n");
    printf("  %-12s %7s %10s %10s %10s\n", "ligne", "impuls", "largeur", "min", "max");
    int active_lines = 0;
    double width_med[LINE_COUNT];
    for (int i = 0; i < LINE_COUNT; ++i) {
        line_t *l = &g_lines[i];
        if (l->nrise == 0) {
            printf("  %-12s %7s\n", LINES[i].name, "aucune");
            width_med[i] = 0.0;
            continue;
        }
        ++active_lines;
        /* Largeur : chaque descente appariee a la montee qui la precede. */
        static double w[MAX_EDGES];
        int nw = 0;
        for (int r = 0, ff = 0; r < l->nrise && nw < MAX_EDGES; ++r) {
            while (ff < l->nfall && l->fall[ff] <= l->rise[r]) ++ff;
            if (ff < l->nfall) w[nw++] = ms(l->fall[ff] - l->rise[r]);
        }
        if (nw) {
            qsort(w, nw, sizeof(double), cmp_d);
            width_med[i] = w[nw / 2];
            printf("  %-12s %7d %8.2f ms %7.2f ms %7.2f ms\n",
                   LINES[i].name, l->nrise, w[nw / 2], w[0], w[nw - 1]);
        } else {
            width_med[i] = 0.0;
            printf("  %-12s %7d %8s\n", LINES[i].name, l->nrise, "?");
        }
    }

    /* --- Motif joue, sur la ligne du channel 1 ---------------------------- */
    /*
     * ON NE SUPPOSE PAS LA PHASE DU PLAYHEAD. `transport.start()` a lieu dans
     * setup(), donc le moteur tourne deja depuis ~1 s quand l'injection arrive :
     * la premiere impulsion observee n'est PAS le premier step actif du pattern,
     * et une premiere version de cette sonde, qui l'avait suppose, a declare
     * « hors grille » un firmware parfaitement juste.
     *
     * Ce qui se verifie sans connaitre la phase, c'est la SUITE DES ECARTS entre
     * impulsions : elle doit etre une rotation cyclique des ecarts du pattern.
     * L'affirmation est exactement celle qui a un sens musical — le sequenceur
     * joue le motif ecrit, quelle que soit la position de depart.
     */
    printf("\n=== TRAIN (channel 1, mode %s) ===\n", mode);
    line_t *ref = &g_lines[0];

    int exp_gaps[MAX_ACTIVE];
    const int nexp = expected_gaps(exp_gaps);
    if (ratchet) {
        printf("  aucun ecart attendu : le ratchet place les onsets HORS de la\n"
               "  grille des steps. Le critere est le nombre d impulsions.\n");
    } else if (cvreset) {
        printf("  la grille se re-origine au front injecte ; les temps bruts\n"
               "  suivent, et le script porte les trois criteres.\n");
    } else if (cvpattern) {
        printf("  fixture P35 : les temps bruts suivent, et le script compte les\n"
               "  onsets du step ou l'index PATTERN change.\n");
    } else if (cvstep) {
        printf("  six instances, CV1 -> STEP : les temps bruts des six sorties\n"
               "  suivent, et le script tient un horaire litteral par sortie.\n");
    } else if (seq) {
        printf("  ecarts attendus (steps) :");
        for (int i = 0; i < nexp; ++i) printf(" %d", exp_gaps[i]);
        printf("   (a une rotation pres)\n");
    } else {
        printf("  ecart attendu (steps) : 1, a chaque step, malgre le motif charge\n");
    }

    double step_measured = 0.0;
    int gap_ok = 0, gap_total = 0;
    if (ratchet) {
        printf("  %d impulsions observees sur le channel 1\n", ref->nrise);
    } else if (raw_edges) {
        printf("  %d impulsions observees sur le channel 1\n", ref->nrise);
        printf("EDGES");
        for (int r = 0; r < ref->nrise; ++r) printf(" %.2f", ms(ref->rise[r]));
        printf("\n");
        if (cvstep) {
            for (int i = 0; i < OUT_COUNT; ++i) {
                printf("EDGES%d", i + 1);
                for (int r = 0; r < g_lines[i].nrise; ++r)
                    printf(" %.2f", ms(g_lines[i].rise[r]));
                printf("\n");
            }
        }
    } else if (ref->nrise >= 3) {
        /* DROP=<n> ignore un front sur n : deux steps se fondent en un seul
         * ecart, le train cesse d'etre conforme et le critere doit rougir. Sans
         * ce chemin, le vert ne prouverait rien. */
        const char *drop_env = getenv("DROP");
        const int drop = drop_env ? atoi(drop_env) : 0;

        static uint64_t kept[MAX_EDGES];
        int nkept = 0;
        for (int r = 0; r < ref->nrise; ++r) {
            if (drop > 1 && r > 0 && r % drop == 0) continue;
            kept[nkept++] = ref->rise[r];
        }

        static double gaps[MAX_EDGES];
        int nobs = 0;
        for (int r = 1; r < nkept; ++r) {
            gaps[nobs++] = ms(kept[r] - kept[r - 1]);
        }
        gap_total = nobs;

        if (!seq) {
            /* En CLOCK, tout ecart vaut UN step : la duree de step est
             * directement la mediane des intervalles, sans arrondi ni phase
             * supposee. C'est ce qui rend cette mesure independante du tempo
             * attendu — une erreur d'un facteur exact se voit, alors qu'un
             * compte de steps arrondi sur le tempo attendu serait
             * auto-confirmant. */
            static double sorted[MAX_EDGES];
            for (int i = 0; i < nobs; ++i) sorted[i] = gaps[i];
            qsort(sorted, nobs, sizeof(double), cmp_d);
            step_measured = sorted[nobs / 2];
            for (int i = 0; i < nobs; ++i) {
                const double err = fabs(gaps[i] - step_measured) / step_measured;
                if (err <= 0.05) ++gap_ok;
                else printf("    ecart %d : %.2f ms au lieu de %.2f   <-- IRREGULIER\n",
                            i + 1, gaps[i], step_measured);
            }
            printf("  %d ecarts observes, mediane %.2f ms\n", nobs, step_measured);
        } else {
            /* En SEQ un ecart vaut PLUSIEURS steps, donc chacun est converti en
             * nombre de steps avant d'etre compare. La conversion part de la
             * duree de step attendue, mais elle ne peut pas se confirmer
             * elle-meme : le resultat doit tomber sur un ENTIER a 5 % pres, et la
             * duree de step rapportee est ensuite recalculee sur la somme
             * observee divisee par la somme des steps — une mesure, et non
             * l'hypothese de depart. La PHASE reste inconnue : l'affirmation est
             * que la suite des ecarts est une rotation cyclique de celle du
             * motif, ce qui est exactement l'affirmation qui a un sens musical. */
            static int k[MAX_EDGES];
            double sum_gap = 0.0;
            long sum_k = 0;
            for (int i = 0; i < nobs; ++i) {
                const int rounded = (int)(gaps[i] / step_ms + 0.5);
                const double ideal = (double)rounded * step_ms;
                if (rounded < 1 || fabs(gaps[i] - ideal) > 0.05 * step_ms) {
                    k[i] = 0;
                    printf("    ecart %d : %.2f ms ne tombe pas sur un step\n",
                           i + 1, gaps[i]);
                } else {
                    k[i] = rounded;
                    sum_gap += gaps[i];
                    sum_k += rounded;
                }
            }
            int best = -1, best_phase = 0;
            for (int ph = 0; ph < nexp; ++ph) {
                int hits = 0;
                for (int i = 0; i < nobs; ++i) {
                    if (k[i] == exp_gaps[(i + ph) % nexp]) ++hits;
                }
                if (hits > best) { best = hits; best_phase = ph; }
            }
            gap_ok = best < 0 ? 0 : best;
            if (sum_k > 0) step_measured = sum_gap / (double)sum_k;
            printf("  %d ecarts observes, %d conformes, phase %d\n",
                   nobs, gap_ok, best_phase);
            if (gap_ok != nobs) {
                printf("    suite observee (steps) :");
                for (int i = 0; i < nobs && i < 32; ++i) printf(" %d", k[i]);
                printf("\n    suite attendue         :");
                for (int i = 0; i < nobs && i < 32; ++i)
                    printf(" %d", exp_gaps[(i + best_phase) % nexp]);
                printf("\n");
            }
        }
    } else {
        printf("  moins de trois impulsions : rien a comparer\n");
    }

    /* --- Gigue ------------------------------------------------------------ */
    printf("\n=== GIGUE (ecart a la grille ideale) ===\n");
    double jit_max = 0.0, jit_med = 0.0;
    if (ratchet) {
        printf("  sans objet : les onsets d un ratchet ne sont pas sur la grille\n");
    } else if (cvreset) {
        printf("  sans objet : la grille se re-origine au front injecte\n");
    } else if (cvpattern) {
        printf("  sans objet : la fixture etire les steps par le TRIPLET\n");
    } else if (cvstep) {
        printf("  sans objet : chaque sortie tient son propre horaire\n");
    } else if (ref->nrise >= 3) {
        static double jit[MAX_EDGES];
        int nj = 0;
        /* D79 (2026-09-02) : le PREMIER front est l'onset ARME du step 0, emis
         * au premier tick draine apres le Start — il vit HORS de la grille des
         * franchissements, decale d'un passage de boucle par construction.
         * L'ancrer decalerait toute la grille ideale et ferait lire ce decalage
         * comme de la gigue (mesure : 3,4 % pour un budget de 2). La grille
         * s'ancre donc sur le premier front DE FRANCHISSEMENT, et le front arme
         * reste tenu par ses propres criteres : le silence avant PLAY ici, la
         * fenetre de la course pile, l'appariement de la sonde de derive. */
        const int first_grid = 1;
        const uint64_t origin = ref->rise[first_grid];
        for (int r = first_grid + 1; r < ref->nrise; ++r) {
            const double t = ms(ref->rise[r] - origin);
            const int step = (int)((t + step_ms / 2.0) / step_ms);
            double d = t - (double)step * step_ms;
            if (d < 0) d = -d;
            jit[nj++] = d;
        }
        qsort(jit, nj, sizeof(double), cmp_d);
        jit_med = jit[nj / 2];
        jit_max = jit[nj - 1];
        printf("  n=%d   mediane %.2f ms   p90 %.2f ms   max %.2f ms\n",
               nj, jit_med, jit[(nj * 9) / 10], jit_max);
        printf("  soit %.1f %% d'un step de %.0f ms au pire\n",
               100.0 * jit_max / step_ms, step_ms);

    } else {
        printf("  trop peu d'impulsions pour mesurer une gigue\n");
    }

    printf("\n=== TEMPO APPLIQUE ===\n");
    printf("  duree de step mesuree %.2f ms   attendue %.2f ms a %d BPM   ecart %.2f %%\n",
           step_measured, step_ms, BPM,
           step_ms > 0.0 ? 100.0 * (step_measured - step_ms) / step_ms : 0.0);

    /* --- Concordance entre channels --------------------------------------- */
    printf("\n=== CONCORDANCE DES SIX CHANNELS ===\n");
    int same_count = 1, coincident = 1;
    for (int i = 1; i < OUT_COUNT; ++i) {
        if (g_lines[i].nrise != ref->nrise) { same_count = 0; continue; }
        for (int r = 0; r < ref->nrise; ++r) {
            const double d = us(g_lines[i].rise[r] > ref->rise[r]
                                ? g_lines[i].rise[r] - ref->rise[r]
                                : ref->rise[r] - g_lines[i].rise[r]);
            if (d > 200.0) { coincident = 0; break; }   /* 200 us = quelques passages d'ISR */
        }
    }
    printf("  meme nombre d'impulsions sur les 6 : %s\n", same_count ? "oui" : "NON");
    printf("  fronts coincidents (< 200 us)      : %s\n", coincident ? "oui" : "NON");
    printf("  PULSE (expandeur MIDI)             : %d impulsion(s)\n", g_lines[6].nrise);

    /* --- Recapitulatif lisible par le script ----------------------------- */
    printf("\nRESULTAT lignes_actives=%d attendu=%d ecarts_ok=%d/%d "
           "largeur_med=%.2f gigue_med=%.2f gigue_max=%.2f step_ms=%.1f "
           "meme_compte=%d coincident=%d impulsions_ch1=%d pulse=%d "
           "step_mesure=%.2f bpm=%d mode=%s premier_front_ms=%.1f play_ms=%.1f\n",
           active_lines, OUT_COUNT, gap_ok, gap_total,
           width_med[0], jit_med, jit_max, step_ms,
           same_count, coincident, ref->nrise, g_lines[6].nrise,
           step_measured, BPM, mode,
           ref->nrise > 0 ? ms(ref->rise[0]) : -1.0, g_play_ms);
    printf("RESET reset_ms=%.1f\n", g_pulse_mv > 0 ? g_pulse_at_ms : -1.0);
    if (g_trace_n > 0) {
        printf("=== TEMOIN DU TIMER (OCR1A et prediviseur) ===\n");
        printf("  %10s %16s %14s\n", "t (ms)", "periode tick (us)", "step /1 (ms)");
        double last = -2.0;
        for (int i = 0; i < g_trace_n; ++i) {
            /* n'imprimer qu'un CHANGEMENT : une trace plate n'apprend rien */
            if (i > 0 && g_trace_period_us[i] == last) continue;
            last = g_trace_period_us[i];
            if (g_trace_period_us[i] < 0.0) {
                printf("  %10.1f %16s %14s\n", g_trace_at[i], "timer arrete", "-");
            } else {
                printf("  %10.1f %16.2f %14.2f\n", g_trace_at[i],
                       g_trace_period_us[i], g_trace_period_us[i] * 96.0 / 1000.0);
            }
        }
        printf("  %d echantillon(s), %.1f ms d'intervalle\n", g_trace_n, g_trace_ms);
    }
    printf("EXTCLK ext_period_us=%ld ext_pulse_us=%ld ext_start_ms=%.1f "
           "ext_edges=%ld\n",
           g_ext_period_us, g_ext_period_us > 0 ? g_ext_pulse_us : 0L,
           g_ext_period_us > 0 ? g_ext_start_ms : -1.0, g_ext_edges);
    return 0;
}
