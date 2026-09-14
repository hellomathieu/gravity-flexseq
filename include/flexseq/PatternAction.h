#ifndef FLEXSEQ_PATTERN_ACTION_H
#define FLEXSEQ_PATTERN_ACTION_H

#include <stdint.h>

namespace flexseq {

// PRD 5.0 amendement 1ter : SHIFT plus une rotation charge le template, et le
// champ d action a quitte l onglet de canal. SAVE et la question ne sont plus
// des codes — SAVE n a plus d interface, PRD 12.9 le rend au lot E, et la
// question est un drapeau du controleur.
//
// Les codes vivent ICI et dans aucun autre fichier. Le controleur les pose et le
// service les consomme : deux definitions du meme code finiraient par diverger.
enum PatternAction : uint8_t {
    PATTERN_ACTION_LOAD = 0,
    // Aucune action demandee. Le controleur POSE une demande, et le service qui
    // connait l EEPROM la consomme : ADR 0002 interdit au domaine de lire le
    // materiel.
    PATTERN_ACTION_NONE = 0xFF,
};

}  // namespace flexseq

#endif // FLEXSEQ_PATTERN_ACTION_H
