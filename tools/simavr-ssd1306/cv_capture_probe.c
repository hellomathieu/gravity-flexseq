/*
 * cv_capture_probe — verifie qu'une impulsion CV de largeur donnee est VUE par
 * le firmware, dans les conditions reelles : rendu OLED actif, donc boucle
 * principale chargee.
 *
 * Pourquoi ce harnais. Le CV est desormais echantillonne sous interruption
 * (include/flexseq/CvSampler.h) pour garantir la capture d'une impulsion de
 * 1 ms, alors que le pire passage de boucle dure 7,74 ms (ADR 0001). Une
 * garantie non mesuree n'est pas une garantie.
 *
 * Le firmware n'est PAS instrumente. simavr accepte des valeurs en millivolts
 * sur ADC_IRQ_ADC7 et notifie ADC_IRQ_OUT_TRIGGER au debut de chaque conversion :
 * on y repond par la tension courante de l'impulsion simulee. Et le harnais joue
 * le CONSOMMATEUR du verrou — il lit le drapeau `pending` dans la RAM simulee et
 * l'efface, exactement ce que fera la boucle principale quand la destination
 * CV -> RESET existera. L'adresse du drapeau vient de avr-nm, passee en argument.
 *
 * L'esclave SSD1306 est attache : sans lui les transferts avortent sur NACK, la
 * boucle est irrealistement rapide, et la mesure ne prouverait rien.
 *
 * DEUX MODES. Par defaut le harnais consomme le verrou lui-meme, ce qu'il faut
 * pour un firmware qui n'a pas encore de consommateur. Avec EDGE_COUNTER=<adr>,
 * il n'y touche pas et compte les incrementations d'un compteur 16 bits du
 * firmware : la chaine verifiee inclut alors le consommateur reel. C'est le mode
 * utile pour env:bringup, et il le restera quand CV -> RESET existera.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <sim_avr.h>
#include <sim_elf.h>
#include <sim_hex.h>
#include <sim_irq.h>
#include <avr_adc.h>
#include <avr_twi.h>
#include <parts/ssd1306_virt.h>

#include "simavr_uart_quiet.h"

#define MCU      "atmega328p"
#define F_CPU_HZ 16000000UL

/* Tensions a l'entree du convertisseur, en millivolts. L'entree CV du Gravity
 * est bipolaire +/-5 V, conditionnee en 0..5 V : le zero mesure vaut 537 en
 * unites brutes, soit ~2625 mV. Le seuil d'armement (+1 V) est a 634, ~3099 mV.
 * On envoie franchement au-dessus. */
#define MV_IDLE  2625
#define MV_PULSE 4000

static avr_t *g_avr;
static avr_irq_t *g_adc_irq;      /* ADC_IRQ_ADC7 : la voie CV1 */
static uint64_t pulse_start;      /* cycle de debut de l'impulsion en cours */
static uint64_t pulse_cycles;     /* largeur, en cycles */
static uint64_t period_cycles;
static uint32_t injected;
static uint32_t hook_calls, hook_src7, hook_other_src;
static uint32_t last_src = 999;

/* Vrai si l'instant courant tombe dans une impulsion. */
static int inside_pulse(void)
{
    if (pulse_start == 0 || g_avr->cycle < pulse_start) return 0;
    return (g_avr->cycle - pulse_start) < pulse_cycles;
}

/* simavr demande la valeur de la voie au moment ou la conversion demarre. */
static void adc_trigger_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
    /* La valeur transporte un avr_adc_mux_t (champs de bits, sans membre
     * scalaire) : on la recopie plutot que de la transtyper. */
    avr_adc_mux_t mux;
    memcpy(&mux, &value, sizeof(mux) < sizeof(value) ? sizeof(mux) : sizeof(value));
    ++hook_calls;
    last_src = mux.src;
    if (mux.src != 7) { ++hook_other_src; return; }   /* seule CV1 est pilotee */
    ++hook_src7;
    avr_raise_irq(g_adc_irq, inside_pulse() ? MV_PULSE : MV_IDLE);
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr,
                "usage: %s <firmware.hex> <adresse_pending> <largeur_us> "
                "[periode_us] [duree_s] [adresse_completed]\n", argv[0]);
        return 2;
    }
    const char *fw = argv[1];
    const uint16_t pending_addr = (uint16_t)strtol(argv[2], NULL, 0);
    const double width_us = atof(argv[3]);
    const double period_us = (argc > 4) ? atof(argv[4]) : 400000.0;
    const double seconds = (argc > 5) ? atof(argv[5]) : 6.0;
    const uint16_t done_addr = (argc > 6) ? (uint16_t)strtol(argv[6], NULL, 0) : 0;

    pulse_cycles = (uint64_t)(width_us * (double)F_CPU_HZ / 1e6);
    period_cycles = (uint64_t)(period_us * (double)F_CPU_HZ / 1e6);
    if (pulse_cycles == 0) pulse_cycles = 1;

    /* Sortie NON TAMPONNEE : redirigee vers un fichier, stdout l'est par blocs,
     * et le rapport disparaissait entierement si le harnais plantait — on
     * cherchait alors le defaut la ou il n'etait pas. */
    setvbuf(stdout, NULL, _IONBF, 0);

    elf_firmware_t f = {{0}};
    /* AVANT le chargement : voir blocking_probe.c. */
    strcpy(f.mmcu, MCU);
    f.frequency = F_CPU_HZ;
    sim_setup_firmware(fw, 0, &f, "cv_capture_probe");
    /* Les tensions d'alimentation et de reference, en millivolts. Elles viennent
     * normalement de la section .mmcu du firmware, que le .hex ne porte pas :
     * sans elles simavr retient une reference de ~3,3 V, et 2625 mV de repos se
     * lisent alors 813 au lieu de 537 — au-dessus du seuil d'armement, donc la
     * porte s'arme une fois et ne se rearme jamais. Diagnostique en constatant
     * `latest` fige a 813. */
    f.vcc = 5000;
    f.avcc = 5000;
    f.aref = 5000;

    avr_t *avr = avr_make_mcu_by_name(MCU);
    if (!avr) { fprintf(stderr, "MCU inconnu\n"); return 1; }
    g_avr = avr;
    avr_init(avr);
    avr_load_firmware(avr, &f);

    /* Journal de console de l'UART desarme : le firmware emet du MIDI, et ce
     * chemin de simavr lit un octet hors bornes. Voir simavr_uart_quiet.h. */
    uart_quiet(avr, '0');

    /* L'ecran, pour que la boucle soit chargee comme en vrai. */
    static ssd1306_t oled;
    ssd1306_init(avr, &oled, 128, 64);
    avr_irq_t *twi_out = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_OUTPUT);
    avr_irq_t *twi_in  = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT);
    avr_connect_irq(twi_out, oled.irq + IRQ_SSD1306_TWI_OUT);
    avr_connect_irq(oled.irq + IRQ_SSD1306_TWI_IN, twi_in);

    /* Le CV : on repond a chaque demarrage de conversion. */
    g_adc_irq = avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_ADC7);
    avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_OUT_TRIGGER),
                            adc_trigger_hook, NULL);

    const uint64_t target = (uint64_t)(seconds * (double)F_CPU_HZ);
    /* On laisse le firmware demarrer (Init de l'ecran, premiere image) avant
     * d'injecter : une impulsion perdue pendant l'initialisation ne dirait rien
     * du regime de fonctionnement. */
    const uint64_t warmup = (uint64_t)(1.0 * (double)F_CPU_HZ);
    /* Fenetre de grace LARGE, et deliberement. Le verrou tient l'evenement : ce
     * qui est verifie est qu'il n'est pas PERDU, pas qu'il est consomme vite. Un
     * consommateur peut etre en retard d'un rendu entier — 56 ms pour une image
     * complete non etalee (env:bringup). 200 ms laisse la place, et la latence
     * reelle est mesuree puis rapportee plutot que confondue avec une perte. */
    const double grace_ms = getenv("GRACE_MS") ? atof(getenv("GRACE_MS")) : 200.0;
    const uint64_t grace = (uint64_t)(grace_ms / 1000.0 * F_CPU_HZ);

    /* La PERIODE doit dominer la fenetre de grace : sinon l'injection suivante
     * declare perdue une impulsion qui n'avait pas fini d'attendre son
     * consommateur. Diagnostique en obtenant 42 % de captures avec une periode
     * de 120 ms pour une latence allant jusqu'a 116 ms. GRACE_MS la regle : elle
     * borne la latence du CONSOMMATEUR, qui depend du firmware, et non la
     * garantie de capture, qui est celle du verrou. */
    if (period_cycles <= grace + pulse_cycles) {
        fprintf(stderr,
                "periode (%.0f us) trop courte : il faut plus que la fenetre de grace "
                "(%.0f us) plus la largeur d'impulsion\n",
                period_us, 1e6 * grace / F_CPU_HZ);
        return 2;
    }
    uint64_t next_pulse = warmup;
    uint32_t detected = 0, missed = 0;
    uint64_t deadline = 0;   /* fin de la fenetre de grace d'une impulsion */
    int awaiting = 0;
    uint64_t await_start = 0;
    double lat_sum = 0.0, lat_max = 0.0, lat_min = 1e18;

    printf("firmware    %s\n", fw);
    printf("impulsion   %.0f us toutes les %.0f us, %.1f s simulees\n",
           width_us, period_us, seconds);
    printf("drapeau     0x%04x (RAM simulee)\n\n", pending_addr);

    /* EDGE_COUNTER=<adresse> : observer le compteur du firmware au lieu de
     * consommer le verrou nous-memes. */
    const uint16_t counter_addr =
        getenv("EDGE_COUNTER") ? (uint16_t)strtol(getenv("EDGE_COUNTER"), NULL, 0) : 0;
    uint16_t counter_seen = 0;
    if (counter_addr) {
        counter_seen = (uint16_t)(avr->data[counter_addr] | (avr->data[counter_addr + 1] << 8));
        printf("compteur    0x%04x (observe, verrou laisse au firmware)\n", counter_addr);
    }

    /* DEBUG=<adresse de `latest`> : suit ce que le firmware echantillonne. */
    const uint16_t dbg = getenv("DEBUG") ? (uint16_t)strtol(getenv("DEBUG"), NULL, 0) : 0;
    uint64_t next_dbg = (uint64_t)(0.5 * F_CPU_HZ);

    while (avr->cycle < target) {
        int state = avr_run(avr);
        if (state == cpu_Done || state == cpu_Crashed) {
            printf("!! CPU arrete (state=%d)\n", state);
            break;
        }

        if (dbg && avr->cycle >= next_dbg) {
            next_dbg += (uint64_t)(0.5 * F_CPU_HZ);
            const uint16_t cv1 = (uint16_t)(avr->data[dbg] | (avr->data[dbg + 1] << 8));
            const uint16_t cv2 = (uint16_t)(avr->data[dbg + 2] | (avr->data[dbg + 3] << 8));
            const uint32_t done = (uint32_t)avr->data[dbg + 9] |
                                 ((uint32_t)avr->data[dbg + 10] << 8) |
                                 ((uint32_t)avr->data[dbg + 11] << 16) |
                                 ((uint32_t)avr->data[dbg + 12] << 24);
            printf("  t=%6.2f s  latest cv1=%4u cv2=%4u  conversions=%u  pending=0x%02x  ADMUX=0x%02x\n",
                   (double)avr->cycle / F_CPU_HZ, cv1, cv2, done,
                   avr->data[pending_addr], avr->data[0x7c]);
        }

        /* Injection d'une nouvelle impulsion. On n'en lance pas dont la fenetre
         * de grace depasserait la fin de la simulation : elle compterait comme
         * ratee sans avoir eu sa chance. */
        if (avr->cycle >= next_pulse &&
            avr->cycle + pulse_cycles + grace < target) {
            if (awaiting) { ++missed; }        /* la precedente n'a jamais ete vue */
            pulse_start = avr->cycle;
            ++injected;
            awaiting = 1;
            await_start = avr->cycle;
            /* Fenetre de grace : le temps d'un pire passage de boucle, large. */
            deadline = avr->cycle + pulse_cycles + grace;
            next_pulse += period_cycles;
        }

        if (counter_addr) {
            /* Mode observation : le firmware consomme, on compte ses increments. */
            const uint16_t now_count =
                (uint16_t)(avr->data[counter_addr] | (avr->data[counter_addr + 1] << 8));
            if (awaiting && now_count != counter_seen) {
                counter_seen = now_count;
                const double lat_ms =
                    (double)(avr->cycle - await_start) * 1000.0 / F_CPU_HZ;
                lat_sum += lat_ms;
                if (lat_ms > lat_max) lat_max = lat_ms;
                if (lat_ms < lat_min) lat_min = lat_ms;
                ++detected;
                awaiting = 0;
            }
        } else if (awaiting && (avr->data[pending_addr] & 0x01)) {
            /* Mode consommation : le harnais joue le consommateur du verrou. */
            avr->data[pending_addr] &= (uint8_t)~0x01;
            ++detected;
            awaiting = 0;
        }
        if (awaiting && avr->cycle > deadline) {
            ++missed;
            awaiting = 0;
        }
    }

    printf("=== CROCHET ADC ===\n");
    printf("  appels %u   dont src=7 : %u   autres : %u   dernier src=%u\n\n",
           hook_calls, hook_src7, hook_other_src, last_src);

    printf("=== CAPTURE ===\n");
    printf("  injectees %u   vues %u   ratees %u   taux %.1f %%\n",
           injected, detected, missed, injected ? 100.0 * detected / injected : 0.0);
    if (counter_addr && detected) {
        /* La latence n'a de sens qu'en mode observation : en mode consommation le
         * harnais est un consommateur instantane. */
        printf("  latence de consommation : min %.1f  moy %.1f  max %.1f ms\n",
               lat_min, lat_sum / detected, lat_max);
    }

    if (done_addr) {
        const uint32_t done = (uint32_t)avr->data[done_addr] |
                              ((uint32_t)avr->data[done_addr + 1] << 8) |
                              ((uint32_t)avr->data[done_addr + 2] << 16) |
                              ((uint32_t)avr->data[done_addr + 3] << 24);
        const double per_conv_us = done ? (double)avr->cycle / done * 1e6 / F_CPU_HZ : 0.0;
        printf("\n=== CADENCE ===\n");
        printf("  %u conversions, %.1f us chacune -> une voie toutes les %.1f us (SIMULEE)\n",
               done, per_conv_us, 2.0 * per_conv_us);
        /* simavr planifie la fin de conversion apres `prescale` cycles ; le
         * materiel prend 13 x prescale (fiche technique ATmega328P). La cadence
         * simulee est donc optimiste d'environ 8x, et c'est l'arithmetique — non
         * la simulation — qui porte la garantie jusqu'au materiel. */
        const double hw_conv_us = 13.0 * 128.0 * 1e6 / F_CPU_HZ;
        printf("  materiel : 13 x 128 cycles = %.0f us par conversion -> une voie\n", hw_conv_us);
        printf("             toutes les %.0f us, hors surcout d'ISR\n", 2.0 * hw_conv_us);
    }
    return (missed == 0 && detected > 0) ? 0 : 1;
}
