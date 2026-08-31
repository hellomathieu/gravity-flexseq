#ifndef FLEXSEQ_CV_DESTINATION_H
#define FLEXSEQ_CV_DESTINATION_H

#include <stdint.h>

namespace flexseq {

// PRD 10.2. L'ORDRE est normatif : le code part en EEPROM, octets 7 et 8 du
// record de channel. Le reordonner change le routage de toute image deja
// ecrite. NONE vaut 0 pour qu'une image anterieure, qui porte 0 dans ces deux
// octets, se relise sans migration.
enum CvDestination : uint8_t {
    CV_DEST_NONE = 0,
    CV_DEST_PATTERN = 1,
    CV_DEST_LENGTH = 2,
    CV_DEST_RESET = 3,
    CV_DEST_STEP = 4
};

constexpr uint8_t CV_DESTINATION_COUNT = 5;
constexpr CvDestination DEFAULT_CV_DESTINATION = CV_DEST_NONE;

constexpr uint8_t CV_SOURCE_COUNT = 2;
constexpr uint8_t CV_SOURCE_1 = 0;
constexpr uint8_t CV_SOURCE_2 = 1;

}  // namespace flexseq

#endif // FLEXSEQ_CV_DESTINATION_H
