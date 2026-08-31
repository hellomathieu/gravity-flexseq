#ifndef FLEXSEQ_LENGTH_CV_H
#define FLEXSEQ_LENGTH_CV_H

#include <stdint.h>

namespace flexseq {
namespace lengthcv {

constexpr int16_t CV_MIN = -512;
constexpr int16_t CV_MAX = 512;

constexpr int16_t ZONE_WIDTH = 33;
constexpr uint8_t ZONE_COUNT = 31;
constexpr int8_t OFFSET_MIN = -15;
constexpr int8_t OFFSET_MAX = 15;

constexpr int16_t HYSTERESIS = 8;
constexpr int16_t STAY_WIDTH = ZONE_WIDTH / 2 + HYSTERESIS;

int8_t zoneFor(int16_t cv);
int8_t zoneWithHysteresis(int16_t cv, int8_t current);
uint8_t effectiveLengthFor(uint8_t base, int8_t offset);
uint8_t patternIndexFor(uint8_t base, int8_t offset);

}  // namespace lengthcv
}  // namespace flexseq

#endif // FLEXSEQ_LENGTH_CV_H
