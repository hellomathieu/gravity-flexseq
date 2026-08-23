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
#include <avr_twi.h>
#include <avr_ioport.h>
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

/* Contenu injecte. Positions VOLONTAIREMENT IRREGULIERES : un motif regulier
 * (0,4,8,12) serait indistinguable d'un compteur qui derive, alors que les
 * ecarts 3-1-5-6-1 ne sortent d'aucune erreur d'un pas. */
#define ACTIVE_COUNT 5
static const uint8_t ACTIVE_STEPS[ACTIVE_COUNT] = {0, 3, 4, 9, 15};
#define PATTERN_LENGTH 16            /* SequencerEngine::DEFAULT_LENGTH */
#define TICKS_PER_STEP 96            /* SUBDIV = /1 a 96 PPQN */
#ifndef BPM
#define BPM 120                      /* UiController::DEFAULT_TEMPO */
#endif

#define MAX_EDGES 4096

typedef struct {
    uint64_t rise[MAX_EDGES];
    uint64_t fall[MAX_EDGES];
    int nrise, nfall;
    int last;                 /* dernier niveau vu, -1 = inconnu */
} line_t;

static line_t g_lines[LINE_COUNT];
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

/* Ecrit le contenu de demonstration dans la banque, en RAM simulee.
 * Disposition de Pattern : packedSteps[3] puis packedRatchets[12], le step i
 * occupant le bit (i % 8) de packedSteps[i / 8]. Les ratchets restent a zero,
 * RATCHET_NONE valant 0 : un onset par step actif. */
static void inject_pattern(avr_t *avr, uint16_t bank_addr)
{
    for (uint8_t i = 0; i < 3; ++i) {
        avr->data[bank_addr + i] = 0;
    }
    for (uint8_t i = 0; i < ACTIVE_COUNT; ++i) {
        const uint8_t s = ACTIVE_STEPS[i];
        avr->data[bank_addr + (s / 8)] |= (uint8_t)(1u << (s % 8));
    }
    /* MUTATE=<step> ajoute un step actif a l'INJECTION sans l'ajouter a
     * l'ATTENTE : le verdict doit alors passer au rouge. C'est ainsi que le
     * chemin d'echec de cette sonde a ete exerce — un test vert ne prouve rien
     * tant qu'il n'a pas ete rouge. */
    const char *mut = getenv("MUTATE");
    if (mut) {
        const int extra = atoi(mut);
        if (extra >= 0 && extra < 24) {
            avr->data[bank_addr + (extra / 8)] |= (uint8_t)(1u << (extra % 8));
            printf("MUTATION : step %d ajoute a l'injection, pas a l'attente\n", extra);
        }
    }
    for (uint8_t i = 0; i < 12; ++i) {
        avr->data[bank_addr + 3 + i] = 0;
    }
}

int main(int argc, char **argv)
{
    const char *fw = (argc > 1) ? argv[1] : ".pio/build/nanoatmega328/firmware.hex";
    const double seconds = (argc > 2) ? atof(argv[2]) : 20.0;
    const uint16_t bank_addr = (argc > 3) ? (uint16_t)strtol(argv[3], NULL, 0) : 0;

    setvbuf(stdout, NULL, _IONBF, 0);

    if (!bank_addr) {
        fprintf(stderr, "adresse de `patternBank` requise : sans elle la banque "
                        "reste vide et aucun trigger ne peut sortir.\n");
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

    /* L'esclave SSD1306, dans les deux sens : la charge de rendu fait partie de
     * ce qu'on mesure. Sans lui les transferts avortent sur NACK et la gigue
     * mesuree serait celle d'une boucle qui ne dessine pas. */
    ssd1306_t oled;
    ssd1306_init(avr, &oled, 128, 64);
    avr_irq_t *twi_out = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_OUTPUT);
    avr_irq_t *twi_in  = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT);
    avr_connect_irq(twi_out, oled.irq + IRQ_SSD1306_TWI_OUT);
    avr_connect_irq(oled.irq + IRQ_SSD1306_TWI_IN, twi_in);

    for (int i = 0; i < LINE_COUNT; ++i) {
        g_lines[i].last = -1;
        avr_irq_t *pin = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(LINES[i].port),
                                      LINES[i].bit);
        if (!pin) { fprintf(stderr, "broche introuvable : %s\n", LINES[i].name); return 1; }
        avr_irq_register_notify(pin, pin_hook, &g_lines[i]);
    }

    const uint64_t target = (uint64_t)(seconds * (double)F_CPU_HZ);
    /* Injection apres l'initialisation : constructeurs globaux, Gravity::Init()
     * et l'init de l'afficheur. Une seconde couvre largement. */
    const uint64_t inject_cycle = F_CPU_HZ;
    int injected = 0;

    printf("firmware   %s\n", fw);
    printf("simulation %.1f s ; banque a 0x%x ; steps actifs", seconds, bank_addr);
    for (int i = 0; i < ACTIVE_COUNT; ++i) printf(" %u", ACTIVE_STEPS[i]);
    printf(" sur %d\n\n", PATTERN_LENGTH);

    while (avr->cycle < target) {
        int state = avr_run(avr);
        if (state == cpu_Done || state == cpu_Crashed) {
            printf("!! CPU arrete (state=%d) a %" PRIu64 " cycles\n", state, avr->cycle);
            break;
        }
        if (!injected && avr->cycle >= inject_cycle) {
            inject_pattern(avr, bank_addr);
            injected = 1;
            printf("injection a %.3f s\n\n", ms(avr->cycle) / 1000.0);
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
    printf("\n=== TRAIN CLOCK (channel 1) ===\n");
    line_t *ref = &g_lines[0];

    printf("  ecart attendu (steps) : 1, a chaque step, malgre le motif injecte\n");

    double step_measured = 0.0;
    int gap_ok = 0, gap_total = 0;
    if (ref->nrise >= 3) {
        /* En CLOCK, tout ecart vaut UN step : la duree de step est directement
         * la mediane des intervalles, sans arrondi ni phase supposee. C'est ce
         * qui rend cette mesure independante du tempo attendu — une erreur d'un
         * facteur exact se voit, alors qu'un compte de steps arrondi sur le
         * tempo attendu serait auto-confirmant. */
        /* DROP=<n> ignore un front sur n : deux steps se fondent en un seul
         * ecart, le train cesse d'etre regulier et le critere doit rougir. Sans
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
        static double sorted[MAX_EDGES];
        for (int i = 0; i < nobs; ++i) sorted[i] = gaps[i];
        qsort(sorted, nobs, sizeof(double), cmp_d);
        step_measured = sorted[nobs / 2];

        /* Un ecart est conforme s'il vaut la mediane a 5 % pres : c'est la
         * regularite du train qui est verifiee, pas sa valeur absolue, dont le
         * critere « tempo applique » se charge separement. */
        gap_total = nobs;
        for (int i = 0; i < nobs; ++i) {
            const double err = fabs(gaps[i] - step_measured) / step_measured;
            if (err <= 0.05) ++gap_ok;
            else printf("    ecart %d : %.2f ms au lieu de %.2f   <-- IRREGULIER\n",
                        i + 1, gaps[i], step_measured);
        }
        printf("  %d ecarts observes, mediane %.2f ms\n", nobs, step_measured);
    } else {
        printf("  moins de trois impulsions : rien a comparer\n");
    }

    /* --- Gigue ------------------------------------------------------------ */
    printf("\n=== GIGUE (ecart a la grille ideale) ===\n");
    double jit_max = 0.0, jit_med = 0.0;
    if (ref->nrise >= 3) {
        static double jit[MAX_EDGES];
        int nj = 0;
        const uint64_t origin = ref->rise[0];
        for (int r = 1; r < ref->nrise; ++r) {
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
           "step_mesure=%.2f bpm=%d\n",
           active_lines, OUT_COUNT, gap_ok, gap_total,
           width_med[0], jit_med, jit_max, step_ms,
           same_count, coincident, ref->nrise, g_lines[6].nrise,
           step_measured, BPM);
    return 0;
}
