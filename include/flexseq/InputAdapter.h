#ifndef FLEXSEQ_INPUT_ADAPTER_H
#define FLEXSEQ_INPUT_ADAPTER_H

#include <stdint.h>

#include <flexseq/EncoderFilter.h>
#include <flexseq/UiController.h>

namespace flexseq {
namespace input {

void begin(UiController& controller);
void process(uint32_t nowMs);

EncoderFilter& filter();
bool shiftHeld();
uint16_t suppressedLongPresses();

}  // namespace input
}  // namespace flexseq

#endif // FLEXSEQ_INPUT_ADAPTER_H
