#ifndef FLEXSEQ_ORIGINAL_FONTS_H
#define FLEXSEQ_ORIGINAL_FONTS_H

#include <stdint.h>

#if defined(__AVR__)
#define FLEXSEQ_FONT_SECTION __attribute__((section(".progmem.font")))
#else
#define FLEXSEQ_FONT_SECTION
#endif

namespace flexseq {

extern const uint8_t FONT_STK_L[];
extern const uint8_t FONT_VELVETSCREEN[];

constexpr uint16_t FONT_STK_L_BYTES = 569;
constexpr uint8_t FONT_STK_L_GLYPHS = 21;
constexpr uint8_t FONT_STK_L_MAX_WIDTH = 15;
constexpr uint8_t FONT_STK_L_HEIGHT = 23;

constexpr uint16_t FONT_VELVETSCREEN_BYTES = 437;
constexpr uint8_t FONT_VELVETSCREEN_GLYPHS = 52;
constexpr uint8_t FONT_VELVETSCREEN_MAX_WIDTH = 5;
constexpr uint8_t FONT_VELVETSCREEN_HEIGHT = 5;

constexpr uint8_t FONT_HEADER_GLYPH_COUNT_AT = 0;
constexpr uint8_t FONT_HEADER_MAX_WIDTH_AT = 9;
constexpr uint8_t FONT_HEADER_MAX_HEIGHT_AT = 10;
constexpr uint8_t FONT_HEADER_ASCENT_A_AT = 13;

}  // namespace flexseq

#endif // FLEXSEQ_ORIGINAL_FONTS_H
