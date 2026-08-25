#ifndef FLEXSEQ_HARNESS_BURST_LIMITS_H
#define FLEXSEQ_HARNESS_BURST_LIMITS_H

#include "burst_policy.h"

namespace harness {

const uint8_t LENGTH_BURST_LIMIT = 6;
const uint8_t SUBDIV_BURST_LIMIT = burst::NO_EMPIRICAL_LIMIT;
const uint8_t BAR_LENGTH_BURST_LIMIT = burst::NO_EMPIRICAL_LIMIT;
const uint8_t EDIT_ENTRY_BURST_LIMIT = burst::NO_EMPIRICAL_LIMIT;
const uint8_t RATCHET_BURST_LIMIT = burst::NO_EMPIRICAL_LIMIT;
const uint8_t STEP_BURST_LIMIT = burst::NO_EMPIRICAL_LIMIT;

const uint8_t FIELD_INDEX_LENGTH = 1;

inline uint8_t limitForFieldIndex(uint8_t index)
{
    return index == FIELD_INDEX_LENGTH ? LENGTH_BURST_LIMIT
                                       : burst::NO_EMPIRICAL_LIMIT;
}

}  // namespace harness

#endif
