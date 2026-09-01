/*
 * stack_probe — mesure la pile du firmware DE PRODUCTION, sans l'instrumenter,
 * et en exercant les interruptions.
 *
 * Pourquoi ce harnais remplace le precedent. La premiere version peignait la RAM
 * depuis le firmware lui-meme et publiait le resultat en LARGEUR D'IMPULSION,
 * faute de pouvoir lire la memoire du simulateur : cela exigeait une sonde dans
 * `main.cpp`, un environnement dedie, un etalonnage, et cela laissait deux
 * angles morts.
 *   - La peinture avait lieu au debut de `setup()`, donc APRES les constructeurs
 *     globaux et le `init()` d'Arduino : leur pile n'etait pas comptee.
 *   - Aucune interruption d'entree n'etait exercee : ni l'USART (MIDI), ni le
 *     PCINT de l'encodeur. Or une ISR s'empile PAR-DESSUS le pic.
 *
 * Un harnais C n'a ni l'une ni l'autre limite : il ecrit le motif dans la RAM
 * simulee AVANT le premier cycle — donc avant tout ce que fait le firmware — et
 * relit la frontiere a la fin. Le binaire mesure est celui qui sera flashe, sans
 * un octet de sonde. Et il peut injecter des octets MIDI et remuer les broches de
 * l'encodeur, ce qui fait entrer les ISR dans la mesure.
 *
 * Le balayage part du HAUT : le bas de la RAM libre est le debut du tas
 * (`__heap_start` == `_end`), et une allocation salirait le motif sans que la
 * pile y soit descendue. PATTERN_RUN octets consecutifs sont exiges, un octet de
 * pile pouvant valoir 0xC5 par hasard.
 *
 * L'esclave SSD1306 est attache : sans lui la boucle ne rend pas vraiment, et le
 * chemin de dessin — le plus profond — ne serait pas parcouru.
 */
#ifndef IMAGE_SIZE
#error "IMAGE_SIZE must come from include/flexseq/Persistence.h"
#endif
#ifndef VERSION_OFFSET
#error "VERSION_OFFSET must come from include/flexseq/Persistence.h"
#endif
#ifndef BASE_ADDRESS
#error "BASE_ADDRESS must come from include/flexseq/Persistence.h"
#endif
#if VERSION_OFFSET >= IMAGE_SIZE
#error "the version byte must sit inside the image that is read back"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <sim_avr.h>
#include <sim_elf.h>
#include <sim_hex.h>
#include <sim_irq.h>
#include <sim_interrupts.h>
#include <avr_twi.h>
#include <avr_uart.h>
#include <avr_ioport.h>
#include <avr_eeprom.h>
#include <parts/ssd1306_virt.h>

#include "simavr_uart_quiet.h"

#define MCU         "atmega328p"
#define F_CPU_HZ    16000000UL
#define PATTERN     0xC5
#define PATTERN_RUN 8

/* Vecteurs de l'ATmega328P dont on veut savoir s'ils ont ete parcourus. */
static const struct { uint8_t v; const char* name; } WATCHED[] = {
    {4,  "PCINT1  encodeur (PC3)"},
    {5,  "PCINT2  encodeur (PD4)"},
    {11, "TIMER1  uClock"},
    {16, "TIMER0  millis"},
    {18, "USART   MIDI (RX)"},
    {21, "ADC     CV"},
};
#define WATCHED_COUNT (sizeof(WATCHED) / sizeof(WATCHED[0]))

static uint32_t isr_count[WATCHED_COUNT];

static void isr_hook(struct avr_irq_t* irq, uint32_t value, void* param)
{
    if (value) ++isr_count[(size_t)param];
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr,
                "usage: %s <firmware.hex> <adresse_de__end> [duree_s] [image.bin]\n",
                argv[0]);
        return 2;
    }
    const char* fw = argv[1];
    const uint16_t end_addr = (uint16_t)strtol(argv[2], NULL, 0);
    const double seconds = (argc > 3) ? atof(argv[3]) : 8.0;
    const char* ee_path = (argc > 4 && argv[4][0] != '\0') ? argv[4] : NULL;
    const int quiet = getenv("QUIET") != NULL;   /* sans injection, pour comparer */

    /* Sortie NON TAMPONNEE : redirigee vers un fichier, stdout l'est par blocs,
     * et le rapport disparaissait entierement si le harnais plantait — on
     * cherchait alors le defaut la ou il n'etait pas. */
    setvbuf(stdout, NULL, _IONBF, 0);

    elf_firmware_t f = {{0}};
    /* AVANT le chargement : voir blocking_probe.c. */
    strcpy(f.mmcu, MCU);
    f.frequency = F_CPU_HZ;
    f.vcc = f.avcc = f.aref = 5000;
    sim_setup_firmware(fw, 0, &f, "stack_probe");

    avr_t* avr = avr_make_mcu_by_name(MCU);
    if (!avr) { fprintf(stderr, "MCU inconnu\n"); return 1; }
    avr_init(avr);
    avr_load_firmware(avr, &f);

    /* Journal de console de l'UART desarme : le firmware emet du MIDI, et ce
     * chemin de simavr lit un octet hors bornes. Voir simavr_uart_quiet.h. */
    uart_quiet(avr, '0');

    /* Image de persistance PRECHARGEE, optionnelle. Sans elle le firmware seme
     * la sienne, qui ne route aucun CV : le chemin de chargement d'un template
     * de modulation n'est alors jamais emprunte, donc jamais mesure. Avec elle,
     * bootstrap() accepte l'image et n'ecrit rien, donc le chemin d'ecriture
     * EEPROM sort de la mesure. Les deux courses sont complementaires, aucune
     * ne remplace l'autre. */
    int preloaded = 0;
    if (ee_path != NULL) {
        uint8_t preload[IMAGE_SIZE];
        FILE* fh = fopen(ee_path, "rb");
        if (!fh) {
            fprintf(stderr, "image EEPROM illisible : %s\n", ee_path);
            return 2;
        }
        size_t n = fread(preload, 1, sizeof(preload), fh);
        fclose(fh);
        if (n != sizeof(preload)) {
            fprintf(stderr, "image EEPROM de %zu octets, %zu attendus\n",
                    n, sizeof(preload));
            return 2;
        }
        avr_eeprom_desc_t pre = { .ee = preload, .offset = BASE_ADDRESS,
                                  .size = (uint32_t)n };
        if (avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &pre) != 0) {
            fprintf(stderr, "prechargement EEPROM refuse par simavr\n");
            return 2;
        }
        preloaded = 1;
        printf("EEPROM prechargee : %zu octets a %u, version %u\n",
               n, (unsigned)BASE_ADDRESS, preload[VERSION_OFFSET]);
    }

    const uint16_t ramend = (uint16_t)avr->ramend;
    if (end_addr >= ramend) {
        fprintf(stderr, "_end (0x%04x) au-dela de RAMEND (0x%04x)\n", end_addr, ramend);
        return 2;
    }

    /* La peinture, AVANT le premier cycle : elle couvre donc les constructeurs
     * globaux et le init() d'Arduino, que la sonde embarquee ne voyait pas.
     * On peint au-dessus de _end, la ou .init4 n'ecrit rien. */
    for (uint16_t a = end_addr; a <= ramend; ++a) avr->data[a] = PATTERN;
    const uint16_t free_ram = (uint16_t)(ramend - end_addr + 1);

    static ssd1306_t oled;
    ssd1306_init(avr, &oled, 128, 64);
    avr_irq_t* twi_out = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_OUTPUT);
    avr_irq_t* twi_in = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT);
    avr_connect_irq(twi_out, oled.irq + IRQ_SSD1306_TWI_OUT);
    avr_connect_irq(oled.irq + IRQ_SSD1306_TWI_IN, twi_in);

    for (size_t i = 0; i < WATCHED_COUNT; ++i) {
        avr_irq_t* v = avr_get_interrupt_irq(avr, WATCHED[i].v);
        if (v) avr_irq_register_notify(v + AVR_INT_IRQ_RUNNING, isr_hook, (void*)i);
    }

    /* Les entrees a remuer : broches de l'encodeur (les seules sous PCINT dans
     * libGravity ; les boutons sont scrutes) et l'USART, ou le MIDI arrive. */
    avr_irq_t* encA = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('C'), 3);  /* PC3 = A3 */
    avr_irq_t* encB = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 4);  /* PD4 */
    avr_irq_t* midi = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_INPUT);

    const uint64_t target = (uint64_t)(seconds * (double)F_CPU_HZ);
    const uint64_t enc_period = (uint64_t)(0.005 * F_CPU_HZ);   /* rotation vive */
    const uint64_t midi_period = (uint64_t)(0.002 * F_CPU_HZ);  /* > 320 us / octet */
    const uint64_t inject_until = (uint64_t)(0.5 * (double)target);
    uint64_t next_enc = (uint64_t)(0.5 * F_CPU_HZ);
    uint64_t next_midi = next_enc;
    uint8_t enc_phase = 0;
    uint32_t enc_edges = 0, midi_bytes = 0;

    printf("firmware   %s\n", fw);
    printf("RAM libre  %u o  (_end 0x%04x .. RAMEND 0x%04x)\n", free_ram, end_addr, ramend);
    printf("injection  %s\n\n",
           quiet ? "AUCUNE (QUIET)" : "encodeur + MIDI, premiere moitie seulement");

    while (avr->cycle < target) {
        const int state = avr_run(avr);
        if (state == cpu_Done || state == cpu_Crashed) {
            printf("!! CPU arrete (state=%d)\n", state);
            break;
        }
        /* L'injection s'ARRETE a mi-course. Les ISR ont alors ete parcourues, et
         * la seconde moitie laisse au firmware le silence dont son ecriture
         * differee a besoin : une rotation continue repousserait indefiniment le
         * delai de calme, et le chemin d'ecriture EEPROM sortirait de la mesure
         * sans que rien ne le dise. */
        if (quiet || avr->cycle >= inject_until) continue;

        if (avr->cycle >= next_enc && encA && encB) {
            next_enc += enc_period;
            /* Quadrature : une broche puis l'autre, ce qui produit des fronts sur
             * les deux vecteurs PCINT. */
            enc_phase = (uint8_t)((enc_phase + 1) & 3);
            avr_raise_irq(encA, (enc_phase == 1 || enc_phase == 2) ? 1 : 0);
            avr_raise_irq(encB, (enc_phase == 2 || enc_phase == 3) ? 1 : 0);
            ++enc_edges;
        }
        if (avr->cycle >= next_midi && midi) {
            next_midi += midi_period;
            avr_raise_irq(midi, 0xF8);   /* MIDI clock */
            ++midi_bytes;
        }
    }

    /* Frontiere du motif, depuis le haut. */
    uint16_t watermark = ramend + 1;
    for (uint16_t a = ramend; a > end_addr + PATTERN_RUN; --a) {
        int clean = 1;
        for (uint8_t k = 0; k < PATTERN_RUN; ++k)
            if (avr->data[a - k] != PATTERN) { clean = 0; break; }
        if (clean) { watermark = a; break; }
    }
    const uint16_t used = (uint16_t)(ramend - watermark);

    printf("=== INTERRUPTIONS PARCOURUES ===\n");
    for (size_t i = 0; i < WATCHED_COUNT; ++i)
        printf("  %-24s %10u\n", WATCHED[i].name, isr_count[i]);
    if (!quiet)
        printf("  (injecte : %u transitions d'encodeur, %u octets MIDI)\n", enc_edges, midi_bytes);

    /* La persistance ecrit dans l'EEPROM SIMULEE apres son delai de calme. On le
     * constate plutot que de le supposer : sans cette lecture, la mesure de pile
     * pourrait ne jamais avoir emprunte ce chemin. */
    uint8_t image[IMAGE_SIZE];
    avr_eeprom_desc_t ee = { .ee = image, .offset = BASE_ADDRESS, .size = sizeof(image) };
    int ee_ok = avr_ioctl(avr, AVR_IOCTL_EEPROM_GET, &ee) == 0;
    printf("\n=== PERSISTANCE ===\n");
    if (preloaded) {
        printf("  NON EVALUABLE : image prechargee, donc l'octet de version ne "
               "prouve aucune ecriture du firmware\n");
    } else if (ee_ok) {
        int written = 0;
        for (size_t i = 0; i < sizeof(image); ++i)
            if (image[i] != 0xFF) ++written;
        printf("  octet de version a %u : %u   (%d octets non vierges sur %zu lus)\n",
               (unsigned)(BASE_ADDRESS + VERSION_OFFSET), image[VERSION_OFFSET],
               written, sizeof(image));
    } else {
        printf("  EEPROM simulee illisible\n");
    }

    /* Temoin du chemin de chargement. Un pic identique ne dit PAS que la chaine
     * a ete empruntee : sans ce releve, une image refusee rendrait le meme
     * chiffre et le vert serait faux. On lit donc l'etat de modulation dans la
     * RAM simulee, a l'adresse que le shell a resolue sur l'ELF. */
    const char* loaded_env = getenv("LOADED_ADDR");
    if (loaded_env != NULL && loaded_env[0] != '\0') {
        const uint16_t addr = (uint16_t)strtol(loaded_env, NULL, 0);
        /* Format SANS indentation de deux espaces et sans nombre isole en fin de
         * ligne : le lecteur du rapport reconnait les vecteurs d'ISR a cette
         * forme-la, et un releve indente y serait compte comme un vecteur muet. */
        printf("\n=== MODULATION ===\nloaded[]=");
        for (int i = 0; i < 6; ++i)
            printf("%s%u", i ? "," : "", avr->data[addr + i]);
        printf("\n");
    }

    printf("\n=== PILE ===\n");
    printf("  pic %u o sur %u libres, marge %u o\n", used, free_ram,
           (uint16_t)(free_ram - used));
    return 0;
}
