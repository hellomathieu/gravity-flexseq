#ifndef FLEXSEQ_ENCODER_FILTER_H
#define FLEXSEQ_ENCODER_FILTER_H

#include <stdint.h>

namespace flexseq {

class EncoderFilter {
public:
    static constexpr uint16_t DEFAULT_REVERSAL_WINDOW_MS = 12;
    static constexpr int8_t MAX_DELTA = 127;

    explicit EncoderFilter(uint16_t reversalWindowMs = DEFAULT_REVERSAL_WINDOW_MS)
        : window_(reversalWindowMs), lastMs_(0), suppressed_(0), lastIntervalMs_(0),
          lastSign_(0), sawFirst_(false) {}

    int8_t filter(int16_t delta, uint32_t nowMs) {
        if (delta == 0) {
            return 0;
        }
        const int8_t sign = delta < 0 ? -1 : 1;
        if (!sawFirst_) {
            sawFirst_ = true;
            lastMs_ = nowMs;
            lastSign_ = sign;
            ++suppressed_;
            return 0;
        }
        const uint32_t elapsed = nowMs - lastMs_;
        lastIntervalMs_ = elapsed > 0xFFFFu ? 0xFFFFu : static_cast<uint16_t>(elapsed);
        if (sign != lastSign_ && elapsed < window_) {
            ++suppressed_;
            return 0;
        }
        lastMs_ = nowMs;
        lastSign_ = sign;
        return clamped(delta);
    }

    void reset() {
        lastMs_ = 0;
        lastIntervalMs_ = 0;
        lastSign_ = 0;
        sawFirst_ = false;
    }

    uint16_t reversalWindowMs() const { return window_; }
    void setReversalWindowMs(uint16_t ms) { window_ = ms; }

    uint16_t suppressed() const { return suppressed_; }
    uint16_t lastIntervalMs() const { return lastIntervalMs_; }
    bool sawFirstMovement() const { return sawFirst_; }

private:
    static int8_t clamped(int16_t delta) {
        if (delta > MAX_DELTA) {
            return MAX_DELTA;
        }
        if (delta < -MAX_DELTA) {
            return -MAX_DELTA;
        }
        return static_cast<int8_t>(delta);
    }

    uint16_t window_;
    uint32_t lastMs_;
    uint16_t suppressed_;
    uint16_t lastIntervalMs_;
    int8_t lastSign_;
    bool sawFirst_;
};

}  // namespace flexseq

#endif // FLEXSEQ_ENCODER_FILTER_H
