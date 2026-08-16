#include <stdint.h>
#include <unity.h>

#include <flexseq/Subdiv.h>

using flexseq::subdivToTicks;
using flexseq::DEFAULT_SUBDIV;

void setUp() {}
void tearDown() {}

void test_unity_is_a_quarter_note() {
    TEST_ASSERT_EQUAL_UINT16(96, subdivToTicks(1));
}

void test_positive_divides_slower() {
    TEST_ASSERT_EQUAL_UINT16(192, subdivToTicks(2));
    TEST_ASSERT_EQUAL_UINT16(384, subdivToTicks(4));
    TEST_ASSERT_EQUAL_UINT16(12288, subdivToTicks(128));
}

void test_negative_multiplies_faster() {
    TEST_ASSERT_EQUAL_UINT16(48, subdivToTicks(-2)); // 1/8
    TEST_ASSERT_EQUAL_UINT16(24, subdivToTicks(-4)); // 1/16
    TEST_ASSERT_EQUAL_UINT16(12, subdivToTicks(-8)); // 1/32
    TEST_ASSERT_EQUAL_UINT16(4, subdivToTicks(-24));
}

void test_default_subdiv_is_one_sixteenth() {
    TEST_ASSERT_EQUAL_UINT16(24, subdivToTicks(DEFAULT_SUBDIV));
}

void test_matches_libgravity_clock_mod_pulses() {
    // Official CLOCK_MOD -> CLOCK_MOD_PULSES (firmware/Gravity/channel.h).
    const int16_t mods[] = {128, 64, 32, 24, 16, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2,
                            1, -2, -3, -4, -6, -8, -12, -16, -24};
    const uint16_t pulses[] = {12288, 6144, 3072, 2304, 1536, 1152, 1056, 960, 864,
                               768, 672, 576, 480, 384, 288, 192,
                               96, 48, 32, 24, 16, 12, 8, 6, 4};
    for (uint8_t i = 0; i < sizeof(mods) / sizeof(mods[0]); ++i) {
        TEST_ASSERT_EQUAL_UINT16(pulses[i], subdivToTicks(mods[i]));
    }
}

void test_rejects_zero() {
    TEST_ASSERT_EQUAL_UINT16(0, subdivToTicks(0));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_unity_is_a_quarter_note);
    RUN_TEST(test_positive_divides_slower);
    RUN_TEST(test_negative_multiplies_faster);
    RUN_TEST(test_default_subdiv_is_one_sixteenth);
    RUN_TEST(test_matches_libgravity_clock_mod_pulses);
    RUN_TEST(test_rejects_zero);
    return UNITY_END();
}
