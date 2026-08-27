#include <cmath>
#include <cstdlib>
#include <stdint.h>
#include <stdlib.h>
#include <unity.h>

#include <Arduino.h>
#include <libGravity.h>

#include <flexseq/InputAdapter.h>
#include <flexseq/PatternBank.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>
#include <flexseq/UiController.h>

Encoder* Encoder::_instance = nullptr;
Gravity gravity;

#include "../../src/hal/InputAdapter.cpp"

using flexseq::PatternBank;
using flexseq::SequencerEngine;
using flexseq::Transport;
using flexseq::UiController;

void setUp() {}
void tearDown() {}

namespace {

constexpr uint16_t LONG_PRESS_MS = 750;
constexpr uint16_t SETTLE_MS = 20;

struct Rig {
    PatternBank bank;
    SequencerEngine engine;
    Transport transport;
    UiController ui;
    int position = 0;
    unsigned long now = 0;

    Rig() : engine(), transport(engine), ui(engine, transport) {
        ArduinoMock::reset();
        engine.setPatternBank(&bank);
        for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
            engine.setChannelMode(ch, flexseq::MODE_SEQ);
        }
        release(SHIFT_BTN_PIN);
        release(ENCODER_SW_PIN);
        release(PLAY_BTN_PIN);
        gravity.shift_button.Init(SHIFT_BTN_PIN);
        gravity.play_button.Init(PLAY_BTN_PIN);
        flexseq::input::begin(ui);
        tick(SETTLE_MS);
    }

    void press(uint8_t pin) { ArduinoMock::setDigitalPin(pin, LOW); }
    void release(uint8_t pin) { ArduinoMock::setDigitalPin(pin, HIGH); }

    // One main-loop pass, `ms` after the previous one.
    void tick(unsigned long ms) {
        now += ms;
        ArduinoMock::setMillis(now);
        flexseq::input::process(now);
    }

    void rotate(int detents) {
        position += detents;
        RotaryEncoder::lastInstance()->setPosition(position);
    }

    void enterEdit() {
        for (uint8_t guard = 0; guard < 4; ++guard) {
            if (ui.level() == UiController::LEVEL_TAB_BAR) {
                break;
            }
            ui.handle(UiController::EVENT_LONG_PRESS);
        }
        ui.handle(UiController::EVENT_PRESS);
        for (uint8_t guard = 0; guard < UiController::CHANNEL_TAB_FIELDS; ++guard) {
            if (ui.field() == UiController::FIELD_EDIT_ENTRY) {
                break;
            }
            ui.handle(UiController::EVENT_ROTATE, 1);
        }
        ui.handle(UiController::EVENT_PRESS);
    }
};

}  // namespace

void test_a_long_press_on_the_encoder_goes_back_a_level() {
    Rig r;
    r.enterEdit();
    TEST_ASSERT_EQUAL(UiController::LEVEL_EDIT, r.ui.level());
    const uint16_t before = flexseq::input::suppressedLongPresses();

    r.press(ENCODER_SW_PIN);
    r.tick(SETTLE_MS + 1);
    r.release(ENCODER_SW_PIN);
    r.tick(LONG_PRESS_MS + 50);

    TEST_ASSERT_EQUAL_UINT16(before, flexseq::input::suppressedLongPresses());
    TEST_ASSERT_EQUAL_MESSAGE(UiController::LEVEL_TAB, r.ui.level(),
        "l appui long delibere doit passer");
}

void test_a_rotation_while_the_encoder_is_held_suppresses_the_long_press() {
    Rig r;
    r.enterEdit();
    const uint16_t before = flexseq::input::suppressedLongPresses();

    r.press(ENCODER_SW_PIN);
    r.tick(SETTLE_MS + 1);
    r.rotate(1);
    r.tick(10);
    r.release(ENCODER_SW_PIN);
    r.tick(LONG_PRESS_MS + 50);

    TEST_ASSERT_EQUAL_UINT16_MESSAGE(before + 1, flexseq::input::suppressedLongPresses(),
        "un maintien qui a servi a tourner n est pas un appui long");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::LEVEL_EDIT, r.ui.level(),
        "l interface ne doit pas remonter d un niveau");
}

void test_a_long_press_on_shift_clears_the_pattern() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_PRESS);  // le pas 0 devient actif
    bool active = false;
    r.bank.getPattern(0)->readStep(0, active);
    TEST_ASSERT_TRUE(active);

    r.press(SHIFT_BTN_PIN);
    r.tick(SETTLE_MS + 1);
    r.release(SHIFT_BTN_PIN);
    r.tick(LONG_PRESS_MS + 50);

    r.bank.getPattern(0)->readStep(0, active);
    TEST_ASSERT_FALSE_MESSAGE(active, "l appui long delibere sur SHIFT vide le pattern");
}

void test_a_rotation_while_shift_is_held_spares_the_pattern() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_PRESS);  // le pas 0 devient actif
    const uint16_t before = flexseq::input::suppressedLongPresses();

    r.press(SHIFT_BTN_PIN);
    r.tick(SETTLE_MS + 1);
    r.rotate(1);
    r.tick(10);
    r.rotate(1);
    r.tick(10);
    r.release(SHIFT_BTN_PIN);
    r.tick(LONG_PRESS_MS + 50);

    TEST_ASSERT_EQUAL_UINT16_MESSAGE(before + 1, flexseq::input::suppressedLongPresses(),
        "SHIFT maintenu pour tourner n est pas un appui long");
    bool active = false;
    r.bank.getPattern(0)->readStep(0, active);
    TEST_ASSERT_TRUE_MESSAGE(active, "le pattern ne doit PAS avoir ete vide");
}

void test_the_guard_does_not_leak_to_the_next_hold() {
    Rig r;
    r.enterEdit();

    r.press(ENCODER_SW_PIN);
    r.tick(SETTLE_MS + 1);
    r.rotate(1);
    r.tick(10);
    r.release(ENCODER_SW_PIN);
    r.tick(LONG_PRESS_MS + 50);
    TEST_ASSERT_EQUAL(UiController::LEVEL_EDIT, r.ui.level());

    r.press(ENCODER_SW_PIN);
    r.tick(SETTLE_MS + 1);
    r.release(ENCODER_SW_PIN);
    r.tick(LONG_PRESS_MS + 50);
    TEST_ASSERT_EQUAL_MESSAGE(UiController::LEVEL_TAB, r.ui.level(),
        "le garde ne doit pas survivre au maintien suivant");
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_a_long_press_on_the_encoder_goes_back_a_level);
    RUN_TEST(test_a_rotation_while_the_encoder_is_held_suppresses_the_long_press);
    RUN_TEST(test_a_long_press_on_shift_clears_the_pattern);
    RUN_TEST(test_a_rotation_while_shift_is_held_spares_the_pattern);
    RUN_TEST(test_the_guard_does_not_leak_to_the_next_hold);

    return UNITY_END();
}
