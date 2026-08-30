#include <stdint.h>
#include <unity.h>

#include <flexseq/LengthCv.h>

using flexseq::lengthcv::zoneFor;
using flexseq::lengthcv::zoneWithHysteresis;
using flexseq::lengthcv::effectiveLengthFor;

void setUp() {}
void tearDown() {}

void test_the_contract_constants_hold_their_decided_values() {
    TEST_ASSERT_EQUAL_INT16(-512, flexseq::lengthcv::CV_MIN);
    TEST_ASSERT_EQUAL_INT16(512, flexseq::lengthcv::CV_MAX);
    TEST_ASSERT_EQUAL_INT16(33, flexseq::lengthcv::ZONE_WIDTH);
    TEST_ASSERT_EQUAL_UINT8(31, flexseq::lengthcv::ZONE_COUNT);
    TEST_ASSERT_EQUAL_INT8(-15, flexseq::lengthcv::OFFSET_MIN);
    TEST_ASSERT_EQUAL_INT8(15, flexseq::lengthcv::OFFSET_MAX);
    TEST_ASSERT_EQUAL_INT16(8, flexseq::lengthcv::HYSTERESIS);
    TEST_ASSERT_EQUAL_INT16(24, flexseq::lengthcv::STAY_WIDTH);
}

void test_the_golden_vector_of_the_quantiser() {
    TEST_ASSERT_EQUAL_INT8(-15, zoneFor(-512));
    TEST_ASSERT_EQUAL_INT8(-15, zoneFor(-496));
    TEST_ASSERT_EQUAL_INT8(-15, zoneFor(-495));
    TEST_ASSERT_EQUAL_INT8(-1, zoneFor(-49));
    TEST_ASSERT_EQUAL_INT8(-1, zoneFor(-34));
    TEST_ASSERT_EQUAL_INT8(-1, zoneFor(-17));
    TEST_ASSERT_EQUAL_INT8(0, zoneFor(-16));
    TEST_ASSERT_EQUAL_INT8(0, zoneFor(0));
    TEST_ASSERT_EQUAL_INT8(0, zoneFor(16));
    TEST_ASSERT_EQUAL_INT8(1, zoneFor(17));
    TEST_ASSERT_EQUAL_INT8(1, zoneFor(33));
    TEST_ASSERT_EQUAL_INT8(1, zoneFor(49));
    TEST_ASSERT_EQUAL_INT8(2, zoneFor(50));
    TEST_ASSERT_EQUAL_INT8(14, zoneFor(478));
    TEST_ASSERT_EQUAL_INT8(15, zoneFor(479));
    TEST_ASSERT_EQUAL_INT8(15, zoneFor(495));
    TEST_ASSERT_EQUAL_INT8(15, zoneFor(512));
}

void test_the_two_extreme_zones_are_one_unit_wider() {
    TEST_ASSERT_EQUAL_INT8(-15, zoneFor(-479));
    TEST_ASSERT_EQUAL_INT8(-14, zoneFor(-478));
    TEST_ASSERT_EQUAL_INT8(15, zoneFor(479));
    TEST_ASSERT_EQUAL_INT8(14, zoneFor(478));
}

void test_the_quantiser_covers_the_whole_range_with_thirty_one_zones() {
    bool seen[31];
    for (uint8_t i = 0; i < 31; ++i) {
        seen[i] = false;
    }
    for (int16_t cv = -512; cv <= 512; ++cv) {
        const int8_t zone = zoneFor(cv);
        TEST_ASSERT_TRUE(zone >= -15 && zone <= 15);
        seen[zone + 15] = true;
    }
    for (uint8_t i = 0; i < 31; ++i) {
        TEST_ASSERT_TRUE(seen[i]);
    }
}

void test_the_hysteresis_keeps_the_zone_at_the_exact_boundary() {
    TEST_ASSERT_EQUAL_INT8(0, zoneWithHysteresis(24, 0));
    TEST_ASSERT_EQUAL_INT8(0, zoneWithHysteresis(-24, 0));
    TEST_ASSERT_EQUAL_INT8(1, zoneWithHysteresis(9, 1));
    TEST_ASSERT_EQUAL_INT8(1, zoneWithHysteresis(57, 1));
}

void test_the_hysteresis_yields_one_unit_past_the_boundary() {
    TEST_ASSERT_EQUAL_INT8(1, zoneWithHysteresis(25, 0));
    TEST_ASSERT_EQUAL_INT8(-1, zoneWithHysteresis(-25, 0));
    TEST_ASSERT_EQUAL_INT8(0, zoneWithHysteresis(8, 1));
    TEST_ASSERT_EQUAL_INT8(2, zoneWithHysteresis(58, 1));
}

void test_the_hysteresis_is_measured_from_the_centre_not_the_edge() {
    TEST_ASSERT_EQUAL_INT8(15, zoneWithHysteresis(471, 15));
    TEST_ASSERT_EQUAL_INT8(14, zoneWithHysteresis(470, 15));
    TEST_ASSERT_EQUAL_INT8(-15, zoneWithHysteresis(-471, -15));
    TEST_ASSERT_EQUAL_INT8(-14, zoneWithHysteresis(-470, -15));
}

void test_the_measured_noise_never_crosses_a_boundary() {
    for (int8_t zone = -15; zone <= 15; ++zone) {
        const int16_t centre = static_cast<int16_t>(33 * zone);
        const int16_t probes[5] = {centre, static_cast<int16_t>(centre + 16),
                                   static_cast<int16_t>(centre - 16),
                                   static_cast<int16_t>(centre + 17),
                                   static_cast<int16_t>(centre - 17)};
        for (uint8_t p = 0; p < 5; ++p) {
            for (int16_t delta = -3; delta <= 3; ++delta) {
                int16_t cv = static_cast<int16_t>(probes[p] + delta);
                if (cv < -512) {
                    cv = -512;
                }
                if (cv > 512) {
                    cv = 512;
                }
                TEST_ASSERT_EQUAL_INT8(zone, zoneWithHysteresis(cv, zone));
            }
        }
    }
}

void test_a_rising_ramp_never_steps_backwards() {
    int8_t current = -15;
    int8_t previous = -15;
    for (int16_t cv = -512; cv <= 512; ++cv) {
        current = zoneWithHysteresis(cv, current);
        TEST_ASSERT_TRUE(current >= previous);
        previous = current;
    }
    TEST_ASSERT_EQUAL_INT8(15, current);
}

void test_the_offset_shortens_and_lengthens() {
    TEST_ASSERT_EQUAL_UINT8(18, effectiveLengthFor(18, 0));
    TEST_ASSERT_EQUAL_UINT8(33, effectiveLengthFor(18, 15));
    TEST_ASSERT_EQUAL_UINT8(3, effectiveLengthFor(18, -15));
    TEST_ASSERT_EQUAL_UINT8(19, effectiveLengthFor(18, 1));
    TEST_ASSERT_EQUAL_UINT8(17, effectiveLengthFor(18, -1));
}

void test_the_effective_length_saturates_at_one_and_thirty_six() {
    TEST_ASSERT_EQUAL_UINT8(1, effectiveLengthFor(1, -15));
    TEST_ASSERT_EQUAL_UINT8(16, effectiveLengthFor(1, 15));
    TEST_ASSERT_EQUAL_UINT8(36, effectiveLengthFor(36, 15));
    TEST_ASSERT_EQUAL_UINT8(21, effectiveLengthFor(36, -15));
    TEST_ASSERT_EQUAL_UINT8(1, effectiveLengthFor(2, -15));
    TEST_ASSERT_EQUAL_UINT8(36, effectiveLengthFor(35, 15));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_the_contract_constants_hold_their_decided_values);
    RUN_TEST(test_the_golden_vector_of_the_quantiser);
    RUN_TEST(test_the_two_extreme_zones_are_one_unit_wider);
    RUN_TEST(test_the_quantiser_covers_the_whole_range_with_thirty_one_zones);
    RUN_TEST(test_the_hysteresis_keeps_the_zone_at_the_exact_boundary);
    RUN_TEST(test_the_hysteresis_yields_one_unit_past_the_boundary);
    RUN_TEST(test_the_hysteresis_is_measured_from_the_centre_not_the_edge);
    RUN_TEST(test_the_measured_noise_never_crosses_a_boundary);
    RUN_TEST(test_a_rising_ramp_never_steps_backwards);
    RUN_TEST(test_the_offset_shortens_and_lengthens);
    RUN_TEST(test_the_effective_length_saturates_at_one_and_thirty_six);
    return UNITY_END();
}
