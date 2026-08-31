#include <flexseq/LengthCv.h>

#include <flexseq/SequencerEngine.h>

namespace flexseq {
namespace lengthcv {

namespace {

constexpr int16_t HALF_WIDTH = ZONE_WIDTH / 2;

int8_t clampZone(int16_t zone) {
    if (zone < OFFSET_MIN) {
        return OFFSET_MIN;
    }
    if (zone > OFFSET_MAX) {
        return OFFSET_MAX;
    }
    return static_cast<int8_t>(zone);
}

}  // namespace

int8_t zoneFor(int16_t cv) {
    if (cv >= 0) {
        return clampZone(static_cast<int16_t>((cv + HALF_WIDTH) / ZONE_WIDTH));
    }
    return clampZone(static_cast<int16_t>(-((-cv + HALF_WIDTH) / ZONE_WIDTH)));
}

int8_t zoneWithHysteresis(int16_t cv, int8_t current) {
    if (current < OFFSET_MIN || current > OFFSET_MAX) {
        return zoneFor(cv);
    }
    int16_t distance = static_cast<int16_t>(cv - ZONE_WIDTH * current);
    if (distance < 0) {
        distance = static_cast<int16_t>(-distance);
    }
    if (distance <= STAY_WIDTH) {
        return current;
    }
    return zoneFor(cv);
}

uint8_t patternIndexFor(uint8_t base, int8_t offset) {
    int16_t wanted = static_cast<int16_t>(static_cast<int16_t>(base) + offset);
    if (wanted < 0) {
        wanted = 0;
    }
    if (wanted > static_cast<int16_t>(SequencerEngine::PATTERN_COUNT - 1)) {
        wanted = static_cast<int16_t>(SequencerEngine::PATTERN_COUNT - 1);
    }
    return static_cast<uint8_t>(wanted);
}

uint8_t effectiveLengthFor(uint8_t base, int8_t offset) {
    int16_t wanted = static_cast<int16_t>(static_cast<int16_t>(base) + offset);
    if (wanted < static_cast<int16_t>(SequencerEngine::MIN_LENGTH)) {
        wanted = static_cast<int16_t>(SequencerEngine::MIN_LENGTH);
    }
    if (wanted > static_cast<int16_t>(SequencerEngine::MAX_LENGTH)) {
        wanted = static_cast<int16_t>(SequencerEngine::MAX_LENGTH);
    }
    return static_cast<uint8_t>(wanted);
}

}  // namespace lengthcv
}  // namespace flexseq
