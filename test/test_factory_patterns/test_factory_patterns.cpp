#include <stdint.h>
#include <unity.h>

#include <flexseq/FactoryPatterns.h>
#include <flexseq/Pattern.h>
#include <flexseq/PatternBank.h>
#include <flexseq/Persistence.h>

using flexseq::FACTORY_MASK_BYTES;
using flexseq::FACTORY_PATTERN_COUNT;
using flexseq::FACTORY_STEP_COUNT;
using flexseq::factoryStepMask;
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

uint16_t expectedMask(uint8_t index) {
    if (index >= FACTORY_PATTERN_COUNT) {
        return 0;
    }
    const Factory& f = kExpected[index];
    uint16_t mask = 0;
    for (uint8_t i = 0; i < f.count; ++i) {
        mask = static_cast<uint16_t>(mask | (1u << f.steps[i]));
    }
    return mask;
}

uint8_t expectedRecordByte(uint8_t index, uint8_t offset) {
    if (index >= flexseq::PATTERN_COUNT || offset >= 24) {
        return 0;
    }
    if (offset == 23) {
        return 16;
    }
    if (offset >= 2) {
        return 0;
    }
    const uint16_t mask = expectedMask(index);
    return static_cast<uint8_t>((mask >> (offset * 8)) & 0xFF);
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

void test_the_factory_mask_reproduces_the_originals_steps() {
    for (uint8_t index = 0; index < FACTORY_PATTERN_COUNT; ++index) {
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(expectedMask(index), factoryStepMask(index),
                                         kExpected[index].name);
    }
}

void test_the_b_slots_have_no_factory_mask() {
    for (uint8_t index = FACTORY_PATTERN_COUNT; index < flexseq::PATTERN_COUNT; ++index) {
        TEST_ASSERT_EQUAL_UINT16(0, factoryStepMask(index));
    }
}

void test_the_factory_mask_refuses_an_index_out_of_range() {
    TEST_ASSERT_EQUAL_UINT16(0, factoryStepMask(16));
    TEST_ASSERT_EQUAL_UINT16(0, factoryStepMask(64));
    TEST_ASSERT_EQUAL_UINT16(0, factoryStepMask(255));
}

void test_the_factory_constants_hold_their_values() {
    TEST_ASSERT_EQUAL_UINT8(8, FACTORY_PATTERN_COUNT);
    TEST_ASSERT_EQUAL_UINT8(16, FACTORY_STEP_COUNT);
    TEST_ASSERT_EQUAL_UINT8(2, FACTORY_MASK_BYTES);
    TEST_ASSERT_EQUAL_UINT8(16, flexseq::persist::v3::FACTORY_TEMPLATE_LENGTH);
}

void test_the_whole_factory_template_zone_matches_the_contract() {
    uint8_t expected[384];
    uint8_t produced[384];
    uint16_t cursor = 0;
    for (uint8_t index = 0; index < flexseq::PATTERN_COUNT; ++index) {
        for (uint8_t offset = 0; offset < 24; ++offset) {
            expected[cursor] = expectedRecordByte(index, offset);
            produced[cursor] = flexseq::persist::v3::factoryTemplateByte(index, offset);
            ++cursor;
        }
    }
    TEST_ASSERT_EQUAL_UINT16(384, cursor);
    TEST_ASSERT_EQUAL_UINT16(384, flexseq::PATTERN_COUNT * flexseq::persist::v3::TEMPLATE_RECORD);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, produced, 384);
}

void test_a_factory_record_is_little_endian() {
    for (uint8_t index = 0; index < FACTORY_PATTERN_COUNT; ++index) {
        const uint16_t mask = expectedMask(index);
        TEST_ASSERT_EQUAL_UINT8(mask & 0xFF,
                                flexseq::persist::v3::factoryTemplateByte(index, 0));
        TEST_ASSERT_EQUAL_UINT8((mask >> 8) & 0xFF,
                                flexseq::persist::v3::factoryTemplateByte(index, 1));
    }
    TEST_ASSERT_EQUAL_UINT8(0x11, flexseq::persist::v3::factoryTemplateByte(0, 0));
    TEST_ASSERT_EQUAL_UINT8(0x91, flexseq::persist::v3::factoryTemplateByte(0, 1));
}

void test_a_factory_record_carries_no_step_above_fifteen() {
    for (uint8_t index = 0; index < flexseq::PATTERN_COUNT; ++index) {
        for (uint8_t offset = 2; offset < 5; ++offset) {
            TEST_ASSERT_EQUAL_UINT8(0,
                                    flexseq::persist::v3::factoryTemplateByte(index, offset));
        }
    }
}

void test_a_factory_record_carries_no_ratchet() {
    for (uint8_t index = 0; index < flexseq::PATTERN_COUNT; ++index) {
        for (uint8_t offset = 5; offset < 23; ++offset) {
            TEST_ASSERT_EQUAL_UINT8(0,
                                    flexseq::persist::v3::factoryTemplateByte(index, offset));
        }
    }
}

void test_every_factory_record_declares_sixteen_steps() {
    for (uint8_t index = 0; index < flexseq::PATTERN_COUNT; ++index) {
        TEST_ASSERT_EQUAL_UINT8(16, flexseq::persist::v3::factoryTemplateByte(index, 23));
    }
}

void test_the_b_slots_carry_content_free_records() {
    for (uint8_t index = FACTORY_PATTERN_COUNT; index < flexseq::PATTERN_COUNT; ++index) {
        for (uint8_t offset = 0; offset < 23; ++offset) {
            TEST_ASSERT_EQUAL_UINT8(0,
                                    flexseq::persist::v3::factoryTemplateByte(index, offset));
        }
        TEST_ASSERT_EQUAL_UINT8(16, flexseq::persist::v3::factoryTemplateByte(index, 23));
    }
}

void test_a_factory_record_refuses_an_offset_out_of_range() {
    for (uint8_t index = 0; index < flexseq::PATTERN_COUNT; ++index) {
        TEST_ASSERT_EQUAL_UINT8(0, flexseq::persist::v3::factoryTemplateByte(index, 24));
        TEST_ASSERT_EQUAL_UINT8(0, flexseq::persist::v3::factoryTemplateByte(index, 255));
    }
}

void test_a_factory_record_refuses_an_index_out_of_range() {
    for (uint8_t offset = 0; offset < 24; ++offset) {
        TEST_ASSERT_EQUAL_UINT8(0, flexseq::persist::v3::factoryTemplateByte(16, offset));
        TEST_ASSERT_EQUAL_UINT8(0, flexseq::persist::v3::factoryTemplateByte(255, offset));
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_eight_factory_patterns_carry_the_originals_content);
    RUN_TEST(test_the_b_bank_stays_empty);
    RUN_TEST(test_no_factory_pattern_carries_a_ratchet);
    RUN_TEST(test_the_steps_the_original_never_had_stay_off);
    RUN_TEST(test_loading_twice_changes_nothing);
    RUN_TEST(test_a_reload_erases_an_edit);
    RUN_TEST(test_the_factory_mask_reproduces_the_originals_steps);
    RUN_TEST(test_the_b_slots_have_no_factory_mask);
    RUN_TEST(test_the_factory_mask_refuses_an_index_out_of_range);
    RUN_TEST(test_the_factory_constants_hold_their_values);
    RUN_TEST(test_the_whole_factory_template_zone_matches_the_contract);
    RUN_TEST(test_a_factory_record_is_little_endian);
    RUN_TEST(test_a_factory_record_carries_no_step_above_fifteen);
    RUN_TEST(test_a_factory_record_carries_no_ratchet);
    RUN_TEST(test_every_factory_record_declares_sixteen_steps);
    RUN_TEST(test_the_b_slots_carry_content_free_records);
    RUN_TEST(test_a_factory_record_refuses_an_offset_out_of_range);
    RUN_TEST(test_a_factory_record_refuses_an_index_out_of_range);
    return UNITY_END();
}
