#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

extern "C" {
#include <sim_avr.h>
#include <sim_elf.h>
#include <sim_hex.h>
#include <sim_irq.h>
#include <avr_twi.h>
#include <avr_ioport.h>
#include <avr_uart.h>
#include <avr_eeprom.h>
#include <parts/ssd1306_virt.h>
}

#include "simavr_uart_quiet.h"

#include <flexseq/MainScreen.h>
#include <flexseq/PatternScreen.h>
#include <flexseq/PatternBank.h>
#include <flexseq/FactoryPatterns.h>

namespace ms = flexseq::mainscreen;
namespace scr = flexseq::screen;

#define MCU        "atmega328p"
#define F_CPU_HZ   16000000UL

#define ENC_A_PORT 'C'
#define ENC_A_BIT  3
#define ENC_B_PORT 'D'
#define ENC_B_BIT  4
#define ENC_SW_PORT 'C'
#define ENC_SW_BIT  0
#define SHIFT_PORT 'B'
#define SHIFT_BIT  4
#define PLAY_PORT  'D'
#define PLAY_BIT   5

#define EDGE_SPACING_MS      1
#define PRESS_MS            60
#define LONG_PRESS_MS      900
#define SETTLE_MS           50
#define BUTTON_MARGIN_MS    15
#define SHIFT_BURST_DETENTS 20
#define FRAME_SETTLE_MS    250

#define TIMER1_COMPA_VECTOR 11
#define OUT_COUNT 6
#define MAX_ONSETS 4096
#define ACTIVE_STEPS 5

#define PATTERN_COUNT 16
#define PATTERN_BYTES 20
#define STEP_BYTES     4
#define RATCHET_BYTES 16

static avr_t *g_avr;
static uint32_t g_twi_bytes;
static uint16_t g_bank_addr;
static avr_irq_t *g_enc_a;
static avr_irq_t *g_enc_b;
static avr_irq_t *g_enc_sw;
static int g_level_sw = 1;
static avr_irq_t *g_shift;
static int g_level_shift = 1;
static avr_irq_t *g_play;
static int g_level_play = 1;
static uint32_t g_ticks;

static const struct { char port; uint8_t bit; const char *name; } OUTS[OUT_COUNT] = {
    {'D', 7, "OUT1"}, {'B', 0, "OUT2"}, {'B', 2, "OUT3"},
    {'D', 6, "OUT4"}, {'B', 1, "OUT5"}, {'B', 3, "OUT6"},
};

typedef struct { uint32_t tick[MAX_ONSETS]; uint32_t n; int last; } outline_t;
static outline_t g_out[OUT_COUNT];

static void tick_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
    (void)irq; (void)param;
    if (value) ++g_ticks;
}

static void out_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
    (void)irq;
    outline_t *l = (outline_t *)param;
    const int level = value ? 1 : 0;
    if (level == l->last) return;
    l->last = level;
    if (!level) return;
    if (l->n < MAX_ONSETS) l->tick[l->n++] = g_ticks;
}

static uint32_t minGapInWindow(const outline_t *l, uint32_t from, uint32_t to)
{
    uint32_t best = 0xFFFFFFFFu;
    for (uint32_t i = 1; i < l->n; ++i) {
        if (l->tick[i] < from || l->tick[i] > to) continue;
        const uint32_t gap = l->tick[i] - l->tick[i - 1];
        if (gap > 0 && gap < best) best = gap;
    }
    return best == 0xFFFFFFFFu ? 0 : best;
}

static uint32_t periodInWindow(const outline_t *l, uint32_t from, uint32_t to)
{
    for (uint32_t i = 0; i + ACTIVE_STEPS < l->n; ++i) {
        if (l->tick[i] < from) continue;
        if (l->tick[i + ACTIVE_STEPS] > to) break;
        return l->tick[i + ACTIVE_STEPS] - l->tick[i];
    }
    return 0;
}

static uint32_t firstOnsetWithGap(const outline_t *l, uint32_t from, uint32_t gapWanted)
{
    for (uint32_t i = 1; i < l->n; ++i) {
        if (l->tick[i] < from) continue;
        if (l->tick[i] - l->tick[i - 1] == gapWanted) return l->tick[i];
    }
    return 0;
}
static int g_level_a = 1;
static int g_level_b = 1;

static ssd1306_t g_oled;

static uint8_t rotX(uint8_t x) { return (uint8_t)(scr::WIDTH - 1 - x); }
static uint8_t rotY(uint8_t y) { return (uint8_t)(scr::HEIGHT - 1 - y); }

static int pixelAt(uint8_t x, uint8_t y)
{
    if (x >= scr::WIDTH || y >= scr::HEIGHT) return 0;
    return (g_oled.vram[y / 8][x] >> (y % 8)) & 1;
}

static int inkInTabSlot(uint8_t tab)
{
    int ink = 0;
    for (uint8_t dx = 0; dx < ms::TAB_SLOT_W; ++dx) {
        for (uint8_t dy = 0; dy < ms::TAB_BOX_H; ++dy) {
            const uint8_t x = (uint8_t)(ms::tabSlotX(tab) + dx);
            const uint8_t y = (uint8_t)(ms::TAB_BOX_Y + dy);
            ink += pixelAt(rotX(x), rotY(y));
        }
    }
    return ink;
}

static int selectedTab(void)
{
    int best = -1, bestInk = -1, ties = 0;
    for (uint8_t tab = 0; tab < ms::TAB_COUNT; ++tab) {
        const int ink = inkInTabSlot(tab);
        if (ink > bestInk) { bestInk = ink; best = tab; ties = 0; }
        else if (ink == bestInk) { ++ties; }
    }
    return ties > 0 ? -1 : best;
}

static void twi_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
    (void)irq; (void)value; (void)param;
    ++g_twi_bytes;
}

static uint64_t ms_to_cycles(double ms)
{
    return (uint64_t)(ms * 1e-3 * (double)F_CPU_HZ);
}

static void run_for(avr_t *avr, double ms)
{
    const uint64_t until = avr->cycle + ms_to_cycles(ms);
    while (avr->cycle < until) {
        const int state = avr_run(avr);
        if (state == cpu_Done || state == cpu_Crashed) {
            printf("!! CPU arrete (state=%d)\n", state);
            return;
        }
    }
}

static void read_bank(const avr_t *avr, uint8_t *out)
{
    memcpy(out, &avr->data[g_bank_addr], (size_t)PATTERN_COUNT * PATTERN_BYTES);
}

static uint16_t low_mask_of(const uint8_t *pattern)
{
    return (uint16_t)(pattern[0] | ((uint16_t)pattern[1] << 8));
}

static void setPin(avr_irq_t *pin, int *level, int value)
{
    if (*level == value) return;
    *level = value;
    avr_raise_irq(pin, value);
}

static void detentAfirst(avr_t *avr)
{
    setPin(g_enc_a, &g_level_a, 0); run_for(avr, EDGE_SPACING_MS);
    setPin(g_enc_b, &g_level_b, 0); run_for(avr, EDGE_SPACING_MS);
    setPin(g_enc_a, &g_level_a, 1); run_for(avr, EDGE_SPACING_MS);
    setPin(g_enc_b, &g_level_b, 1); run_for(avr, EDGE_SPACING_MS);
}

static void detentBfirst(avr_t *avr)
{
    setPin(g_enc_b, &g_level_b, 0); run_for(avr, EDGE_SPACING_MS);
    setPin(g_enc_a, &g_level_a, 0); run_for(avr, EDGE_SPACING_MS);
    setPin(g_enc_b, &g_level_b, 1); run_for(avr, EDGE_SPACING_MS);
    setPin(g_enc_a, &g_level_a, 1); run_for(avr, EDGE_SPACING_MS);
}

static double shiftRotate(avr_t *avr, int detents, int aFirst)
{
    const uint64_t start = avr->cycle;
    setPin(g_shift, &g_level_shift, 0);
    run_for(avr, BUTTON_MARGIN_MS);
    for (int i = 0; i < detents; ++i) {
        if (aFirst) detentAfirst(avr); else detentBfirst(avr);
        run_for(avr, SETTLE_MS);
    }
    run_for(avr, BUTTON_MARGIN_MS);
    const double heldMs = (double)(avr->cycle - start) * 1e3 / (double)F_CPU_HZ;
    setPin(g_shift, &g_level_shift, 1);
    run_for(avr, BUTTON_MARGIN_MS);
    run_for(avr, FRAME_SETTLE_MS);
    return heldMs;
}

static void playPress(avr_t *avr)
{
    run_for(avr, BUTTON_MARGIN_MS);
    setPin(g_play, &g_level_play, 0);
    run_for(avr, (double)PRESS_MS);
    setPin(g_play, &g_level_play, 1);
    run_for(avr, BUTTON_MARGIN_MS);
}

static uint32_t screenSignature(void)
{
    uint32_t sum = 2166136261u;
    for (int page = 0; page < 8; ++page) {
        for (int col = 0; col < scr::WIDTH; ++col) {
            sum ^= g_oled.vram[page][col];
            sum *= 16777619u;
        }
    }
    return sum;
}

static void pressFor(avr_t *avr, double ms)
{
    run_for(avr, BUTTON_MARGIN_MS);
    setPin(g_enc_sw, &g_level_sw, 0);
    run_for(avr, ms);
    setPin(g_enc_sw, &g_level_sw, 1);
    run_for(avr, BUTTON_MARGIN_MS);
    run_for(avr, FRAME_SETTLE_MS);
}

static void rotate(avr_t *avr, int detents, int aFirst)
{
    for (int i = 0; i < detents; ++i) {
        if (aFirst) detentAfirst(avr); else detentBfirst(avr);
        run_for(avr, SETTLE_MS);
    }
    run_for(avr, FRAME_SETTLE_MS);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <firmware.hex> <adresse_patternBank> [duree_boot_ms]\n",
                argv[0]);
        return 2;
    }
    const char *fw = argv[1];
    const long bank_symbol = strtol(argv[2], NULL, 0);
    g_bank_addr = (uint16_t)(bank_symbol & 0xFFFF);
    const double boot_ms = (argc > 3) ? atof(argv[3]) : 1200.0;
    const char *ee_path = (argc > 4 && argv[4][0]) ? argv[4] : NULL;
    const uint16_t ee_base = (argc > 5) ? (uint16_t)strtol(argv[5], NULL, 0) : 384;
    const char *phase = (argc > 6) ? argv[6] : "structure";
    const int temporal = strcmp(phase, "temporal") == 0;

    setvbuf(stdout, NULL, _IONBF, 0);

    elf_firmware_t f = {{0}};
    strcpy(f.mmcu, MCU);
    f.frequency = F_CPU_HZ;
    f.vcc = f.avcc = f.aref = 5000;
    sim_setup_firmware(fw, 0, &f, "gesture_probe");

    avr_t *avr = avr_make_mcu_by_name(MCU);
    if (!avr) { fprintf(stderr, "MCU inconnu\n"); return 1; }
    g_avr = avr;
    avr_init(avr);
    avr_load_firmware(avr, &f);
    uart_quiet(avr, '0');

    if (g_bank_addr == 0 || g_bank_addr >= avr->ramend) {
        fprintf(stderr, "adresse de patternBank hors RAM : 0x%04x\n", g_bank_addr);
        return 2;
    }

    if (ee_path) {
        FILE *ef = fopen(ee_path, "rb");
        if (!ef) { fprintf(stderr, "image EEPROM illisible : %s\n", ee_path); return 2; }
        static uint8_t image[1024];
        const size_t en = fread(image, 1, sizeof(image), ef);
        fclose(ef);
        avr_eeprom_desc_t ee = { .ee = image, .offset = ee_base, .size = (uint32_t)en };
        if (en == 0 || avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &ee) != 0) {
            fprintf(stderr, "image EEPROM refusee\n"); return 2;
        }
        printf("eeprom_prechargee  %zu octets a %u\n", en, ee_base);
    }

    ssd1306_init(avr, &g_oled, 128, 64);
    avr_irq_t *twi_out = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_OUTPUT);
    avr_irq_t *twi_in  = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT);
    avr_connect_irq(twi_out, g_oled.irq + IRQ_SSD1306_TWI_OUT);
    avr_connect_irq(g_oled.irq + IRQ_SSD1306_TWI_IN, twi_in);
    avr_irq_register_notify(twi_out, twi_hook, NULL);

    avr_irq_t *enc_a = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(ENC_A_PORT), ENC_A_BIT);
    avr_irq_t *enc_b = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(ENC_B_PORT), ENC_B_BIT);
    g_enc_a = enc_a;
    g_enc_b = enc_b;
    avr_irq_t *enc_sw = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(ENC_SW_PORT), ENC_SW_BIT);
    g_enc_sw = enc_sw;
    avr_irq_t *shift = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(SHIFT_PORT), SHIFT_BIT);
    g_shift = shift;
    avr_irq_t *play = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(PLAY_PORT), PLAY_BIT);
    g_play = play;
    avr_irq_t *tickVec = avr_get_interrupt_irq(avr, TIMER1_COMPA_VECTOR);
    if (tickVec) avr_irq_register_notify(tickVec + AVR_INT_IRQ_RUNNING, tick_hook, NULL);
    for (int i = 0; i < OUT_COUNT; ++i) {
        g_out[i].last = -1;
        avr_irq_t *pin = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(OUTS[i].port), OUTS[i].bit);
        if (!pin) { fprintf(stderr, "sortie introuvable : %s\n", OUTS[i].name); return 1; }
        avr_irq_register_notify(pin, out_hook, &g_out[i]);
    }
    if (!enc_a || !enc_b || !enc_sw || !shift || !play) {
        fprintf(stderr, "broche introuvable\n");
        return 1;
    }

    printf("firmware           %s\n", fw);
    printf("patternBank        symbole 0x%06lx, RAM 0x%04x\n",
           (unsigned long)bank_symbol, g_bank_addr);
    printf("contrat            rotate=quadrature, press=%d ms, longPress=%d ms,"
           " shiftRotate=%d crans max par salve\n",
           PRESS_MS, LONG_PRESS_MS, SHIFT_BURST_DETENTS);

    avr_raise_irq(enc_a, 1);
    avr_raise_irq(enc_b, 1);
    avr_raise_irq(enc_sw, 1);
    avr_raise_irq(shift, 1);
    avr_raise_irq(play, 1);

    run_for(avr, boot_ms);

    static uint8_t bank[PATTERN_COUNT * PATTERN_BYTES];
    read_bank(avr, bank);

    printf("twi_bytes_boot     %u\n", g_twi_bytes);
    printf("bank_low_masks    ");
    for (int p = 0; p < PATTERN_COUNT; ++p) {
        printf(" %04x", low_mask_of(&bank[p * PATTERN_BYTES]));
    }
    printf("\n");
    printf("bank_ratchets_a1  ");
    for (int b = 0; b < RATCHET_BYTES; ++b) {
        printf(" %02x", bank[STEP_BYTES + b]);
    }
    printf("\n");

    run_for(avr, FRAME_SETTLE_MS);
    const int tabStart = selectedTab();
    printf("tab_start          %d\n", tabStart);

    const uint32_t twiBeforePrime = g_twi_bytes;
    rotate(avr, 1, 1);
    const int tabAfterPrime = selectedTab();
    printf("amorce             onglet %d -> %d, twi %u\n",
           tabStart, tabAfterPrime, g_twi_bytes - twiBeforePrime);

    int previous = tabAfterPrime;
    for (int i = 1; i <= 3; ++i) {
        const uint32_t before = g_twi_bytes;
        rotate(avr, 1, 1);
        const int now = selectedTab();
        printf("cran_A_%d           onglet %d -> %d, twi %u\n",
               i, previous, now, g_twi_bytes - before);
        previous = now;
    }
    for (int i = 1; i <= 3; ++i) {
        const uint32_t before = g_twi_bytes;
        rotate(avr, 1, 0);
        const int now = selectedTab();
        printf("cran_B_%d           onglet %d -> %d, twi %u\n",
               i, previous, now, g_twi_bytes - before);
        previous = now;
    }

    static flexseq::PatternBank expectedBank;
    flexseq::loadFactoryPatterns(expectedBank);
    const uint8_t *expectedBytes = reinterpret_cast<const uint8_t *>(&expectedBank);
    const int controlOk =
        sizeof(flexseq::PatternBank) == (size_t)(PATTERN_COUNT * PATTERN_BYTES)
        && (temporal || memcmp(expectedBytes, bank, sizeof(flexseq::PatternBank)) == 0);
    printf("controle_usine     %d\n", controlOk);
    if (!controlOk) {
        printf("controle_detail    taille native %zu, attendue %d\n",
               sizeof(flexseq::PatternBank), PATTERN_COUNT * PATTERN_BYTES);
        return 3;
    }

    if (temporal) {
        playPress(avr);
        run_for(avr, 500.0);

        const uint32_t tickAfterPlay = g_ticks;
        run_for(avr, 6500.0);
        const uint32_t tickBeforeLength = g_ticks;
        printf("p25_fenetre_avant  %u %u\n", tickAfterPlay, tickBeforeLength);
        for (int i = 0; i < OUT_COUNT; ++i) {
            printf("p25_avant_%s %u %u %u\n", OUTS[i].name,
                   periodInWindow(&g_out[i], tickAfterPlay, tickBeforeLength),
                   minGapInWindow(&g_out[i], tickAfterPlay, tickBeforeLength),
                   g_out[i].n);
        }

        pressFor(avr, (double)PRESS_MS);
        rotate(avr, 1, 1);
        const uint32_t tickLengthGesture = g_ticks;
        const uint32_t twiBeforeLength = g_twi_bytes;
        shiftRotate(avr, 3, 1);
        printf("p25_geste_length   %u %u\n", tickLengthGesture, g_twi_bytes - twiBeforeLength);

        const uint32_t tickAfterLength = g_ticks;
        run_for(avr, 9000.0);
        const uint32_t tickEndLength = g_ticks;
        printf("p25_fenetre_apres  %u %u\n", tickAfterLength, tickEndLength);
        for (int i = 0; i < OUT_COUNT; ++i) {
            printf("p25_apres_%s %u %u\n", OUTS[i].name,
                   periodInWindow(&g_out[i], tickAfterLength, tickEndLength),
                   minGapInWindow(&g_out[i], tickAfterLength, tickEndLength));
        }

        rotate(avr, 1, 1);
        const uint32_t tickSubdivGesture = g_ticks;
        const uint32_t twiBeforeSubdiv = g_twi_bytes;
        shiftRotate(avr, 1, 1);
        const uint32_t expectedBoundary = ((tickSubdivGesture / 96) + 1) * 96;
        run_for(avr, 9000.0);
        const uint32_t applyTick = firstOnsetWithGap(&g_out[0], tickSubdivGesture, 32);
        printf("p25_subdiv_geste   %u %u\n", tickSubdivGesture, g_twi_bytes - twiBeforeSubdiv);
        printf("p25_subdiv_frontiere %u\n", expectedBoundary);
        printf("p25_subdiv_applique %u\n", applyTick);
        printf("p25_subdiv_avant   %u\n",
               minGapInWindow(&g_out[0], tickAfterLength, tickSubdivGesture));
        printf("p25_subdiv_premier32 %u\n", firstOnsetWithGap(&g_out[0], 0, 32));
        printf("p25_subdiv_apres   %u\n",
               minGapInWindow(&g_out[0], applyTick ? applyTick : tickSubdivGesture, g_ticks));
        return 0;
    }

    static uint8_t bankAfterRotations[PATTERN_COUNT * PATTERN_BYTES];
    read_bank(avr, bankAfterRotations);
    printf("bank_inchangee     %d\n",
           memcmp(bank, bankAfterRotations, sizeof(bank)) == 0 ? 1 : 0);

    run_for(avr, FRAME_SETTLE_MS);
    const uint32_t sigTabBar = screenSignature();
    printf("signature_tabbar   %08x\n", sigTabBar);

    uint32_t twiMark = g_twi_bytes;
    pressFor(avr, 5.0);
    printf("appui_5ms          signature %08x, twi %u\n",
           screenSignature(), g_twi_bytes - twiMark);

    twiMark = g_twi_bytes;
    pressFor(avr, (double)PRESS_MS);
    const uint32_t sigAfterShort = screenSignature();
    printf("appui_60ms         signature %08x, twi %u\n",
           sigAfterShort, g_twi_bytes - twiMark);

    twiMark = g_twi_bytes;
    pressFor(avr, (double)LONG_PRESS_MS);
    printf("appui_900ms        signature %08x, twi %u\n",
           screenSignature(), g_twi_bytes - twiMark);

    pressFor(avr, (double)PRESS_MS);
    rotate(avr, 4, 1);
    pressFor(avr, (double)PRESS_MS);
    printf("entree_edit        signature %08x\n", screenSignature());

    static uint8_t bankBeforeShift[PATTERN_COUNT * PATTERN_BYTES];
    read_bank(avr, bankBeforeShift);
    printf("ratchet_avant      %02x\n", bankBeforeShift[STEP_BYTES]);

    twiMark = g_twi_bytes;
    const double heldMs = shiftRotate(avr, 3, 1);
    static uint8_t bankAfterShift[PATTERN_COUNT * PATTERN_BYTES];
    read_bank(avr, bankAfterShift);
    printf("shift_maintien_ms  %.1f\n", heldMs);
    printf("ratchet_apres      %02x\n", bankAfterShift[STEP_BYTES]);
    printf("shift_twi          %u\n", g_twi_bytes - twiMark);
    printf("masques_intacts    %d\n",
           memcmp(bankBeforeShift, bankAfterShift, STEP_BYTES) == 0
           && low_mask_of(&bankAfterShift[0]) == 0x9111 ? 1 : 0);

    shiftRotate(avr, 2, 1);
    static uint8_t bankTriplet[PATTERN_COUNT * PATTERN_BYTES];
    read_bank(avr, bankTriplet);
    printf("triolet_pose       %02x\n", bankTriplet[STEP_BYTES] & 0x0F);

    shiftRotate(avr, 5, 0);
    static uint8_t bankNoRatchet[PATTERN_COUNT * PATTERN_BYTES];
    read_bank(avr, bankNoRatchet);
    printf("triolet_retire     %02x\n", bankNoRatchet[STEP_BYTES] & 0x0F);
    printf("banque_restauree   %d\n",
           memcmp(expectedBytes, bankNoRatchet, sizeof(flexseq::PatternBank)) == 0 ? 1 : 0);

    rotate(avr, 1, 1);
    pressFor(avr, (double)PRESS_MS);
    static uint8_t bankToggled[PATTERN_COUNT * PATTERN_BYTES];
    read_bank(avr, bankToggled);
    printf("step1_bascule      %04x\n", low_mask_of(&bankToggled[0]));

    pressFor(avr, (double)PRESS_MS);
    static uint8_t bankRestored[PATTERN_COUNT * PATTERN_BYTES];
    read_bank(avr, bankRestored);
    printf("step1_rebascule    %04x\n", low_mask_of(&bankRestored[0]));
    printf("banque_finale      %d\n",
           memcmp(expectedBytes, bankRestored, sizeof(flexseq::PatternBank)) == 0 ? 1 : 0);

    return 0;
}
