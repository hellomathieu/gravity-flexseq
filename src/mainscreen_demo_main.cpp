#include <Arduino.h>
#include <libGravity.h>

#include <flexseq/MainScreen.h>
#include <flexseq/PatternBank.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>
#include <flexseq/UiController.h>

namespace {

flexseq::PatternBank patternBank;
flexseq::SequencerEngine engine;
flexseq::Transport transport(engine);
flexseq::UiController ui(engine, transport);

constexpr uint8_t DEMO_TAB = 2;
constexpr uint8_t DEMO_CURSOR = 2;

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
    const int8_t channel = ui.selectedChannel();
    frozen.tab = ui.currentTab();
    frozen.insideTab = (ui.level() == flexseq::UiController::LEVEL_TAB);
    frozen.cursor = ui.cursor();
    frozen.fieldOpen = ui.fieldOpen();
    frozen.fieldCount = ui.fieldCount();
    frozen.patternIndex = channel >= 0 ? engine.getSelectedPattern(static_cast<uint8_t>(channel)) : -1;
    frozen.length = channel >= 0 ? engine.getEffectiveLength(static_cast<uint8_t>(channel)) : 0;
    frozen.subdiv = channel >= 0 ? engine.getSubdiv(static_cast<uint8_t>(channel)) : 0;
    frozen.barLength = channel >= 0
        ? static_cast<uint8_t>(engine.getBarLength(static_cast<uint8_t>(channel)))
        : 0;
    frozen.tempo = ui.tempo();
    frozen.clockSource = ui.clockSource();
    char headline[6];
    flexseq::detail::headlineOf(frozen, headline);
    frozen.headlineWidth = headline[0] == '\0'
        ? 0
        : static_cast<uint8_t>(gravity.display.getStrWidth(headline));

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
    gravity.display.setFont(u8g2_font_5x7_tr);

    engine.setPatternBank(&patternBank);
    engine.setSelectedPattern(DEMO_TAB - 1, 9);
    engine.setEffectiveLength(DEMO_TAB - 1, 20);
    engine.setSubdiv(DEMO_TAB - 1, -4);
    engine.setBarLength(DEMO_TAB - 1, 3);

    while (ui.currentTab() != DEMO_TAB) {
        ui.handle(flexseq::UiController::EVENT_ROTATE, 1);
    }
    ui.handle(flexseq::UiController::EVENT_PRESS);
    if (ui.level() == flexseq::UiController::LEVEL_TAB) {
        for (uint8_t i = 0; i < DEMO_CURSOR; ++i) {
            ui.handle(flexseq::UiController::EVENT_ROTATE, 1);
        }
    }

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
