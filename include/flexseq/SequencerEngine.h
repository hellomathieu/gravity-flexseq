#ifndef FLEXSEQ_SEQUENCER_ENGINE_H
#define FLEXSEQ_SEQUENCER_ENGINE_H

#include <stdint.h>

namespace flexseq {

// SequencerEngine — FlexSeq temporal core (mirror of the TS reference model).
//
//  - masterPhase: monotonic counter of 96 PPQN ticks (uint32, wraps naturally).
//    1/16 is not hard-coded: each channel divides the phase by its own
//    ticksPerStep.
//  - masterPhase is independent of LENGTH / pattern. Global reset -> 0;
//    stop() keeps the phase.
//  - Per-channel execution state (selectedPattern, effectiveLength,
//    ticksPerStep, local phase). effectiveStep uses a SMOOTHED local phase:
//    changing LENGTH does not jump the playhead; localStep is only folded into
//    range when it falls outside the new length.
//
// Out of scope (deferred): Transport (clock->progression), METER/SUBDIV/
// MEASURES grid, CV modulation. ticksPerStep provisionally equals 1/16.
class SequencerEngine {
public:
    static constexpr uint8_t PPQN = 96;
    static constexpr uint16_t TICKS_PER_SIXTEENTH = PPQN / 4; // 24
    static constexpr uint8_t MIN_LENGTH = 1;
    static constexpr uint8_t MAX_LENGTH = 24;
    static constexpr uint8_t DEFAULT_LENGTH = 16;
    static constexpr uint8_t CHANNEL_COUNT = 6;
    static constexpr uint8_t PATTERN_COUNT = 16;

    SequencerEngine();

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

    // Logical position of the channel, in [0, effectiveLength). -1 if invalid.
    int8_t effectiveStep(uint8_t channel) const;

    // True if the channel crossed at least one step boundary during the LAST
    // advance() call (i.e. a new step onset). Used to emit triggers. Reset at
    // the start of every advance().
    bool hasStepped(uint8_t channel) const;

private:
    struct ChannelState {
        uint8_t selectedPattern;
        uint8_t effectiveLength;
        uint16_t ticksPerStep;
        uint8_t localStep; // in [0, effectiveLength)
        uint16_t acc;      // ticks into the current step, in [0, ticksPerStep)
    };

    bool validChannel(uint8_t channel) const { return channel < CHANNEL_COUNT; }

    uint32_t phase_;
    bool running_;
    uint8_t stepped_; // bitmask: channels that crossed a step in the last advance()
    ChannelState channels_[CHANNEL_COUNT];
};

}  // namespace flexseq

#endif // FLEXSEQ_SEQUENCER_ENGINE_H
