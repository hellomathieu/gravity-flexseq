#include <unity.h>

#include <flexseq/OriginalFonts.h>

void setUp() {}
void tearDown() {}

// La police vient du firmware d'origine, GPLv3, et elle est copiee OCTET POUR
// OCTET. Ces assertions gardent ses octets : un caractere perdu dans le
// litteral ne se verrait pas autrement, et u8g2 dessinerait n'importe quoi.
// La provenance et l'attribution vivent dans NOTICE.

void test_the_font_holds_the_bytes_of_the_original() {
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(569, flexseq::FONT_STK_L_BYTES,
        "the font of the original holds 569 bytes, terminator included");
}

void test_the_header_of_the_font_names_twenty_one_glyphs() {
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        21, flexseq::FONT_STK_L[flexseq::FONT_HEADER_GLYPH_COUNT_AT],
        "the ten digits, then / x and %, then A and B, then E X T M I D:"
        " the clock tab writes EXT and MIDI large");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(21, flexseq::FONT_STK_L_GLYPHS,
        "the declared glyph count must equal the one the bytes carry");
}

void test_the_glyphs_are_fifteen_wide_at_most() {
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        15, flexseq::FONT_STK_L[flexseq::FONT_HEADER_MAX_WIDTH_AT],
        "the main parameter box holds 55 px, and /128 is four characters");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(15, flexseq::FONT_STK_L_MAX_WIDTH,
        "the declared width must equal the one the bytes carry");
}

void test_the_glyphs_are_twenty_three_tall() {
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        23, flexseq::FONT_STK_L[flexseq::FONT_HEADER_MAX_HEIGHT_AT],
        "23 px, and this is the rule of the original: PRD 12.1 planned 18");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        23, flexseq::FONT_STK_L[flexseq::FONT_HEADER_ASCENT_A_AT],
        "a capital reaches the full height of the font");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(23, flexseq::FONT_STK_L_HEIGHT,
        "the declared height must equal the one the bytes carry");
}

void test_the_font_ends_on_the_terminator_u8g2_expects() {
    // u8g2 walks the glyph list until it reads a zero byte.
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        0, flexseq::FONT_STK_L[flexseq::FONT_STK_L_BYTES - 1],
        "the last byte must be the terminator, or u8g2 walks past the array");
}

void test_the_label_font_holds_the_bytes_of_the_original() {
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(437, flexseq::FONT_VELVETSCREEN_BYTES,
        "the label font of the original holds 437 bytes, terminator included");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        0, flexseq::FONT_VELVETSCREEN[flexseq::FONT_VELVETSCREEN_BYTES - 1],
        "the last byte must be the terminator, or u8g2 walks past the array");
}

void test_the_label_font_names_fifty_two_glyphs() {
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        52, flexseq::FONT_VELVETSCREEN[flexseq::FONT_HEADER_GLYPH_COUNT_AT],
        "space, the punctuation, the ten digits, A to Z, and the six glyphs of"
        " the sequencer: p q r t w x");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(52, flexseq::FONT_VELVETSCREEN_GLYPHS,
        "the declared glyph count must equal the one the bytes carry");
}

void test_the_label_font_is_five_by_five() {
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        5, flexseq::FONT_VELVETSCREEN[flexseq::FONT_HEADER_MAX_WIDTH_AT],
        "five pixels wide at most, and the advance is proportional");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        5, flexseq::FONT_VELVETSCREEN[flexseq::FONT_HEADER_MAX_HEIGHT_AT],
        "five pixels tall: two less than the 5x7 of u8g2 that FlexSeq uses today");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(5, flexseq::FONT_VELVETSCREEN_MAX_WIDTH, "declared width");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(5, flexseq::FONT_VELVETSCREEN_HEIGHT, "declared height");
}

void test_the_two_fonts_are_not_the_same_bytes() {
    // Une copie collee deux fois passerait toutes les autres assertions.
    TEST_ASSERT_NOT_EQUAL_MESSAGE(
        flexseq::FONT_STK_L[flexseq::FONT_HEADER_MAX_HEIGHT_AT],
        flexseq::FONT_VELVETSCREEN[flexseq::FONT_HEADER_MAX_HEIGHT_AT],
        "23 px for the main parameter, 5 px for the labels");
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_the_label_font_holds_the_bytes_of_the_original);
    RUN_TEST(test_the_label_font_names_fifty_two_glyphs);
    RUN_TEST(test_the_label_font_is_five_by_five);
    RUN_TEST(test_the_two_fonts_are_not_the_same_bytes);
    RUN_TEST(test_the_font_holds_the_bytes_of_the_original);
    RUN_TEST(test_the_header_of_the_font_names_twenty_one_glyphs);
    RUN_TEST(test_the_glyphs_are_fifteen_wide_at_most);
    RUN_TEST(test_the_glyphs_are_twenty_three_tall);
    RUN_TEST(test_the_font_ends_on_the_terminator_u8g2_expects);
    return UNITY_END();
}
