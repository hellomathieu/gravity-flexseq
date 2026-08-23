#include <flexseq/EncoderProbe.h>

#include <avr/pgmspace.h>

#include <flexseq/MainScreen.h>

namespace flexseq {
namespace probe {

namespace {

constexpr uint8_t SLOT_COUNT = 9;
constexpr uint8_t PASS_UNDER_8 = 0;
constexpr uint8_t PASS_UNDER_16 = 1;
constexpr uint8_t PASS_UNDER_32 = 2;
constexpr uint8_t PASS_OVER_32 = 3;
constexpr uint8_t PASS_MAX_MS = 4;
constexpr uint8_t CHANGE_1 = 5;
constexpr uint8_t CHANGE_2 = 6;
constexpr uint8_t CHANGE_3 = 7;
constexpr uint8_t CHANGE_4PLUS = 8;

uint16_t live[SLOT_COUNT] = {0};
uint16_t shown[SLOT_COUNT] = {0};

uint8_t page = 0;
uint32_t pageMs = 0;
bool changed = true;

void bump(uint8_t slot) {
    if (live[slot] < 0xFFFFu) {
        ++live[slot];
    }
}

void line(char* out, const char* text, uint16_t value) {
    uint8_t n = 0;
    char c;
    while ((c = static_cast<char>(pgm_read_byte(text + n))) != '\0') {
        out[n] = c;
        ++n;
    }
    const uint8_t written = detail::writeUnsigned(out + n, value);
    out[n + written] = '\0';
}

}  // namespace

void recordPass(uint32_t elapsedUs) {
    const uint16_t ms = static_cast<uint16_t>(elapsedUs / 1000u);
    if (ms > live[PASS_MAX_MS]) {
        live[PASS_MAX_MS] = ms;
    }
    if (elapsedUs < 8000u) {
        bump(PASS_UNDER_8);
    } else if (elapsedUs < 16000u) {
        bump(PASS_UNDER_16);
    } else if (elapsedUs < 32000u) {
        bump(PASS_UNDER_32);
    } else {
        bump(PASS_OVER_32);
    }
}

void recordChange(int16_t value) {
    if (value == 0) {
        return;
    }
    const uint16_t magnitude = value < 0 ? static_cast<uint16_t>(-value)
                                         : static_cast<uint16_t>(value);
    if (magnitude == 1) {
        bump(CHANGE_1);
    } else if (magnitude == 2) {
        bump(CHANGE_2);
    } else if (magnitude == 3) {
        bump(CHANGE_3);
    } else {
        bump(CHANGE_4PLUS);
    }
}

void advancePage(uint32_t nowMs) {
    if (nowMs - pageMs < PAGE_MS) {
        return;
    }
    pageMs = nowMs;
    page = static_cast<uint8_t>((page + 1) % PAGE_COUNT);
    changed = true;
    if (page == 0) {
        for (uint8_t slot = 0; slot < SLOT_COUNT; ++slot) {
            shown[slot] = live[slot];
            live[slot] = 0;
        }
    }
}

bool pageChanged() {
    const bool result = changed;
    changed = false;
    return result;
}

void writeReport(char* title, char* footer) {
    switch (page) {
        case 0:
            line(title, PSTR("P<8 "), shown[PASS_UNDER_8]);
            line(footer, PSTR("P<16 "), shown[PASS_UNDER_16]);
            break;
        case 1:
            line(title, PSTR("P<32 "), shown[PASS_UNDER_32]);
            line(footer, PSTR("P>=32 "), shown[PASS_OVER_32]);
            break;
        case 2:
            line(title, PSTR("PMAXms "), shown[PASS_MAX_MS]);
            line(footer, PSTR("C1 "), shown[CHANGE_1]);
            break;
        case 3:
            line(title, PSTR("C2 "), shown[CHANGE_2]);
            line(footer, PSTR("C3 "), shown[CHANGE_3]);
            break;
        default:
            line(title, PSTR("C4+ "), shown[CHANGE_4PLUS]);
            line(footer, PSTR("WINs "), PAGE_COUNT * (PAGE_MS / 1000u));
            break;
    }
}

}  // namespace probe
}  // namespace flexseq
