#ifndef FLEXSEQ_SEQUENCER_ENGINE_H
#define FLEXSEQ_SEQUENCER_ENGINE_H

#include <stdint.h>

#include <flexseq/ChannelMode.h>
#include <flexseq/Subdiv.h>

namespace flexseq {

class PatternBank;

// SequencerEngine — FlexSeq temporal core (mirror of the TS reference model).
//
//  - masterPhase: monotonic counter of 96 PPQN ticks (uint32, wraps naturally).
//    Each channel divides the phase by its own ticksPerStep (from SUBDIV).
//  - ONE STEP IS ONE UNIT OF TIME. SUBDIV alone sets how fast that unit is; the
//    measure separation is a purely graphical reading aid (barLength) with no
//    effect whatsoever on timing.
//  - Per-channel execution state (selectedPattern, effectiveLength, subdiv,
//    ticksPerStep, local phase). effectiveStep uses a SMOOTHED local phase:
//    changing LENGTH does not jump the playhead.
//  - RATCHETS (pattern content) shape the step: codes 2/3/4/6 fire N triggers
//    inside one step duration, RATCHET_TRIPLET fires 3 triggers over TWO step
//    durations (it stretches time and pushes the rest of the pattern later).
//
// Out of scope (deferred): Transport (clock->progression), CV modulation.
class SequencerEngine {
public:
    static constexpr uint8_t PPQN = 96;
    static constexpr uint16_t TICKS_PER_SIXTEENTH = PPQN / 4; // 24
    static constexpr uint8_t MIN_LENGTH = 1;
    static constexpr uint8_t MAX_LENGTH = 24;
    static constexpr uint8_t DEFAULT_LENGTH = 16;
    static constexpr uint8_t CHANNEL_COUNT = 6;
    static constexpr uint8_t PATTERN_COUNT = 16;

    // Measure separation: draw a bar every N steps. Graphical only.
    static constexpr uint8_t BAR_NONE = 0;
    static constexpr uint8_t DEFAULT_BAR_LENGTH = 4;

    SequencerEngine();

    // Optional shared bank. Once set, the engine reads each step's ratchet code
    // to shape its duration and its trigger count. Without a bank every step is
    // a plain one-unit, one-trigger step.
    void setPatternBank(const PatternBank* bank);

    // Transport
    uint32_t masterPhase() const;
    bool isRunning() const;
    void start();
    void stop();
    void reset();
    void advance(uint16_t ticks = 1);

    // Per-channel selected pattern (0..15). Query returns -1 for an invalid channel.
    int8_t getSelectedPattern(uint8_t channel) const;
    bool setSelectedPattern(uint8_t channel, uint8_t index);

    // Per-channel length (1..24). Query returns 0 for an invalid channel.
    uint8_t getEffectiveLength(uint8_t channel) const;
    bool setEffectiveLength(uint8_t channel, uint8_t length);

    // Per-channel ticks per step (>= 1). Query returns 0 for an invalid channel.
    uint16_t getTicksPerStep(uint8_t channel) const;
    bool setTicksPerStep(uint8_t channel, uint16_t ticks);

    // Per-channel SUBDIV (libGravity). setSubdiv also updates ticksPerStep via
    // the official mapping. Query returns 0 for an invalid channel.
    int16_t getSubdiv(uint8_t channel) const;
    bool setSubdiv(uint8_t channel, int16_t subdiv);

    ChannelMode getChannelMode(uint8_t channel) const;
    bool setChannelMode(uint8_t channel, ChannelMode mode);

    static constexpr uint8_t MAX_OFFSET = 255;

    uint8_t getOffset(uint8_t channel) const;
    bool setOffset(uint8_t channel, uint16_t offset);

    uint8_t getSkipChance(uint8_t channel) const;
    bool setSkipChance(uint8_t channel, uint8_t tenths);

    // Per-channel measure separation: a bar every N steps, N in {0, 2, 3, 4, 6}
    // (0 = none). GRAPHICAL ONLY — never affects timing. -1 if channel invalid.
    int8_t getBarLength(uint8_t channel) const;
    bool setBarLength(uint8_t channel, uint8_t steps);

    // Re-read the current step's ratchet after the PATTERN CONTENT changed.
    // Without this the cached timing would only pick the edit up on the next
    // pass over that step. Call it after any ratchet edit.
    void refreshTiming();
    void refreshTiming(uint8_t channel);

    // Duration in ticks of the channel's CURRENT step (doubled by a triplet).
    // 0 for an invalid channel.
    uint16_t currentStepTicks(uint8_t channel) const;

    // Number of triggers the current step emits (1, or the ratchet count).
    uint8_t currentStepTriggers(uint8_t channel) const;

    // Logical position of the channel, in [0, effectiveLength). -1 if invalid.
    int8_t effectiveStep(uint8_t channel) const;

    // True if the channel crossed a STEP boundary during the last advance().
    bool hasStepped(uint8_t channel) const;

    // Trigger onsets emitted during the last advance(): 1 per plain step, N for
    // a ratchet. Used to drive the outputs. Reset at every advance().
    uint8_t onsetCount(uint8_t channel) const;

private:
    struct ChannelState {
        uint8_t selectedPattern;
        uint8_t effectiveLength;
        int16_t subdiv;
        uint16_t ticksPerStep;
        uint8_t barLength;  // graphical measure separation, in steps
        uint8_t mode;
        uint8_t offset;
        uint8_t skipChance;
        uint8_t localStep;  // in [0, effectiveLength)
        uint16_t acc;       // ticks into the current step, in [0, stepTicks)
        // Cached timing of the CURRENT step, refreshed on every step boundary
        // (and on rate changes) so the hot path needs no division.
        uint16_t stepTicks; // ticksPerStep x span
        uint16_t slotTicks; // stepTicks / triggers
        uint8_t triggers;   // onsets inside this step
        uint8_t subOnset;   // sub-onsets already fired in this step
    };

    bool validChannel(uint8_t channel) const { return channel < CHANNEL_COUNT; }

    // Recompute stepTicks/slotTicks/triggers from the current step's ratchet.
    void refreshStepTiming(uint8_t channel, bool resetSubOnset = true);

    void clampOffset(uint8_t channel);

    const PatternBank* bank_; // optional; drives ratchet timing
    uint32_t phase_;
    bool running_;
    uint8_t stepped_; // bitmask: channels that crossed a step in the last advance()
    uint8_t onsets_[CHANNEL_COUNT];
    ChannelState channels_[CHANNEL_COUNT];
};

}  // namespace flexseq

#endif // FLEXSEQ_SEQUENCER_ENGINE_H
