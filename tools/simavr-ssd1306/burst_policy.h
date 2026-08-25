#ifndef FLEXSEQ_HARNESS_BURST_POLICY_H
#define FLEXSEQ_HARNESS_BURST_POLICY_H

#include <stdint.h>

namespace burst {

enum Decision {
    REFUSE_INVALID_REQUEST,
    REFUSE_PHYSICAL_LIMIT,
    REFUSE_EMPIRICAL_LIMIT,
    ACCEPT_SINGLE_BURST,
    ACCEPT_SPLIT
};

const uint8_t NO_EMPIRICAL_LIMIT = 0;
const uint8_t MAX_DETENTS_REQUEST = 64;
const uint8_t MAX_BURSTS = 64;

struct Request {
    uint8_t detents;
    uint8_t physicalLimit;
    uint8_t empiricalLimit;
    bool allowSplit;
};

struct Verdict {
    Decision decision;
    uint8_t effectiveLimit;
    uint8_t bindingLimit;
};

inline Verdict decide(const Request& request)
{
    Verdict verdict;
    verdict.decision = REFUSE_INVALID_REQUEST;
    verdict.effectiveLimit = 0;
    verdict.bindingLimit = 0;

    if (request.detents < 1 || request.detents > MAX_DETENTS_REQUEST
        || request.physicalLimit < 1) {
        return verdict;
    }

    uint8_t effective = request.physicalLimit;
    if (request.empiricalLimit != NO_EMPIRICAL_LIMIT
        && request.empiricalLimit < effective) {
        effective = request.empiricalLimit;
    }
    verdict.effectiveLimit = effective;

    if (request.detents <= effective) {
        verdict.decision = ACCEPT_SINGLE_BURST;
        return verdict;
    }
    if (request.allowSplit) {
        verdict.decision = ACCEPT_SPLIT;
        return verdict;
    }

    verdict.bindingLimit = effective;
    verdict.decision = (request.empiricalLimit != NO_EMPIRICAL_LIMIT
                        && request.empiricalLimit < request.physicalLimit)
                     ? REFUSE_EMPIRICAL_LIMIT
                     : REFUSE_PHYSICAL_LIMIT;
    return verdict;
}

inline uint8_t split(uint8_t detents, uint8_t effectiveLimit,
                     uint8_t* out, uint8_t outCapacity)
{
    if (out == 0 || detents < 1 || effectiveLimit < 1) {
        return 0;
    }
    const uint16_t needed = static_cast<uint16_t>(
        (detents + effectiveLimit - 1) / effectiveLimit);
    if (needed > outCapacity || needed > MAX_BURSTS) {
        return 0;
    }

    uint8_t count = 0;
    uint8_t left = detents;
    while (left > 0) {
        const uint8_t take = left < effectiveLimit ? left : effectiveLimit;
        out[count++] = take;
        left = static_cast<uint8_t>(left - take);
    }
    return count;
}

}  // namespace burst

#endif
