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
 * DEUX REGIMES DANS UNE SEULE EXECUTION, pour retirer un artefact du chiffre
 * publie. simavr planifie la fin d'une conversion apres `prescale` cycles au lieu
 * de 13 x prescale : son ISR d'ADC se declenche ~4x trop souvent, ce qui gonfle
 * la duree d'un passage. A la moitie de la simulation, le harnais efface le bit
 * ADIE d'ADCSRA — et comme c'est l'ISR qui relance les conversions, les couper les
 * arrete toutes. On obtient donc, du MEME binaire et dans les memes conditions :
 *   - regime 1 : la boucle telle que simavr la fait tourner, ADC comprise ;
 *   - regime 2 : la meme boucle sans aucune activite d'ADC.
 * La taxe simulee est leur difference. La taxe REELLE en est le quart, dans le
 * rapport des deux cadences d'ISR — celle de simavr etant MESUREE au compteur de
 * conversions du firmware, non supposee. L'estimation materielle publiee est donc
 * regime 2 + taxe/4.
 *
 * AUCUNE ALLOCATION APRES avr_init(). Le tas est corrompu pendant un run : la
 * premiere version de ce harnais allouait ses tableaux de statistiques, et un run
 * sur deux mourait dans `libsystem_malloc` (EXC_BAD_ACCESS dans `mfm_alloc`) —
 * apres avoir imprime son rapport complet, et avec des compteurs prouves dans
 * leurs bornes. Les trois autres harnais, qui n'allouent rien apres
 * l'initialisation, n'ont jamais fauté. On travaille donc sur des tableaux
 * statiques : les mesures ne changent pas, et on ne retouche pas un allocateur
 * dont l'etat n'est plus garanti.
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
/* Borne des transactions retenues. Un run de 16 s en produit ~2100 : 20000 laisse
 * dix fois la marge. Les tableaux sont STATIQUES et non alloues — voir la note
 * sur le tas ci-dessous. */
#define MAX_TX       20000
#define U8G2_CTRL_DATA 0x40   /* octet de controle U8g2 : donnees d'affichage */
#define BANDS_PER_FRAME 8      /* mode _1_ de U8g2 : au PLUS 8 bandes par image */
#define FRAME_GAP_US    100000 /* au-dela, on a change d'image */
#define ADCSRA_ADDR    0x7A   /* ATmega328P */
#define ADIE_BIT       (1 << 3)

typedef struct { uint64_t start, stop; uint32_t bytes; uint8_t ctrl, page; } tx_t;

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
            txs[tx_count].page = 0xFF;
            tx_open = 1;
        }
    }
    if (tx_open && (v.u.twi.msg & TWI_COND_WRITE)) {
        /* L'adresse voyage dans le message START ; le premier octet ECRIT est
         * donc l'octet de controle U8g2. */
        if (txs[tx_count].bytes == 0) {
            txs[tx_count].ctrl = v.u.twi.data;
        } else if (txs[tx_count].ctrl != U8G2_CTRL_DATA &&
                   (v.u.twi.data & 0xF8) == 0xB0) {
            /* Adressage de page du SSD1306 : 0xB0 | page. U8g2 groupe plusieurs
             * commandes dans une transaction, donc on le cherche parmi TOUS les
             * octets et non seulement le premier. */
            txs[tx_count].page = (uint8_t)(v.u.twi.data & 0x07);
        }
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
    printf("  %-34s n=%-5d min %8.3f  med %8.3f  p90 %8.3f  max %8.3f %s\n",
           label, n, v[0], v[n / 2], v[(n * 9) / 10], v[n - 1], unit);
    (void)sum;
}

int main(int argc, char **argv)
{
    const char *fw = (argc > 1) ? argv[1] : ".pio/build/nanoatmega328/firmware.hex";
    double seconds = (argc > 2) ? atof(argv[2]) : 4.0;
    /* Adresse du compteur `completed` de CvSampler : sert a MESURER la cadence
     * d'ISR simulee, donc le facteur de correction. */
    const uint16_t done_addr = (argc > 3) ? (uint16_t)strtol(argv[3], NULL, 0) : 0;

    /* On charge le .hex : la libsimavr installee est compilee SANS libelf, donc
     * elf_read_firmware() echoue meme depuis un harnais — la limite n'est pas
     * propre au binaire run_avr. Le .hex ne portant ni cible ni frequence, on les
     * renseigne nous-memes, exactement comme les -m/-f de la ligne de commande. */
    /* Sortie NON TAMPONNEE : redirigee vers un fichier, stdout l'est par blocs,
     * et le rapport disparaissait entierement si le harnais plantait — on
     * cherchait alors le defaut la ou il n'etait pas. */
    setvbuf(stdout, NULL, _IONBF, 0);

    elf_firmware_t f = {{0}};
    /* AVANT le chargement : le loader de .hex utilise ces champs, et les
     * renseigner apres coup laissait simavr travailler sur une structure non
     * configuree — SIGTRAP intermittent, deux fois sur trois. */
    strcpy(f.mmcu, MCU);
    f.frequency = F_CPU_HZ;
    sim_setup_firmware(fw, 0, &f, "blocking_probe");

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

    const uint64_t switch_cycle = target / 2;
    int adc_off = 0;
    uint32_t conv_at_switch = 0;

    while (avr->cycle < target) {
        int state = avr_run(avr);
        if (state == cpu_Done || state == cpu_Crashed) {
            printf("!! CPU arrete (state=%d) a %" PRIu64 " cycles\n", state, avr->cycle);
            break;
        }
        if (!adc_off && avr->cycle >= switch_cycle) {
            if (done_addr) {
                conv_at_switch = (uint32_t)avr->data[done_addr] |
                                 ((uint32_t)avr->data[done_addr + 1] << 8) |
                                 ((uint32_t)avr->data[done_addr + 2] << 16) |
                                 ((uint32_t)avr->data[done_addr + 3] << 24);
            }
            avr->data[ADCSRA_ADDR] &= (uint8_t)~ADIE_BIT;  /* plus d'ISR, donc plus
                                                            * de relance */
            adc_off = 1;
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

    if (tx_count >= MAX_TX) {
        printf("!! %d transactions : borne MAX_TX atteinte, mesure tronquee\n", tx_count);
        return 1;
    }
    if (tx_count < 2) {
        printf("Aucun transfert I2C observe (%d) — l'esclave n'a pas repondu.\n", tx_count);
        return 1;
    }

    /* --- bandes : suites maximales de transactions de DONNEES -------------- */
    static uint8_t band_page[MAX_TX];   /* adresse de page qui precede la bande */
    static double band_len[MAX_TX];
    static double band_per[MAX_TX];
    static double band_bytes[MAX_TX];
    static int band_chunks[MAX_TX];
    static uint64_t band_start[MAX_TX];
    int nb = 0;

    uint8_t page_seen = 0xFF;
    for (int i = 0; i < tx_count; ) {
        if (txs[i].ctrl != U8G2_CTRL_DATA) {
            /* Adressage de page du SSD1306 : 0xB0 | page. C'est ce qui delimite
             * les images sans aucun seuil de temps — la sequence redescend a 0 a
             * chaque nouvelle image, meme quand des bandes sont sautees. */
            if (txs[i].page != 0xFF) page_seen = txs[i].page;
            i++;
            continue;
        }
        int j = i;
        uint32_t bytes = 0;
        while (j < tx_count && txs[j].ctrl == U8G2_CTRL_DATA) {
            bytes += txs[j].bytes - 1;   /* moins l'octet de controle */
            j++;
        }
        band_page[nb] = page_seen;
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

    /* Les images ne font PLUS un nombre fixe de bandes : depuis que PagedScreen
     * saute les bandes inchangees (le titre), il en envoie 7 la plupart du temps
     * et 8 lors du rafraichissement complet periodique. On les delimite donc par
     * l'ECART, et le nombre de bandes par image devient une OBSERVATION.
     *
     * Le seuil est legitime ici, contrairement au decoupage en bandes : les deux
     * populations sont separees d'un facteur ~50 — quelques millisecondes entre
     * deux bandes d'une meme image, des centaines entre deux images. Le programme
     * imprime la distribution pour que cela se verifie.
     *
     * Les intervalles ne sont retenus qu'A L'INTERIEUR d'une image : entre deux
     * images la boucle ne rend rien, et l'attente ne mesure aucun blocage. */
    static double frame_len[MAX_TX];
    static int frame_bands[MAX_TX];
    static double by_index[BANDS_PER_FRAME];
    static int n_index[BANDS_PER_FRAME];
    static double per_a[MAX_TX];  /* ADC active */
    static double per_b[MAX_TX];  /* ADC coupee */
    int frames = 0, np = 0, na = 0, nbp = 0, straddling = 0;

    int pages_known = 1;
    for (int k = 0; k < nb; k++) if (band_page[k] == 0xFF) pages_known = 0;

    int i = 0;
    while (i < nb) {
        int j = i + 1;
        if (pages_known) {
            /* Nouvelle image des que l'adresse de page cesse de croitre. */
            while (j < nb && band_page[j] > band_page[j - 1]) ++j;
        } else {
            /* Repli si l'adressage n'a pas ete observe : l'ecart de temps. */
            while (j < nb && us(band_start[j] - band_start[j - 1]) < FRAME_GAP_US) ++j;
        }
        /* [i, j) est une image. */
        const int first_a = band_start[i] < switch_cycle;
        const int last_a = band_start[j - 1] < switch_cycle;
        for (int k = i + 1; k < j; ++k) {
            const double d = us(band_start[k] - band_start[k - 1]);
            band_per[np++] = d;
            if (first_a != last_a) continue;
            if (first_a) {
                per_a[na++] = d;
            } else {
                per_b[nbp++] = d;
                const int pos = k - i;
                if (pos < BANDS_PER_FRAME) {
                    by_index[pos] += d;
                    ++n_index[pos];
                }
            }
        }
        if (first_a != last_a) ++straddling;
        frame_len[frames] = us(band_start[j - 1] - band_start[i]) + band_len[j - 1];
        frame_bands[frames] = j - i;
        ++frames;
        i = j;
    }

    /* Verification du decoupage : une bande DOIT faire 128 octets, et une image
     * ne peut pas depasser 8 bandes. */
    int wrong = 0, oversized = 0;
    for (int k = 0; k < nb; k++) if (band_bytes[k] != 128) wrong++;
    for (int k = 0; k < frames; k++) if (frame_bands[k] > BANDS_PER_FRAME) oversized++;

    printf("=== TRAFIC ===\n");
    printf("  %d transactions START..STOP\n", tx_count);
    printf("  %d bandes de donnees en %d images  %s\n", nb, frames,
           pages_known ? "(delimitees par l'adressage de page)" : "(delimitees par le temps)");
    {
        int hist[BANDS_PER_FRAME + 1] = {0};
        for (int k = 0; k < frames; k++)
            if (frame_bands[k] <= BANDS_PER_FRAME) ++hist[frame_bands[k]];
        printf("  bandes par image :");
        for (int b = 1; b <= BANDS_PER_FRAME; ++b)
            if (hist[b]) printf("  %d->%d fois", b, hist[b]);
        printf("%s\n", oversized ? "   <-- IMAGE TROP LONGUE" : "");
    }
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

    /* --- les deux regimes, et l'estimation materielle ----------------------- */
    printf("=== COUT PAR POSITION DANS L'IMAGE (regime sans ADC) ===\n");
    printf("  intervalle k = transfert de la bande k-1 + dessin de la bande k\n");
    for (int k = 1; k < BANDS_PER_FRAME; ++k)
        if (n_index[k])
            printf("    k=%d  %8.3f us   (n=%d)\n", k, by_index[k] / n_index[k], n_index[k]);
    printf("\n");

    printf("=== DEUX REGIMES ===\n");
    stats("passage, ADC active (simulee)", per_a, na, "us");
    stats("passage, ADC coupee", per_b, nbp, "us");
    if (straddling) printf("  %d image(s) a cheval sur la bascule, ecartee(s)\n", straddling);

    double corrected_max = 0.0, corrected_med = 0.0, corrected_p90 = 0.0;
    if (na > 0 && nbp > 0) {
        qsort(per_a, na, sizeof(double), cmp_d);
        qsort(per_b, nbp, sizeof(double), cmp_d);
        const double med_a = per_a[na / 2], med_b = per_b[nbp / 2];

        /* Modele : l'ISR vole une FRACTION du CPU, elle n'ajoute pas une duree
         * fixe. Soustraire les maxima des deux regimes serait faux — leurs
         * valeurs extremes viennent d'evenements differents.
         *
         *   f_sim = 1 - med_sans / med_avec       (fraction volee, en simulation)
         *   cout par ISR = f_sim x periode_simulee
         *   f_mat = cout / periode_materielle
         *   duree_materielle = duree_sans / (1 - f_mat)
         *
         * La periode simulee est MESUREE au compteur de conversions du firmware ;
         * la materielle est arithmetique : 13 x 128 cycles. */
        const double f_sim = (med_a > 0.0) ? 1.0 - med_b / med_a : 0.0;
        const double sim_conv_us = (done_addr && conv_at_switch)
            ? us(switch_cycle) / conv_at_switch : 0.0;
        const double hw_conv_us = 13.0 * 128.0 * 1e6 / F_CPU_HZ;
        const double cost_us = f_sim * sim_conv_us;
        const double f_hw = (hw_conv_us > 0.0) ? cost_us / hw_conv_us : 0.0;
        const double factor = (f_hw < 0.5) ? 1.0 / (1.0 - f_hw) : 0.0;

        corrected_med = med_b * factor;
        corrected_p90 = per_b[(nbp * 9) / 10] * factor;
        corrected_max = per_b[nbp - 1] * factor;

        printf("\n  CPU vole par l'ISR : %.1f %% en simulation", 100.0 * f_sim);
        if (sim_conv_us > 0.0) {
            printf(", soit %.2f us par ISR\n", cost_us);
            printf("  cadence d'ISR : %.1f us simulee (mesuree) contre %.0f us materielle\n",
                   sim_conv_us, hw_conv_us);
            printf("  => %.1f %% sur materiel, donc un facteur %.3f sur le regime sans ADC\n",
                   100.0 * f_hw, factor);
        } else {
            printf("\n  cadence d'ISR non mesuree (adresse de `completed` absente) :\n"
                   "  la taxe est reportee telle quelle, donc SUREVALUEE\n");
        }
    }

    printf("\n=== LECTURE ===\n");
    printf("  Blocage d'un passage par le bus     : %.2f ms (mediane) / %.2f ms (pire)\n",
           med_len / 1000.0, max_len / 1000.0);
    printf("  Duree d'un passage pendant le rendu : %.2f ms (mediane) / %.2f ms (pire)\n",
           med_per / 1000.0, max_per / 1000.0);
    printf("  Une image : %d x %.2f = %.1f ms de bus AU MINIMUM ; sans etalement\n",
           BANDS_PER_FRAME, med_len / 1000.0, BANDS_PER_FRAME * med_len / 1000.0);
    printf("            la boucle resterait bloquee cela, plus les %d dessins.\n",
           BANDS_PER_FRAME);
    if (corrected_max > 0.0)
        printf("  ESTIME SUR MATERIEL : %.2f ms au pire, %.2f ms en p90, %.2f ms en median\n",
               corrected_max / 1000.0, corrected_p90 / 1000.0, corrected_med / 1000.0);

    return 0;
}
