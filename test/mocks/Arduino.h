#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdint.h>
#include <stddef.h>

// -----------------------------------------------------------------------------
// Arduino types
// -----------------------------------------------------------------------------

using byte = uint8_t;

// -----------------------------------------------------------------------------
// Arduino constants
// -----------------------------------------------------------------------------

#define HIGH 0x1
#define LOW  0x0

#define INPUT        0x0
#define OUTPUT       0x1
#define INPUT_PULLUP 0x2

#define RISING 0x3

// -----------------------------------------------------------------------------
// Arduino compatibility helpers
// -----------------------------------------------------------------------------

// macOS defines MAX_INPUT in syslimits.h.
// libGravity uses MAX_INPUT for the ADC resolution constant.
#ifdef MAX_INPUT
#undef MAX_INPUT
#endif

template <typename T, typename U, typename V>
inline T constrain(T value, U min_value, V max_value) {
    if (value < min_value) {
        return static_cast<T>(min_value);
    }

    if (value > max_value) {
        return static_cast<T>(max_value);
    }

    return value;
}

inline long map(
    long value,
    long from_low,
    long from_high,
    long to_low,
    long to_high
) {
    return (value - from_low) * (to_high - to_low)
           / (from_high - from_low)
           + to_low;
}

// -----------------------------------------------------------------------------
// Arduino / AVR pin aliases used by libGravity
// -----------------------------------------------------------------------------

#ifndef A0
#define A0 14
#endif

#ifndef A1
#define A1 15
#endif

#ifndef A2
#define A2 16
#endif

#ifndef A3
#define A3 17
#endif

#ifndef A6
#define A6 20
#endif

#ifndef A7
#define A7 21
#endif

#ifndef SCL
#define SCL 19
#endif

#ifndef SDA
#define SDA 18
#endif

// -----------------------------------------------------------------------------
// AVR binary constants used by libGravity
// -----------------------------------------------------------------------------

#ifndef B00000110
#define B00000110 0x06
#endif

#ifndef B00010000
#define B00010000 0x10
#endif

#ifndef B00001000
#define B00001000 0x08
#endif

// -----------------------------------------------------------------------------
// AVR register mocks
// -----------------------------------------------------------------------------
//
// libGravity accesses these registers directly:
//
//     PCICR
//     PCMSK1
//     PCMSK2
//
// The native test environment is not an AVR, so these are represented by
// references to static storage while preserving lvalue semantics.
//

namespace ArduinoMock {

inline uint8_t& pcicr() {
    static uint8_t value = 0;
    return value;
}

inline uint8_t& pcmsk1() {
    static uint8_t value = 0;
    return value;
}

inline uint8_t& pcmsk2() {
    static uint8_t value = 0;
    return value;
}

} // namespace ArduinoMock

#define PCICR  ArduinoMock::pcicr()
#define PCMSK1 ArduinoMock::pcmsk1()
#define PCMSK2 ArduinoMock::pcmsk2()

// -----------------------------------------------------------------------------
// Mock state
// -----------------------------------------------------------------------------

namespace ArduinoMock {

static uint8_t digital_pin_state[32] = {};
static uint8_t digital_pin_output_state[32] = {};
static unsigned long current_millis = 0;

// Compteurs de LECTURES, par broche. Ils servent a caracteriser ce qu'une
// fonction de libGravity va REELLEMENT chercher sur le materiel — par exemple a
// figer la composition de `Gravity::Process()`, dont FlexSeq n'appelle plus que
// certains morceaux (voir test_gravity).
static uint16_t analog_read_count[32] = {};
static uint16_t digital_read_count[32] = {};

inline uint16_t analogReads(uint8_t pin) {
    return (pin < 32) ? analog_read_count[pin] : 0;
}

inline uint16_t digitalReads(uint8_t pin) {
    return (pin < 32) ? digital_read_count[pin] : 0;
}

inline uint16_t totalAnalogReads() {
    uint16_t n = 0;
    for (auto count : analog_read_count) n = static_cast<uint16_t>(n + count);
    return n;
}

inline void reset() {
    for (auto &state : digital_pin_state) {
        state = HIGH;
    }

    for (auto &count : analog_read_count) {
        count = 0;
    }

    for (auto &count : digital_read_count) {
        count = 0;
    }

    for (auto &state : digital_pin_output_state) {
        state = LOW;
    }

    current_millis = 0;

    pcicr() = 0;
    pcmsk1() = 0;
    pcmsk2() = 0;
}

inline void setDigitalPin(uint8_t pin, uint8_t state) {
    digital_pin_state[pin] = state;
}

inline uint8_t getDigitalPin(uint8_t pin) {
    return digital_pin_state[pin];
}

inline void setDigitalOutput(uint8_t pin, uint8_t state) {
    digital_pin_output_state[pin] = state;
}

inline uint8_t getDigitalOutput(uint8_t pin) {
    return digital_pin_output_state[pin];
}

inline void setMillis(unsigned long value) {
    current_millis = value;
}

inline void advanceMillis(unsigned long amount) {
    current_millis += amount;
}

} // namespace ArduinoMock

// -----------------------------------------------------------------------------
// GPIO
// -----------------------------------------------------------------------------
//
// Some test suites, such as test_digital_output, provide their own minimal
// Arduino GPIO stubs. They can define ARDUINO_MOCK_NO_GPIO before including
// Arduino.h to avoid duplicate definitions.
//

#ifndef ARDUINO_MOCK_NO_GPIO

inline void pinMode(uint8_t pin, uint8_t mode) {
    (void)pin;
    (void)mode;
}

inline int digitalRead(uint8_t pin) {
    if (pin < 32) {
        ++ArduinoMock::digital_read_count[pin];
    }
    return ArduinoMock::getDigitalPin(pin);
}

inline void digitalWrite(uint8_t pin, uint8_t state) {
    ArduinoMock::setDigitalOutput(pin, state);
}

#endif

// -----------------------------------------------------------------------------
// Analog input
// -----------------------------------------------------------------------------

#ifndef ARDUINO_MOCK_NO_GPIO

inline int analogRead(uint8_t pin) {
    if (pin < 32) {
        ++ArduinoMock::analog_read_count[pin];
    }
    return 0;
}

#endif

// -----------------------------------------------------------------------------
// Interrupts
// -----------------------------------------------------------------------------

inline int digitalPinToInterrupt(uint8_t pin) {
    return static_cast<int>(pin);
}

inline void attachInterrupt(
    int interrupt,
    void (*callback)(void),
    int mode
) {
    (void)interrupt;
    (void)callback;
    (void)mode;
}

inline void detachInterrupt(int interrupt) {
    (void)interrupt;
}

// -----------------------------------------------------------------------------
// AVR Pin Change Interrupts
// -----------------------------------------------------------------------------
//
// libGravity declares:
//
//     ISR(PCINT2_vect)
//     ISR(PCINT1_vect)
//
// In native tests, ISR() becomes an ordinary function.
//

#ifndef ISR

#define ISR(vector) void vector()

#endif

#ifndef PCINT1_vect
#define PCINT1_vect gravity_mock_pcint1_vect
#endif

#ifndef PCINT2_vect
#define PCINT2_vect gravity_mock_pcint2_vect
#endif

// -----------------------------------------------------------------------------
// Timing
// -----------------------------------------------------------------------------
//
// Like GPIO, millis() can be supplied by an individual test when required.
//

#ifndef ARDUINO_MOCK_NO_GPIO

inline unsigned long millis() {
    return ArduinoMock::current_millis;
}

#endif

#endif