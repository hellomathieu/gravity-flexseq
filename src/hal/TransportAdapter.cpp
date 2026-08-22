#include <flexseq/TransportAdapter.h>

#include <Arduino.h>
#include <libGravity.h>

namespace flexseq {
namespace transport {

static_assert(UiController::CLOCK_SOURCE_COUNT == static_cast<uint8_t>(Clock::SOURCE_LAST),
              "the UI source list must match libGravity's enum, sentinel excluded");

namespace {

volatile uint16_t extTicks = 0;
uint16_t lastTempo = 0;
uint8_t lastSource = 0xFF;

void onExternalEdge() {
    ++extTicks;
    gravity.clock.Tick();
}

Clock::Source sourceFor(uint8_t index) {
    if (index >= static_cast<uint8_t>(Clock::SOURCE_LAST)) {
        return Clock::SOURCE_INTERNAL;
    }
    return static_cast<Clock::Source>(index);
}

}  // namespace

void begin(UiController& controller) {
    gravity.clock.AttachExtHandler(onExternalEdge);
    lastTempo = 0;
    lastSource = 0xFF;
    apply(controller);
}

void apply(UiController& controller) {
    const uint16_t tempo = controller.tempo();
    if (tempo != lastTempo) {
        lastTempo = tempo;
        gravity.clock.SetTempo(static_cast<int>(tempo));
    }
    const uint8_t source = controller.clockSource();
    if (source != lastSource) {
        lastSource = source;
        gravity.clock.SetSource(sourceFor(source));
    }
}

uint16_t externalTicks() {
    uint16_t value;
    noInterrupts();
    value = extTicks;
    interrupts();
    return value;
}

uint16_t appliedTempo() { return lastTempo; }

uint8_t appliedSource() { return lastSource; }

}  // namespace transport
}  // namespace flexseq
