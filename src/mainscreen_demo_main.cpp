#include <flexseq/ChannelMode.h>
#include <flexseq/MainScreenModel.h>
#include <flexseq/OriginalFonts.h>
#include <Arduino.h>
#include <libGravity.h>

#include <flexseq/MainScreen.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>
#include <flexseq/PatternBank.h>
#include <flexseq/UiController.h>

namespace {

flexseq::SequencerEngine engine;
flexseq::PatternBank patternBank;
flexseq::Transport transport(engine);
flexseq::UiController ui(engine, transport);

constexpr uint8_t DEMO_CHANNEL = 1;
#if defined(FLEXSEQ_DEMO_CLOCK_SOURCE)
constexpr uint8_t DEMO_TAB = flexseq::UiController::TAB_CLOCK;
#else
constexpr uint8_t DEMO_TAB = flexseq::UiController::TAB_FIRST_CHANNEL + DEMO_CHANNEL;
#endif
#if defined(FLEXSEQ_DEMO_CLOCK_SOURCE)
constexpr uint8_t DEMO_CURSOR = 1;
#elif defined(FLEXSEQ_DEMO_MOD)
constexpr uint8_t DEMO_CURSOR = flexseq::UiController::SEQ_FIELD_INDEX_CONFIG;
#else
constexpr uint8_t DEMO_CURSOR = 2;
#endif

flexseq::MainScreenModel frozen;
uint8_t row = 0;
uint8_t tiles = 1;
bool busy = false;
uint32_t lastFrameMs = 0;
constexpr uint32_t FRAME_INTERVAL_MS = 100;

volatile uint16_t pendingTicks = 0;

void onOutputTick(uint32_t) {
    ++pendingTicks;
}

void freezeModel() {
    frozen = flexseq::mainScreenModelOf(ui, engine);
    tiles = gravity.display.getBufferTileHeight();
    if (tiles == 0) {
        tiles = 1;
    }
    row = 0;
    busy = true;
}

void renderBand() {
    const uint8_t last = static_cast<uint8_t>(gravity.display.getDisplayHeight() - 1);
    const uint8_t d0 = static_cast<uint8_t>(row * 8);
    uint8_t d1 = static_cast<uint8_t>(d0 + tiles * 8 - 1);
    if (d1 > last) {
        d1 = last;
    }
    const flexseq::Band band{static_cast<uint8_t>(last - d1), static_cast<uint8_t>(last - d0)};

    gravity.display.setBufferCurrTileRow(row);
    gravity.display.clearBuffer();
    drawMainScreen(gravity.display, frozen, band);
    gravity.display.sendBuffer();

    row = static_cast<uint8_t>(row + tiles);
    if (row >= gravity.display.getDisplayHeight() / 8) {
        busy = false;
    }
}

}  // namespace

void setup() {
    gravity.Init();
    engine.setPatternBank(&patternBank);
    gravity.display.setFont(flexseq::FONT_VELVETSCREEN);

#if defined(FLEXSEQ_DEMO_MODE_SEQ)
    engine.setChannelMode(DEMO_CHANNEL, flexseq::MODE_SEQ);
#endif
    engine.setSelectedPattern(DEMO_CHANNEL, 9);
    engine.setBaseLength(DEMO_CHANNEL, 20);
    engine.setSubdiv(DEMO_CHANNEL, -4);
    engine.setBarLength(DEMO_CHANNEL, 3);

#if defined(FLEXSEQ_DEMO_MOD)
    engine.setChannelMode(DEMO_CHANNEL, flexseq::MODE_SEQ);
    engine.setCvDestination(DEMO_CHANNEL, flexseq::CV_SOURCE_1, flexseq::CV_DEST_PATTERN);
    engine.setCvDestination(DEMO_CHANNEL, flexseq::CV_SOURCE_2, flexseq::CV_DEST_LENGTH);
#endif
#if defined(FLEXSEQ_DEMO_CLOCK_SOURCE)
    ui.setClockSource(FLEXSEQ_DEMO_CLOCK_SOURCE);
#endif
    while (ui.currentTab() != DEMO_TAB) {
        ui.handle(flexseq::UiController::EVENT_ROTATE, 1);
    }
    ui.handle(flexseq::UiController::EVENT_PRESS);
    if (ui.level() == flexseq::UiController::LEVEL_TAB) {
        for (uint8_t i = 0; i < DEMO_CURSOR; ++i) {
            ui.handle(flexseq::UiController::EVENT_ROTATE, 1);
        }
    }

#if defined(FLEXSEQ_DEMO_MOD)
    ui.handle(flexseq::UiController::EVENT_PRESS);
    for (uint8_t i = 0; i < flexseq::UiController::CONFIG_FIELD_INDEX_MOD; ++i) {
        ui.handle(flexseq::UiController::EVENT_ROTATE, 1);
    }
#endif

    gravity.clock.AttachIntHandler(onOutputTick);
    transport.start();
}

void loop() {
    gravity.shift_button.Process();
    gravity.play_button.Process();
    gravity.encoder.Process();

    uint16_t ticks;
    noInterrupts();
    ticks = pendingTicks;
    pendingTicks = 0;
    interrupts();

    if (ticks > 0) {
        transport.tick(ticks);
    }

    if (busy) {
        renderBand();
    } else if (millis() - lastFrameMs >= FRAME_INTERVAL_MS) {
        lastFrameMs = millis();
        freezeModel();
    }

    for (uint8_t ch = 0; ch < flexseq::SequencerEngine::CHANNEL_COUNT; ++ch) {
        gravity.outputs[ch].Process();
    }
}
