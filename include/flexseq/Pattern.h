#ifndef FLEXSEQ_PATTERN_H
#define FLEXSEQ_PATTERN_H

#include <stdint.h>

namespace flexseq {

// Pattern holds only shared musical content: 24 binary steps + local triplet
// groups. It carries NO length: LENGTH is a per-channel execution state (see
// the sequencer), so a single Pattern can be referenced by several channels
// (shared bank model, matching the original Sitka firmware seqA1..seqB8).
//
// Triplet validity is therefore independent of any length: a group is valid on
// the 24-step grid (start <= 21, no overlap), per the PRD ("triplets are
// independent of LENGTH").
class Pattern {
public:
    static constexpr uint8_t DEFAULT_TOTAL_STEPS = 24;

    Pattern();

    bool readStep(uint8_t index, bool& active) const;
    bool writeStep(uint8_t index, bool active);

    void clear();

    bool addTriplet(uint8_t startIndex);
    bool removeTriplet(uint8_t startIndex);

    bool isTripletStart(uint8_t index) const;
    bool isTripletStep(uint8_t index) const;

    void clearTriplets();

private:
    uint8_t packedSteps[3];
    uint8_t tripletStarts[3];
};

static_assert(sizeof(Pattern) == 6, "Pattern must remain 6 bytes (content only)");

} // namespace flexseq

#endif // FLEXSEQ_PATTERN_H
