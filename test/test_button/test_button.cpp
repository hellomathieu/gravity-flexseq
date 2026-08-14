#include <button.h>
#include <unity.h>

#include "Arduino.h"

#include <new>
#include <stdint.h>
#include <string.h>

static constexpr uint8_t BUTTON_PIN = 5;

static uint32_t short_press_count;
static uint32_t long_press_count;

static void on_short_press() {
    short_press_count++;
}

static void on_long_press() {
    long_press_count++;
}

void setUp() {
    ArduinoMock::reset();

    short_press_count = 0;
    long_press_count = 0;
}

void tearDown() {
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static Button* createButton() {
    /*
     * Construct the Button in zeroed storage.
     *
     * This makes the native test deterministic even though libGravity's
     * default constructor does not explicitly initialize all members.
     *
     * IMPORTANT:
     * This is a test harness technique. It does not modify libGravity.
     */
    alignas(Button) static uint8_t storage[sizeof(Button)];

    memset(storage, 0, sizeof(storage));

    Button* button = new (storage) Button();

    ArduinoMock::setDigitalPin(BUTTON_PIN, HIGH);
    button->Init(BUTTON_PIN);

    return button;
}

static void pressButton(Button& button) {
    ArduinoMock::advanceMillis(11);

    ArduinoMock::setDigitalPin(BUTTON_PIN, LOW);
    button.Process();
}

static void releaseButton(Button& button, unsigned long duration) {
    ArduinoMock::advanceMillis(duration);

    ArduinoMock::setDigitalPin(BUTTON_PIN, HIGH);
    button.Process();
}

// -----------------------------------------------------------------------------
// INITIAL STATE
// -----------------------------------------------------------------------------

void test_init_starts_unchanged() {
    Button* button = createButton();

    TEST_ASSERT_EQUAL(
        Button::CHANGE_UNCHANGED,
        button->Change()
    );
}

void test_released_button_is_off() {
    Button* button = createButton();

    TEST_ASSERT_FALSE(button->On());
}

void test_pressed_button_is_on() {
    Button* button = createButton();

    ArduinoMock::advanceMillis(11);
    ArduinoMock::setDigitalPin(BUTTON_PIN, LOW);

    TEST_ASSERT_TRUE(button->On());
}

// -----------------------------------------------------------------------------
// PRESS / RELEASE
// -----------------------------------------------------------------------------

void test_high_to_low_transition_is_pressed() {
    Button* button = createButton();

    pressButton(*button);

    TEST_ASSERT_EQUAL(
        Button::CHANGE_PRESSED,
        button->Change()
    );
}

void test_low_to_high_transition_is_released() {
    Button* button = createButton();

    pressButton(*button);
    releaseButton(*button, 100);

    TEST_ASSERT_EQUAL(
        Button::CHANGE_RELEASED,
        button->Change()
    );
}

void test_no_transition_is_unchanged() {
    Button* button = createButton();

    ArduinoMock::advanceMillis(11);
    button->Process();

    TEST_ASSERT_EQUAL(
        Button::CHANGE_UNCHANGED,
        button->Change()
    );
}

// -----------------------------------------------------------------------------
// DEBOUNCE
// -----------------------------------------------------------------------------

void test_press_before_debounce_window_is_ignored() {
    Button* button = createButton();

    ArduinoMock::advanceMillis(5);

    ArduinoMock::setDigitalPin(BUTTON_PIN, LOW);
    button->Process();

    TEST_ASSERT_EQUAL(
        Button::CHANGE_UNCHANGED,
        button->Change()
    );
}

void test_press_after_debounce_window_is_detected() {
    Button* button = createButton();

    ArduinoMock::advanceMillis(11);

    ArduinoMock::setDigitalPin(BUTTON_PIN, LOW);
    button->Process();

    TEST_ASSERT_EQUAL(
        Button::CHANGE_PRESSED,
        button->Change()
    );
}

/*
 * FUNCTIONAL DEBOUNCE TEST
 *
 * A valid press is followed by a release inside the debounce window.
 *
 * The early release must not immediately generate CHANGE_RELEASED.
 *
 * Once the input remains released beyond the debounce window, the release
 * should be detected.
 */
void test_release_during_debounce_is_reported_after_stable_window() {
    Button* button = createButton();

    // Valid press.
    pressButton(*button);

    TEST_ASSERT_EQUAL(
        Button::CHANGE_PRESSED,
        button->Change()
    );

    // Release only 5 ms after the press.
    ArduinoMock::advanceMillis(5);
    ArduinoMock::setDigitalPin(BUTTON_PIN, HIGH);
    button->Process();

    TEST_ASSERT_EQUAL(
        Button::CHANGE_UNCHANGED,
        button->Change()
    );

    // The input remains released.
    ArduinoMock::advanceMillis(10);
    button->Process();

    TEST_ASSERT_EQUAL(
        Button::CHANGE_RELEASED,
        button->Change()
    );
}

// -----------------------------------------------------------------------------
// SHORT PRESS
// -----------------------------------------------------------------------------

void test_short_press_at_749ms_is_released() {
    Button* button = createButton();

    pressButton(*button);
    releaseButton(*button, 749);

    TEST_ASSERT_EQUAL(
        Button::CHANGE_RELEASED,
        button->Change()
    );
}

void test_short_press_callback_is_called_once() {
    Button* button = createButton();

    button->AttachPressHandler(on_short_press);

    pressButton(*button);
    releaseButton(*button, 100);

    TEST_ASSERT_EQUAL_UINT32(1, short_press_count);

    button->Process();

    TEST_ASSERT_EQUAL_UINT32(1, short_press_count);
}

void test_short_press_does_not_call_long_callback() {
    Button* button = createButton();

    button->AttachPressHandler(on_short_press);
    button->AttachLongPressHandler(on_long_press);

    pressButton(*button);
    releaseButton(*button, 100);

    TEST_ASSERT_EQUAL_UINT32(1, short_press_count);
    TEST_ASSERT_EQUAL_UINT32(0, long_press_count);
}

// -----------------------------------------------------------------------------
// LONG PRESS
// -----------------------------------------------------------------------------

void test_press_at_exactly_750ms_is_long() {
    Button* button = createButton();

    pressButton(*button);
    releaseButton(*button, 750);

    TEST_ASSERT_EQUAL(
        Button::CHANGE_RELEASED_LONG,
        button->Change()
    );
}

void test_press_longer_than_750ms_is_long() {
    Button* button = createButton();

    pressButton(*button);
    releaseButton(*button, 751);

    TEST_ASSERT_EQUAL(
        Button::CHANGE_RELEASED_LONG,
        button->Change()
    );
}

void test_long_press_callback_is_called_once() {
    Button* button = createButton();

    button->AttachLongPressHandler(on_long_press);

    pressButton(*button);
    releaseButton(*button, 750);

    TEST_ASSERT_EQUAL_UINT32(1, long_press_count);

    button->Process();

    TEST_ASSERT_EQUAL_UINT32(1, long_press_count);
}

void test_long_press_does_not_call_short_callback() {
    Button* button = createButton();

    button->AttachPressHandler(on_short_press);
    button->AttachLongPressHandler(on_long_press);

    pressButton(*button);
    releaseButton(*button, 750);

    TEST_ASSERT_EQUAL_UINT32(0, short_press_count);
    TEST_ASSERT_EQUAL_UINT32(1, long_press_count);
}

// -----------------------------------------------------------------------------
// Unity
// -----------------------------------------------------------------------------

int main() {
    UNITY_BEGIN();

    // Initial state.
    RUN_TEST(test_init_starts_unchanged);
    RUN_TEST(test_released_button_is_off);
    RUN_TEST(test_pressed_button_is_on);

    // Press / release.
    RUN_TEST(test_high_to_low_transition_is_pressed);
    RUN_TEST(test_low_to_high_transition_is_released);
    RUN_TEST(test_no_transition_is_unchanged);

    // Debounce.
    RUN_TEST(test_press_before_debounce_window_is_ignored);
    RUN_TEST(test_press_after_debounce_window_is_detected);
    RUN_TEST(test_release_during_debounce_is_reported_after_stable_window);

    // Short press.
    RUN_TEST(test_short_press_at_749ms_is_released);
    RUN_TEST(test_short_press_callback_is_called_once);
    RUN_TEST(test_short_press_does_not_call_long_callback);

    // Long press.
    RUN_TEST(test_press_at_exactly_750ms_is_long);
    RUN_TEST(test_press_longer_than_750ms_is_long);
    RUN_TEST(test_long_press_callback_is_called_once);
    RUN_TEST(test_long_press_does_not_call_short_callback);

    return UNITY_END();
}