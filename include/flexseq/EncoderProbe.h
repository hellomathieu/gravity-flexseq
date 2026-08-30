#ifndef FLEXSEQ_ENCODER_PROBE_H
#define FLEXSEQ_ENCODER_PROBE_H

#include <stdint.h>

namespace flexseq {
namespace probe {

constexpr uint8_t PAGE_COUNT = 10;
constexpr uint16_t PAGE_MS = 2000;

void recordPass(uint32_t elapsedUs);
void recordChange(int16_t value);
void advancePage(uint32_t nowMs);
bool pageChanged();
void writeReport(char* title);

}  // namespace probe
}  // namespace flexseq

#endif  // FLEXSEQ_ENCODER_PROBE_H
