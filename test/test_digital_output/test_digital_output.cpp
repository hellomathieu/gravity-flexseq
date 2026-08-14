#include <stdint.h>
#include <string.h>
#include <new>

// test_digital_output provides its own minimal Arduino API stubs.
#define ARDUINO_MOCK_NO_GPIO

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Minimal Arduino API stubs
// -----------------------------------------------------------------------------

static unsigned long fake_millis = 0;
static uint8_t configured_pin = 0;
static uint8_t last_pin = 0;
static uint8_t last_state = LOW;

void pinMode(uint8_t pin, uint8_t) {
    configured_pin = pin;
}

void digitalWrite(uint8_t pin, uint8_t state) {
    last_pin = pin;
    last_state = state;
}

unsigned long millis() {
    return fake_millis;
}

// -----------------------------------------------------------------------------
// libGravity class under test
// -----------------------------------------------------------------------------

#include <digital_output.h>
#include <unity.h>

// -----------------------------------------------------------------------------
// Test helpers
// -----------------------------------------------------------------------------

void reset_fake_hardware() {
    fake_millis = 0;
    configured_pin = 0;
    last_pin = 0;
    last_state = LOW;
}

// Create a DigitalOutput in deliberately poisoned memory.
//
// The memory is filled with 0xFF before the object is created.
// The object is default-initialized WITHOUT parentheses so that scalar
// members are not automatically zero-initialized.
//
// This allows us to verify that Init() really initializes its state.
DigitalOutput* make_poisoned_output() {
    alignas(DigitalOutput) static uint8_t storage[sizeof(DigitalOutput)];

    memset(storage, 0xFF, sizeof(storage));

    return new (storage) DigitalOutput;
}

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

void test_init_starts_output_off() {
    reset_fake_hardware();

    DigitalOutput* output = make_poisoned_output();

    output->Init(7);

    // Init() must leave the logical output OFF.
    TEST_ASSERT_FALSE(output->On());

    // Init() must configure the requested pin.
    TEST_ASSERT_EQUAL_UINT8(7, configured_pin);
}

void test_high_turns_output_on() {
    reset_fake_hardware();

    DigitalOutput* output = make_poisoned_output();

    output->Init(7);
    output->High();

    TEST_ASSERT_TRUE(output->On());
    TEST_ASSERT_EQUAL_UINT8(7, last_pin);
    TEST_ASSERT_EQUAL_UINT8(HIGH, last_state);
}

void test_low_turns_output_off() {
    reset_fake_hardware();

    DigitalOutput* output = make_poisoned_output();

    output->Init(7);
    output->High();
    output->Low();

    TEST_ASSERT_FALSE(output->On());
    TEST_ASSERT_EQUAL_UINT8(LOW, last_state);
}

void test_trigger_starts_output_on() {
    reset_fake_hardware();

    DigitalOutput* output = make_poisoned_output();

    output->Init(7);
    output->Trigger();

    TEST_ASSERT_TRUE(output->On());
    TEST_ASSERT_EQUAL_UINT8(7, last_pin);
    TEST_ASSERT_EQUAL_UINT8(HIGH, last_state);
}

void test_process_ends_trigger_after_duration() {
    reset_fake_hardware();

    DigitalOutput* output = make_poisoned_output();

    output->Init(7);
    output->Trigger();

    // The trigger should still be active before 5 ms.
    fake_millis = DEFAULT_TRIGGER_DURATION_MS - 1;
    output->Process();

    TEST_ASSERT_TRUE(output->On());

    // The trigger should end after 5 ms.
    fake_millis = DEFAULT_TRIGGER_DURATION_MS;
    output->Process();

    TEST_ASSERT_FALSE(output->On());
    TEST_ASSERT_EQUAL_UINT8(LOW, last_state);
}

void test_init_does_not_create_phantom_trigger() {
    reset_fake_hardware();

    DigitalOutput* output = make_poisoned_output();

    // Init() is called, but Trigger() is never called.
    output->Init(7);

    fake_millis = DEFAULT_TRIGGER_DURATION_MS;
    output->Process();

    // A freshly initialized output must not be considered active.
    TEST_ASSERT_FALSE(output->On());
}

void test_reinit_turns_active_output_off() {
    reset_fake_hardware();

    DigitalOutput* output = make_poisoned_output();

    output->Init(7);
    output->Trigger();

    TEST_ASSERT_TRUE(output->On());

    output->Init(7);

    TEST_ASSERT_FALSE(output->On());
}

void test_reinit_does_not_leave_previous_trigger_active() {
    reset_fake_hardware();

    DigitalOutput* output = make_poisoned_output();

    output->Init(7);
    output->Trigger();

    TEST_ASSERT_TRUE(output->On());

    // Réinitialisation avant la fin du trigger.
    output->Init(7);

    // Le temps avance largement au-delà de la durée d'un trigger.
    fake_millis = DEFAULT_TRIGGER_DURATION_MS + 100;

    output->Process();

    TEST_ASSERT_FALSE(output->On());
}

// -----------------------------------------------------------------------------
// Unity entry point
// -----------------------------------------------------------------------------

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_init_starts_output_off);
    RUN_TEST(test_high_turns_output_on);
    RUN_TEST(test_low_turns_output_off);
    RUN_TEST(test_trigger_starts_output_on);
    RUN_TEST(test_process_ends_trigger_after_duration);
    RUN_TEST(test_init_does_not_create_phantom_trigger);
    RUN_TEST(test_reinit_turns_active_output_off);
    RUN_TEST(test_reinit_does_not_leave_previous_trigger_active);

    return UNITY_END();
}