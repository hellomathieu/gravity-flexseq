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

// PRD 10.2 : le champ MOD d'un canal en SEQ porte vingt et une valeurs. La
// position nomme l'entree, CV1 avant CV2. Le cycle ne propose PAS deux entrees
// sur la meme destination ; le moteur garde cette capacite et le nommage
// l'accepte, seule la rotation ne peut pas la produire.
constexpr uint8_t MOD_CHOICE_COUNT = 21;
constexpr uint8_t MOD_ROUTED_COUNT = CV_DESTINATION_COUNT - 1;

inline void modChoiceAt(uint8_t index, CvDestination* first, CvDestination* second) {
    if (index == 0 || index >= MOD_CHOICE_COUNT) {
        *first = CV_DEST_NONE;
        *second = CV_DEST_NONE;
        return;
    }
    if (index <= MOD_ROUTED_COUNT) {
        *first = static_cast<CvDestination>(index);
        *second = CV_DEST_NONE;
        return;
    }
    if (index <= 2 * MOD_ROUTED_COUNT) {
        *first = CV_DEST_NONE;
        *second = static_cast<CvDestination>(index - MOD_ROUTED_COUNT);
        return;
    }
    const uint8_t rank = static_cast<uint8_t>(index - 1 - 2 * MOD_ROUTED_COUNT);
    const uint8_t a = static_cast<uint8_t>(rank / (MOD_ROUTED_COUNT - 1) + 1);
    const uint8_t b = static_cast<uint8_t>(rank % (MOD_ROUTED_COUNT - 1) + 1);
    *first = static_cast<CvDestination>(a);
    *second = static_cast<CvDestination>(b >= a ? b + 1 : b);
}

inline int8_t modIndexOf(CvDestination first, CvDestination second) {
    for (uint8_t index = 0; index < MOD_CHOICE_COUNT; ++index) {
        CvDestination a = CV_DEST_NONE;
        CvDestination b = CV_DEST_NONE;
        modChoiceAt(index, &a, &b);
        if (a == first && b == second) {
            return static_cast<int8_t>(index);
        }
    }
    return -1;
}

}  // namespace flexseq

#endif // FLEXSEQ_CV_DESTINATION_H
