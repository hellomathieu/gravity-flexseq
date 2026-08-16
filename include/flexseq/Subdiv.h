#ifndef FLEXSEQ_SUBDIV_H
#define FLEXSEQ_SUBDIV_H

#include <stdint.h>

namespace flexseq {

// SUBDIV -> ticksPerStep, per the official libGravity convention (96 PPQN).
// Source: firmware/Gravity/channel.h (CLOCK_MOD / CLOCK_MOD_PULSES). The unity
// (1) is a QUARTER note = 96 ticks. Positive = divide (slower) => 96 * v ;
// negative = multiply (faster) => 96 / |v|. `processClockTick` fires a step
// when `tick % mod_pulses == 0`, so mod_pulses == ticksPerStep.
//
// Note: the historical Sitka Trigger Sequencer step is 1/16, which here is
// SUBDIV = -4 (96/4 = 24 ticks) — the per-channel default.

constexpr uint16_t QUARTER_TICKS = 96; // == SequencerEngine::PPQN
constexpr int16_t DEFAULT_SUBDIV = -4; // 1/16 (24 ticks)

// Returns ticksPerStep for a SUBDIV value, or 0 if invalid (0, or a multiplier
// that does not divide 96 evenly).
inline uint16_t subdivToTicks(int16_t subdiv) {
    if (subdiv == 0) {
        return 0;
    }
    if (subdiv > 0) {
        return static_cast<uint16_t>(QUARTER_TICKS * subdiv);
    }
    const int16_t mult = static_cast<int16_t>(-subdiv);
    if (QUARTER_TICKS % mult != 0) {
        return 0;
    }
    return static_cast<uint16_t>(QUARTER_TICKS / mult);
}

}  // namespace flexseq

#endif // FLEXSEQ_SUBDIV_H
