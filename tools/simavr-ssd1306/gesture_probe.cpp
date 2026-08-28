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
#include "burst_policy.h"
#include "harness_burst_limits.h"

static const uint8_t DIAGNOSTIC_MEASURES_THE_POLICY = burst::NO_EMPIRICAL_LIMIT;

#include <flexseq/MainScreen.h>
#include <flexseq/PatternScreen.h>
#include <flexseq/PatternBank.h>
#include <flexseq/FactoryPatterns.h>
#include <flexseq/Persistence.h>
#include <flexseq/Transport.h>
#include <flexseq/UiController.h>

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

#define NO_OP_PRESS_MS       5
#define PRESS_MS            60
#define LONG_PRESS_MS      900

#define BUTTON_MARGIN_MS    15
#define DETENT_SETTLE_MS    50
#define FRAME_SETTLE_MS    250

#define EDGE_SPACING_MS      1
#define EDGES_PER_DETENT     4

#define BUTTON_DEBOUNCE_MS    10
#define SHIFT_LONG_PRESS_MS  750
#define LOOP_POLL_RESERVE_MS  20

#define DETENT_MS             (EDGES_PER_DETENT * EDGE_SPACING_MS + DETENT_SETTLE_MS)
#define SHIFT_HOLD_FIXED_MS   (2 * BUTTON_MARGIN_MS)
#define SHIFT_HOLD_MS(n)      (SHIFT_HOLD_FIXED_MS + (n) * DETENT_MS)
#define SHIFT_HOLD_CEILING_MS (SHIFT_LONG_PRESS_MS - LOOP_POLL_RESERVE_MS)
#define SHIFT_BURST_DETENTS   ((SHIFT_HOLD_CEILING_MS - 1 - SHIFT_HOLD_FIXED_MS) / DETENT_MS)

static double g_de_entry_ms   = (double)BUTTON_MARGIN_MS;
static double g_de_release_ms = (double)BUTTON_MARGIN_MS;
static double g_di_settle_ms  = (double)DETENT_SETTLE_MS;
#define SHIFT_BURST_GAP_MS    (BUTTON_DEBOUNCE_MS + LOOP_POLL_RESERVE_MS + BUTTON_MARGIN_MS)

static_assert(SHIFT_BURST_DETENTS >= 1,
              "a burst must carry at least one detent");
static_assert(SHIFT_HOLD_MS(SHIFT_BURST_DETENTS) < SHIFT_HOLD_CEILING_MS,
              "the longest burst must stay below the long-press ceiling");
static_assert(SHIFT_HOLD_MS(SHIFT_BURST_DETENTS + 1) >= SHIFT_HOLD_CEILING_MS,
              "the burst size must be the largest one that fits");
static_assert(SHIFT_HOLD_MS(SHIFT_BURST_DETENTS) + LOOP_POLL_RESERVE_MS < SHIFT_LONG_PRESS_MS,
              "the hold plus one polling pass must stay below the long press");
static_assert(SHIFT_BURST_GAP_MS > BUTTON_DEBOUNCE_MS,
              "the gap between two bursts must clear the debounce window");

#define TIMER1_COMPA_VECTOR 11
#define OUT_COUNT 6
#define MAX_ONSETS 4096
#define ACTIVE_STEPS 5

#define PATTERN_COUNT 16
#define PATTERN_BYTES 23

static constexpr uint8_t OBSERVED_CHANNEL_COUNT =
    flexseq::SequencerEngine::CHANNEL_COUNT;
static constexpr size_t OBSERVED_INSTANCE_BYTES =
    (size_t)OBSERVED_CHANNEL_COUNT * sizeof(flexseq::Pattern);
#define STEP_BYTES     5
#define RATCHET_BYTES 18

static constexpr size_t OBSERVED_TEMPLATE_BYTES =
    (size_t)PATTERN_COUNT * sizeof(flexseq::Pattern);

static constexpr int8_t NO_CHANNEL = -1;
static constexpr size_t OUT_OF_BUFFER = (size_t)-1;

static_assert(sizeof(flexseq::Pattern) == PATTERN_BYTES,
              "the pattern record must be 23 bytes");
static_assert(STEP_BYTES + RATCHET_BYTES == PATTERN_BYTES,
              "the step bytes plus the ratchet bytes must fill the record");
static_assert(OBSERVED_CHANNEL_COUNT == 6,
              "the probe observes six channel instances");
static_assert(OBSERVED_INSTANCE_BYTES == 138,
              "the observed buffer must be 138 bytes");
static_assert(OBSERVED_TEMPLATE_BYTES == 368,
              "the observed template zone must be 368 bytes");
static_assert(OBSERVED_TEMPLATE_BYTES
                  == (size_t)PATTERN_COUNT * PATTERN_BYTES,
              "the template zone must hold one record per template");
static_assert(OBSERVED_TEMPLATE_BYTES != OBSERVED_INSTANCE_BYTES,
              "the two observed domains must not share a size");
static_assert(OBSERVED_INSTANCE_BYTES
                  == (size_t)OBSERVED_CHANNEL_COUNT * PATTERN_BYTES,
              "the observed buffer must hold one record per channel");
static_assert(flexseq::UiController::TAB_FIRST_CHANNEL == 1,
              "the first channel tab must be tab 1");
static_assert((size_t)flexseq::UiController::TAB_FIRST_CHANNEL
                  + OBSERVED_CHANNEL_COUNT
              <= (size_t)flexseq::UiController::TAB_COUNT,
              "the channel tabs must fit in the tab bar");

static constexpr int8_t channelOfTab(int tab)
{
    return (tab < (int)flexseq::UiController::TAB_FIRST_CHANNEL
            || tab - (int)flexseq::UiController::TAB_FIRST_CHANNEL
                   >= (int)OBSERVED_CHANNEL_COUNT)
        ? NO_CHANNEL
        : (int8_t)(tab - (int)flexseq::UiController::TAB_FIRST_CHANNEL);
}

static constexpr size_t instanceOffset(int8_t channel, size_t byteInRecord)
{
    return (channel < 0
            || channel >= (int8_t)OBSERVED_CHANNEL_COUNT
            || byteInRecord >= (size_t)PATTERN_BYTES)
        ? OUT_OF_BUFFER
        : (size_t)channel * PATTERN_BYTES + byteInRecord;
}

static_assert(channelOfTab(0) == NO_CHANNEL,
              "the clock tab carries no channel");
static_assert(channelOfTab(1) == 0, "tab 1 is channel 0");
static_assert(channelOfTab(3) == 2, "tab 3 is channel 2");
static_assert(channelOfTab(4) == 3, "tab 4 is channel 3");
static_assert(channelOfTab(5) == 4, "tab 5 is channel 4");
static_assert(channelOfTab(6) == 5, "tab 6 is channel 5");
static_assert(channelOfTab(7) == NO_CHANNEL,
              "the settings tab carries no channel");
static_assert(channelOfTab(8) == NO_CHANNEL,
              "a tab above the tab bar carries no channel");
static_assert(channelOfTab(-1) == NO_CHANNEL,
              "a negative tab carries no channel");

static_assert(instanceOffset(0, 0) == 0, "channel 0 starts at byte 0");
static_assert(instanceOffset(0, 5) == 5,
              "the first ratchet byte of channel 0 is byte 5");
static_assert(instanceOffset(3, 7) == 76,
              "the third ratchet byte of channel 3 is byte 76");
static_assert(instanceOffset(5, 22) == 137,
              "the last byte of channel 5 is byte 137");
static_assert(instanceOffset(6, 0) == (size_t)-1,
              "the first channel above the count is out of the buffer");
static_assert(instanceOffset(6, 22) == (size_t)-1,
              "no byte of a channel above the count is in the buffer");
static_assert(instanceOffset(7, 0) == (size_t)-1,
              "a channel well above the count is out of the buffer");
static_assert(instanceOffset(NO_CHANNEL, 0) == (size_t)-1,
              "no channel is out of the buffer");
static_assert(instanceOffset(-2, 0) == (size_t)-1,
              "a negative channel is out of the buffer");
static_assert(instanceOffset(3, 23) == (size_t)-1,
              "a byte above the record is out of the buffer");
static_assert(instanceOffset(5, 22) < OBSERVED_INSTANCE_BYTES,
              "the last byte of the last channel is inside the buffer");
static_assert(instanceOffset(6, 0) >= OBSERVED_INSTANCE_BYTES,
              "an out-of-buffer offset must fail the caller bound test");

static constexpr int8_t channelOfOffset(size_t offset)
{
    return (offset >= OBSERVED_INSTANCE_BYTES)
        ? NO_CHANNEL
        : (int8_t)(offset / PATTERN_BYTES);
}

static constexpr size_t byteOfOffset(size_t offset)
{
    return (offset >= OBSERVED_INSTANCE_BYTES)
        ? OUT_OF_BUFFER
        : offset % PATTERN_BYTES;
}

static_assert(channelOfOffset(0) == 0, "byte 0 belongs to channel 0");
static_assert(byteOfOffset(0) == 0, "byte 0 is the first byte of its record");
static_assert(channelOfOffset(7) == 0, "byte 7 belongs to channel 0");
static_assert(byteOfOffset(7) == 7, "byte 7 is byte 7 of its record");
static_assert(channelOfOffset(76) == 3, "byte 76 belongs to channel 3");
static_assert(byteOfOffset(76) == 7, "byte 76 is byte 7 of its record");
static_assert(channelOfOffset(137) == 5, "byte 137 belongs to channel 5");
static_assert(byteOfOffset(137) == 22, "byte 137 is byte 22 of its record");
static_assert(channelOfOffset(138) == NO_CHANNEL,
              "byte 138 is outside the buffer");
static_assert(byteOfOffset(138) == (size_t)-1,
              "byte 138 has no rank inside a record");
static_assert(instanceOffset(channelOfOffset(76), byteOfOffset(76)) == 76,
              "the decomposition must invert the composition");

static const uint8_t *instanceOf(const uint8_t *buffer, int8_t channel)
{
    if (buffer == NULL) return NULL;
    const size_t offset = instanceOffset(channel, 0);
    if (offset >= OBSERVED_INSTANCE_BYTES) return NULL;
    return buffer + offset;
}

static uint8_t *mutableInstanceOf(uint8_t *buffer, int8_t channel)
{
    if (buffer == NULL) return NULL;
    const size_t offset = instanceOffset(channel, 0);
    if (offset >= OBSERVED_INSTANCE_BYTES) return NULL;
    return buffer + offset;
}

static avr_t *g_avr;
static uint32_t g_twi_bytes;
static uint16_t g_bank_addr;
static uint16_t g_template_addr;
static long g_base_force = -1;
static long g_channel_force = -1;
static uint32_t g_bank_reads;
static uint32_t g_bank_read_faults;
static uint32_t g_instance_faults;
static uint16_t g_base_seen;

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

static uint32_t gcd32(uint32_t a, uint32_t b)
{
    while (b != 0) { const uint32_t t = a % b; a = b; b = t; }
    return a;
}

#define GAP_KINDS 16

static uint32_t gapGcdInWindow(const outline_t *l, uint32_t from, uint32_t to,
                               uint32_t *keptOut, uint32_t *countOut,
                               uint32_t *droppedOut)
{
    uint32_t value[GAP_KINDS];
    uint32_t tally[GAP_KINDS];
    uint32_t kinds = 0, count = 0;
    for (uint32_t i = 1; i < l->n; ++i) {
        if (l->tick[i - 1] < from || l->tick[i] > to) continue;
        const uint32_t gap = l->tick[i] - l->tick[i - 1];
        if (gap == 0) continue;
        ++count;
        uint32_t k = 0;
        while (k < kinds && value[k] != gap) ++k;
        if (k == kinds) {
            if (kinds == GAP_KINDS) continue;
            value[kinds] = gap;
            tally[kinds] = 0;
            ++kinds;
        }
        ++tally[k];
    }
    uint32_t g = 0, kept = 0, dropped = 0;
    for (uint32_t k = 0; k < kinds; ++k) {
        if (tally[k] < 2) { ++dropped; continue; }
        g = gcd32(g, value[k]);
        ++kept;
    }
    if (keptOut) *keptOut = kept;
    if (countOut) *countOut = count;
    if (droppedOut) *droppedOut = dropped;
    return g;
}

static uint32_t activeStepsInPattern(const uint8_t *bank)
{
    uint32_t n = 0;
    for (uint32_t b = 0; b < (uint32_t)STEP_BYTES; ++b) {
        for (uint32_t bit = 0; bit < 8; ++bit) {
            if (bank[b] & (1u << bit)) ++n;
        }
    }
    return n;
}

static int8_t observedChannel(int8_t channel);

static uint32_t activeStepsInInstance(const uint8_t *buffer, int8_t channel)
{
    const uint8_t *instance = instanceOf(buffer, observedChannel(channel));
    if (instance == NULL) { ++g_instance_faults; return 0; }
    return activeStepsInPattern(instance);
}

static uint32_t periodOverOnsets(const outline_t *l, uint32_t from, uint32_t to,
                                 uint32_t m, uint32_t *onsetsOut)
{
    uint32_t onsets = 0, period = 0;
    for (uint32_t i = 0; i < l->n; ++i) {
        if (l->tick[i] < from || l->tick[i] > to) continue;
        ++onsets;
        if (period == 0 && i + m < l->n && l->tick[i + m] <= to) {
            period = l->tick[i + m] - l->tick[i];
        }
    }
    if (onsetsOut) *onsetsOut = onsets;
    return period;
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

static int inkInBox(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h)
{
    int ink = 0;
    for (uint8_t dx = 0; dx < w; ++dx) {
        for (uint8_t dy = 0; dy < h; ++dy) {
            ink += pixelAt(rotX((uint8_t)(x0 + dx)), rotY((uint8_t)(y0 + dy)));
        }
    }
    return ink;
}

static void printChampInk(const char *etiquette)
{
    printf("diag_champs        %-10s titre %d LEN %d SUB %d SEP %d EDIT %d\n", etiquette,
           inkInBox(ms::HEADLINE_BOX_X, ms::HEADLINE_BOX_Y, ms::HEADLINE_BOX_W, ms::HEADLINE_BOX_H),
           inkInBox(ms::COL_LEFT_X,  ms::ROW_A_BOX_Y, ms::COL_W, ms::ROW_BOX_H),
           inkInBox(ms::COL_RIGHT_X, ms::ROW_A_BOX_Y, ms::COL_W, ms::ROW_BOX_H),
           inkInBox(ms::COL_LEFT_X,  ms::ROW_B_BOX_Y, ms::COL_W, ms::ROW_BOX_H),
           inkInBox(ms::COL_RIGHT_X, ms::ROW_B_BOX_Y, ms::COL_W, ms::ROW_BOX_H));
}

static int tabBandSlotsWithInk(void)
{
    int slots = 0;
    for (uint8_t tab = 0; tab < ms::TAB_COUNT; ++tab) {
        if (inkInTabSlot(tab) > 0) ++slots;
    }
    return slots;
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

static uint16_t instance_base(const avr_t *avr)
{
    if (g_base_force >= 0) return (uint16_t)(g_base_force & 0xFFFF);
    const uint16_t lo = avr->data[g_bank_addr];
    const uint16_t hi = avr->data[g_bank_addr + 1];
    return (uint16_t)(lo | (hi << 8));
}

static int baseIsReadable(const avr_t *avr, uint16_t base)
{
    if (base == 0) return 0;
    if ((uint32_t)base + OBSERVED_INSTANCE_BYTES > (uint32_t)avr->ramend) return 0;
    return 1;
}

static void read_bank(const avr_t *avr, uint8_t *out)
{
    const uint16_t base = instance_base(avr);
    g_base_seen = base;
    ++g_bank_reads;
    if (!baseIsReadable(avr, base)) {
        ++g_bank_read_faults;
        memset(out, 0, (size_t)OBSERVED_INSTANCE_BYTES);
        return;
    }
    memcpy(out, &avr->data[base], (size_t)OBSERVED_INSTANCE_BYTES);
}

static void reportInstanceAccess(void)
{
    printf("instances_acces    lectures %u echecs %u fautes %u\n",
           g_bank_reads, g_bank_read_faults, g_instance_faults);
}

static int templateZoneIsReadable(const avr_t *avr)
{
    if (g_template_addr == 0) return 0;
    if ((uint32_t)g_template_addr + OBSERVED_TEMPLATE_BYTES
        > (uint32_t)avr->ramend) {
        return 0;
    }
    return 1;
}

static long g_template_mutate = -1;




static int8_t observedChannel(int8_t channel)
{
    return (g_channel_force >= 0) ? (int8_t)g_channel_force : channel;
}

#define SUPPRESSED_BYTES 2

static uint32_t g_suppressed_addr;
static int g_suppressed_valid;
static long g_suppressed_bias;
static uint32_t g_enc_sw_lows;

static int suppressedAddrFits(const avr_t *avr)
{
    if (g_suppressed_addr == 0) return 0;
    if (g_suppressed_addr <= (uint32_t)avr->ioend) return 0;
    if (g_suppressed_addr + (SUPPRESSED_BYTES - 1) > (uint32_t)avr->ramend) return 0;
    return 1;
}

static int readSuppressedCounter(const avr_t *avr, uint32_t *out)
{
    if (!g_suppressed_valid) return 0;
    *out = (uint32_t)avr->data[g_suppressed_addr]
         | ((uint32_t)avr->data[g_suppressed_addr + 1] << 8);
    return 1;
}

static uint8_t g_image[1024];
static uint8_t g_image_ref[1024];
/* L'image que le JUMEAU doit lire. Elle vaut g_image_ref, sauf pour le parcours
 * bootstrap, ou le scenario simule est une image corrompue : le jumeau doit
 * alors prendre le meme repli que le firmware. g_image_ref garde sa semantique
 * d'image de reference fournie, sur laquelle repose IMAGE_MUTATE. */
static uint8_t g_expected_image[1024];
static size_t g_image_bytes;
static uint16_t g_image_base = 384;

static const uint16_t EE_TEMPLATES_AT = 1;
static const uint16_t EE_TEMPLATE_RECORD = 24;
static const uint16_t EE_TEMPLATE_COUNT = 16;
static const uint16_t EE_TEMPLATES_BYTES = 384;
static const uint16_t EE_CONTENT_BYTES = 23;
static const uint8_t EE_TEMPLATE_LENGTH = 16;

static const uint16_t EE_FACTORY_MASKS[EE_TEMPLATE_COUNT] = {
    0x9111, 0x0810, 0x1249, 0xCCCC, 0xEEEE, 0x5454, 0x7FBF, 0xB733,
    0, 0, 0, 0, 0, 0, 0, 0,
};

static const uint16_t EE_VERSION_AT = 0;
static const uint8_t BOOTSTRAP_BROKEN_VERSION = 0xFC;
static const uint16_t BOOTSTRAP_BROKEN_TEMPLATE_AT = 0;
static const uint8_t BOOTSTRAP_BROKEN_TEMPLATE_VALUE = 0x5A;
static const uint8_t EE_EXPECTED_VERSION = 3;
static const uint16_t EE_A1_LOW_MASK = 0x9111;
static const double BOOTSTRAP_SLICE_MS = 250.0;
static const double BOOTSTRAP_CEILING_MS = 10000.0;

static int eepromTemplatesReadable(void)
{
    return g_image_bytes >= (size_t)(EE_TEMPLATES_AT + EE_TEMPLATES_BYTES);
}

static uint8_t eepromExpectedTemplateByte(uint16_t index, uint16_t offset)
{
    if (offset == EE_CONTENT_BYTES) return EE_TEMPLATE_LENGTH;
    if (offset == 0) return (uint8_t)(EE_FACTORY_MASKS[index] & 0xFF);
    if (offset == 1) return (uint8_t)((EE_FACTORY_MASKS[index] >> 8) & 0xFF);
    return 0;
}

static uint32_t eepromTemplateFactoryDiff(uint32_t *firstOut)
{
    uint32_t n = 0, first = 0xFFFFFFFFu;
    for (uint16_t index = 0; index < EE_TEMPLATE_COUNT; ++index) {
        for (uint16_t offset = 0; offset < EE_TEMPLATE_RECORD; ++offset) {
            const uint32_t at = (uint32_t)EE_TEMPLATES_AT
                + (uint32_t)index * EE_TEMPLATE_RECORD + offset;
            if (g_image[at] != eepromExpectedTemplateByte(index, offset)) {
                ++n;
                if (first == 0xFFFFFFFFu) first = at;
            }
        }
    }
    if (firstOut) *firstOut = (first == 0xFFFFFFFFu) ? 0 : first;
    return n;
}

/* L'EEPROM simulee n'est PAS g_image : AVR_IOCTL_EEPROM_SET copie le tampon
 * dans le modele. Toute lecture d'apres execution passe donc par un GET, comme
 * stack_probe.c le fait deja. Lire g_image ici comparerait deux tampons que le
 * firmware ne touche jamais, et le temoin serait vert quoi qu'il arrive. */
static int readSimulatedEeprom(avr_t *avr, uint8_t *out, uint32_t bytes)
{
    avr_eeprom_desc_t ee;
    ee.ee = out;
    ee.offset = g_image_base;
    ee.size = bytes;
    return avr_ioctl(avr, AVR_IOCTL_EEPROM_GET, &ee) == 0;
}

/* TEMPLATE_MUTATE visait la copie RAM de patternBank, que la v3 ne remplit plus.
 * Il vise maintenant la zone EEPROM des templates, cible du temoin B. L'EEPROM
 * simulee ne s'ecrit pas directement : GET, inversion, SET. */
static int mutateEepromTemplates(avr_t *avr)
{
    if (g_template_mutate < 0) return 0;
    if ((uint32_t)g_template_mutate >= (uint32_t)EE_TEMPLATES_BYTES) return 0;
    static uint8_t buffer[sizeof(g_image)];
    if (!readSimulatedEeprom(avr, buffer, (uint32_t)g_image_bytes)) return 0;
    const uint32_t at = (uint32_t)EE_TEMPLATES_AT + (uint32_t)g_template_mutate;
    buffer[at] = (uint8_t)(buffer[at] ^ 0xFF);
    avr_eeprom_desc_t ee;
    ee.ee = buffer;
    ee.offset = g_image_base;
    ee.size = (uint32_t)g_image_bytes;
    return avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &ee) == 0;
}

static uint32_t eepromTemplateDrift(const uint8_t *apres, uint32_t *firstOut)
{
    uint32_t n = 0, first = 0xFFFFFFFFu;
    for (uint32_t at = EE_TEMPLATES_AT;
         at < (uint32_t)(EE_TEMPLATES_AT + EE_TEMPLATES_BYTES); ++at) {
        if (apres[at] != g_image_ref[at]) {
            ++n;
            if (first == 0xFFFFFFFFu) first = at;
        }
    }
    if (firstOut) *firstOut = (first == 0xFFFFFFFFu) ? 0 : first;
    return n;
}

struct ImageStorage {
    uint8_t read(uint16_t address) const
    {
        const uint32_t index = (uint32_t)address - (uint32_t)g_image_base;
        if (g_image_bytes == 0 || index >= (uint32_t)g_image_bytes) return 0xFF;
        return g_expected_image[index];
    }
    void write(uint16_t, uint8_t) {}
};

static flexseq::SequencerEngine g_expected_engine;

/* Le jumeau de simulation. Il predit l'etat RAM que le firmware devrait avoir,
 * en rejouant le contrat v3 sur l'image de REFERENCE, celle fournie a la machine
 * avant toute corruption. Il n'est pas un oracle du format : les oracles
 * independants sont les attentes litterales de run-eeprom-image-check.sh, des
 * temoins EEPROM et du parcours bootstrap. */
static const char *buildExpectedState()
{
    static flexseq::Transport transport(g_expected_engine);
    static flexseq::UiController ui(g_expected_engine, transport);
    static flexseq::Preferences preferences;
    flexseq::PersistentImageV3 image(g_expected_engine, ui, preferences);
    flexseq::PersistenceScheduler scheduler;

    ImageStorage storage;
    if (scheduler.load(storage, image)) {
        return "image";
    }
    /* Repli de bootstrap(), reproduit SANS ecrire. bootstrap() seme d'abord les
     * templates depuis la table PROGMEM, puis les relit ; le jumeau compose donc
     * directement factoryTemplateByte() et applyContentByte(). Lire ImageStorage
     * ici rendrait 0xFF sur un parcours sans image EEPROM, comme "structure".
     * load() a deja remis les defauts, donc selectedPattern vaut 0 : les six
     * canaux prennent A1. Ce chemin n'est pas un oracle du contenu d'usine ; les
     * oracles litteraux sont les temoins EEPROM et le parcours bootstrap. */
    for (uint8_t ch = 0; ch < OBSERVED_CHANNEL_COUNT; ++ch) {
        flexseq::Pattern *instance = g_expected_engine.instanceForChannel(ch);
        const int8_t selected = g_expected_engine.getSelectedPattern(ch);
        if (instance == NULL || selected < 0) continue;
        for (uint8_t offset = 0; offset < flexseq::persist::v3::CONTENT_BYTES; ++offset) {
            flexseq::persist::v3::applyContentByte(
                *instance, offset,
                flexseq::persist::v3::factoryTemplateByte((uint8_t)selected, offset));
        }
    }
    return "usine";
}

static void buildExpectedInstances(uint8_t *out)
{
    for (uint8_t ch = 0; ch < OBSERVED_CHANNEL_COUNT; ++ch) {
        uint8_t *slot = mutableInstanceOf(out, (int8_t)ch);
        if (slot == NULL) continue;
        const flexseq::Pattern *source = g_expected_engine.instanceForChannel(ch);
        if (source == NULL) {
            memset(slot, 0, PATTERN_BYTES);
            continue;
        }
        memcpy(slot, reinterpret_cast<const uint8_t *>(source), PATTERN_BYTES);
    }
}

static uint32_t bankDiffCount(const uint8_t *a, const uint8_t *b, uint32_t *firstOut)
{
    uint32_t n = 0, first = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < (uint32_t)(OBSERVED_INSTANCE_BYTES); ++i) {
        if (a[i] != b[i]) { ++n; if (first == 0xFFFFFFFFu) first = i; }
    }
    if (firstOut) *firstOut = (first == 0xFFFFFFFFu) ? 0 : first;
    return n;
}

static uint16_t low_mask_of(const uint8_t *pattern)
{
    return (uint16_t)(pattern[0] | ((uint16_t)pattern[1] << 8));
}

static uint16_t lowMaskOfInstance(const uint8_t *buffer, int8_t channel)
{
    const uint8_t *instance = instanceOf(buffer, observedChannel(channel));
    if (instance == NULL) { ++g_instance_faults; return 0; }
    return low_mask_of(instance);
}

static uint8_t byteOfInstance(const uint8_t *buffer, int8_t channel, size_t rank)
{
    const size_t offset = instanceOffset(observedChannel(channel), rank);
    if (buffer == NULL || offset >= OBSERVED_INSTANCE_BYTES) {
        ++g_instance_faults;
        return 0;
    }
    return buffer[offset];
}

static uint32_t g_pin_writes = 0;

static void setPin(avr_irq_t *pin, int *level, int value)
{
    ++g_pin_writes;
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

static int g_shift_bursts;
static double g_shift_hold_max;

static double shiftBurst(avr_t *avr, int detents, int aFirst)
{
    const double holdPlanned =
        g_de_entry_ms
        + (double)detents * (EDGES_PER_DETENT * EDGE_SPACING_MS + g_di_settle_ms)
        + g_de_release_ms;
    if (detents < 1 || detents > SHIFT_BURST_DETENTS
        || SHIFT_HOLD_MS(detents) >= SHIFT_HOLD_CEILING_MS
        || holdPlanned >= (double)SHIFT_HOLD_CEILING_MS) {
        printf("shift_garde        salve de %d crans refusee avant injection,"
               " plafond %d crans (%d ms), maintien prevu %.1f ms, seuil %d ms\n",
               detents, SHIFT_BURST_DETENTS,
               SHIFT_HOLD_MS(SHIFT_BURST_DETENTS), holdPlanned,
               SHIFT_LONG_PRESS_MS);
        exit(4);
    }
    const uint64_t start = avr->cycle;
    const uint32_t twiAvant = g_twi_bytes;
    setPin(g_shift, &g_level_shift, 0);
    run_for(avr, g_de_entry_ms);
    for (int i = 0; i < detents; ++i) {
        if (aFirst) detentAfirst(avr); else detentBfirst(avr);
        run_for(avr, g_di_settle_ms);
    }
    run_for(avr, g_de_release_ms);
    const double heldMs = (double)(avr->cycle - start) * 1e3 / (double)F_CPU_HZ;
    setPin(g_shift, &g_level_shift, 1);
    run_for(avr, BUTTON_MARGIN_MS);
    run_for(avr, FRAME_SETTLE_MS);

    ++g_shift_bursts;
    if (heldMs > g_shift_hold_max) g_shift_hold_max = heldMs;
    printf("shift_salve        %d crans, maintien %.1f ms, plafond %d ms, twi %u\n",
           detents, heldMs, SHIFT_HOLD_CEILING_MS, g_twi_bytes - twiAvant);
    return heldMs;
}

static const char *decisionName(burst::Decision decision)
{
    switch (decision) {
        case burst::REFUSE_INVALID_REQUEST: return "REFUSE_INVALID_REQUEST";
        case burst::REFUSE_PHYSICAL_LIMIT:  return "REFUSE_PHYSICAL_LIMIT";
        case burst::REFUSE_EMPIRICAL_LIMIT: return "REFUSE_EMPIRICAL_LIMIT";
        case burst::ACCEPT_SINGLE_BURST:    return "ACCEPT_SINGLE_BURST";
        default:                            return "ACCEPT_SPLIT";
    }
}

static double shiftRotate(avr_t *avr, int detents, int aFirst,
                          uint8_t empiricalLimit, bool allowSplit)
{
    burst::Request request;
    request.detents = (detents < 0 || detents > 255)
                    ? 255 : static_cast<uint8_t>(detents);
    request.physicalLimit = SHIFT_BURST_DETENTS;
    request.empiricalLimit = empiricalLimit;
    request.allowSplit = allowSplit;

    const burst::Verdict verdict = burst::decide(request);
    printf("shift_plan         demande %d physique %u empirique %u effectif %u"
           " liant %u fractionnement %s decision %s\n",
           detents, (unsigned)request.physicalLimit, (unsigned)empiricalLimit,
           (unsigned)verdict.effectiveLimit, (unsigned)verdict.bindingLimit,
           allowSplit ? "autorise" : "refuse", decisionName(verdict.decision));

    if (verdict.decision == burst::REFUSE_INVALID_REQUEST
        || verdict.decision == burst::REFUSE_PHYSICAL_LIMIT
        || verdict.decision == burst::REFUSE_EMPIRICAL_LIMIT) {
        printf("shift_refus        aucune injection, aucune broche pilotee\n");
        exit(4);
    }

    uint8_t plan[burst::MAX_BURSTS];
    uint8_t count = 0;
    if (verdict.decision == burst::ACCEPT_SINGLE_BURST) {
        plan[0] = request.detents;
        count = 1;
    } else {
        count = burst::split(request.detents, verdict.effectiveLimit,
                             plan, burst::MAX_BURSTS);
        if (count == 0) {
            printf("shift_refus        decoupage impossible, aucune injection\n");
            exit(4);
        }
    }

    printf("shift_salves_plan  %u salves :", (unsigned)count);
    for (uint8_t i = 0; i < count; ++i) printf(" %u", (unsigned)plan[i]);
    printf("\n");

    double worst = 0.0;
    for (uint8_t i = 0; i < count; ++i) {
        const double heldMs = shiftBurst(avr, plan[i], aFirst);
        if (heldMs > worst) worst = heldMs;
        if (i + 1 < count) run_for(avr, SHIFT_BURST_GAP_MS);
    }
    return worst;
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
    ++g_enc_sw_lows;
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
        run_for(avr, DETENT_SETTLE_MS);
    }
    run_for(avr, FRAME_SETTLE_MS);
}

static void alignTab(avr_t *avr, int tab)
{
    constexpr int tabs = (int)flexseq::UiController::TAB_COUNT;
    const int steps = ((tab - selectedTab()) % tabs + tabs) % tabs;
    if (steps > 0) rotate(avr, steps, 1);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--observed-instance-bytes") == 0) {
        printf("%zu\n", OBSERVED_INSTANCE_BYTES);
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "usage: %s <firmware.hex> <adresse_patternBank> [duree_boot_ms]\n",
                argv[0]);
        return 2;
    }
    const char *fw = argv[1];
    const long bank_symbol = strtol(argv[2], NULL, 0);
    g_bank_addr = (uint16_t)(bank_symbol & 0xFFFF);
    {
        const char *forced = getenv("INSTANCE_BASE_FORCE");
        if (forced != NULL) g_base_force = strtol(forced, NULL, 0);
        const char *forcedChan = getenv("INSTANCE_CHANNEL_FORCE");
        if (forcedChan != NULL) g_channel_force = strtol(forcedChan, NULL, 0);
        const char *tplMutate = getenv("TEMPLATE_MUTATE");
        if (tplMutate != NULL) g_template_mutate = strtol(tplMutate, NULL, 0);
    }
    atexit(reportInstanceAccess);
    const double boot_ms = (argc > 3) ? atof(argv[3]) : 1200.0;
    const char *ee_path = (argc > 4 && argv[4][0]) ? argv[4] : NULL;
    const uint16_t ee_base = (argc > 5) ? (uint16_t)strtol(argv[5], NULL, 0) : 384;
    const char *phase = (argc > 6) ? argv[6] : "structure";
    const int temporal = strcmp(phase, "temporal") == 0;
    const int rig = strcmp(phase, "rig") == 0;
    const int recettesA = strcmp(phase, "recettesA") == 0;
    const int recettesB = strcmp(phase, "recettesB") == 0;
    const int skipBGeste = getenv("SKIP_B_GESTE") != NULL;
    const int recetteR2 = strcmp(phase, "recetteR2") == 0;
    const int recetteR11 = strcmp(phase, "recetteR11") == 0;
    const int recetteR5R7 = strcmp(phase, "recetteR5R7") == 0;
    const int diagDa = strcmp(phase, "diagDa") == 0;
    const int diagDb = strcmp(phase, "diagDb") == 0;
    const int diagDd = strcmp(phase, "diagDd") == 0;
    const int diagDh = strcmp(phase, "diagDh") == 0;
    const int policycheck = strcmp(phase, "policycheck") == 0;
    const int parcoursInstances = strcmp(phase, "instances") == 0;
    const int parcoursBootstrap = strcmp(phase, "bootstrap") == 0;
    if (argc > 8) {
        g_template_addr = (uint16_t)(strtol(argv[8], NULL, 0) & 0xFFFF);
    }

    {
        const char *e = getenv("DE_ENTRY_MS");
        const char *r = getenv("DE_RELEASE_MS");
        const char *t = getenv("DI_SETTLE_MS");
        if (t != NULL) g_di_settle_ms = atof(t);
        if (g_di_settle_ms < 1.0 || g_di_settle_ms > 500.0) {
            fprintf(stderr, "DI_SETTLE_MS hors bornes\n");
            return 2;
        }
        if (t != NULL) {
            printf("di_rythme          settle %.0f ms, reference %d ms,"
                   " cran %.0f ms\n",
                   g_di_settle_ms, DETENT_SETTLE_MS,
                   EDGES_PER_DETENT * EDGE_SPACING_MS + g_di_settle_ms);
        }
        if (e != NULL) g_de_entry_ms = atof(e);
        if (r != NULL) g_de_release_ms = atof(r);
        if (g_de_entry_ms < 1.0 || g_de_entry_ms > 2000.0
            || g_de_release_ms < 1.0 || g_de_release_ms > 2000.0) {
            fprintf(stderr, "DE_ENTRY_MS / DE_RELEASE_MS hors bornes\n");
            return 2;
        }
        if (e != NULL || r != NULL) {
            printf("de_fenetres        entree %.0f ms, relachement %.0f ms,"
                   " marge de reference %d ms\n",
                   g_de_entry_ms, g_de_release_ms, BUTTON_MARGIN_MS);
        }
    }
    int r5Crans = 30;
    int r7CransRetour = 16;
    {
        const char *a = getenv("R5_CRANS");
        if (a != NULL) r5Crans = (int)strtol(a, NULL, 0);
        const char *b = getenv("R7_CRANS_RETOUR");
        if (b != NULL) r7CransRetour = (int)strtol(b, NULL, 0);
        if (r5Crans < 0 || r5Crans > 60 || r7CransRetour < 0 || r7CransRetour > 60) {
            fprintf(stderr, "levier R5/R7 hors bornes : %d %d\n", r5Crans, r7CransRetour);
            return 2;
        }
    }
    int r11CransSubdiv = 8;
    {
        const char *text = getenv("R11_CRANS_SUBDIV");
        if (text != NULL) r11CransSubdiv = (int)strtol(text, NULL, 0);
        if (r11CransSubdiv < 0 || r11CransSubdiv > 24) {
            fprintf(stderr, "R11_CRANS_SUBDIV hors de la liste : %d\n", r11CransSubdiv);
            return 2;
        }
    }
    int r2Rotations = 2;
    {
        const char *text = getenv("R2_ROTATIONS");
        if (text != NULL) r2Rotations = (int)strtol(text, NULL, 0);
        if (r2Rotations < 0 || r2Rotations > 4) {
            fprintf(stderr, "R2_ROTATIONS hors des champs : %d\n", r2Rotations);
            return 2;
        }
    }
    int r10Step = 4;
    {
        const char *text = getenv("R10_STEP");
        if (text != NULL) r10Step = (int)strtol(text, NULL, 0);
        if (r10Step < 0 || r10Step >= 24) {
            fprintf(stderr, "R10_STEP hors de la grille : %d\n", r10Step);
            return 2;
        }
    }
    g_suppressed_addr = (argc > 7) ? (uint32_t)strtoul(argv[7], NULL, 0) : 0;
    const int symbolcheck = strcmp(phase, "symbolcheck") == 0;
    {
        const char *bias = getenv("SUPPRESSED_BIAS");
        g_suppressed_bias = bias ? strtol(bias, NULL, 0) : 0;
    }
    const int skipShift = getenv("SKIP_SHIFT") != NULL;
    const int skipEdit = getenv("SKIP_EDIT") != NULL;

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

    g_suppressed_valid = suppressedAddrFits(avr);
    printf("suppressed_addr    0x%04x taille %d ioend 0x%04x ramend 0x%04x valide %d\n",
           (unsigned)g_suppressed_addr, SUPPRESSED_BYTES,
           (unsigned)avr->ioend, (unsigned)avr->ramend, g_suppressed_valid);
    if (symbolcheck) {
        return 0;
    }

    if (ee_path) {
        FILE *ef = fopen(ee_path, "rb");
        if (!ef) { fprintf(stderr, "image EEPROM illisible : %s\n", ee_path); return 2; }
        const size_t en = fread(g_image, 1, sizeof(g_image), ef);
        fclose(ef);
        if (en == 0) { fprintf(stderr, "image EEPROM vide\n"); return 2; }
        g_image_bytes = en;
        g_image_base = ee_base;
        memcpy(g_image_ref, g_image, en);
        memcpy(g_expected_image, g_image, en);

        const char *mutateText = getenv("IMAGE_MUTATE");
        if (mutateText != NULL) {
            const long index = strtol(mutateText, NULL, 0);
            if (index < 0 || (size_t)index >= en) {
                fprintf(stderr, "IMAGE_MUTATE hors de l'image : %ld\n", index);
                return 2;
            }
            g_image[index] = (uint8_t)(g_image[index] ^ 0xFF);
            printf("image_mutee        octet %ld inverse dans la copie donnee a la machine,"
                   " l attendu garde l image d origine\n", index);
        }

        if (parcoursBootstrap) {
            if (en <= (size_t)(EE_TEMPLATES_AT + BOOTSTRAP_BROKEN_TEMPLATE_AT)) {
                fprintf(stderr, "image trop courte pour le parcours bootstrap : %zu\n", en);
                return 2;
            }
            g_image[EE_VERSION_AT] = BOOTSTRAP_BROKEN_VERSION;
            g_image[EE_TEMPLATES_AT + BOOTSTRAP_BROKEN_TEMPLATE_AT] =
                BOOTSTRAP_BROKEN_TEMPLATE_VALUE;
            g_expected_image[EE_VERSION_AT] = BOOTSTRAP_BROKEN_VERSION;
            g_expected_image[EE_TEMPLATES_AT + BOOTSTRAP_BROKEN_TEMPLATE_AT] =
                BOOTSTRAP_BROKEN_TEMPLATE_VALUE;
            printf("boot_corruption    version %u a l offset %u, template %u a l offset %u\n",
                   BOOTSTRAP_BROKEN_VERSION, EE_VERSION_AT,
                   BOOTSTRAP_BROKEN_TEMPLATE_VALUE,
                   EE_TEMPLATES_AT + BOOTSTRAP_BROKEN_TEMPLATE_AT);
            printf("attendu_bootstrap  meme corruption appliquee au scenario du jumeau\n");
        }

        avr_eeprom_desc_t ee = { .ee = g_image, .offset = ee_base, .size = (uint32_t)en };
        if (avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &ee) != 0) {
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
           " shiftRotate=%d crans max par salve (%d ms, seuil %d ms)\n",
           PRESS_MS, LONG_PRESS_MS, SHIFT_BURST_DETENTS,
           SHIFT_HOLD_MS(SHIFT_BURST_DETENTS), SHIFT_LONG_PRESS_MS);

    avr_raise_irq(enc_a, 1);
    avr_raise_irq(enc_b, 1);
    avr_raise_irq(enc_sw, 1);
    avr_raise_irq(shift, 1);
    avr_raise_irq(play, 1);

    run_for(avr, boot_ms);

    static uint8_t bank[OBSERVED_INSTANCE_BYTES];
    read_bank(avr, bank);

    printf("instances_pointeur base 0x%04x lisible %d canal_force %ld\n",
           g_base_seen, baseIsReadable(avr, g_base_seen), g_channel_force);
    printf("twi_bytes_boot     %u\n", g_twi_bytes);
    printf("bank_low_masks    ");
    for (uint8_t p = 0; p < OBSERVED_CHANNEL_COUNT; ++p) {
        printf(" %04x", lowMaskOfInstance(bank, (int8_t)p));
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

    const char *expectedSource = buildExpectedState();
    static uint8_t expectedBytes[OBSERVED_INSTANCE_BYTES];
    buildExpectedInstances(expectedBytes);
    const int controlOk =
        sizeof(flexseq::Pattern) == (size_t)PATTERN_BYTES
        && OBSERVED_INSTANCE_BYTES
               == (size_t)(flexseq::SequencerEngine::CHANNEL_COUNT * PATTERN_BYTES)
        && memcmp(expectedBytes, bank, OBSERVED_INSTANCE_BYTES) == 0;
    printf("controle_source    %s\n", expectedSource);
    printf("inst_attendus     ");
    for (uint8_t ch = 0; ch < OBSERVED_CHANNEL_COUNT; ++ch) {
        printf(" %04x", lowMaskOfInstance(expectedBytes, (int8_t)ch));
    }
    printf("\n");
    printf("image_lue          masque %04x subdiv %d length %u mode %d\n",
           low_mask_of(expectedBytes),
           (int)g_expected_engine.getSubdiv(0),
           (unsigned)g_expected_engine.getEffectiveLength(0),
           (int)g_expected_engine.getChannelMode(0));
    printf("controle_usine     %d\n", controlOk);
    if (!controlOk) {
        printf("controle_detail    Pattern %zu octets, attendu %d ; zone d instances"
               " %zu octets, attendue %d\n",
               sizeof(flexseq::Pattern), PATTERN_BYTES,
               OBSERVED_INSTANCE_BYTES,
               (int)(flexseq::SequencerEngine::CHANNEL_COUNT * PATTERN_BYTES));
        return 3;
    }

    if (policycheck) {
        struct Cas { const char *nom; int crans; uint8_t emp; bool split; };
        static const Cas cas[6] = {
            { "V4_length_30_sans_split",  30, harness::LENGTH_BURST_LIMIT, false },
            { "V4_length_7_sans_split",    7, harness::LENGTH_BURST_LIMIT, false },
            { "V5_length_30_avec_split",  30, harness::LENGTH_BURST_LIMIT, true  },
            { "V6_subdiv_8_non_mesure",    8, harness::SUBDIV_BURST_LIMIT, false },
            { "V6_subdiv_7_non_mesure",    7, harness::SUBDIV_BURST_LIMIT, false },
            { "V7_ratchet_50_avec_split", 50, harness::RATCHET_BURST_LIMIT, true },
        };
        const char *only = getenv("POLICY_CASE");
        for (int i = 0; i < 6; ++i) {
            if (only != NULL && strcmp(only, cas[i].nom) != 0) continue;
            burst::Request req;
            req.detents = (uint8_t)cas[i].crans;
            req.physicalLimit = SHIFT_BURST_DETENTS;
            req.empiricalLimit = cas[i].emp;
            req.allowSplit = cas[i].split;
            const burst::Verdict v = burst::decide(req);
            printf("pc_%-24s decision %s effectif %u liant %u",
                   cas[i].nom, decisionName(v.decision),
                   (unsigned)v.effectiveLimit, (unsigned)v.bindingLimit);
            if (v.decision == burst::ACCEPT_SPLIT) {
                uint8_t plan[burst::MAX_BURSTS];
                const uint8_t n = burst::split(req.detents, v.effectiveLimit,
                                               plan, burst::MAX_BURSTS);
                printf(" plan %u :", (unsigned)n);
                for (uint8_t k = 0; k < n; ++k) printf(" %u", (unsigned)plan[k]);
            }
            printf("\n");
        }
        printf("pc_broches         ecritures %u\n", g_pin_writes);
        if (only != NULL && strcmp(only, "injection") == 0) {
        }
        return 0;
    }

    if (diagDh) {
        static uint8_t vu[OBSERVED_INSTANCE_BYTES];
        constexpr int ongletDh = 5;
        constexpr int8_t canalDh = channelOfTab(ongletDh);
        static_assert(canalDh == 4, "tab 5 drives channel 4");
        int crans = 12;
        {
            const char *c = getenv("DH_CRANS");
            if (c != NULL) crans = atoi(c);
            if (crans < 1 || crans > SHIFT_BURST_DETENTS) {
                fprintf(stderr, "DH_CRANS hors bornes 1..%d\n", SHIFT_BURST_DETENTS);
                return 2;
            }
        }
        printf("dh_crans           %d demandes\n", crans);

        playPress(avr);
        run_for(avr, 1000.0);
        rotate(avr, 1, 1);
        {
            alignTab(avr, ongletDh);
        }
        pressFor(avr, (double)PRESS_MS);
        rotate(avr, 1, 1);
        printChampInk("avant");

        uint32_t depart = g_ticks;
        run_for(avr, 20000.0);
        for (int o = 0; o < OUT_COUNT; ++o) {
            uint32_t k = 0, g = 0, d = 0, on = 0;
            printf("dh_avant_OUT%d pgcd %u periode3 %u distances %u retenues %u onsets %u\n",
                   o + 1,
                   gapGcdInWindow(&g_out[o], depart, g_ticks, &k, &g, &d),
                   periodOverOnsets(&g_out[o], depart, g_ticks,
                                    activeStepsInInstance(expectedBytes, (int8_t)o), &on), g, k, on);
        }

        const uint32_t m = g_twi_bytes;
        shiftRotate(avr, crans, 0, DIAGNOSTIC_MEASURES_THE_POLICY, false);
        const uint32_t twiSalve = g_twi_bytes - m;
        printChampInk("apres");

        run_for(avr, 2500.0);
        depart = g_ticks;
        run_for(avr, 20000.0);
        read_bank(avr, vu);
        uint32_t prem = 0;
        printf("dh_apres           onglet %d twi %u ecarts %u\n",
               selectedTab(), twiSalve, bankDiffCount(vu, expectedBytes, &prem));
        for (int o = 0; o < OUT_COUNT; ++o) {
            uint32_t k = 0, g = 0, d = 0, on = 0;
            printf("dh_apres_OUT%d pgcd %u periode3 %u distances %u retenues %u onsets %u\n",
                   o + 1,
                   gapGcdInWindow(&g_out[o], depart, g_ticks, &k, &g, &d),
                   periodOverOnsets(&g_out[o], depart, g_ticks,
                                    activeStepsInInstance(expectedBytes, (int8_t)o), &on), g, k, on);
        }
        return 0;
    }

    if (diagDd) {
        static uint8_t vu[OBSERVED_INSTANCE_BYTES];
        constexpr int ongletDd = 5;
        constexpr int8_t canalDd = channelOfTab(ongletDd);
        static_assert(canalDd == 4, "tab 5 drives channel 4");
        const uint32_t pasActifs = activeStepsInInstance(expectedBytes, canalDd);
        double gapMs = 0.0;
        {
            const char *g = getenv("DB_GAP_MS");
            if (g != NULL) gapMs = atof(g);
            if (gapMs < 0.0 || gapMs > 10000.0) {
                fprintf(stderr, "DB_GAP_MS hors bornes\n"); return 2;
            }
        }
        printf("dd_repos           additionnel %.0f ms, structurel %d ms, total %.0f ms\n",
               gapMs, BUTTON_MARGIN_MS + FRAME_SETTLE_MS,
               gapMs + BUTTON_MARGIN_MS + FRAME_SETTLE_MS);

        playPress(avr);
        run_for(avr, 1000.0);
        rotate(avr, 1, 1);
        {
            alignTab(avr, ongletDd);
        }
        pressFor(avr, (double)PRESS_MS);
        rotate(avr, 1, 1);

        uint32_t depart = g_ticks;
        run_for(avr, 20000.0);
        {
            uint32_t k = 0, g = 0, d = 0, on = 0;
            printf("dd_base            pgcd %u periode3 %u distances %u retenues %u onsets %u\n",
                   gapGcdInWindow(&g_out[canalDd], depart, g_ticks, &k, &g, &d),
                   periodOverOnsets(&g_out[canalDd], depart, g_ticks, pasActifs, &on), g, k, on);
        }
        printChampInk("avant");

        struct { const char *nom; int crans; int aFirst; } sv[4] = {
            { "s1_bas12", 12, 0 }, { "s2_bas12", 12, 0 },
            { "s3_haut12", 12, 1 }, { "s4_haut3", 3, 1 },
        };
        for (int i = 0; i < 4; ++i) {
            const uint32_t m = g_twi_bytes;
            shiftRotate(avr, sv[i].crans, sv[i].aFirst, DIAGNOSTIC_MEASURES_THE_POLICY, false);
            read_bank(avr, vu);
            uint32_t prem = 0;
            printf("dd_%-9s twi %u banque %u onglet %d\n", sv[i].nom,
                   g_twi_bytes - m, bankDiffCount(vu, expectedBytes, &prem),
                   selectedTab());
            printChampInk(sv[i].nom);
            if (i < 3 && gapMs > 0.0) run_for(avr, gapMs);
        }

        run_for(avr, 2500.0);
        depart = g_ticks;
        run_for(avr, 20000.0);
        read_bank(avr, vu);
        uint32_t premier = 0;
        printf("dd_final           onglet %d ecarts %u\n",
               selectedTab(), bankDiffCount(vu, expectedBytes, &premier));
        for (int o = 0; o < OUT_COUNT; ++o) {
            uint32_t k = 0, g = 0, d = 0, on = 0;
            printf("dd_final_OUT%d pgcd %u periode3 %u distances %u retenues %u onsets %u\n",
                   o + 1,
                   gapGcdInWindow(&g_out[o], depart, g_ticks, &k, &g, &d),
                   periodOverOnsets(&g_out[o], depart, g_ticks,
                                    activeStepsInInstance(expectedBytes, (int8_t)o), &on), g, k, on);
        }
        return 0;
    }

    if (diagDb) {
        static uint8_t vu[OBSERVED_INSTANCE_BYTES];
        constexpr int ongletDb = 5;
        constexpr int8_t canalDb = channelOfTab(ongletDb);
        static_assert(canalDb == 4, "tab 5 drives channel 4");
        uint32_t premier = 0;

        playPress(avr);
        run_for(avr, 1000.0);
        rotate(avr, 1, 1);
        {
            alignTab(avr, ongletDb);
        }
        pressFor(avr, (double)PRESS_MS);
        rotate(avr, 1, 1);
        printChampInk("sur-LEN");

        struct { const char *nom; int crans; int aFirst; } salves[5] = {
            { "depart",   0,  0 },
            { "bas12",   12,  0 },
            { "bas24",   12,  0 },
            { "haut12",  12,  1 },
            { "haut15",   3,  1 },
        };

        const int serre = getenv("DB_TIGHT") != NULL;
        for (int i = 0; i < 5; ++i) {
            uint32_t twiGeste = 0;
            if (salves[i].crans > 0) {
                const uint32_t m = g_twi_bytes;
                shiftRotate(avr, salves[i].crans, salves[i].aFirst, DIAGNOSTIC_MEASURES_THE_POLICY, false);
                twiGeste = g_twi_bytes - m;
            }
            printChampInk(salves[i].nom);
            if (serre && i > 0 && i < 4) {
                printf("dbg_%-7s SERRE : aucune fenetre, twi %u\n", salves[i].nom, twiGeste);
                continue;
            }

            run_for(avr, 2500.0);
            const uint32_t depart = g_ticks;
            run_for(avr, 20000.0);

            read_bank(avr, vu);
            const uint32_t ecarts = bankDiffCount(vu, expectedBytes, &premier);
            printf("dbg_%-7s onglet %d twi %u ecarts %u\n",
                   salves[i].nom, selectedTab(), twiGeste, ecarts);
            for (int o = 0; o < OUT_COUNT; ++o) {
                uint32_t kept = 0, gaps = 0, dropped = 0, onsets = 0;
                const uint32_t pgcd = gapGcdInWindow(&g_out[o], depart, g_ticks,
                                                     &kept, &gaps, &dropped);
                const uint32_t per = periodOverOnsets(&g_out[o], depart, g_ticks,
                                                      activeStepsInInstance(
                                                          expectedBytes, (int8_t)o),
                                                      &onsets);
                printf("dbg_%s_OUT%d pgcd %u periode3 %u distances %u retenues %u onsets %u\n",
                       salves[i].nom, o + 1, pgcd, per, gaps, kept, onsets);
            }
        }
        return 0;
    }

    if (diagDa) {
        constexpr int ongletDiag = 5;
        constexpr int8_t canalDiag = channelOfTab(ongletDiag);
        constexpr int sortieDiag = canalDiag;
        static_assert(canalDiag == 4, "tab 5 drives channel 4");
        static_assert(sortieDiag == 4, "channel 4 is output 5");
        const uint32_t pasActifs = activeStepsInInstance(expectedBytes, canalDiag);

        playPress(avr);
        run_for(avr, 1000.0);
        printf("diag_steps_actifs  %u\n", pasActifs);
        rotate(avr, 1, 1);

        struct { const char *nom; int crans; int aFirst; } marches[3] = {
            { "descente4",  4, 0 },
            { "remonte4",   4, 1 },
            { "descente13", 13, 0 },
        };

        for (int etape = 0; etape <= 3; ++etape) {
            uint32_t twiGeste = 0;
            if (etape == 0) {
                alignTab(avr, ongletDiag);
                printChampInk("barre");
                const uint32_t m = g_twi_bytes;
                pressFor(avr, (double)PRESS_MS);
                printChampInk("dans-tab");
                rotate(avr, 1, 1);
                printChampInk("sur-LEN");
                twiGeste = g_twi_bytes - m;
            } else {
                const uint32_t m = g_twi_bytes;
                shiftRotate(avr, marches[etape - 1].crans,
                            marches[etape - 1].aFirst, DIAGNOSTIC_MEASURES_THE_POLICY, false);
                twiGeste = g_twi_bytes - m;
                printChampInk(marches[etape - 1].nom);
            }

            run_for(avr, 2500.0);
            const uint32_t depart = g_ticks;
            run_for(avr, 20000.0);
            uint32_t kept = 0, gaps = 0, dropped = 0, onsets = 0;
            const uint32_t pgcd = gapGcdInWindow(&g_out[sortieDiag], depart, g_ticks,
                                                 &kept, &gaps, &dropped);
            const uint32_t per = periodOverOnsets(&g_out[sortieDiag], depart, g_ticks,
                                                  pasActifs, &onsets);
            printf("diag_%-10s onglet %d twi %u pgcd %u periode3 %u distances %u retenues %u onsets %u\n",
                   etape == 0 ? "base" : marches[etape - 1].nom,
                   selectedTab(), twiGeste, pgcd, per, gaps, kept, onsets);
        }
        return 0;
    }

    if (recetteR5R7) {
        static uint8_t vu[OBSERVED_INSTANCE_BYTES];
        constexpr int ongletR5 = 5;
        constexpr int8_t canalR5 = channelOfTab(ongletR5);
        static_assert(canalR5 == 4, "tab 5 drives channel 4");
        const uint32_t pasActifs = activeStepsInInstance(expectedBytes, canalR5);
        uint32_t premier = 0, marque = 0;

        playPress(avr);
        run_for(avr, 1000.0);
        printf("rE_steps_actifs    %u\n", pasActifs);

        marque = g_twi_bytes;
        rotate(avr, 1, 1);
        printf("rE_amorce          onglet %d twi %u\n", selectedTab(), g_twi_bytes - marque);

        for (int etape = 0; etape < 5; ++etape) {
            const char *nom = (etape == 0) ? "base"
                            : (etape == 1) ? "r5"
                            : (etape == 2) ? "r5rest"
                            : (etape == 3) ? "r7" : "r7rest";
            uint32_t twiGeste = 0;

            if (etape == 1) {
                alignTab(avr, ongletR5);
                marque = g_twi_bytes;
                pressFor(avr, (double)PRESS_MS);
                rotate(avr, 1, 1);
                if (!skipBGeste && r5Crans > 0) shiftRotate(avr, r5Crans, 1, harness::LENGTH_BURST_LIMIT, false);
                twiGeste = g_twi_bytes - marque;
            } else if (etape == 2) {
                marque = g_twi_bytes;
                if (!skipBGeste) { shiftRotate(avr, 24, 0, harness::LENGTH_BURST_LIMIT, false);
                                   shiftRotate(avr, 15, 1, harness::LENGTH_BURST_LIMIT, false); }
                twiGeste = g_twi_bytes - marque;
            } else if (etape == 3) {
                pressFor(avr, (double)LONG_PRESS_MS);
                alignTab(avr, ongletR5);
                marque = g_twi_bytes;
                pressFor(avr, (double)PRESS_MS);
                rotate(avr, 2, 1);
                if (!skipBGeste) {
                    shiftRotate(avr, 50, 1, harness::SUBDIV_BURST_LIMIT, false);
                    if (r7CransRetour > 0) shiftRotate(avr, r7CransRetour, 0, harness::SUBDIV_BURST_LIMIT, false);
                }
                twiGeste = g_twi_bytes - marque;
            } else if (etape == 4) {
                marque = g_twi_bytes;
                if (!skipBGeste) { shiftRotate(avr, 24, 0, harness::SUBDIV_BURST_LIMIT, false);
                                   shiftRotate(avr, 8, 1, harness::SUBDIV_BURST_LIMIT, false); }
                twiGeste = g_twi_bytes - marque;
            }

            run_for(avr, 2500.0);
            const uint32_t depart = g_ticks;
            run_for(avr, etape == 1 ? 25000.0 : 20000.0);

            read_bank(avr, vu);
            const uint32_t ecarts = bankDiffCount(vu, expectedBytes, &premier);
            printf("rE_%s onglet %d twi %u ecarts %u\n", nom, selectedTab(), twiGeste, ecarts);
            for (int o = 0; o < OUT_COUNT; ++o) {
                uint32_t kept = 0, gaps = 0, dropped = 0, onsets = 0;
                const uint32_t pas = gapGcdInWindow(&g_out[o], depart, g_ticks,
                                                    &kept, &gaps, &dropped);
                const uint32_t per = periodOverOnsets(&g_out[o], depart, g_ticks,
                                                      activeStepsInInstance(
                                                          expectedBytes, (int8_t)o),
                                                      &onsets);
                printf("rE_%s_OUT%d pas %u periode %u distances %u retenues %u onsets %u\n",
                       nom, o + 1, pas, per, gaps, kept, onsets);
            }
        }
        printf("rE_leviers         r5crans %d r7retour %d\n", r5Crans, r7CransRetour);
        return 0;
    }

    if (recetteR11) {
        static uint8_t vu[OBSERVED_INSTANCE_BYTES];
        constexpr int ongletR11 = 4;
        constexpr int8_t canalR11 = channelOfTab(ongletR11);
        constexpr int sortieR11 = canalR11;
        constexpr size_t rangR11 = STEP_BYTES + 2;
        constexpr size_t offsetR11 = instanceOffset(canalR11, rangR11);
        static_assert(canalR11 == 3, "tab 4 drives channel 3");
        static_assert(sortieR11 == 3, "channel 3 is output 4");
        static_assert(rangR11 == 7, "the ratchet of step 5 is byte 7 of the record");
        static_assert(offsetR11 == 76, "channel 3 byte 7 is buffer byte 76");
        const uint32_t pasActifs = activeStepsInInstance(expectedBytes, canalR11);
        uint32_t premier = 0, ecarts = 0, kept = 0, gaps = 0, dropped = 0, marque = 0;

        playPress(avr);
        run_for(avr, 1000.0);
        printf("rD_steps_actifs    %u\n", pasActifs);

        marque = g_twi_bytes;
        rotate(avr, 1, 1);
        printf("rD_amorce          onglet %d twi %u\n", selectedTab(), g_twi_bytes - marque);

        uint32_t depart = g_ticks;
        run_for(avr, 20000.0);
        read_bank(avr, vu);
        ecarts = bankDiffCount(vu, expectedBytes, &premier);
        uint32_t cad = gapGcdInWindow(&g_out[sortieR11], depart, g_ticks, &kept, &gaps, &dropped);
        printf("rD_base            cadence %u distances %u retenues %u ecarts %u\n",
               cad, gaps, kept, ecarts);

        alignTab(avr, ongletR11);
        marque = g_twi_bytes;
        pressFor(avr, (double)PRESS_MS);
        rotate(avr, 2, 1);
        if (!skipBGeste && r11CransSubdiv > 0)
            shiftRotate(avr, r11CransSubdiv, 0, harness::SUBDIV_BURST_LIMIT, false);
        printf("rD_nav_subdiv      onglet %d twi %u crans %d\n",
               selectedTab(), g_twi_bytes - marque, r11CransSubdiv);

        run_for(avr, 2500.0);
        depart = g_ticks;
        run_for(avr, 20000.0);
        read_bank(avr, vu);
        ecarts = bankDiffCount(vu, expectedBytes, &premier);
        cad = gapGcdInWindow(&g_out[sortieR11], depart, g_ticks, &kept, &gaps, &dropped);
        printf("rD_x24             cadence %u distances %u retenues %u ecarts %u\n",
               cad, gaps, kept, ecarts);

        pressFor(avr, (double)LONG_PRESS_MS);
        const int creneauxAvant = tabBandSlotsWithInk();
        marque = g_twi_bytes;
        pressFor(avr, (double)PRESS_MS);
        rotate(avr, 4, 1);
        if (!skipBGeste) pressFor(avr, (double)PRESS_MS);
        printf("rD_nav_edit        creneaux %d %d twi %u\n",
               creneauxAvant, tabBandSlotsWithInk(), g_twi_bytes - marque);

        {
            int rotations = 5;
            const char *r = getenv("R11_STEP_ROTATIONS");
            if (r != NULL) rotations = atoi(r);
            if (rotations < 1 || rotations > 24) {
                fprintf(stderr, "R11_STEP_ROTATIONS hors bornes 1..24\n");
                return 2;
            }
            rotate(avr, rotations, 1);
        }

        for (int cran = 1; cran <= 3; ++cran) {
            marque = g_twi_bytes;
            if (!skipBGeste) shiftRotate(avr, 1, 1, harness::RATCHET_BURST_LIMIT, false);
            read_bank(avr, vu);
            ecarts = bankDiffCount(vu, expectedBytes, &premier);
            printf("rD_cran%d           nibble %02x octet %02x ecarts %u premier %u twi %u"
                   " canal %d octet_instance %u\n",
                   cran, (unsigned)(byteOfInstance(vu, canalR11, rangR11) >> 4),
                   (unsigned)byteOfInstance(vu, canalR11, rangR11),
                   ecarts, premier, g_twi_bytes - marque,
                   (int)channelOfOffset(premier), (unsigned)byteOfOffset(premier));
        }

        marque = g_twi_bytes;
        if (!skipBGeste) shiftRotate(avr, 6, 0, harness::RATCHET_BURST_LIMIT, false);
        read_bank(avr, vu);
        ecarts = bankDiffCount(vu, expectedBytes, &premier);
        printf("rD_retour          nibble %02x octet %02x ecarts %u twi %u\n",
               (unsigned)(byteOfInstance(vu, canalR11, rangR11) >> 4),
                   (unsigned)byteOfInstance(vu, canalR11, rangR11),
               ecarts, g_twi_bytes - marque);

        pressFor(avr, (double)LONG_PRESS_MS);
        pressFor(avr, (double)LONG_PRESS_MS);
        alignTab(avr, ongletR11);
        marque = g_twi_bytes;
        pressFor(avr, (double)PRESS_MS);
        rotate(avr, 2, 1);
        if (!skipBGeste && r11CransSubdiv > 0)
            shiftRotate(avr, r11CransSubdiv, 1, harness::SUBDIV_BURST_LIMIT, false);
        run_for(avr, 2500.0);
        depart = g_ticks;
        run_for(avr, 20000.0);
        read_bank(avr, vu);
        ecarts = bankDiffCount(vu, expectedBytes, &premier);
        cad = gapGcdInWindow(&g_out[sortieR11], depart, g_ticks, &kept, &gaps, &dropped);
        printf("rD_cadence_fin     cadence %u distances %u retenues %u ecarts %u twi %u\n",
               cad, gaps, kept, ecarts, g_twi_bytes - marque);
        return 0;
    }

    if (recetteR2) {
        static uint8_t vu[OBSERVED_INSTANCE_BYTES];
        constexpr int ongletR2 = 4;
        constexpr int8_t canalR2 = channelOfTab(ongletR2);
        static_assert(canalR2 == 3, "tab 4 drives channel 3");
        const uint32_t pasActifs = activeStepsInInstance(expectedBytes, canalR2);

        playPress(avr);
        run_for(avr, 1000.0);
        printf("rC_steps_actifs    %u\n", pasActifs);

        uint32_t marque = g_twi_bytes;
        rotate(avr, 1, 1);
        printf("rC_amorce          onglet %d twi %u\n", selectedTab(), g_twi_bytes - marque);

        for (int etape = 0; etape < 5; ++etape) {
            const char *nom = (etape == 0) ? "base"
                            : (etape == 1) ? "length"
                            : (etape == 2) ? "lenrest"
                            : (etape == 3) ? "subdiv" : "subrest";
            uint32_t twiGeste = 0;

            if (etape == 1 || etape == 3) {
                alignTab(avr, ongletR2);
                marque = g_twi_bytes;
                pressFor(avr, (double)PRESS_MS);
                const int rotations = (etape == 1) ? 1 : r2Rotations;
                if (rotations > 0) rotate(avr, rotations, 1);
                if (!skipBGeste)
                    shiftRotate(avr, 1, etape == 1 ? 1 : 0,
                                harness::limitForFieldIndex((uint8_t)rotations), false);
                twiGeste = g_twi_bytes - marque;
            } else if (etape == 2 || etape == 4) {
                marque = g_twi_bytes;
                if (!skipBGeste)
                    shiftRotate(avr, 1, etape == 2 ? 0 : 1,
                                harness::limitForFieldIndex(
                                    (uint8_t)(etape == 2 ? 1 : r2Rotations)), false);
                twiGeste = g_twi_bytes - marque;
            }

            run_for(avr, 2500.0);
            const uint32_t depart = g_ticks;
            run_for(avr, 20000.0);

            read_bank(avr, vu);
            uint32_t premier = 0;
            const uint32_t ecarts = bankDiffCount(vu, expectedBytes, &premier);
            printf("rC_%s onglet %d twi %u ecarts %u\n", nom, selectedTab(), twiGeste, ecarts);
            for (int o = 0; o < OUT_COUNT; ++o) {
                uint32_t kept = 0, gaps = 0, dropped = 0, onsets = 0;
                const uint32_t pas = gapGcdInWindow(&g_out[o], depart, g_ticks,
                                                    &kept, &gaps, &dropped);
                const uint32_t per = periodOverOnsets(&g_out[o], depart, g_ticks,
                                                      activeStepsInInstance(
                                                          expectedBytes, (int8_t)o),
                                                      &onsets);
                printf("rC_%s_OUT%d pas %u periode %u distances %u retenues %u onsets %u\n",
                       nom, o + 1, pas, per, gaps, kept, onsets);
            }

            if (etape == 2) {
                marque = g_twi_bytes;
                pressFor(avr, (double)LONG_PRESS_MS);
                printf("rC_retour_barre    onglet %d creneaux %d twi %u\n",
                       selectedTab(), tabBandSlotsWithInk(), g_twi_bytes - marque);
            }
        }
        return 0;
    }

    if (recettesB) {
        playPress(avr);
        run_for(avr, 1000.0);

        constexpr int cibleR13 = 3;
        constexpr int8_t canalR13 = channelOfTab(cibleR13);
        static_assert(canalR13 == 2, "tab 3 drives channel 2");
        const uint32_t pasActifs = activeStepsInInstance(expectedBytes, canalR13);
        printf("rB_steps_actifs    %u\n", pasActifs);

        uint32_t marque = g_twi_bytes;
        rotate(avr, 1, 1);
        printf("rB_amorce          onglet %d twi %u\n",
               selectedTab(), g_twi_bytes - marque);

        for (int k = 1; k <= 6; ++k) {
            alignTab(avr, k);
            const int surBarre = selectedTab();
            marque = g_twi_bytes;
            pressFor(avr, (double)PRESS_MS);
            const int dedans = selectedTab();
            printf("rB_r1_nav          k %d barre %d dedans %d twi %u\n",
                   k, surBarre, dedans, g_twi_bytes - marque);

            rotate(avr, 2, 1);

            for (int etape = 0; etape < 2; ++etape) {
                marque = g_twi_bytes;
                if (!skipBGeste) shiftRotate(avr, 1, etape == 0 ? 0 : 1, harness::SUBDIV_BURST_LIMIT, false);
                const uint32_t twiGeste = g_twi_bytes - marque;
                run_for(avr, 2000.0);
                const uint32_t depart = g_ticks;
                run_for(avr, 20000.0);
                printf("rB_r1_%s k %d twi %u pas",
                       etape == 0 ? "change   " : "restaure ", k, twiGeste);
                uint32_t minDist = 0xFFFFFFFFu, minRet = 0xFFFFFFFFu;
                for (int o = 0; o < OUT_COUNT; ++o) {
                    uint32_t kept = 0, gaps = 0, dropped = 0;
                    const uint32_t g = gapGcdInWindow(&g_out[o], depart, g_ticks,
                                                      &kept, &gaps, &dropped);
                    printf(" %u", g);
                    if (gaps < minDist) minDist = gaps;
                    if (kept < minRet) minRet = kept;
                }
                printf(" distances %u retenues %u\n", minDist, minRet);
            }

            marque = g_twi_bytes;
            pressFor(avr, (double)LONG_PRESS_MS);
            printf("rB_r1_retour       k %d onglet %d creneaux %d twi %u\n",
                   k, selectedTab(), tabBandSlotsWithInk(), g_twi_bytes - marque);
        }

        alignTab(avr, cibleR13);
        marque = g_twi_bytes;
        pressFor(avr, (double)PRESS_MS);
        printf("rB_r13_nav         onglet %d twi %u\n", selectedTab(), g_twi_bytes - marque);

        rotate(avr, 1, 1);

        for (int etape = 0; etape < 2; ++etape) {
            marque = g_twi_bytes;
            if (!skipBGeste) shiftRotate(avr, 3, etape == 0 ? 1 : 0, harness::LENGTH_BURST_LIMIT, false);
            const uint32_t twiGeste = g_twi_bytes - marque;
            const uint32_t twiRetour0 = g_twi_bytes;
            pressFor(avr, (double)LONG_PRESS_MS);
            const uint32_t twiRetour = g_twi_bytes - twiRetour0;
            run_for(avr, 2000.0);
            const uint32_t depart = g_ticks;
            run_for(avr, etape == 0 ? 20000.0 : 18000.0);
            printf("rB_r13_%s onglet %d creneaux %d twi %u %u periode",
                   etape == 0 ? "change  " : "restaure", selectedTab(),
                   tabBandSlotsWithInk(), twiGeste, twiRetour);
            uint32_t minOnsets = 0xFFFFFFFFu;
            for (int o = 0; o < OUT_COUNT; ++o) {
                uint32_t onsets = 0;
                const uint32_t p = periodOverOnsets(&g_out[o], depart, g_ticks,
                                                    activeStepsInInstance(
                                                        expectedBytes, (int8_t)o),
                                                    &onsets);
                printf(" %u", p);
                if (onsets < minOnsets) minOnsets = onsets;
            }
            printf(" onsets %u\n", minOnsets);
            if (etape == 0) {
                alignTab(avr, cibleR13);
                pressFor(avr, (double)PRESS_MS);
                rotate(avr, 1, 1);
            }
        }
        return 0;
    }

    if (parcoursBootstrap) {
        static uint8_t apres[sizeof(g_image)];
        static uint8_t instances[OBSERVED_INSTANCE_BYTES];

        run_for(avr, boot_ms);

        const int luApresSetup = readSimulatedEeprom(avr, apres, (uint32_t)g_image_bytes);
        const uint16_t reparAt = EE_TEMPLATES_AT + BOOTSTRAP_BROKEN_TEMPLATE_AT;
        printf("boot_repare        lu %d octet %u valeur %u attendu %u\n",
               luApresSetup, reparAt,
               luApresSetup ? apres[reparAt] : 0,
               eepromExpectedTemplateByte(0, BOOTSTRAP_BROKEN_TEMPLATE_AT));

        uint32_t usinePremier = 0;
        uint32_t usineEcarts = 0;
        if (luApresSetup) {
            for (uint16_t index = 0; index < EE_TEMPLATE_COUNT; ++index) {
                for (uint16_t offset = 0; offset < EE_TEMPLATE_RECORD; ++offset) {
                    const uint32_t at = (uint32_t)EE_TEMPLATES_AT
                        + (uint32_t)index * EE_TEMPLATE_RECORD + offset;
                    if (apres[at] != eepromExpectedTemplateByte(index, offset)) {
                        ++usineEcarts;
                        if (usineEcarts == 1) usinePremier = at;
                    }
                }
            }
        }
        printf("boot_semis         lu %d ecarts %u premier %u\n",
               luApresSetup, usineEcarts, usinePremier);

        read_bank(avr, instances);
        printf("boot_instances    ");
        for (uint8_t c = 0; c < OBSERVED_CHANNEL_COUNT; ++c) {
            printf(" %04x", lowMaskOfInstance(instances, (int8_t)c));
        }
        printf(" attendu %04x\n", EE_A1_LOW_MASK);

        double attendu = 0.0;
        int versionVue = 0;
        uint8_t versionLue = 0;
        while (attendu < BOOTSTRAP_CEILING_MS) {
            run_for(avr, BOOTSTRAP_SLICE_MS);
            attendu += BOOTSTRAP_SLICE_MS;
            if (!readSimulatedEeprom(avr, apres, (uint32_t)g_image_bytes)) {
                continue;
            }
            versionLue = apres[EE_VERSION_AT];
            if (versionLue == EE_EXPECTED_VERSION) { versionVue = 1; break; }
        }
        printf("boot_version       vue %d valeur %u attendu %u attente_ms %.0f plafond_ms %.0f\n",
               versionVue, versionLue, EE_EXPECTED_VERSION, attendu, BOOTSTRAP_CEILING_MS);
        return 0;
    }

    if (parcoursInstances) {
        static uint8_t avant[OBSERVED_INSTANCE_BYTES];
        static uint8_t apres[OBSERVED_INSTANCE_BYTES];
        constexpr int ongletInstances = 4;
        constexpr int8_t canalInstances = channelOfTab(ongletInstances);
        constexpr size_t rangInstances = 0;
        static_assert(canalInstances == 3, "tab 4 drives channel 3");
        static_assert(instanceOffset(canalInstances, rangInstances) == 69,
                      "channel 3 byte 0 is buffer byte 69");

        playPress(avr);
        run_for(avr, 1000.0);

        read_bank(avr, avant);

        uint32_t eepromUsinePremier = 0;
        const uint32_t eepromUsineEcarts = eepromTemplateFactoryDiff(&eepromUsinePremier);

        printf("inst_zone          templates_eeprom %u lisible %d octets %u"
               " instances %zu\n",
               EE_TEMPLATES_AT, eepromTemplatesReadable(),
               EE_TEMPLATES_BYTES, OBSERVED_INSTANCE_BYTES);

        printf("inst_masques      ");
        for (uint8_t c = 0; c < OBSERVED_CHANNEL_COUNT; ++c) {
            printf(" %04x", lowMaskOfInstance(avant, (int8_t)c));
        }
        printf("\n");

        printf("inst_actifs       ");
        for (uint8_t c = 0; c < OBSERVED_CHANNEL_COUNT; ++c) {
            printf(" %u", activeStepsInInstance(avant, (int8_t)c));
        }
        printf("\n");

        printf("inst_selection    ");
        for (uint8_t c = 0; c < OBSERVED_CHANNEL_COUNT; ++c) {
            printf(" %d", (int)g_expected_engine.getSelectedPattern(c));
        }
        printf("\n");

        uint32_t marque = g_twi_bytes;
        alignTab(avr, ongletInstances);
        const int ongletAligne = selectedTab();
        const int creneauxBarre = tabBandSlotsWithInk();
        pressFor(avr, (double)PRESS_MS);
        rotate(avr, 4, 1);
        pressFor(avr, (double)PRESS_MS);
        printf("inst_nav_edit      onglet_aligne %d vise %d creneaux %d %d twi %u\n",
               ongletAligne, ongletInstances, creneauxBarre,
               tabBandSlotsWithInk(), g_twi_bytes - marque);

        marque = g_twi_bytes;
        pressFor(avr, (double)PRESS_MS);
        const int mute = mutateEepromTemplates(avr);
        read_bank(avr, apres);
        if (mute) printf("inst_mutation      octet %ld de la zone EEPROM des templates\n",
                         g_template_mutate);

        uint32_t premier = 0;
        const uint32_t ecarts = bankDiffCount(avant, apres, &premier);
        printf("inst_edit          canal_vise %d ecarts %u premier %u"
               " canal %d octet_instance %u twi %u\n",
               (int)canalInstances, ecarts, premier,
               (int)channelOfOffset(premier), (unsigned)byteOfOffset(premier),
               g_twi_bytes - marque);

        printf("inst_tpl_usine     ecarts %u premier %u lisible %d\n",
               eepromUsineEcarts, eepromUsinePremier, eepromTemplatesReadable());

        static uint8_t eepromApres[sizeof(g_image)];
        const int eepromLu = eepromTemplatesReadable()
            && readSimulatedEeprom(avr, eepromApres, (uint32_t)g_image_bytes);
        uint32_t eepromDerivePremier = 0;
        const uint32_t eepromDeriveEcarts =
            eepromLu ? eepromTemplateDrift(eepromApres, &eepromDerivePremier) : 0;
        printf("inst_tpl_eeprom    ecarts %u premier %u lu %d\n",
               eepromDeriveEcarts, eepromDerivePremier, eepromLu);
        return 0;
    }

    if (recettesA) {
        static uint8_t vu[OBSERVED_INSTANCE_BYTES];
        constexpr int ongletA = 1;
        constexpr int8_t canalA = channelOfTab(ongletA);
        static_assert(canalA == 0, "tab 1 drives channel 0");
        static_assert(instanceOffset(canalA, 0) == 0,
                      "channel 0 starts at buffer byte 0");
        static_assert(instanceOffset(canalA, STEP_BYTES + 2) == 7,
                      "the ratchet of step 5 is buffer byte 7 on channel 0");
        static_assert(instanceOffset(canalA, STEP_BYTES + 4) == 9,
                      "the ratchet of step 9 is buffer byte 9 on channel 0");
        uint32_t premier = 0, ecarts = 0;
        uint32_t marque = 0;

        const int creneauxAvant = tabBandSlotsWithInk();
        marque = g_twi_bytes;
        if (!skipEdit) {
            alignTab(avr, ongletA);
            pressFor(avr, (double)PRESS_MS);
            rotate(avr, 4, 1);
            pressFor(avr, (double)PRESS_MS);
        }
        printf("rA_edit            twi %u creneaux %d %d\n",
               g_twi_bytes - marque, creneauxAvant, tabBandSlotsWithInk());

        read_bank(avr, vu);
        ecarts = bankDiffCount(vu, expectedBytes, &premier);
        printf("rA_base            masque %04x octet6 %02x octet8 %02x ecarts %u\n",
               lowMaskOfInstance(vu, canalA), byteOfInstance(vu, canalA, STEP_BYTES + 2), byteOfInstance(vu, canalA, STEP_BYTES + 4), ecarts);

        rotate(avr, 3, 1);
        marque = g_twi_bytes;
        pressFor(avr, (double)PRESS_MS);
        read_bank(avr, vu);
        ecarts = bankDiffCount(vu, expectedBytes, &premier);
        printf("rA_r8_pose         masque %04x ecarts %u premier %u twi %u"
               " canal %d octet_instance %u\n",
               lowMaskOfInstance(vu, canalA), ecarts, premier,
               g_twi_bytes - marque,
               (int)channelOfOffset(premier), (unsigned)byteOfOffset(premier));
        marque = g_twi_bytes;
        pressFor(avr, (double)PRESS_MS);
        read_bank(avr, vu);
        ecarts = bankDiffCount(vu, expectedBytes, &premier);
        printf("rA_r8_retour       masque %04x ecarts %u twi %u\n",
               lowMaskOfInstance(vu, canalA), ecarts, g_twi_bytes - marque);

        rotate(avr, 2, 1);
        marque = g_twi_bytes;
        shiftRotate(avr, 4, 1, harness::STEP_BURST_LIMIT, false);
        read_bank(avr, vu);
        ecarts = bankDiffCount(vu, expectedBytes, &premier);
        printf("rA_r9_pose         octet6 %02x ecarts %u premier %u twi %u"
               " canal %d octet_instance %u\n",
               byteOfInstance(vu, canalA, STEP_BYTES + 2), ecarts, premier,
               g_twi_bytes - marque,
               (int)channelOfOffset(premier), (unsigned)byteOfOffset(premier));
        marque = g_twi_bytes;
        shiftRotate(avr, 4, 0, harness::RATCHET_BURST_LIMIT, false);
        read_bank(avr, vu);
        ecarts = bankDiffCount(vu, expectedBytes, &premier);
        printf("rA_r9_retour       octet6 %02x ecarts %u twi %u\n",
               byteOfInstance(vu, canalA, STEP_BYTES + 2), ecarts, g_twi_bytes - marque);

        const int versR10 = (r10Step - 5 + 24) % 24;
        rotate(avr, versR10 == 0 ? 24 : versR10, 1);
        marque = g_twi_bytes;
        shiftRotate(avr, 4, 1, harness::STEP_BURST_LIMIT, false);
        read_bank(avr, vu);
        ecarts = bankDiffCount(vu, expectedBytes, &premier);
        printf("rA_r10             cible %d rotations %d octet6 %02x ecarts %u twi %u\n",
               r10Step, versR10 == 0 ? 24 : versR10, byteOfInstance(vu, canalA, STEP_BYTES + 2), ecarts,
               g_twi_bytes - marque);

        rotate(avr, (9 - r10Step + 24) % 24, 1);
        marque = g_twi_bytes;
        shiftRotate(avr, 5, 1, harness::RATCHET_BURST_LIMIT, false);
        read_bank(avr, vu);
        ecarts = bankDiffCount(vu, expectedBytes, &premier);
        printf("rA_r12_pose        octet8 %02x ecarts %u premier %u twi %u"
               " canal %d octet_instance %u\n",
               byteOfInstance(vu, canalA, STEP_BYTES + 4), ecarts, premier,
               g_twi_bytes - marque,
               (int)channelOfOffset(premier), (unsigned)byteOfOffset(premier));
        marque = g_twi_bytes;
        shiftRotate(avr, 5, 0, harness::RATCHET_BURST_LIMIT, false);
        read_bank(avr, vu);
        ecarts = bankDiffCount(vu, expectedBytes, &premier);
        printf("rA_r12_retour      octet8 %02x ecarts %u twi %u\n",
               byteOfInstance(vu, canalA, STEP_BYTES + 4), ecarts, g_twi_bytes - marque);
        return 0;
    }

    if (rig) {
        playPress(avr);
        run_for(avr, 1000.0);

        uint32_t kept = 0, gaps = 0, dropped = 0;
        const uint32_t tStart = g_ticks;
        run_for(avr, 16000.0);
        uint32_t g = gapGcdInWindow(&g_out[0], tStart, g_ticks, &kept, &gaps, &dropped);
        printf("rig_pas_initial    %u pgcd, %u retenues, %u distances, %u isolees rejetees\n",
               g, kept, gaps, dropped);

        pressFor(avr, (double)PRESS_MS);
        rotate(avr, 1, 1);
        rotate(avr, 1, 1);

        static const int marche[3] = { 7, 1, 1 };
        int cumule = 0;
        for (int etape = 0; etape < 3; ++etape) {
            const uint32_t twiBefore = g_twi_bytes;
            shiftRotate(avr, marche[etape], 0, harness::SUBDIV_BURST_LIMIT, false);
            cumule += marche[etape];
            run_for(avr, 2000.0);
            const uint32_t from = g_ticks;
            run_for(avr, 2000.0);
            g = gapGcdInWindow(&g_out[0], from, g_ticks, &kept, &gaps, &dropped);
            printf("rig_marche         %d crans, %u pgcd, %u retenues, %u distances,"
                   " %u isolees rejetees, twi %u\n",
                   cumule, g, kept, gaps, dropped, g_twi_bytes - twiBefore);
        }
        return 0;
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
        shiftRotate(avr, 3, 1, harness::LENGTH_BURST_LIMIT, false);
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
        shiftRotate(avr, 1, 1, harness::SUBDIV_BURST_LIMIT, false);
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

    static uint8_t bankAfterRotations[OBSERVED_INSTANCE_BYTES];
    read_bank(avr, bankAfterRotations);
    printf("bank_inchangee     %d\n",
           memcmp(bank, bankAfterRotations, sizeof(bank)) == 0 ? 1 : 0);

    run_for(avr, FRAME_SETTLE_MS);
    const uint32_t sigTabBar = screenSignature();
    printf("signature_tabbar   %08x\n", sigTabBar);

    uint32_t twiMark = g_twi_bytes;
    pressFor(avr, (double)NO_OP_PRESS_MS);
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

    constexpr int ongletPrincipal = 1;
    constexpr int8_t canalPrincipal = channelOfTab(ongletPrincipal);
    static_assert(canalPrincipal == 0, "tab 1 drives channel 0");
    static_assert(instanceOffset(canalPrincipal, 0) == 0,
                  "channel 0 starts at buffer byte 0");
    static_assert(instanceOffset(canalPrincipal, STEP_BYTES) == 5,
                  "the ratchet of step 0 is buffer byte 5 on channel 0");

    const int slotsBeforeEdit = tabBandSlotsWithInk();
    twiMark = g_twi_bytes;
    if (!skipEdit) {
        alignTab(avr, ongletPrincipal);
        pressFor(avr, (double)PRESS_MS);
        rotate(avr, 4, 1);
        pressFor(avr, (double)PRESS_MS);
    }
    const uint32_t sigEdit = screenSignature();
    printf("entree_edit        signature %08x twi %u creneaux %d %d distincte %d\n",
           sigEdit, g_twi_bytes - twiMark, slotsBeforeEdit, tabBandSlotsWithInk(),
           (sigEdit != sigTabBar && sigEdit != sigAfterShort) ? 1 : 0);

    static uint8_t bankBeforeShift[OBSERVED_INSTANCE_BYTES];
    read_bank(avr, bankBeforeShift);
    printf("ratchet_avant      %02x\n", byteOfInstance(bankBeforeShift, canalPrincipal, STEP_BYTES));

    twiMark = g_twi_bytes;
    const double heldMs = skipShift ? 192.0 : shiftRotate(avr, 3, 1, harness::RATCHET_BURST_LIMIT, false);
    static uint8_t bankAfterShift[OBSERVED_INSTANCE_BYTES];
    read_bank(avr, bankAfterShift);
    printf("shift_maintien_ms  %.1f\n", heldMs);
    printf("ratchet_apres      %02x\n", byteOfInstance(bankAfterShift, canalPrincipal, STEP_BYTES));
    printf("shift_twi          %u\n", g_twi_bytes - twiMark);
    printf("masques_intacts    %d\n",
           memcmp(instanceOf(bankBeforeShift, canalPrincipal),
                  instanceOf(bankAfterShift, canalPrincipal), STEP_BYTES) == 0
           && lowMaskOfInstance(bankAfterShift, canalPrincipal) == 0x9111 ? 1 : 0);

    twiMark = g_twi_bytes;
    shiftRotate(avr, 2, 1, harness::RATCHET_BURST_LIMIT, false);
    static uint8_t bankTriplet[OBSERVED_INSTANCE_BYTES];
    read_bank(avr, bankTriplet);
    printf("triolet_pose       %02x twi %u\n",
           byteOfInstance(bankTriplet, canalPrincipal, STEP_BYTES) & 0x0F, g_twi_bytes - twiMark);

    twiMark = g_twi_bytes;
    shiftRotate(avr, 5, 0, harness::RATCHET_BURST_LIMIT, false);
    static uint8_t bankNoRatchet[OBSERVED_INSTANCE_BYTES];
    read_bank(avr, bankNoRatchet);
    printf("triolet_retire     %02x twi %u\n",
           byteOfInstance(bankNoRatchet, canalPrincipal, STEP_BYTES) & 0x0F, g_twi_bytes - twiMark);
    printf("banque_restauree   %d\n",
           memcmp(expectedBytes, bankNoRatchet, OBSERVED_INSTANCE_BYTES) == 0 ? 1 : 0);

    rotate(avr, 1, 1);
    twiMark = g_twi_bytes;
    pressFor(avr, (double)PRESS_MS);
    static uint8_t bankToggled[OBSERVED_INSTANCE_BYTES];
    read_bank(avr, bankToggled);
    printf("step1_bascule      %04x twi %u\n",
           lowMaskOfInstance(bankToggled, canalPrincipal), g_twi_bytes - twiMark);

    twiMark = g_twi_bytes;
    pressFor(avr, (double)PRESS_MS);
    static uint8_t bankRestored[OBSERVED_INSTANCE_BYTES];
    read_bank(avr, bankRestored);
    printf("step1_rebascule    %04x twi %u\n",
           lowMaskOfInstance(bankRestored, canalPrincipal), g_twi_bytes - twiMark);
    printf("banque_finale      %d\n",
           memcmp(expectedBytes, bankRestored, OBSERVED_INSTANCE_BYTES) == 0 ? 1 : 0);

    rotate(avr, 1, 0);

    const int longDetents = SHIFT_BURST_DETENTS * 2 + 2;
    uint32_t suppressedBefore = 0;
    const int readableBefore = readSuppressedCounter(avr, &suppressedBefore);
    const uint32_t encSwLowsBefore = g_enc_sw_lows;
    const int burstsBefore = g_shift_bursts;
    twiMark = g_twi_bytes;

    printf("fract_demande      %d crans, plafond %d crans par salve\n",
           longDetents, SHIFT_BURST_DETENTS);
    const double heldUp = shiftRotate(avr, longDetents, 1, harness::RATCHET_BURST_LIMIT, true);
    static uint8_t bankLongUp[OBSERVED_INSTANCE_BYTES];
    read_bank(avr, bankLongUp);
    const int burstsUp = g_shift_bursts - burstsBefore;
    printf("fract_salves       %d\n", burstsUp);
    printf("fract_maintien_up  %.1f\n", heldUp);
    printf("fract_ratchet      %02x\n", byteOfInstance(bankLongUp, canalPrincipal, STEP_BYTES) & 0x0F);
    printf("fract_masques      %d\n",
           memcmp(instanceOf(bankRestored, canalPrincipal),
                  instanceOf(bankLongUp, canalPrincipal), STEP_BYTES) == 0
           && lowMaskOfInstance(bankLongUp, canalPrincipal) == 0x9111 ? 1 : 0);
    printf("fract_twi          %u\n", g_twi_bytes - twiMark);

    const double heldDown = shiftRotate(avr, longDetents, 0, harness::RATCHET_BURST_LIMIT, true);
    static uint8_t bankLongDown[OBSERVED_INSTANCE_BYTES];
    read_bank(avr, bankLongDown);
    uint32_t suppressedAfter = 0;
    const int readableAfter = readSuppressedCounter(avr, &suppressedAfter);
    printf("fract_maintien_max %.1f\n", heldDown > heldUp ? heldDown : heldUp);
    printf("fract_retour       %d\n",
           memcmp(expectedBytes, bankLongDown, OBSERVED_INSTANCE_BYTES) == 0 ? 1 : 0);
    printf("fract_enc_sw       %u\n", g_enc_sw_lows - encSwLowsBefore);
    printf("fract_compteur     lisible %d avant %ld apres %ld\n",
           (readableBefore && readableAfter) ? 1 : 0,
           readableBefore ? (long)suppressedBefore : -1L,
           readableAfter ? (long)suppressedAfter + g_suppressed_bias : -1L);
    if (readableBefore && readableAfter) {
        printf("fract_suppressions %ld\n",
               (long)suppressedAfter + g_suppressed_bias - (long)suppressedBefore);
    }
    printf("shift_salves       %d\n", g_shift_bursts);
    printf("shift_maintien_max %.1f\n", g_shift_hold_max);
    printf("shift_plafond      %d %d %d\n",
           SHIFT_BURST_DETENTS, SHIFT_HOLD_MS(SHIFT_BURST_DETENTS), SHIFT_LONG_PRESS_MS);

    return 0;
}
