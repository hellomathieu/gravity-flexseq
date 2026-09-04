#ifndef FLEXSEQ_MAIN_SCREEN_H
#define FLEXSEQ_MAIN_SCREEN_H

#include <stdint.h>
#include <string.h>

#if defined(__AVR__)
#include <avr/pgmspace.h>
#endif

#include <flexseq/ChannelMode.h>
#include <flexseq/MainScreenModel.h>
#include <flexseq/OriginalFonts.h>
#include <flexseq/Subdiv.h>
#include <flexseq/PatternScreen.h>

namespace flexseq {

namespace mainscreen {

constexpr uint8_t TAB_COUNT = 8;
constexpr uint8_t TAB_SLOT_W = screen::WIDTH / TAB_COUNT;
constexpr uint8_t TAB_BASELINE_Y = screen::HEIGHT - 1;
constexpr uint8_t TAB_TOP_Y =
    TAB_BASELINE_Y - (FONT_VELVETSCREEN_HEIGHT - 1);
constexpr uint8_t TAB_BOX_H_ = 8;
constexpr uint8_t TAB_BOX_Y = screen::HEIGHT - TAB_BOX_H_;
constexpr uint8_t TAB_BOX_H = TAB_BOX_H_;
constexpr uint8_t RULE_Y = 52;
constexpr uint8_t RULE_X = 4;
constexpr uint8_t RULE_W = 120;

constexpr uint8_t HEADLINE_BOX_Y = 1;
constexpr uint8_t HEADLINE_BOX_H = 10;
constexpr uint8_t HEADLINE_BASELINE_Y = 8;
constexpr uint8_t HEADLINE_BOX_X = 2;
constexpr uint8_t HEADLINE_BOX_W = screen::WIDTH - 2 * HEADLINE_BOX_X;

constexpr uint8_t ROW_A_BOX_Y = 14;
constexpr uint8_t ROW_B_BOX_Y = 22;

constexpr uint8_t ROW_BOX_H = 8;
constexpr uint8_t ROW_A_BASELINE_Y = ROW_A_BOX_Y + 7;
constexpr uint8_t ROW_B_BASELINE_Y = ROW_B_BOX_Y + 7;

constexpr uint8_t COL_LEFT_X = 2;
constexpr uint8_t COL_RIGHT_X = 66;
constexpr uint8_t COL_W = 60;
constexpr uint8_t TEXT_INSET = 2;

constexpr uint8_t GLYPH_SIZE = 7;

constexpr uint8_t LINE_LABEL_X = 62;
constexpr uint8_t LINE_VALUE_X = 99;
constexpr uint8_t LINE_0_BASELINE_Y = 8;
constexpr uint8_t LINE_1_BASELINE_Y = 19;
constexpr uint8_t LINE_2_BASELINE_Y = 30;
constexpr uint8_t LINE_SPACING_Y = 11;

constexpr uint8_t MAIN_CENTRE_X = 29;
constexpr uint8_t MAIN_BOX_W = 55;
constexpr uint8_t MAIN_VALUE_BASELINE_Y = 28;
constexpr uint8_t MAIN_LABEL_BASELINE_Y = 41;

static_assert(LINE_1_BASELINE_Y == LINE_0_BASELINE_Y + LINE_SPACING_Y,
              "the three lines of the original are evenly spaced");
static_assert(LINE_2_BASELINE_Y == LINE_0_BASELINE_Y + 2 * LINE_SPACING_Y,
              "the three lines of the original are evenly spaced");
static_assert(LINE_LABEL_X < LINE_VALUE_X,
              "the label column comes before the value column");
static_assert(LINE_0_BASELINE_Y < LINE_1_BASELINE_Y,
              "the three lines go down the screen");
static_assert(LINE_1_BASELINE_Y < LINE_2_BASELINE_Y,
              "the three lines go down the screen");
static_assert(LINE_1_BASELINE_Y - LINE_0_BASELINE_Y >= FONT_VELVETSCREEN_HEIGHT,
              "two lines of the column must not overlap");
static_assert(LINE_2_BASELINE_Y - LINE_1_BASELINE_Y >= FONT_VELVETSCREEN_HEIGHT,
              "two lines of the column must not overlap");
static_assert(LINE_2_BASELINE_Y < RULE_Y,
              "the third line must clear the rule");
static_assert(MAIN_CENTRE_X + MAIN_BOX_W / 2 < LINE_LABEL_X,
              "the main parameter must clear the column of fields");
static_assert(MAIN_VALUE_BASELINE_Y >= FONT_STK_L_HEIGHT - 1,
              "the large value must fit above its baseline");
static_assert(MAIN_LABEL_BASELINE_Y - MAIN_VALUE_BASELINE_Y >= FONT_VELVETSCREEN_HEIGHT,
              "the label must clear the large value");
static_assert(MAIN_LABEL_BASELINE_Y < RULE_Y,
              "the label of the main parameter must clear the rule");

static_assert(ROW_B_BASELINE_Y < RULE_Y, "the second field row must clear the rule");
static_assert(RULE_Y < TAB_BOX_Y, "the rule must clear the tab bar");
static_assert(HEADLINE_BOX_Y + HEADLINE_BOX_H <= ROW_A_BOX_Y,
              "the headline must clear the first field row");
static_assert(ROW_B_BOX_Y + ROW_BOX_H < RULE_Y,
              "the space below the second row is reserved for the CV fields of PRD 10.2");
static_assert(ROW_A_BOX_Y + ROW_BOX_H <= ROW_B_BOX_Y,
              "the two field rows must not overlap");
static_assert(COL_LEFT_X + COL_W <= COL_RIGHT_X,
              "the two field columns must not overlap");
static_assert(TAB_TOP_Y / 8 == TAB_BASELINE_Y / 8,
              "the tab bar must fit in a single 8-pixel band");
static_assert(TAB_BOX_Y + TAB_BOX_H == screen::HEIGHT,
              "the tab highlight must end exactly at the last row");
static_assert(TAB_BOX_Y / 8 == TAB_BASELINE_Y / 8,
              "the tab highlight must not spill into the band above");

inline uint8_t tabSlotX(uint8_t tab) {
    return static_cast<uint8_t>(tab * TAB_SLOT_W);
}

inline uint8_t tabCentreX(uint8_t tab) {
    return static_cast<uint8_t>(tabSlotX(tab) + TAB_SLOT_W / 2);
}

}  // namespace mainscreen


namespace detail {

inline uint8_t writeUnsigned(char* out, uint16_t value) {
    constexpr uint8_t MAX_DIGITS = 5;
    char digits[MAX_DIGITS];
    uint8_t n = 0;
    do {
        digits[n++] = static_cast<char>('0' + value % 10u);
        value = static_cast<uint16_t>(value / 10u);
    } while (value != 0 && n < MAX_DIGITS);
    for (uint8_t i = 0; i < n; ++i) {
        out[i] = digits[n - 1 - i];
    }
    out[n] = '\0';
    return n;
}

inline void patternName(int8_t index, char* out) {
    if (index < 0) {
        out[0] = '-';
        out[1] = '-';
        out[2] = '\0';
        return;
    }
    out[0] = index < 8 ? 'A' : 'B';
    out[1] = static_cast<char>('1' + (index % 8));
    out[2] = '\0';
}

inline void subdivLabel(int16_t subdiv, char* out) {
    if (subdiv == 0) {
        out[0] = '?';
        out[1] = '\0';
        return;
    }
    const bool multiply = subdiv < 0;
    out[0] = multiply ? 'x' : '/';
    writeUnsigned(out + 1, static_cast<uint16_t>(multiply ? -subdiv : subdiv));
}

inline void barLabel(uint8_t steps, char* out) {
    if (steps == 0) {
        out[0] = '-';
        out[1] = '\0';
        return;
    }
    writeUnsigned(out, steps);
}

inline const char* sourceLabel(uint8_t source) {
    if (source == 0) return "INT";
    if (source == 1) return "EXT24";
    if (source == 2) return "EXT4";
    if (source == 3) return "EXT2";
    if (source == 4) return "EXT1";
    return "MIDI";
}

inline void headlineOf(const MainScreenModel& model, char* out) {
    if (model.tab == 0) {
        writeUnsigned(out, model.tempo);
    } else if (model.tab == mainscreen::TAB_COUNT - 1) {
        out[0] = '\0';
    } else {
        patternName(model.patternIndex, out);
    }
}

template <typename Canvas>
void drawClockGlyph(Canvas& canvas, uint8_t cx, uint8_t cy) {
    const uint8_t x = static_cast<uint8_t>(cx - 3);
    const uint8_t y = static_cast<uint8_t>(cy - 3);
    canvas.drawFrame(x, y, mainscreen::GLYPH_SIZE, mainscreen::GLYPH_SIZE);
    canvas.drawHLine(static_cast<uint8_t>(x + 4), static_cast<uint8_t>(y + 2), 2);
    canvas.drawHLine(static_cast<uint8_t>(x + 4), static_cast<uint8_t>(y + 3), 2);
    canvas.drawHLine(static_cast<uint8_t>(x + 3), static_cast<uint8_t>(y + 2), 1);
}

template <typename Canvas>
void drawSettingsGlyph(Canvas& canvas, uint8_t cx, uint8_t cy) {
    canvas.drawBox(static_cast<uint8_t>(cx - 2), static_cast<uint8_t>(cy - 2), 5, 5);
}

#if defined(__AVR__)
#define FLEXSEQ_LABEL(name, text) static const char name[] PROGMEM = text
inline const char* label(const char* flash, char* scratch) {
    strcpy_P(scratch, flash);
    return scratch;
}
inline void copyLabel(const char* flash, char* dest) { strcpy_P(dest, flash); }
#else
#define FLEXSEQ_LABEL(name, text) static const char name[] = text
inline const char* label(const char* flash, char*) { return flash; }
inline void copyLabel(const char* flash, char* dest) { strcpy(dest, flash); }
#endif

FLEXSEQ_LABEL(LBL_MODE, "MODE:");
FLEXSEQ_LABEL(LBL_OFFSET, "OFFSET:");
FLEXSEQ_LABEL(LBL_SUBDIV_FIELD, "SUBDIV:");
FLEXSEQ_LABEL(LBL_MOD, "MOD:");
FLEXSEQ_LABEL(LBL_OFF, "OFF");
FLEXSEQ_LABEL(LBL_CLOCK, "CLOCK");
FLEXSEQ_LABEL(LBL_RAND, "RAND");
FLEXSEQ_LABEL(LBL_SEQ, "SEQ");
FLEXSEQ_LABEL(LBL_SUBDIVISION, "SUBDIVISION");
FLEXSEQ_LABEL(LBL_SKIP_CHANCE, "SKIP CHANCE");

inline const char* modeText(uint8_t mode) {
    switch (static_cast<ChannelMode>(mode)) {
        case MODE_CLOCK:  return LBL_CLOCK;
        case MODE_RANDOM: return LBL_RAND;
        default:          return LBL_SEQ;
    }
}

inline void skipText(uint8_t skipChance, char* out) {
    const uint8_t n = writeUnsigned(out, skipChance);
    out[n] = '0';
    out[n + 1] = '%';
    out[n + 2] = '\0';
}

inline void mainValueOf(const MainScreenModel& model, char* out) {
    if (model.mainParameter == MAIN_NONE) {
        out[0] = '\0';
        return;
    }
    if (model.mainParameter == MAIN_SKIP_CHANCE) {
        skipText(model.skipChance, out);
        return;
    }
    subdivLabel(model.subdiv, out);
}

inline const char* mainLabelOf(const MainScreenModel& model) {
    return model.mainParameter == MAIN_SKIP_CHANCE ? LBL_SKIP_CHANCE : LBL_SUBDIVISION;
}

inline void legacyLine(const MainScreenModel& model, uint8_t index,
                       const char** out, char* value) {
    if (index == 0) {
        *out = LBL_MODE;
        copyLabel(modeText(model.mode), value);
        return;
    }
    if (index == 1) {
        if (model.mode == static_cast<uint8_t>(MODE_CLOCK)) {
            *out = LBL_OFFSET;
            const uint8_t n = writeUnsigned(value, model.offset);
            value[n] = '/';
            writeUnsigned(value + n + 1, model.stepTicks);
            return;
        }
        *out = LBL_SUBDIV_FIELD;
        subdivLabel(model.subdiv, value);
        return;
    }
    *out = LBL_MOD;
    copyLabel(LBL_OFF, value);
}

template <typename Canvas>
void drawLabelledField(Canvas& canvas, const Band& band, uint8_t x, uint8_t y,
                       const char* label, const char* value, bool framed, bool inverted) {
    if (!touches(band, y, static_cast<int16_t>(y + mainscreen::ROW_BOX_H - 1))) {
        return;
    }
    if (inverted) {
        canvas.drawBox(x, y, mainscreen::COL_W, mainscreen::ROW_BOX_H);
        canvas.setDrawColor(0);
    } else if (framed) {
        canvas.drawFrame(x, y, mainscreen::COL_W, mainscreen::ROW_BOX_H);
    }
    const uint8_t textX = static_cast<uint8_t>(x + mainscreen::TEXT_INSET);
    const uint8_t baseline = static_cast<uint8_t>(y + 7);
    const uint8_t used = canvas.drawStr(textX, baseline, label);
    if (value != nullptr) {
        canvas.drawStr(static_cast<uint8_t>(textX + used + 4), baseline, value);
    }
    if (inverted) {
        canvas.setDrawColor(1);
    }
}

}  // namespace detail

namespace detail {

template <typename Canvas>
void drawLegacyChannel(Canvas& canvas, const Band& band, const MainScreenModel& model) {
    namespace ms = mainscreen;
    char value[10];

    const int16_t valueTop =
        static_cast<int16_t>(ms::MAIN_VALUE_BASELINE_Y) - (FONT_STK_L_HEIGHT - 1);
    if (touches(band, valueTop, ms::MAIN_VALUE_BASELINE_Y)) {
        mainValueOf(model, value);
        if (value[0] != '\0') {
            canvas.setFont(FONT_STK_L);
            const uint8_t w = canvas.getStrWidth(value);
            canvas.drawStr(static_cast<uint8_t>(ms::MAIN_CENTRE_X - w / 2),
                           ms::MAIN_VALUE_BASELINE_Y, value);
            canvas.setFont(FONT_VELVETSCREEN);
        }
    }

    const int16_t labelTop =
        static_cast<int16_t>(ms::MAIN_LABEL_BASELINE_Y) - (FONT_VELVETSCREEN_HEIGHT - 1);
    if (touches(band, labelTop, ms::MAIN_LABEL_BASELINE_Y)) {
        canvas.setFont(FONT_VELVETSCREEN);
        char scratch[14];
        const char* text = label(mainLabelOf(model), scratch);
        const uint8_t w = canvas.getStrWidth(text);
        canvas.drawStr(static_cast<uint8_t>(ms::MAIN_CENTRE_X - w / 2),
                       ms::MAIN_LABEL_BASELINE_Y, text);
    }

    for (uint8_t line = 0; line < 3; ++line) {
        const uint8_t base = static_cast<uint8_t>(
            ms::LINE_0_BASELINE_Y + line * ms::LINE_SPACING_Y);
        const int16_t top =
            static_cast<int16_t>(base) - (FONT_VELVETSCREEN_HEIGHT + 1);
        if (!touches(band, top, static_cast<int16_t>(base) + 1)) {
            continue;
        }
        canvas.setFont(FONT_VELVETSCREEN);
        const char* flashLabel = nullptr;
        legacyLine(model, line, &flashLabel, value);
        char scratch[10];
        const char* text = label(flashLabel, scratch);
        const bool onCursor = model.insideTab && model.cursor == line;

        const uint8_t labelW = canvas.getStrWidth(text);
        if (onCursor && !model.fieldOpen) {
            canvas.drawBox(static_cast<uint8_t>(ms::LINE_LABEL_X - 1),
                           static_cast<uint8_t>(base - FONT_VELVETSCREEN_HEIGHT - 1),
                           static_cast<uint8_t>(labelW + 2),
                           static_cast<uint8_t>(FONT_VELVETSCREEN_HEIGHT + 2));
            canvas.setDrawColor(0);
            canvas.drawStr(ms::LINE_LABEL_X, base, text);
            canvas.setDrawColor(1);
        } else {
            canvas.drawStr(ms::LINE_LABEL_X, base, text);
        }

        if (value[0] != '\0') {
            if (onCursor && model.fieldOpen) {
                const uint8_t w = canvas.getStrWidth(value);
                canvas.drawFrame(static_cast<uint8_t>(ms::LINE_VALUE_X - 2),
                                 static_cast<uint8_t>(base - FONT_VELVETSCREEN_HEIGHT - 2),
                                 static_cast<uint8_t>(w + 4),
                                 static_cast<uint8_t>(FONT_VELVETSCREEN_HEIGHT + 4));
            }
            canvas.drawStr(ms::LINE_VALUE_X, base, value);
        }
    }
}

}  // namespace detail

template <typename Canvas>
void drawMainScreen(Canvas& canvas, const MainScreenModel& model,
                    Band band = Band{0, screen::HEIGHT - 1}) {
    namespace ms = mainscreen;

    const bool legacy = model.legacyLayout && model.tab != 0
                        && model.tab != ms::TAB_COUNT - 1;

    const bool cursorOnHeadline = model.insideTab && model.cursor == 0;
    if (!legacy
        && touches(band, ms::HEADLINE_BOX_Y, ms::HEADLINE_BOX_Y + ms::HEADLINE_BOX_H - 1)) {
        char headline[6];
        detail::headlineOf(model, headline);
        if (headline[0] != '\0') {
            const uint8_t w = model.headlineWidth != 0
                                  ? model.headlineWidth
                                  : static_cast<uint8_t>(canvas.getStrWidth(headline));
            canvas.drawStr(static_cast<uint8_t>((screen::WIDTH - w) / 2),
                           ms::HEADLINE_BASELINE_Y, headline);
        }
        if (cursorOnHeadline) {
            if (model.fieldOpen) {
                canvas.drawFrame(ms::HEADLINE_BOX_X, ms::HEADLINE_BOX_Y,
                                 ms::HEADLINE_BOX_W, ms::HEADLINE_BOX_H);
                canvas.drawFrame(static_cast<uint8_t>(ms::HEADLINE_BOX_X + 1),
                                 static_cast<uint8_t>(ms::HEADLINE_BOX_Y + 1),
                                 static_cast<uint8_t>(ms::HEADLINE_BOX_W - 2),
                                 static_cast<uint8_t>(ms::HEADLINE_BOX_H - 2));
            } else {
                canvas.drawFrame(ms::HEADLINE_BOX_X, ms::HEADLINE_BOX_Y,
                                 ms::HEADLINE_BOX_W, ms::HEADLINE_BOX_H);
            }
        }
    }

    if (model.tab == 0) {
        detail::drawLabelledField(canvas, band, ms::COL_LEFT_X, ms::ROW_A_BOX_Y,
                                  "SRC", detail::sourceLabel(model.clockSource),
                                  model.insideTab && model.cursor == 1,
                                  model.insideTab && model.cursor == 1 && model.fieldOpen);
    } else if (legacy) {
        detail::drawLegacyChannel(canvas, band, model);
    } else if (model.tab != ms::TAB_COUNT - 1) {
        char lengthText[4];
        detail::writeUnsigned(lengthText, model.length);
        char subdivText[6];
        detail::subdivLabel(model.subdiv, subdivText);
        char barText[4];
        detail::barLabel(model.barLength, barText);

        detail::drawLabelledField(canvas, band, ms::COL_LEFT_X, ms::ROW_A_BOX_Y,
                                  "LEN", lengthText,
                                  model.insideTab && model.cursor == 1,
                                  model.insideTab && model.cursor == 1 && model.fieldOpen);
        detail::drawLabelledField(canvas, band, ms::COL_RIGHT_X, ms::ROW_A_BOX_Y,
                                  "SUB", subdivText,
                                  model.insideTab && model.cursor == 2,
                                  model.insideTab && model.cursor == 2 && model.fieldOpen);
        detail::drawLabelledField(canvas, band, ms::COL_LEFT_X, ms::ROW_B_BOX_Y,
                                  "SEP", barText,
                                  model.insideTab && model.cursor == 3,
                                  model.insideTab && model.cursor == 3 && model.fieldOpen);
        detail::drawLabelledField(canvas, band, ms::COL_RIGHT_X, ms::ROW_B_BOX_Y,
                                  "EDIT", nullptr,
                                  model.insideTab && model.cursor == 4,
                                  false);
    }

    if (touches(band, ms::RULE_Y, ms::RULE_Y)) {
        canvas.drawHLine(ms::RULE_X, ms::RULE_Y, ms::RULE_W);
    }

    if (touches(band, ms::TAB_BOX_Y, ms::TAB_BOX_Y + ms::TAB_BOX_H - 1)) {
        for (uint8_t tab = 0; tab < ms::TAB_COUNT; ++tab) {
            const bool selected = (tab == model.tab);
            if (selected) {
                canvas.drawBox(ms::tabSlotX(tab), ms::TAB_BOX_Y, ms::TAB_SLOT_W,
                               ms::TAB_BOX_H);
                canvas.setDrawColor(0);
            }
            const uint8_t cx = ms::tabCentreX(tab);
            if (tab == 0) {
                detail::drawClockGlyph(canvas, cx, static_cast<uint8_t>(ms::TAB_TOP_Y + 3));
            } else if (tab == ms::TAB_COUNT - 1) {
                detail::drawSettingsGlyph(canvas, cx,
                                          static_cast<uint8_t>(ms::TAB_TOP_Y + 3));
            } else {
                char digit[2];
                digit[0] = static_cast<char>('0' + tab);
                digit[1] = '\0';
                canvas.drawStr(static_cast<uint8_t>(cx - 2), ms::TAB_BASELINE_Y, digit);
            }
            if (selected) {
                canvas.setDrawColor(1);
            }
        }
    }
}

}  // namespace flexseq

#endif // FLEXSEQ_MAIN_SCREEN_H
