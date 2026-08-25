#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <sim_avr.h>
#include <sim_elf.h>
#include <sim_hex.h>
#include <sim_irq.h>
#include <avr_twi.h>
#include <avr_ioport.h>
#include <avr_uart.h>
#include <avr_eeprom.h>
#include <parts/ssd1306_virt.h>

#include "simavr_uart_quiet.h"

#define MCU        "atmega328p"
#define F_CPU_HZ   16000000UL

#define TIMER1_COMPA_VECTOR 11

#define MIDI_CLOCK 0xF8
#define MIDI_START 0xFA
#define MIDI_STOP  0xFC

#define OUT_COUNT 6
static const struct { char port; uint8_t bit; const char *name; } OUTS[OUT_COUNT] = {
    {'D', 7, "OUT1"}, {'B', 0, "OUT2"}, {'B', 2, "OUT3"},
    {'D', 6, "OUT4"}, {'B', 1, "OUT5"}, {'B', 3, "OUT6"},
};

#define MAX_STEPS 32
#define RATCHET_TRIPLET 7
#define MIN_SLOT_TICKS 2
#define LATE_TOLERANCE_TICKS 2

static avr_t *g_avr;

static uint64_t *g_isr_cycle;
static uint32_t  g_isr_count, g_isr_cap;
static int64_t   g_start_isr = -1;
static uint64_t  g_start_cycle;

typedef struct { uint64_t *cycle; uint32_t n, cap; int last; } line_t;
static line_t g_line[OUT_COUNT];

static uint32_t g_midi_clock, g_midi_start, g_midi_stop;
#define MAX_TRANSPORT_EVENTS 64
static struct { uint8_t byte; uint64_t cycle; } g_transport[MAX_TRANSPORT_EVENTS];
static uint32_t g_transport_n;
static uint64_t g_first_edge_cycle;
static uint32_t g_restart_after_edges;
static uint64_t g_start_byte_cycle;
static uint32_t g_sync_gap_min = 0xFFFFFFFFu, g_sync_gap_max;
static int64_t  g_last_sync_tick = -1;

static void grow(uint64_t **buf, uint32_t *cap, uint32_t need)
{
    if (need < *cap) return;
    uint32_t next = *cap ? *cap * 2 : 4096;
    while (next <= need) next *= 2;
    uint64_t *p = (uint64_t *)realloc(*buf, (size_t)next * sizeof(uint64_t));
    if (!p) { fprintf(stderr, "memoire epuisee\n"); exit(1); }
    *buf = p;
    *cap = next;
}

static void isr_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
    (void)irq; (void)param;
    if (!value) return;
    grow(&g_isr_cycle, &g_isr_cap, g_isr_count);
    g_isr_cycle[g_isr_count++] = g_avr->cycle;
    if (g_start_isr < 0 && g_midi_start > 0 && g_avr->cycle > g_start_byte_cycle) {
        g_start_isr = (int64_t)g_isr_count - 1;
        g_start_cycle = g_avr->cycle;
    }
}

static void uart_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
    (void)irq; (void)param;
    const uint8_t byte = (uint8_t)value;
    if (byte == MIDI_START || byte == MIDI_STOP) {
        if (g_transport_n < MAX_TRANSPORT_EVENTS) {
            g_transport[g_transport_n].byte = byte;
            g_transport[g_transport_n].cycle = g_avr->cycle;
            ++g_transport_n;
        }
    }
    if (byte == MIDI_START) {
        ++g_midi_start;
        g_start_byte_cycle = g_avr->cycle;
        g_start_isr = -1;
        g_last_sync_tick = -1;
        g_sync_gap_min = 0xFFFFFFFFu;
        g_sync_gap_max = 0;
        if (g_first_edge_cycle != 0) ++g_restart_after_edges;
        g_first_edge_cycle = 0;
        for (int i = 0; i < OUT_COUNT; ++i) g_line[i].n = 0;
    } else if (byte == MIDI_STOP) {
        ++g_midi_stop;
    } else if (byte == MIDI_CLOCK) {
        ++g_midi_clock;
        if (g_start_isr >= 0) {
            const int64_t tick = (int64_t)g_isr_count - g_start_isr;
            if (g_last_sync_tick >= 0) {
                const uint32_t gap = (uint32_t)(tick - g_last_sync_tick);
                if (gap < g_sync_gap_min) g_sync_gap_min = gap;
                if (gap > g_sync_gap_max) g_sync_gap_max = gap;
            }
            g_last_sync_tick = tick;
        }
    }
}

static void pin_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
    (void)irq;
    line_t *l = (line_t *)param;
    const int level = value ? 1 : 0;
    if (level == l->last) return;
    l->last = level;
    if (!level) return;
    if (g_start_isr < 0) return;
    if (g_first_edge_cycle == 0) g_first_edge_cycle = g_avr->cycle;
    grow(&l->cycle, &l->cap, l->n);
    l->cycle[l->n++] = g_avr->cycle;
}

static uint8_t ratchet_triggers(uint8_t code)
{
    if (code == RATCHET_TRIPLET) return 3;
    if (code == 2 || code == 3 || code == 4 || code == 6) return code;
    return 1;
}

static uint8_t ratchet_span(uint8_t code) { return code == RATCHET_TRIPLET ? 2 : 1; }

static uint32_t sub_onset_tick(uint32_t step_ticks, uint8_t triggers, uint8_t k)
{
    if (triggers <= 1) return step_ticks;
    return (step_ticks * (uint32_t)k) / (uint32_t)triggers;
}

typedef struct { uint32_t tick; uint8_t step; uint8_t sub; } onset_t;

static onset_t *g_expect;
static uint32_t g_expect_n, g_expect_cap;

static void push_expect(uint32_t tick, uint8_t step, uint8_t sub)
{
    if (g_expect_n >= g_expect_cap) {
        uint32_t next = g_expect_cap ? g_expect_cap * 2 : 8192;
        onset_t *p = (onset_t *)realloc(g_expect, (size_t)next * sizeof(onset_t));
        if (!p) { fprintf(stderr, "memoire epuisee\n"); exit(1); }
        g_expect = p; g_expect_cap = next;
    }
    g_expect[g_expect_n].tick = tick;
    g_expect[g_expect_n].step = step;
    g_expect[g_expect_n].sub  = sub;
    ++g_expect_n;
}

static void build_expected(const uint8_t *active, const uint8_t *ratchet,
                           uint8_t length, uint32_t ticks_per_step,
                           uint32_t horizon, int seq_mode)
{
    uint32_t tick = 0;
    uint8_t step = 0;

    for (;;) {
        const uint8_t code = ratchet[step];
        uint8_t triggers = ratchet_triggers(code);
        const uint32_t step_ticks = ticks_per_step * ratchet_span(code);
        if (triggers > 1 && step_ticks / triggers < MIN_SLOT_TICKS) triggers = 1;

        const int plays = !seq_mode || active[step];
        for (uint8_t k = 1; k < triggers; ++k) {
            const uint32_t at = tick + sub_onset_tick(step_ticks, triggers, k);
            if (at > horizon) return;
            if (plays) push_expect(at, step, k);
        }

        tick += step_ticks;
        if (tick > horizon) return;
        step = (uint8_t)((step + 1) % length);
        if (!seq_mode || active[step]) push_expect(tick, step, 0);
    }
}

static uint32_t tick_at(uint64_t cycle)
{
    uint32_t lo = (uint32_t)g_start_isr, hi = g_isr_count;
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        if (g_isr_cycle[mid] <= cycle) lo = mid + 1; else hi = mid;
    }
    return lo - (uint32_t)g_start_isr;
}

static uint64_t cycle_of_tick(uint32_t tick)
{
    if (tick == 0) return g_start_cycle;
    const uint32_t idx = (uint32_t)g_start_isr + tick - 1;
    if (idx >= g_isr_count) return 0;
    return g_isr_cycle[idx];
}

static double us_of(uint64_t cycles) { return (double)cycles * 1e6 / (double)F_CPU_HZ; }

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr,
            "usage: %s <firmware.hex> <secondes> <image.eeprom> [base] [steps]\n"
            "          [ratchets] [ticksPerStep] [length] [mode] [csv]\n", argv[0]);
        return 2;
    }
    const char *fw = argv[1];
    const double seconds = atof(argv[2]);
    const char *ee_path = argv[3];
    const uint16_t ee_base = (argc > 4) ? (uint16_t)strtol(argv[4], NULL, 0) : 384;
    const char *steps_txt = (argc > 5) ? argv[5] : "0,3,4,9,15";
    const char *ratch_txt = (argc > 6) ? argv[6] : "";
    const uint32_t ticks_per_step = (argc > 7) ? (uint32_t)strtoul(argv[7], NULL, 10) : 96;
    const uint8_t length = (argc > 8) ? (uint8_t)strtoul(argv[8], NULL, 10) : 16;
    const char *mode = (argc > 9) ? argv[9] : "seq";
    const char *csv_path = (argc > 10 && argv[10][0]) ? argv[10] : NULL;
    const int seq_mode = strcmp(mode, "seq") == 0;

    setvbuf(stdout, NULL, _IONBF, 0);

    uint8_t active[MAX_STEPS] = {0}, ratchet[MAX_STEPS] = {0};
    for (const char *p = steps_txt; *p; ) {
        char *end = NULL;
        const long v = strtol(p, &end, 10);
        if (end == p || v < 0 || v >= MAX_STEPS) { fprintf(stderr, "steps refuses\n"); return 2; }
        active[v] = 1;
        p = end; if (*p == ',') ++p;
    }
    for (const char *p = ratch_txt; *p; ) {
        char *end = NULL;
        const long s = strtol(p, &end, 10);
        if (end == p || *end != ':' || s < 0 || s >= MAX_STEPS) { fprintf(stderr, "ratchets refuses\n"); return 2; }
        p = end + 1;
        const long c = strtol(p, &end, 10);
        if (end == p || c < 0 || c > 7) { fprintf(stderr, "ratchets refuses\n"); return 2; }
        ratchet[s] = (uint8_t)c;
        p = end; if (*p == ',') ++p;
    }

    elf_firmware_t f = {{0}};
    strcpy(f.mmcu, MCU);
    f.frequency = F_CPU_HZ;
    f.vcc = f.avcc = f.aref = 5000;
    sim_setup_firmware(fw, 0, &f, "drift_probe");

    avr_t *avr = avr_make_mcu_by_name(MCU);
    if (!avr) { fprintf(stderr, "MCU inconnu\n"); return 1; }
    g_avr = avr;
    avr_init(avr);
    avr_load_firmware(avr, &f);
    uart_quiet(avr, '0');

    FILE *ef = fopen(ee_path, "rb");
    if (!ef) { fprintf(stderr, "image EEPROM illisible : %s\n", ee_path); return 2; }
    static uint8_t image[1024];
    const size_t en = fread(image, 1, sizeof(image), ef);
    fclose(ef);
    avr_eeprom_desc_t ee = { .ee = image, .offset = ee_base, .size = (uint32_t)en };
    if (en == 0 || avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &ee) != 0) {
        fprintf(stderr, "image EEPROM refusee\n"); return 2;
    }

    static ssd1306_t oled;
    ssd1306_init(avr, &oled, 128, 64);
    avr_irq_t *twi_out = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_OUTPUT);
    avr_irq_t *twi_in  = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT);
    avr_connect_irq(twi_out, oled.irq + IRQ_SSD1306_TWI_OUT);
    avr_connect_irq(oled.irq + IRQ_SSD1306_TWI_IN, twi_in);

    avr_irq_t *vec = avr_get_interrupt_irq(avr, TIMER1_COMPA_VECTOR);
    if (!vec) { fprintf(stderr, "vecteur TIMER1_COMPA introuvable\n"); return 1; }
    avr_irq_register_notify(vec + AVR_INT_IRQ_RUNNING, isr_hook, NULL);

    avr_irq_t *tx = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT);
    if (!tx) { fprintf(stderr, "UART introuvable\n"); return 1; }
    avr_irq_register_notify(tx, uart_hook, NULL);

    for (int i = 0; i < OUT_COUNT; ++i) {
        g_line[i].last = -1;
        avr_irq_t *pin = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(OUTS[i].port), OUTS[i].bit);
        if (!pin) { fprintf(stderr, "broche introuvable : %s\n", OUTS[i].name); return 1; }
        avr_irq_register_notify(pin, pin_hook, &g_line[i]);
    }

    printf("firmware   %s\n", fw);
    printf("simulation %.1f s ; mode %s ; ticksPerStep %u ; length %u\n",
           seconds, mode, ticks_per_step, length);

    const uint64_t target  = (uint64_t)(seconds * (double)F_CPU_HZ);
    const uint64_t play_dn = (uint64_t)(0.60 * (double)F_CPU_HZ);
    const uint64_t play_up = (uint64_t)(0.66 * (double)F_CPU_HZ);
    const int no_play = getenv("NO_PLAY") != NULL;
    avr_irq_t *play = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 5);
    if (!play) { fprintf(stderr, "broche PLAY introuvable\n"); return 1; }
    int play_state = 2;
    printf("play_injection     %s\n", no_play ? "aucune" : "60 ms a 600 ms");

    while (avr->cycle < target) {
        const int want = (!no_play && avr->cycle >= play_dn && avr->cycle < play_up) ? 0 : 1;
        if (want != play_state) { play_state = want; avr_raise_irq(play, want); }
        const int state = avr_run(avr);
        if (state == cpu_Done || state == cpu_Crashed) {
            printf("!! CPU arrete (state=%d)\n", state);
            break;
        }
    }

    if (g_start_isr < 0) {
        fprintf(stderr, "aucun MIDI Start observe : le transport n a pas demarre\n");
        return 3;
    }

    const uint32_t ticks = g_isr_count - (uint32_t)g_start_isr;
    build_expected(active, ratchet, length, ticks_per_step, ticks, seq_mode);

    int monotonic = 1;
    for (uint32_t i = 1; i < g_isr_count; ++i)
        if (g_isr_cycle[i] <= g_isr_cycle[i - 1]) { monotonic = 0; break; }
    for (int c = 0; c < OUT_COUNT && monotonic; ++c)
        for (uint32_t i = 1; i < g_line[c].n; ++i)
            if (g_line[c].cycle[i] <= g_line[c].cycle[i - 1]) { monotonic = 0; break; }

    FILE *csv = NULL;
    if (csv_path) {
        csv = fopen(csv_path, "w");
        if (!csv) { fprintf(stderr, "CSV impossible : %s\n", csv_path); return 1; }
        fprintf(csv, "channel,onset_index,step,sub_index,expected_tick,actual_tick,"
                     "grid_error_ticks,expected_us,actual_us,timing_error_us\n");
    }

    uint32_t total_matched = 0, total_dropped = 0, total_unexpected = 0;

    for (int c = 0; c < OUT_COUNT; ++c) {
        uint32_t i = 0, j = 0;
        const line_t *l = &g_line[c];
        while (i < g_expect_n && j < l->n) {
            const uint32_t at = tick_at(l->cycle[j]);
            if (at + LATE_TOLERANCE_TICKS < g_expect[i].tick) {
                ++total_unexpected; ++j;
                continue;
            }
            if (i + 1 < g_expect_n && at >= g_expect[i + 1].tick) {
                ++total_dropped; ++i;
                continue;
            }
            const uint64_t exp_cycle = cycle_of_tick(g_expect[i].tick);
            if (csv) {
                fprintf(csv, "%d,%u,%u,%u,%u,%u,%d,%.3f,%.3f,%.3f\n",
                        c + 1, i, g_expect[i].step, g_expect[i].sub,
                        g_expect[i].tick, at, (int)at - (int)g_expect[i].tick,
                        us_of(exp_cycle), us_of(l->cycle[j]),
                        us_of(l->cycle[j]) - us_of(exp_cycle));
            }
            ++total_matched; ++i; ++j;
        }
        total_unexpected += (l->n - j);
        while (i < g_expect_n) {
            if (g_expect[i].tick + LATE_TOLERANCE_TICKS < ticks) ++total_dropped;
            ++i;
        }
    }
    if (csv) fclose(csv);

    double period_mean = 0.0;
    uint64_t pmin = 0, pmax = 0;
    if (ticks > 2) {
        const uint64_t span = g_isr_cycle[g_isr_count - 1] - g_isr_cycle[g_start_isr];
        period_mean = (double)span / (double)(ticks - 1);
        pmin = (uint64_t)-1;
        for (uint32_t k = (uint32_t)g_start_isr + 1; k < g_isr_count; ++k) {
            const uint64_t d = g_isr_cycle[k] - g_isr_cycle[k - 1];
            if (d < pmin) pmin = d;
            if (d > pmax) pmax = d;
        }
    }

    printf("\n=== MESURE ===\n");
    printf("isr_total          %u\n", g_isr_count);
    printf("start_isr_index    %" PRId64 "\n", g_start_isr);
    printf("ticks              %u\n", ticks);
    printf("midi_start         %u\n", g_midi_start);
    printf("midi_stop          %u\n", g_midi_stop);
    printf("midi_clock         %u\n", g_midi_clock);
    printf("restart_after_edge %u\n", g_restart_after_edges);
    printf("anchor_ms          %.3f\n",
           (double)g_start_cycle * 1e3 / (double)F_CPU_HZ);
    for (uint32_t i = 0; i < g_transport_n; ++i)
        printf("transport_event    %s a %.3f ms\n",
               g_transport[i].byte == MIDI_START ? "START" : "STOP",
               (double)g_transport[i].cycle * 1e3 / (double)F_CPU_HZ);
    printf("sync_gap_ticks     %u..%u\n",
           g_sync_gap_min == 0xFFFFFFFFu ? 0 : g_sync_gap_min, g_sync_gap_max);
    printf("monotonic          %d\n", monotonic);
    printf("expected_per_line  %u\n", g_expect_n);
    for (int c = 0; c < OUT_COUNT; ++c)
        printf("edges_%s           %u\n", OUTS[c].name, g_line[c].n);
    printf("matched            %u\n", total_matched);
    printf("dropped            %u\n", total_dropped);
    printf("unexpected         %u\n", total_unexpected);
    printf("isr_period_cycles  %.4f (min %" PRIu64 " max %" PRIu64 ")\n",
           period_mean, pmin, pmax);
    printf("isr_period_us      %.6f\n", us_of(1) * period_mean);
    if (csv_path) printf("csv                %s\n", csv_path);

    return 0;
}
