#ifndef FLEXSEQ_PREFERENCES_H
#define FLEXSEQ_PREFERENCES_H

#include <stdint.h>

namespace flexseq {

struct Preferences {
    static constexpr uint8_t DEFAULT_ROTATE_SCREEN = 1;
    static constexpr uint8_t DEFAULT_REVERSE_ENCODER = 1;
    static constexpr int16_t DEFAULT_CV_CALIBRATION = 0;
    static constexpr uint8_t CV_CHANNELS = 2;

    uint8_t rotateScreen;
    uint8_t reverseEncoder;
    int16_t cvCalibration[CV_CHANNELS];

    Preferences()
        : rotateScreen(DEFAULT_ROTATE_SCREEN),
          reverseEncoder(DEFAULT_REVERSE_ENCODER),
          cvCalibration{DEFAULT_CV_CALIBRATION, DEFAULT_CV_CALIBRATION} {}
};

static_assert(sizeof(Preferences) == 6, "Preferences must match the 6 bytes of PRD 11.1");

}  // namespace flexseq

#endif // FLEXSEQ_PREFERENCES_H
