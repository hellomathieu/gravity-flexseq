#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "../../src/hal/EncoderProbe.cpp"

using namespace flexseq::probe;

namespace {

char title[16];

void resetProbe() {
    for (uint8_t slot = 0; slot < SLOT_COUNT; ++slot) {
        live[slot] = 0;
        shown[slot] = 0;
    }
    page = 0;
    pageMs = 0;
    changed = true;
}

// Remplit les neuf compteurs avec des valeurs toutes distinctes, puis fait
// tourner un cycle complet pour que la capture les publie.
void fillAndSnapshot() {
    resetProbe();
    for (uint8_t i = 0; i < 3; ++i)  recordPass(1000u);
    for (uint8_t i = 0; i < 5; ++i)  recordPass(9000u);
    for (uint8_t i = 0; i < 7; ++i)  recordPass(20000u);
    for (uint8_t i = 0; i < 2; ++i)  recordPass(40000u);
    for (uint8_t i = 0; i < 11; ++i) recordChange(1);
    for (uint8_t i = 0; i < 12; ++i) recordChange(-2);
    for (uint8_t i = 0; i < 13; ++i) recordChange(3);
    for (uint8_t i = 0; i < 14; ++i) recordChange(-5);
    for (uint8_t i = 1; i <= 10; ++i) {
        advancePage(static_cast<uint32_t>(i) * 2000u);
    }
}

const char* reportAtPage(uint8_t wanted) {
    page = wanted;
    memset(title, 0, sizeof(title));
    writeReport(title);
    return title;
}

}  // namespace

void test_the_cycle_holds_ten_pages(void) {
    TEST_ASSERT_EQUAL_UINT8(10, PAGE_COUNT);
}

void test_a_page_lasts_two_seconds(void) {
    TEST_ASSERT_EQUAL_UINT16(2000, PAGE_MS);
}

void test_the_window_is_twenty_seconds(void) {
    fillAndSnapshot();
    TEST_ASSERT_EQUAL_STRING("WINs 20", reportAtPage(9));
}

void test_each_page_shows_its_own_counter(void) {
    fillAndSnapshot();
    TEST_ASSERT_EQUAL_STRING("P<8 3",     reportAtPage(0));
    TEST_ASSERT_EQUAL_STRING("P<16 5",    reportAtPage(1));
    TEST_ASSERT_EQUAL_STRING("P<32 7",    reportAtPage(2));
    TEST_ASSERT_EQUAL_STRING("P>=32 2",   reportAtPage(3));
    TEST_ASSERT_EQUAL_STRING("PMAXms 40", reportAtPage(4));
    TEST_ASSERT_EQUAL_STRING("C1 11",     reportAtPage(5));
    TEST_ASSERT_EQUAL_STRING("C2 12",     reportAtPage(6));
    TEST_ASSERT_EQUAL_STRING("C3 13",     reportAtPage(7));
    TEST_ASSERT_EQUAL_STRING("C4+ 14",    reportAtPage(8));
}

// Le decoupage a deux lignes par page devenait un decoupage a une ligne : le
// defaut qu'il fallait attraper est un compteur perdu ou publie deux fois.
void test_the_nine_counters_appear_once_each(void) {
    fillAndSnapshot();
    uint8_t seen[SLOT_COUNT] = {0};
    const uint16_t expected[SLOT_COUNT] = {3, 5, 7, 2, 40, 11, 12, 13, 14};
    for (uint8_t p = 0; p < 10; ++p) {
        const char* text = reportAtPage(p);
        const char* digits = strrchr(text, ' ');
        TEST_ASSERT_NOT_NULL(digits);
        const uint16_t value = static_cast<uint16_t>(atoi(digits + 1));
        for (uint8_t slot = 0; slot < SLOT_COUNT; ++slot) {
            if (value == expected[slot]) {
                ++seen[slot];
            }
        }
    }
    for (uint8_t slot = 0; slot < SLOT_COUNT; ++slot) {
        TEST_ASSERT_EQUAL_UINT8(1, seen[slot]);
    }
}

// Chaque page porte une etiquette differente : deux pages identiques
// masqueraient un compteur sans changer aucun total.
void test_the_ten_labels_are_all_distinct(void) {
    fillAndSnapshot();
    char labels[10][16];
    for (uint8_t p = 0; p < 10; ++p) {
        const char* text = reportAtPage(p);
        const char* space = strrchr(text, ' ');
        const size_t n = static_cast<size_t>(space - text);
        memcpy(labels[p], text, n);
        labels[p][n] = '\0';
    }
    for (uint8_t a = 0; a < 10; ++a) {
        for (uint8_t b = static_cast<uint8_t>(a + 1); b < 10; ++b) {
            TEST_ASSERT_TRUE(strcmp(labels[a], labels[b]) != 0);
        }
    }
}

// La capture n'a lieu qu'au retour sur la page 0, donc apres dix avances.
void test_the_snapshot_lands_after_ten_advances(void) {
    resetProbe();
    for (uint8_t i = 0; i < 4; ++i) {
        recordPass(1000u);
    }
    for (uint8_t i = 1; i <= 9; ++i) {
        advancePage(static_cast<uint32_t>(i) * 2000u);
    }
    TEST_ASSERT_EQUAL_UINT16(0, shown[PASS_UNDER_8]);
    TEST_ASSERT_EQUAL_UINT16(4, live[PASS_UNDER_8]);
    advancePage(20000u);
    TEST_ASSERT_EQUAL_UINT16(4, shown[PASS_UNDER_8]);
    TEST_ASSERT_EQUAL_UINT16(0, live[PASS_UNDER_8]);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_cycle_holds_ten_pages);
    RUN_TEST(test_a_page_lasts_two_seconds);
    RUN_TEST(test_the_window_is_twenty_seconds);
    RUN_TEST(test_each_page_shows_its_own_counter);
    RUN_TEST(test_the_nine_counters_appear_once_each);
    RUN_TEST(test_the_ten_labels_are_all_distinct);
    RUN_TEST(test_the_snapshot_lands_after_ten_advances);
    return UNITY_END();
}
