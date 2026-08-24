#include <stdint.h>
#include <unity.h>

#include <flexseq/FactoryPatterns.h>
#include <flexseq/Pattern.h>
#include <flexseq/PatternBank.h>

using flexseq::FACTORY_PATTERN_COUNT;
using flexseq::Pattern;
using flexseq::PatternBank;
using flexseq::loadFactoryPatterns;

void setUp() {}
void tearDown() {}

namespace {

// The original firmware's eight factory patterns, read from Gravity.ino:83-90 and
// written here as LITERAL step lists. The production table packs the same steps
// into a bitmask; comparing against that table would move the expectation with
// the value, which is how a mutation went undetected on 2026-08-23.
struct Factory {
    const char* name;
    uint8_t count;
    uint8_t steps[16];
};

const Factory kExpected[FACTORY_PATTERN_COUNT] = {
    {"A1",  5, {0, 4, 8, 12, 15}},
    {"A2",  2, {4, 11}},
    {"A3",  5, {0, 3, 6, 9, 12}},
    {"A4",  8, {2, 3, 6, 7, 10, 11, 14, 15}},
    {"A5", 12, {1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15}},
    {"A6",  6, {2, 4, 6, 10, 12, 14}},
    {"A7", 14, {0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14}},
    {"A8", 10, {0, 1, 4, 5, 8, 9, 10, 12, 13, 15}},
};

bool expectedActive(const Factory& f, uint8_t step) {
    for (uint8_t i = 0; i < f.count; ++i) {
        if (f.steps[i] == step) {
            return true;
        }
    }
    return false;
}

void assertMatchesFactory(PatternBank& bank) {
    for (uint8_t index = 0; index < FACTORY_PATTERN_COUNT; ++index) {
        const Factory& f = kExpected[index];
        const Pattern* pattern = bank.getPattern(index);
        TEST_ASSERT_NOT_NULL(pattern);
        for (uint8_t step = 0; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
            bool active = false;
            TEST_ASSERT_TRUE(pattern->readStep(step, active));
            TEST_ASSERT_EQUAL_MESSAGE(expectedActive(f, step), active, f.name);
        }
    }
}

}  // namespace

void test_the_eight_factory_patterns_carry_the_originals_content() {
    PatternBank bank;
    loadFactoryPatterns(bank);
    assertMatchesFactory(bank);
}

// The original ships A1..A8 with content and B1..B8 empty (Gravity.ino:91-98).
void test_the_b_bank_stays_empty() {
    PatternBank bank;
    loadFactoryPatterns(bank);
    for (uint8_t index = FACTORY_PATTERN_COUNT; index < flexseq::PATTERN_COUNT; ++index) {
        const Pattern* pattern = bank.getPattern(index);
        TEST_ASSERT_NOT_NULL(pattern);
        for (uint8_t step = 0; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
            bool active = true;
            TEST_ASSERT_TRUE(pattern->readStep(step, active));
            TEST_ASSERT_FALSE(active);
        }
    }
}

// The original has no ratchets at all: they are a FlexSeq addition (PRD 6.3), so
// a factory pattern must not carry one.
void test_no_factory_pattern_carries_a_ratchet() {
    PatternBank bank;
    loadFactoryPatterns(bank);
    for (uint8_t index = 0; index < flexseq::PATTERN_COUNT; ++index) {
        const Pattern* pattern = bank.getPattern(index);
        for (uint8_t step = 0; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
            TEST_ASSERT_EQUAL_UINT8(0, pattern->getRatchet(step));
        }
    }
}

// Steps 16 and above do not exist in the original, so they must stay off. This
// assertion outlives lot A: at 32 steps the silence simply gets longer.
void test_the_steps_the_original_never_had_stay_off() {
    PatternBank bank;
    loadFactoryPatterns(bank);
    for (uint8_t index = 0; index < FACTORY_PATTERN_COUNT; ++index) {
        const Pattern* pattern = bank.getPattern(index);
        for (uint8_t step = 16; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
            bool active = true;
            TEST_ASSERT_TRUE(pattern->readStep(step, active));
            TEST_ASSERT_FALSE(active);
        }
    }
}

void test_loading_twice_changes_nothing() {
    PatternBank bank;
    loadFactoryPatterns(bank);
    loadFactoryPatterns(bank);
    assertMatchesFactory(bank);
}

// A reload must OVERWRITE, not merge. Without the clear() the edited step would
// survive and the factory content would no longer be a known state.
void test_a_reload_erases_an_edit() {
    PatternBank bank;
    loadFactoryPatterns(bank);
    Pattern* first = bank.getPattern(0);
    TEST_ASSERT_TRUE(first->writeStep(1, true));
    TEST_ASSERT_TRUE(first->setRatchet(0, flexseq::RATCHET_3));
    loadFactoryPatterns(bank);
    assertMatchesFactory(bank);
    TEST_ASSERT_EQUAL_UINT8(0, first->getRatchet(0));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_eight_factory_patterns_carry_the_originals_content);
    RUN_TEST(test_the_b_bank_stays_empty);
    RUN_TEST(test_no_factory_pattern_carries_a_ratchet);
    RUN_TEST(test_the_steps_the_original_never_had_stay_off);
    RUN_TEST(test_loading_twice_changes_nothing);
    RUN_TEST(test_a_reload_erases_an_edit);
    return UNITY_END();
}
