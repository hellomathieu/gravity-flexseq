#include <flexseq/InputAdapter.h>

#include <Arduino.h>
#include <libGravity.h>

#if FLEXSEQ_ENCODER_PROBE
#include <flexseq/EncoderProbe.h>
#endif

namespace flexseq {
namespace input {

namespace {

UiController* controller = nullptr;
EncoderFilter encoderFilter;
Button encoderLongPress;
uint32_t frameMs = 0;

int8_t filtered(int value) {
    return encoderFilter.filter(static_cast<int16_t>(value), frameMs);
}

void onRotate(int value) {
#if FLEXSEQ_ENCODER_PROBE
    probe::recordChange(static_cast<int16_t>(value));
#endif
    if (controller == nullptr) {
        return;
    }
    const int8_t delta = filtered(value);
    if (delta == 0) {
        return;
    }
    controller->handle(gravity.shift_button.On() ? UiController::EVENT_SHIFT_ROTATE
                                                 : UiController::EVENT_ROTATE,
                       delta);
}

void onEncoderPress() {
    if (controller != nullptr) {
        controller->handle(UiController::EVENT_PRESS);
    }
}

void onEncoderLongPress() {
    if (controller != nullptr) {
        controller->handle(UiController::EVENT_LONG_PRESS);
    }
}

void onShiftPress() {
    if (controller != nullptr) {
        controller->handle(UiController::EVENT_SHIFT_PRESS);
    }
}

void onShiftLongPress() {
    if (controller != nullptr) {
        controller->handle(UiController::EVENT_SHIFT_LONG_PRESS);
    }
}

void onPlayPress() {
    if (controller != nullptr) {
        controller->handle(gravity.shift_button.On()
                               ? UiController::EVENT_SHIFT_PLAY_PRESS
                               : UiController::EVENT_PLAY_PRESS);
    }
}

}  // namespace

void begin(UiController& c) {
    controller = &c;
    encoderFilter.reset();

    gravity.encoder.SetReverseDirection(true);
    gravity.encoder.AttachRotateHandler(onRotate);
    gravity.encoder.AttachPressHandler(onEncoderPress);

    encoderLongPress.Init(ENCODER_SW_PIN);
    encoderLongPress.AttachLongPressHandler(onEncoderLongPress);

    gravity.shift_button.AttachPressHandler(onShiftPress);
    gravity.shift_button.AttachLongPressHandler(onShiftLongPress);
    gravity.play_button.AttachPressHandler(onPlayPress);
}

void process(uint32_t nowMs) {
    frameMs = nowMs;
    gravity.shift_button.Process();
    gravity.play_button.Process();
    gravity.encoder.Process();
    encoderLongPress.Process();
}

EncoderFilter& filter() { return encoderFilter; }

bool shiftHeld() { return gravity.shift_button.On(); }

}  // namespace input
}  // namespace flexseq
