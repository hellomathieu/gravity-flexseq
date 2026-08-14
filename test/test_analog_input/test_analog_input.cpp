#include <stdint.h>
#include <stdlib.h>

// -----------------------------------------------------------------------------
// Minimal Arduino API stubs required by AnalogInput on native
// -----------------------------------------------------------------------------

enum {
    INPUT = 0
};

static int fake_analog_value = 538;

void pinMode(uint8_t, int) {}

int analogRead(uint8_t) {
    return fake_analog_value;
}

long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) /
           (in_max - in_min) + out_min;
}

long constrain(long value, long min_value, long max_value) {
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

// -----------------------------------------------------------------------------
// libGravity
// -----------------------------------------------------------------------------

#include <analog_input.h>
#include <unity.h>

// -----------------------------------------------------------------------------
// Test helpers
// -----------------------------------------------------------------------------

static constexpr uint8_t INPUT_PIN = 7;

/*
 * libGravity calibration:
 *
 *     CALIBRATED_LOW  = -566
 *     CALIBRATED_HIGH =  512
 *
 * With the current integer map():
 *
 *     raw 0    -> approximately -566
 *     raw 538  -> approximately    0
 *     raw 539  -> approximately   +1
 *     raw 1023 -> approximately +512
 *
 * Therefore:
 *
 *     538 = zero
 *     539 = smallest positive value
 *     537 = smallest negative value
 */

static void processRaw(AnalogInput& input, int raw) {
    fake_analog_value = raw;
    input.Process();
}

// -----------------------------------------------------------------------------
// FUNCTIONAL TESTS
// -----------------------------------------------------------------------------

void test_negative_to_positive_crossing_is_rising_edge() {
    AnalogInput input;

    input.Init(INPUT_PIN);

    // Negative.
    processRaw(input, 537);

    // Positive.
    processRaw(input, 539);

    TEST_ASSERT_TRUE(
        input.IsRisingEdge(0)
    );
}

void test_negative_to_negative_does_not_trigger() {
    AnalogInput input;

    input.Init(INPUT_PIN);

    // Negative -> negative.
    processRaw(input, 0);
    processRaw(input, 300);

    TEST_ASSERT_FALSE(
        input.IsRisingEdge(0)
    );
}

void test_positive_to_positive_does_not_trigger() {
    AnalogInput input;

    input.Init(INPUT_PIN);

    // Positive -> positive.
    processRaw(input, 700);
    processRaw(input, 900);

    TEST_ASSERT_FALSE(
        input.IsRisingEdge(0)
    );
}

void test_positive_to_negative_does_not_trigger() {
    AnalogInput input;

    input.Init(INPUT_PIN);

    // Positive -> negative.
    processRaw(input, 900);
    processRaw(input, 0);

    TEST_ASSERT_FALSE(
        input.IsRisingEdge(0)
    );
}

void test_zero_to_positive_is_rising_edge() {
    AnalogInput input;

    input.Init(INPUT_PIN);

    // Zero.
    processRaw(input, 538);

    // Small positive value.
    processRaw(input, 539);

    TEST_ASSERT_TRUE(
        input.IsRisingEdge(0)
    );
}

void test_zero_to_negative_does_not_trigger() {
    AnalogInput input;

    input.Init(INPUT_PIN);

    // Zero.
    processRaw(input, 538);

    // Small negative value.
    processRaw(input, 537);

    TEST_ASSERT_FALSE(
        input.IsRisingEdge(0)
    );
}

void test_negative_to_positive_crossing_with_positive_threshold_is_rising_edge() {
    AnalogInput input;

    input.Init(INPUT_PIN);

    // Clearly below +100 threshold.
    processRaw(input, 0);

    // Clearly above +100 threshold.
    processRaw(input, 700);

    TEST_ASSERT_TRUE(
        input.IsRisingEdge(100)
    );
}

void test_positive_to_positive_above_threshold_does_not_trigger() {
    AnalogInput input;

    input.Init(INPUT_PIN);

    // Both values are above the threshold.
    processRaw(input, 700);
    processRaw(input, 900);

    TEST_ASSERT_FALSE(
        input.IsRisingEdge(100)
    );
}

void test_negative_to_negative_below_threshold_does_not_trigger() {
    AnalogInput input;

    input.Init(INPUT_PIN);

    // Both values remain below the threshold.
    processRaw(input, 0);
    processRaw(input, 300);

    TEST_ASSERT_FALSE(
        input.IsRisingEdge(100)
    );
}

// -----------------------------------------------------------------------------
// Unity
// -----------------------------------------------------------------------------

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_negative_to_positive_crossing_is_rising_edge);
    RUN_TEST(test_negative_to_negative_does_not_trigger);
    RUN_TEST(test_positive_to_positive_does_not_trigger);
    RUN_TEST(test_positive_to_negative_does_not_trigger);
    RUN_TEST(test_zero_to_positive_is_rising_edge);
    RUN_TEST(test_zero_to_negative_does_not_trigger);
    RUN_TEST(test_negative_to_positive_crossing_with_positive_threshold_is_rising_edge);
    RUN_TEST(test_positive_to_positive_above_threshold_does_not_trigger);
    RUN_TEST(test_negative_to_negative_below_threshold_does_not_trigger);

    return UNITY_END();
}