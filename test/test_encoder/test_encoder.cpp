#define private public
#define protected public

#include <encoder.h>
#include <unity.h>

#undef protected
#undef private

#include "Arduino.h"

#include <stdint.h>

Encoder* Encoder::_instance = nullptr;

// -----------------------------------------------------------------------------
// Test callbacks
// -----------------------------------------------------------------------------

static int rotate_callback_value = 0;
static int press_rotate_callback_value = 0;
static uint32_t press_callback_count = 0;
static uint32_t rotate_callback_count = 0;
static uint32_t press_rotate_callback_count = 0;

static void on_press() {
    press_callback_count++;
}

static void on_rotate(int value) {
    rotate_callback_count++;
    rotate_callback_value = value;
}

static void on_press_rotate(int value) {
    press_rotate_callback_count++;
    press_rotate_callback_value = value;
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static Encoder* encoder_instance = nullptr;

void setUp() {
    ArduinoMock::reset();

    rotate_callback_value = 0;
    press_rotate_callback_value = 0;

    press_callback_count = 0;
    rotate_callback_count = 0;
    press_rotate_callback_count = 0;

    encoder_instance = new Encoder();
}

void tearDown() {
    delete encoder_instance;
    encoder_instance = nullptr;
}

// -----------------------------------------------------------------------------
// Initial state
// -----------------------------------------------------------------------------

void test_initial_position_produces_no_rotation() {
    Encoder& encoder = *encoder_instance;

    encoder.encoder_.setPosition(0);
    encoder.previous_pos_ = 0;

    TEST_ASSERT_EQUAL(
        0,
        encoder._rotate_change()
    );
}

void test_no_position_change_produces_no_rotation() {
    Encoder& encoder = *encoder_instance;

    encoder.previous_pos_ = 5;
    encoder.encoder_.setPosition(5);

    TEST_ASSERT_EQUAL(
        0,
        encoder._rotate_change()
    );
}

// -----------------------------------------------------------------------------
// Rotation
// -----------------------------------------------------------------------------

void test_positive_rotation_returns_positive_change() {
    Encoder& encoder = *encoder_instance;

    encoder.previous_pos_ = 0;
    encoder.encoder_.setPosition(1);
    encoder.encoder_.setMillisBetweenRotations(100);

    TEST_ASSERT_EQUAL(
        1,
        encoder._rotate_change()
    );
}

void test_negative_rotation_returns_negative_change() {
    Encoder& encoder = *encoder_instance;

    encoder.previous_pos_ = 0;
    encoder.encoder_.setPosition(-1);
    encoder.encoder_.setMillisBetweenRotations(100);

    TEST_ASSERT_EQUAL(
        -1,
        encoder._rotate_change()
    );
}

// -----------------------------------------------------------------------------
// Direction reversal
// -----------------------------------------------------------------------------

void test_reverse_direction_inverts_positive_rotation() {
    Encoder& encoder = *encoder_instance;

    encoder.SetReverseDirection(true);

    encoder.previous_pos_ = 0;
    encoder.encoder_.setPosition(1);
    encoder.encoder_.setMillisBetweenRotations(100);

    TEST_ASSERT_EQUAL(
        -1,
        encoder._rotate_change()
    );
}

void test_reverse_direction_inverts_negative_rotation() {
    Encoder& encoder = *encoder_instance;

    encoder.SetReverseDirection(true);

    encoder.previous_pos_ = 0;
    encoder.encoder_.setPosition(-1);
    encoder.encoder_.setMillisBetweenRotations(100);

    TEST_ASSERT_EQUAL(
        1,
        encoder._rotate_change()
    );
}

// -----------------------------------------------------------------------------
// Rotation acceleration
// -----------------------------------------------------------------------------

void test_rotation_slower_than_32ms_is_not_accelerated() {
    Encoder& encoder = *encoder_instance;

    encoder.previous_pos_ = 0;
    encoder.encoder_.setPosition(1);
    encoder.encoder_.setMillisBetweenRotations(32);

    TEST_ASSERT_EQUAL(
        1,
        encoder._rotate_change()
    );
}

void test_rotation_between_16_and_31ms_is_doubled() {
    Encoder& encoder = *encoder_instance;

    encoder.previous_pos_ = 0;
    encoder.encoder_.setPosition(1);
    encoder.encoder_.setMillisBetweenRotations(20);

    TEST_ASSERT_EQUAL(
        2,
        encoder._rotate_change()
    );
}

void test_rotation_under_16ms_is_tripled() {
    Encoder& encoder = *encoder_instance;

    encoder.previous_pos_ = 0;
    encoder.encoder_.setPosition(1);
    encoder.encoder_.setMillisBetweenRotations(10);

    TEST_ASSERT_EQUAL(
        3,
        encoder._rotate_change()
    );
}

// -----------------------------------------------------------------------------
// Rotation callback
// -----------------------------------------------------------------------------

void test_rotation_callback_receives_change() {
    Encoder& encoder = *encoder_instance;

    encoder.AttachRotateHandler(on_rotate);

    encoder.encoder_.setPosition(1);
    encoder.encoder_.setMillisBetweenRotations(100);

    encoder.Process();

    TEST_ASSERT_EQUAL_UINT32(
        1,
        rotate_callback_count
    );

    TEST_ASSERT_EQUAL(
        1,
        rotate_callback_value
    );
}

// -----------------------------------------------------------------------------
// Button + rotation
// -----------------------------------------------------------------------------

void test_press_rotate_callback_is_used_while_button_is_held() {
    Encoder& encoder = *encoder_instance;

    encoder.AttachPressRotateHandler(on_press_rotate);

    ArduinoMock::setDigitalPin(
        ENCODER_SW_PIN,
        LOW
    );

    encoder.encoder_.setPosition(1);
    encoder.encoder_.setMillisBetweenRotations(100);

    encoder.Process();

    TEST_ASSERT_EQUAL_UINT32(
        1,
        press_rotate_callback_count
    );

    TEST_ASSERT_EQUAL(
        1,
        press_rotate_callback_value
    );
}

// -----------------------------------------------------------------------------
// Button press without rotation
// -----------------------------------------------------------------------------

void test_press_handler_is_called_after_button_release() {
    Encoder& encoder = *encoder_instance;

    encoder.AttachPressHandler(on_press);

    ArduinoMock::setDigitalPin(
        ENCODER_SW_PIN,
        HIGH
    );

    encoder.Process();

    ArduinoMock::advanceMillis(11);

    ArduinoMock::setDigitalPin(
        ENCODER_SW_PIN,
        LOW
    );

    encoder.Process();

    ArduinoMock::advanceMillis(100);

    ArduinoMock::setDigitalPin(
        ENCODER_SW_PIN,
        HIGH
    );

    encoder.Process();

    TEST_ASSERT_EQUAL_UINT32(
        1,
        press_callback_count
    );
}

// -----------------------------------------------------------------------------
// ISR
// -----------------------------------------------------------------------------

void test_isr_ticks_encoder() {
    Encoder& encoder = *encoder_instance;

    int initial_ticks = encoder.encoder_.tickCount();

    Encoder::isr();

    TEST_ASSERT_EQUAL(
        initial_ticks + 1,
        encoder.encoder_.tickCount()
    );
}

void test_isr_targets_latest_encoder_instance() {
    Encoder first;

    Encoder::isr();

    TEST_ASSERT_EQUAL(
        1,
        first.encoder_.tickCount()
    );

    Encoder second;

    Encoder::isr();

    TEST_ASSERT_EQUAL(
        1,
        first.encoder_.tickCount()
    );

    TEST_ASSERT_EQUAL(
        1,
        second.encoder_.tickCount()
    );
}

void test_reverse_direction_remains_active_across_rotations() {
    Encoder& encoder = *encoder_instance;

    encoder.SetReverseDirection(true);

    encoder.previous_pos_ = 0;
    encoder.encoder_.setMillisBetweenRotations(100);

    encoder.encoder_.setPosition(1);

    TEST_ASSERT_EQUAL(
        -1,
        encoder._rotate_change()
    );

    encoder.encoder_.setPosition(2);

    TEST_ASSERT_EQUAL(
        -1,
        encoder._rotate_change()
    );
}

void test_reversed_fast_negative_rotation_is_positive_and_accelerated() {
    Encoder& encoder = *encoder_instance;

    encoder.SetReverseDirection(true);

    encoder.previous_pos_ = 0;
    encoder.encoder_.setPosition(-1);
    encoder.encoder_.setMillisBetweenRotations(10);

    TEST_ASSERT_EQUAL(
        3,
        encoder._rotate_change()
    );
}

void test_returning_to_previous_position_produces_reverse_change() {
    Encoder& encoder = *encoder_instance;

    encoder.previous_pos_ = 0;
    encoder.encoder_.setMillisBetweenRotations(100);

    encoder.encoder_.setPosition(1);

    TEST_ASSERT_EQUAL(
        1,
        encoder._rotate_change()
    );

    encoder.encoder_.setPosition(0);

    TEST_ASSERT_EQUAL(
        -1,
        encoder._rotate_change()
    );
}

// -----------------------------------------------------------------------------
// Initial state investigation
// -----------------------------------------------------------------------------

void test_new_encoder_has_safe_initial_state() {
    Encoder encoder;

    TEST_ASSERT_EQUAL(
        0,
        encoder.previous_pos_
    );

    TEST_ASSERT_FALSE(
        encoder.rotated_while_held_
    );

    TEST_ASSERT_EQUAL(
        0,
        encoder.change
    );

    TEST_ASSERT_NULL(
        encoder.on_press
    );

    TEST_ASSERT_NULL(
        encoder.on_rotate
    );

    TEST_ASSERT_NULL(
        encoder.on_press_rotate
    );
}

/**
 * Investigation test.
 *
 * A newly constructed Encoder must not report a rotation when the underlying
 * RotaryEncoder has not moved.
 *
 * Unlike test_new_encoder_has_safe_initial_state(), this test does not inspect
 * private state. It verifies the externally observable behaviour of Process().
 *
 * A valid rotation callback is installed deliberately so that an invalid
 * initial previous_pos_ can be detected without depending on an uninitialized
 * callback pointer.
 */
void test_new_encoder_process_without_movement_does_not_report_rotation() {
    Encoder encoder;

    rotate_callback_count = 0;
    rotate_callback_value = 0;

    encoder.AttachRotateHandler(on_rotate);

    // RotaryEncoder mock starts at position 0 and remains there.
    encoder.Process();

    TEST_ASSERT_EQUAL_UINT32(
        0,
        rotate_callback_count
    );

    TEST_ASSERT_EQUAL(
        0,
        rotate_callback_value
    );
}

// -----------------------------------------------------------------------------
// Unity
// -----------------------------------------------------------------------------

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_initial_position_produces_no_rotation);
    RUN_TEST(test_no_position_change_produces_no_rotation);

    RUN_TEST(test_positive_rotation_returns_positive_change);
    RUN_TEST(test_negative_rotation_returns_negative_change);

    RUN_TEST(test_reverse_direction_inverts_positive_rotation);
    RUN_TEST(test_reverse_direction_inverts_negative_rotation);

    RUN_TEST(test_rotation_slower_than_32ms_is_not_accelerated);
    RUN_TEST(test_rotation_between_16_and_31ms_is_doubled);
    RUN_TEST(test_rotation_under_16ms_is_tripled);

    RUN_TEST(test_rotation_callback_receives_change);

    RUN_TEST(test_press_rotate_callback_is_used_while_button_is_held);

    RUN_TEST(test_press_handler_is_called_after_button_release);

    RUN_TEST(test_isr_ticks_encoder);

    RUN_TEST(test_isr_targets_latest_encoder_instance);
    RUN_TEST(test_reverse_direction_remains_active_across_rotations);
    RUN_TEST(test_reversed_fast_negative_rotation_is_positive_and_accelerated);
    RUN_TEST(test_returning_to_previous_position_produces_reverse_change);

    RUN_TEST(test_new_encoder_has_safe_initial_state);

    RUN_TEST(
        test_new_encoder_process_without_movement_does_not_report_rotation
    );

    return UNITY_END();
}