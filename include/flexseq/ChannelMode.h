#ifndef FLEXSEQ_CHANNEL_MODE_H
#define FLEXSEQ_CHANNEL_MODE_H

#include <stdint.h>

namespace flexseq {

enum ChannelMode : uint8_t {
    MODE_CLOCK = 0,
    MODE_RANDOM = 1,
    MODE_SEQ = 2
};

constexpr uint8_t CHANNEL_MODE_COUNT = 3;
constexpr uint8_t MAX_SKIP_CHANCE = 10;
constexpr ChannelMode DEFAULT_CHANNEL_MODE = MODE_CLOCK;

}  // namespace flexseq

#endif // FLEXSEQ_CHANNEL_MODE_H
