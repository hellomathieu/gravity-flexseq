/*
 * blocking_probe — mesure le BLOCAGE REEL de la boucle principale de FlexSeq,
 * un vrai esclave SSD1306 sur le bus I2C.
 *
 * Pourquoi un harnais dedie. Le binaire `run_avr` n'attache aucune piece : un
 * transfert y avorte sur NACK des l'octet d'adresse, et toute duree mesuree la
 * serait fausse (bien trop courte). simavr modelise pourtant un esclave SSD1306
 * (`ssd1306_virt`), livre par Homebrew dans libsimavrparts. Ce fichier ne copie
 * aucun code de simavr : il inclut ses en-tetes et lie ses bibliotheques.
 *
 * Ce qu'on mesure, sans toucher au firmware. Pendant le rendu d'une image,
 * chaque passage de la boucle principale effectue exactement UN nextPage()
 * (ADR 0001), donc un envoi de bande. Les transferts I2C bornent donc
 * directement ce qu'on cherche :
 *
 *   - la duree d'une RAFALE (bande)          = le blocage impose par le bus ;
 *   - la periode entre deux rafales           = la duree d'un passage de boucle
 *                                               pendant le rendu ;
 *   - 8 x (rafale + intervalle)               = ce que bloquerait une image
 *                                               entiere sans etalement.
 *
 * La largeur minimale d'impulsion CV captee en decoule : le CV est echantillonne
 * une fois par passage (gravity.Process()), donc une impulsion plus courte que
 * le pire passage peut passer inapercue.
 *
 * Regroupement, sans aucun seuil arbitraire. U8g2 passe par Wire, dont le tampon
 * borne chaque transaction : une bande de 128 octets part en PLUSIEURS
 * transactions START..STOP. Mais le protocole les distingue lui-meme — le
 * premier octet ecrit est l'octet de controle U8g2 : 0x40 = donnees d'affichage,
 * 0x00 = commande. Une BANDE est donc une suite maximale de transactions de
 * donnees, et les commandes (adresse de page et de colonne) les separent. Aucune
 * heuristique de temps n'intervient, et le nombre d'octets par bande verifie le
 * decoupage : il doit valoir 128.
 *
 * Premiere version de ce fichier : les transactions etaient regroupees par un
 * seuil de 100 us. Faux — les morceaux d'une meme bande sont espaces de ~200 us,
 * donc chaque morceau passait pour une bande et le blocage sortait 7x trop
 * petit. Le vidage brut (DUMP=N) a montre la vraie structure.
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
#include <parts/ssd1306_virt.h>

#define MCU          "atmega328p"
#define F_CPU_HZ     16000000UL
#define MAX_TX       200000
#define U8G2_CTRL_DATA 0x40   /* octet de controle U8g2 : donnees d'affichage */
#define BANDS_PER_FRAME 8     /* mode _1_ de U8g2 : 8 bandes par image (ADR 0001) */

typedef struct { uint64_t start, stop; uint32_t bytes; uint8_t ctrl; } tx_t;

static tx_t txs[MAX_TX];
static int tx_count;
static int tx_open;              /* une transaction est-elle en cours ? */
static avr_t *g_avr;

static void twi_watch(struct avr_irq_t *irq, uint32_t value, void *param)
{
    avr_twi_msg_irq_t v;
    v.u.v = value;

    if (v.u.twi.msg & TWI_COND_START) {
        if (tx_count < MAX_TX) {
            txs[tx_count].start = g_avr->cycle;
            txs[tx_count].stop  = g_avr->cycle;
            txs[tx_count].bytes = 0;
            txs[tx_count].ctrl = 0xFF;
            tx_open = 1;
        }
    }
    if (tx_open && (v.u.twi.msg & TWI_COND_WRITE)) {
        /* L'adresse voyage dans le message START ; le premier octet ECRIT est
         * donc l'octet de controle U8g2. */
        if (txs[tx_count].bytes == 0) txs[tx_count].ctrl = v.u.twi.data;
        txs[tx_count].bytes++;
        txs[tx_count].stop = g_avr->cycle;
    }
    if (tx_open && (v.u.twi.msg & TWI_COND_STOP)) {
        txs[tx_count].stop = g_avr->cycle;
        tx_count++;
        tx_open = 0;
    }
}

static double us(uint64_t cycles) { return (double)cycles * 1e6 / (double)F_CPU_HZ; }

static int cmp_d(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void stats(const char *label, double *v, int n, const char *unit)
{
    if (n <= 0) { printf("  %-34s aucun echantillon\n", label); return; }
    qsort(v, n, sizeof(double), cmp_d);
    double sum = 0;
    for (int i = 0; i < n; i++) sum += v[i];
    printf("  %-34s n=%-5d min %8.3f  med %8.3f  max %8.3f  moy %8.3f %s\n",
           label, n, v[0], v[n / 2], v[n - 1], sum / n, unit);
}

int main(int argc, char **argv)
{
    const char *fw = (argc > 1) ? argv[1] : ".pio/build/nanoatmega328/firmware.hex";
    double seconds = (argc > 2) ? atof(argv[2]) : 4.0;

    /* On charge le .hex : la libsimavr installee est compilee SANS libelf, donc
     * elf_read_firmware() echoue meme depuis un harnais — la limite n'est pas
     * propre au binaire run_avr. Le .hex ne portant ni cible ni frequence, on les
     * renseigne nous-memes, exactement comme les -m/-f de la ligne de commande. */
    elf_firmware_t f = {{0}};
    sim_setup_firmware(fw, 0, &f, "blocking_probe");
    strcpy(f.mmcu, MCU);
    f.frequency = F_CPU_HZ;

    avr_t *avr = avr_make_mcu_by_name(MCU);
    if (!avr) { fprintf(stderr, "MCU inconnu : %s\n", MCU); return 1; }
    g_avr = avr;
    avr_init(avr);
    avr_load_firmware(avr, &f);

    /* L'esclave SSD1306, cable sur le TWI dans les DEUX sens : sans le retour,
     * aucun ACK ne revient au maitre et le transfert avorte comme sous run_avr. */
    ssd1306_t oled;
    ssd1306_init(avr, &oled, 128, 64);

    avr_irq_t *twi_out = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_OUTPUT);
    avr_irq_t *twi_in  = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT);
    avr_connect_irq(twi_out, oled.irq + IRQ_SSD1306_TWI_OUT);
    avr_connect_irq(oled.irq + IRQ_SSD1306_TWI_IN, twi_in);
    avr_irq_register_notify(twi_out, twi_watch, NULL);

    uint64_t target = (uint64_t)(seconds * (double)F_CPU_HZ);
    printf("firmware   %s\n", fw);
    printf("simulation %.1f s (%" PRIu64 " cycles a %lu Hz)\n\n", seconds, target, F_CPU_HZ);

    while (avr->cycle < target) {
        int state = avr_run(avr);
        if (state == cpu_Done || state == cpu_Crashed) {
            printf("!! CPU arrete (state=%d) a %" PRIu64 " cycles\n", state, avr->cycle);
            break;
        }
    }

    if (getenv("DUMP")) {
        int n = atoi(getenv("DUMP"));
        printf("=== %d premieres transactions ===\n", n);
        printf("  %4s %12s %10s %7s %6s %s\n", "#", "start(us)", "duree(us)", "octets", "ctrl", "nature");
        for (int i = 0; i < n && i < tx_count; i++)
            printf("  %4d %12.3f %10.3f %7u   0x%02x  %s\n", i,
                   us(txs[i].start), us(txs[i].stop - txs[i].start),
                   txs[i].bytes, txs[i].ctrl,
                   txs[i].ctrl == U8G2_CTRL_DATA ? "donnees" : "commande");
        printf("\n");
    }

    if (tx_count < 2) {
        printf("Aucun transfert I2C observe (%d) — l'esclave n'a pas repondu.\n", tx_count);
        return 1;
    }

    /* --- bandes : suites maximales de transactions de DONNEES -------------- */
    double *band_len = malloc(sizeof(double) * tx_count);
    double *band_per = malloc(sizeof(double) * tx_count);
    double *band_bytes = malloc(sizeof(double) * tx_count);
    int *band_chunks = malloc(sizeof(int) * tx_count);
    uint64_t *band_start = malloc(sizeof(uint64_t) * tx_count);
    int nb = 0;

    for (int i = 0; i < tx_count; ) {
        if (txs[i].ctrl != U8G2_CTRL_DATA) { i++; continue; }
        int j = i;
        uint32_t bytes = 0;
        while (j < tx_count && txs[j].ctrl == U8G2_CTRL_DATA) {
            bytes += txs[j].bytes - 1;   /* moins l'octet de controle */
            j++;
        }
        band_start[nb] = txs[i].start;
        band_len[nb] = us(txs[j - 1].stop - txs[i].start);
        band_bytes[nb] = bytes;
        band_chunks[nb] = j - i;
        nb++;
        i = j;
    }

    if (nb < 2) {
        printf("Moins de deux bandes observees (%d) — rien a mesurer.\n", nb);
        return 1;
    }

    /* Une image fait BANDS_PER_FRAME bandes : on groupe par 8 plutot que par un
     * seuil de temps. Un seuil serait faux — les passages mesures vont de 5 a
     * 16 ms selon la charge, donc aucune valeur ne separe proprement un passage
     * d'une frontiere d'image. Le controle des 128 octets valide le decoupage en
     * bandes, et l'ADR 0001 fixe les 8 bandes par image.
     * Les intervalles ne sont retenus qu'A L'INTERIEUR d'une image : entre deux
     * images, la boucle ne rend rien et l'attente ne mesure aucun blocage. */
    int frames = nb / BANDS_PER_FRAME;
    int np = 0;
    double *frame_len = malloc(sizeof(double) * (frames + 1));
    for (int fr = 0; fr < frames; fr++) {
        int base = fr * BANDS_PER_FRAME;
        for (int k = 1; k < BANDS_PER_FRAME; k++)
            band_per[np++] = us(band_start[base + k] - band_start[base + k - 1]);
        frame_len[fr] = us(band_start[base + BANDS_PER_FRAME - 1] - band_start[base])
                        + band_len[base + BANDS_PER_FRAME - 1];
    }

    /* Verification du decoupage : une bande DOIT faire 128 octets. */
    int wrong = 0;
    for (int i = 0; i < nb; i++) if (band_bytes[i] != 128) wrong++;

    printf("=== TRAFIC ===\n");
    printf("  %d transactions START..STOP\n", tx_count);
    printf("  %d bandes de donnees, soit %d images de %d bandes (reste %d)\n",
           nb, frames, BANDS_PER_FRAME, nb % BANDS_PER_FRAME);
    printf("  decoupage : %d bandes sur %d font exactement 128 octets%s\n",
           nb - wrong, nb, wrong ? "  <-- INCOHERENT" : "  (conforme)");
    printf("  %d transactions Wire par bande, %d octets de donnees chacune\n\n",
           band_chunks[nb / 2], 128 / band_chunks[nb / 2]);

    printf("=== MESURES ===\n");
    stats("bande : transfert (blocage bus)", band_len, nb, "us");
    stats("bande a bande (1 passage)", band_per, np, "us");
    stats("image entiere (8 bandes)", frame_len, frames, "us");

    qsort(band_len, nb, sizeof(double), cmp_d);
    double med_len = band_len[nb / 2], max_len = band_len[nb - 1];
    qsort(band_per, np, sizeof(double), cmp_d);
    double med_per = np ? band_per[np / 2] : 0, max_per = np ? band_per[np - 1] : 0;

    printf("\n=== LECTURE ===\n");
    printf("  Blocage d'un passage par le bus     : %.2f ms (mediane) / %.2f ms (pire)\n",
           med_len / 1000.0, max_len / 1000.0);
    printf("  Duree d'un passage pendant le rendu : %.2f ms (mediane) / %.2f ms (pire)\n",
           med_per / 1000.0, max_per / 1000.0);
    printf("  Une image : %d x %.2f = %.1f ms de bus AU MINIMUM ; sans etalement\n",
           BANDS_PER_FRAME, med_len / 1000.0, BANDS_PER_FRAME * med_len / 1000.0);
    printf("            la boucle resterait bloquee cela, plus les %d dessins.\n",
           BANDS_PER_FRAME);
    printf("  Impulsion CV a capter de facon sure : > %.2f ms\n", max_per / 1000.0);

    free(frame_len); free(band_len); free(band_per); free(band_bytes); free(band_chunks); free(band_start);
    return 0;
}
