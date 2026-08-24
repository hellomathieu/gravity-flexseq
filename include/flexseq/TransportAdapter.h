#ifndef FLEXSEQ_TRANSPORT_ADAPTER_H
#define FLEXSEQ_TRANSPORT_ADAPTER_H

#include <stdint.h>

#include <flexseq/Transport.h>
#include <flexseq/UiController.h>

namespace flexseq {
namespace transport {

void begin(UiController& controller, Transport& transport);
void apply(UiController& controller);

uint16_t externalTicks();
uint16_t appliedTempo();
uint8_t appliedSource();

}  // namespace transport
}  // namespace flexseq

#endif // FLEXSEQ_TRANSPORT_ADAPTER_H
