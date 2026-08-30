#include <flexseq/LengthCv.h>

namespace flexseq {
namespace lengthcv {

int8_t zoneFor(int16_t cv) {
    (void)cv;
    return 0;
}

int8_t zoneWithHysteresis(int16_t cv, int8_t current) {
    (void)cv;
    (void)current;
    return 0;
}

uint8_t effectiveLengthFor(uint8_t base, int8_t offset) {
    (void)offset;
    return base;
}

}  // namespace lengthcv
}  // namespace flexseq
