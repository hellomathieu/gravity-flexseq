#include <stdint.h>

#include <uClock/uClock.h>

#include "NeoHWSerial.h"

// -----------------------------------------------------------------------------
// NeoHWSerial mock instance
// -----------------------------------------------------------------------------

MockNeoHWSerial NeoSerial;

// -----------------------------------------------------------------------------
// uClock test stub
// -----------------------------------------------------------------------------

namespace umodular {
namespace clock {

uClockClass::uClockClass()
    : clock_state(PAUSED) {
}

void uClockClass::init() {
    clock_state = PAUSED;
}

void uClockClass::setOutputPPQN(PPQNResolution resolution) {
    (void)resolution;
}

void uClockClass::setInputPPQN(PPQNResolution resolution) {
    (void)resolution;
}

void uClockClass::setClockMode(ClockMode mode) {
    clock_mode = mode;
}

uClockClass::ClockMode uClockClass::getClockMode() {
    return clock_mode;
}

void uClockClass::start() {
    clock_state = STARTING;
}

void uClockClass::stop() {
    clock_state = PAUSED;
}

void uClockClass::clockMe() {
    if (clock_state == STARTING) {
        clock_state = STARTED;
    }
}

} // namespace clock
} // namespace umodular

// -----------------------------------------------------------------------------
// Global uClock instance
// -----------------------------------------------------------------------------

umodular::clock::uClockClass uClock;