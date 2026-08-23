#ifndef FLEXSEQ_PRNG_H
#define FLEXSEQ_PRNG_H

#include <stdint.h>

namespace flexseq {

class Prng {
public:
    static constexpr uint16_t DEFAULT_SEED = 0xACE1u;

    Prng() : state_(DEFAULT_SEED) {}

    void seed(uint16_t value) { state_ = (value == 0) ? DEFAULT_SEED : value; }

    uint16_t state() const { return state_; }

    uint16_t next() {
        state_ = static_cast<uint16_t>(state_ ^ static_cast<uint16_t>(state_ << 7));
        state_ = static_cast<uint16_t>(state_ ^ static_cast<uint16_t>(state_ >> 9));
        state_ = static_cast<uint16_t>(state_ ^ static_cast<uint16_t>(state_ << 8));
        return state_;
    }

    uint8_t below(uint8_t bound) {
        if (bound == 0) {
            return 0;
        }
        return static_cast<uint8_t>(next() % bound);
    }

private:
    uint16_t state_;
};

}  // namespace flexseq

#endif // FLEXSEQ_PRNG_H
