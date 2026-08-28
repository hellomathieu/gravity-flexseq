/*
 * Mesure l'ISOLATION PHYSIQUE de la fenetre EEPROM de FlexSeq.
 *
 * Pourquoi. Deux `static_assert` de include/flexseq/Persistence.h bornent le
 * FORMAT : l'image commence au-dessus des reglages du firmware d'origine et
 * finit sous son `memCode` de l'adresse 1023. Ils ne disent rien des ECRITURES
 * reelles. Aucune mesure n'observait qu'aucune ecriture ne sort de la fenetre.
 *
 * Methode : temoin. Les deux zones protegees recoivent un motif dependant de
 * l'adresse, la fenetre est laissee VIERGE pour que le demarrage prenne son
 * chemin le plus ecrivant — semis des seize templates, puis balayage complet.
 * Le firmware tourne, puis les deux zones sont relues et comparees octet par
 * octet.
 *
 * Le motif depend de l'adresse : un motif constant ne distinguerait pas une
 * ecriture partielle d'une reecriture de la meme valeur.
 *
 * LA MESURE SE GARDE ELLE-MEME. Si la fenetre n'a pas ete ecrite, les deux
 * zones sont intactes pour une raison sans rapport avec l'isolation, et le
 * resultat serait un vert vide. Le harnais publie donc le nombre d'octets
 * ecrits dans la fenetre et l'octet de version : l'appelant en fait un critere
 * de validite, jamais un succes.
 *
 * CE QUE LA MESURE N'ETABLIT PAS. Elle porte sur les chemins que le firmware
 * emprunte pendant la course. Ce n'est pas une preuve d'absence de tout chemin
 * futur : cette question appartient a l'audit statique des points d'ecriture.
 *
 * BOUNDARY_MUTATE=<adresse> est le levier de contre-epreuve. A trois quarts de
 * course il lit l'EEPROM SIMULEE, modifie un octet, et la reecrit. La
 * modification traverse donc le modele du simulateur, comme une vraie ecriture,
 * et le critere doit rougir.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <sim_avr.h>
#include <sim_elf.h>
#include <sim_hex.h>
#include <sim_irq.h>
#include <avr_twi.h>
#include <avr_uart.h>
#include <avr_ioport.h>
#include <avr_eeprom.h>
#include <parts/ssd1306_virt.h>

#include "simavr_uart_quiet.h"

#define MCU         "atmega328p"
#define F_CPU_HZ    16000000UL
#define EEPROM_SIZE 1024
#define SEED_LOW    0xA5
#define SEED_HIGH   0x3C

static uint8_t witness(uint16_t address, uint8_t seed)
{
    return (uint8_t)(seed ^ (uint8_t)(address * 31u + 7u));
}

static int compare_zone(const uint8_t* read_back, uint16_t first, uint16_t last,
                        uint8_t seed, const char* name)
{
    int diffs = 0;
    long first_diff = -1;
    for (uint16_t a = first; a <= last; ++a) {
        if (read_back[a] != witness(a, seed)) {
            if (first_diff < 0) first_diff = a;
            ++diffs;
        }
        if (a == 0xFFFF) break;
    }
    printf("  %s ecarts %d premier %ld sur %u octets\n",
           name, diffs, first_diff, (unsigned)(last - first + 1));
    return diffs;
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        fprintf(stderr,
                "usage: %s <firmware.hex> <debut_fenetre> <taille_fenetre> [duree_s]\n",
                argv[0]);
        return 2;
    }
    const char* fw = argv[1];
    const long window_first = strtol(argv[2], NULL, 0);
    const long window_size = strtol(argv[3], NULL, 0);
    const double seconds = (argc > 4) ? atof(argv[4]) : 10.0;
    const long window_last = window_first + window_size - 1;
    const char* mutate_txt = getenv("BOUNDARY_MUTATE");

    setvbuf(stdout, NULL, _IONBF, 0);

    if (window_first < 0 || window_size <= 0 || window_last >= EEPROM_SIZE) {
        fprintf(stderr, "fenetre hors EEPROM : %ld..%ld\n", window_first, window_last);
        return 2;
    }

    elf_firmware_t f = {{0}};
    strcpy(f.mmcu, MCU);
    f.frequency = F_CPU_HZ;
    f.vcc = f.avcc = f.aref = 5000;
    sim_setup_firmware(fw, 0, &f, "eeprom_boundary_probe");

    avr_t* avr = avr_make_mcu_by_name(MCU);
    if (!avr) { fprintf(stderr, "MCU inconnu\n"); return 1; }
    avr_init(avr);
    avr_load_firmware(avr, &f);
    uart_quiet(avr, '0');

    static uint8_t image[EEPROM_SIZE];
    for (long a = 0; a < EEPROM_SIZE; ++a) {
        if (a < window_first) image[a] = witness((uint16_t)a, SEED_LOW);
        else if (a > window_last) image[a] = witness((uint16_t)a, SEED_HIGH);
        else image[a] = 0xFF;
    }
    avr_eeprom_desc_t preload = { .ee = image, .offset = 0, .size = EEPROM_SIZE };
    if (avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &preload) != 0) {
        fprintf(stderr, "EEPROM simulee non ecrite\n");
        return 1;
    }

    static ssd1306_t oled;
    ssd1306_init(avr, &oled, 128, 64);
    avr_irq_t* twi_out = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_OUTPUT);
    avr_irq_t* twi_in = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT);
    avr_connect_irq(twi_out, oled.irq + IRQ_SSD1306_TWI_OUT);
    avr_connect_irq(oled.irq + IRQ_SSD1306_TWI_IN, twi_in);

    avr_irq_t* encA = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('C'), 3);
    avr_irq_t* encB = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 4);

    const uint64_t target = (uint64_t)(seconds * (double)F_CPU_HZ);
    const uint64_t inject_until = (uint64_t)(0.5 * (double)target);
    const uint64_t mutate_at = (uint64_t)(0.75 * (double)target);
    const uint64_t enc_period = (uint64_t)(0.005 * F_CPU_HZ);
    uint64_t next_enc = (uint64_t)(0.5 * F_CPU_HZ);
    uint8_t enc_phase = 0;
    uint32_t enc_edges = 0;
    int mutated = 0;

    printf("firmware   %s\n", fw);
    printf("fenetre    %ld..%ld  (%ld octets)\n", window_first, window_last, window_size);
    printf("temoins    0..%ld et %ld..%d\n",
           window_first - 1, window_last + 1, EEPROM_SIZE - 1);

    while (avr->cycle < target) {
        const int state = avr_run(avr);
        if (state == cpu_Done || state == cpu_Crashed) {
            printf("!! CPU arrete (state=%d)\n", state);
            break;
        }
        if (mutate_txt && !mutated && avr->cycle >= mutate_at) {
            mutated = 1;
            const long address = strtol(mutate_txt, NULL, 0);
            static uint8_t live[EEPROM_SIZE];
            avr_eeprom_desc_t get = { .ee = live, .offset = 0, .size = EEPROM_SIZE };
            avr_eeprom_desc_t set = { .ee = live, .offset = 0, .size = EEPROM_SIZE };
            if (address < 0 || address >= EEPROM_SIZE
                || avr_ioctl(avr, AVR_IOCTL_EEPROM_GET, &get) != 0) {
                printf("!! MUTATION IMPOSSIBLE a %ld\n", address);
            } else {
                live[address] = (uint8_t)(live[address] ^ 0xFF);
                if (avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &set) != 0)
                    printf("!! MUTATION NON APPLIQUEE a %ld\n", address);
                else
                    printf("!! MUTATION appliquee a %ld\n", address);
            }
        }
        if (avr->cycle >= inject_until) continue;
        if (avr->cycle >= next_enc && encA && encB) {
            next_enc += enc_period;
            enc_phase = (uint8_t)((enc_phase + 1) & 3);
            avr_raise_irq(encA, (enc_phase == 1 || enc_phase == 2) ? 1 : 0);
            avr_raise_irq(encB, (enc_phase == 2 || enc_phase == 3) ? 1 : 0);
            ++enc_edges;
        }
    }

    static uint8_t back[EEPROM_SIZE];
    avr_eeprom_desc_t final_read = { .ee = back, .offset = 0, .size = EEPROM_SIZE };
    if (avr_ioctl(avr, AVR_IOCTL_EEPROM_GET, &final_read) != 0) {
        fprintf(stderr, "EEPROM simulee illisible\n");
        return 1;
    }

    int written = 0;
    for (long a = window_first; a <= window_last; ++a)
        if (back[a] != 0xFF) ++written;

    printf("\n=== FENETRE ===\n");
    printf("  fenetre_ecrite %d sur %ld octets, version %u a %ld\n",
           written, window_size, back[window_first], window_first);
    printf("  injection %u transitions d'encodeur\n", enc_edges);

    printf("\n=== ZONES PROTEGEES ===\n");
    int low = compare_zone(back, 0, (uint16_t)(window_first - 1), SEED_LOW, "zone_basse");
    int high = compare_zone(back, (uint16_t)(window_last + 1), EEPROM_SIZE - 1,
                            SEED_HIGH, "zone_haute");
    printf("  memcode_1023 %u attendu %u\n", back[EEPROM_SIZE - 1],
           witness(EEPROM_SIZE - 1, SEED_HIGH));
    return (low || high) ? 0 : 0;
}
