#ifndef FLEXSEQ_PATTERN_ACTION_H
#define FLEXSEQ_PATTERN_ACTION_H

#include <stdint.h>

namespace flexseq {

// PRD 5.0 amendement 1bis : la grande valeur d un canal en SEQ s ouvre et porte
// une action. SAVE n existe que si la copie du canal a change, PRD 12.9 point 5.
//
// Les codes vivent ICI et dans aucun autre fichier. Le controleur les choisit et
// l ecran les nomme : deux definitions du meme code finiraient par diverger, et
// l ecran nommerait alors une autre action que celle qui s executerait.
enum PatternAction : uint8_t {
    PATTERN_ACTION_LOAD = 0,
    PATTERN_ACTION_SAVE = 1,
    PATTERN_ACTION_COUNT = 2,
};

}  // namespace flexseq

#endif // FLEXSEQ_PATTERN_ACTION_H
