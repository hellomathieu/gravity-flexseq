#ifndef FLEXSEQ_HARNESS_BURST_LIMITS_H
#define FLEXSEQ_HARNESS_BURST_LIMITS_H

#include "burst_policy.h"

#include <flexseq/UiController.h>

namespace harness {

const uint8_t LENGTH_BURST_LIMIT = 6;
const uint8_t SUBDIV_BURST_LIMIT = burst::NO_EMPIRICAL_LIMIT;
const uint8_t BAR_LENGTH_BURST_LIMIT = burst::NO_EMPIRICAL_LIMIT;
const uint8_t EDIT_ENTRY_BURST_LIMIT = burst::NO_EMPIRICAL_LIMIT;
const uint8_t RATCHET_BURST_LIMIT = burst::NO_EMPIRICAL_LIMIT;
const uint8_t STEP_BURST_LIMIT = burst::NO_EMPIRICAL_LIMIT;

// ⚠️ La limite porte sur le CHAMP, plus sur son index, depuis le lot 12.
// LENGTH et SUBDIV ont demenage sur la page CONFIG, ou ils portent les index 0
// et 1 : les memes que MODE et EDIT sur l'onglet. Un index n'est donc plus une
// cle unique, et une fonction qui en prend un aurait rendu la limite de LENGTH
// pour MODE.
inline uint8_t limitForField(flexseq::UiController::Field field)
{
    return field == flexseq::UiController::FIELD_LENGTH
               ? LENGTH_BURST_LIMIT
               : burst::NO_EMPIRICAL_LIMIT;
}

}  // namespace harness

#endif
