#ifndef FLEXSEQ_PATTERN_H
#define FLEXSEQ_PATTERN_H

#include <stdint.h>

namespace flexseq {

// Ratchet code stored per step (4 bits). A step is ALWAYS one grid position;
// the ratchet says how many triggers fire inside it, and how long it lasts.
//
//   RATCHET_NONE (0) : 1 trigger, 1 step duration
//   2 / 3 / 4 / 6    : N triggers evenly spaced INSIDE one step duration
//                      (the step duration is unchanged — it plays "faster")
//   RATCHET_TRIPLET  : 3 triggers spread over TWO step durations
//                      ("a triplet of quarters is worth a half note"): the step
//                      lasts twice as long and pushes the rest of the pattern
//                      later — this is the one code that stretches time.
//
// Ratchet 5 is deliberately absent: at 96 PPQN (2^5 x 3) a fifth is exact on
// only 2 of the 25 SUBDIV values, so it could not be represented without drift.
enum : uint8_t {
    RATCHET_NONE = 0,
    RATCHET_2 = 2,
    RATCHET_3 = 3,
    RATCHET_4 = 4,
    RATCHET_6 = 6,
    RATCHET_TRIPLET = 7,
};

// True for a storable ratchet code.
bool isValidRatchet(uint8_t code);

// Number of triggers emitted by a step carrying this code (>= 1).
uint8_t ratchetTriggers(uint8_t code);

// How many step durations the step occupies (1, or 2 for the triplet).
uint8_t ratchetSpan(uint8_t code);

constexpr uint8_t MIN_SLOT_TICKS = 2;

// True when the code's triggers fit the step at this rate: every slot must hold
// at least MIN_SLOT_TICKS ticks.
bool ratchetFitsStep(uint8_t code, uint16_t ticksPerStep);

// Pattern holds only shared musical content: 24 binary steps + a per-step
// ratchet code. It carries NO length: LENGTH is a per-channel execution state,
// so a single Pattern can be referenced by several channels (shared bank model,
// matching the original Sitka firmware seqA1..seqB8).
//
// Ratchets are CONTENT (shared with the pattern). The measure separation is NOT
// stored here: it is a per-channel reading aid, purely graphical.
class Pattern {
public:
    static constexpr uint8_t DEFAULT_TOTAL_STEPS = 24;

    Pattern();

    bool readStep(uint8_t index, bool& active) const;
    bool writeStep(uint8_t index, bool active);

    void clear();

    // Per-step ratchet. setRatchet rejects an out-of-range index or an unknown
    // code. getRatchet returns RATCHET_NONE for an invalid index.
    bool setRatchet(uint8_t index, uint8_t code);
    uint8_t getRatchet(uint8_t index) const;

    void clearRatchets();

private:
    uint8_t packedSteps[3];  // 24 bits, one per step
    uint8_t packedRatchets[12]; // 24 nibbles, one per step
};

static_assert(sizeof(Pattern) == 15, "Pattern must remain 15 bytes (content only)");

} // namespace flexseq

#endif // FLEXSEQ_PATTERN_H
