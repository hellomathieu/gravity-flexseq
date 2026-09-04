#include <flexseq/MainScreenModel.h>

#include <flexseq/SequencerEngine.h>
#include <flexseq/UiController.h>

namespace flexseq {

MainScreenModel mainScreenModelOf(const UiController& ui, const SequencerEngine& engine) {
    const int8_t channel = ui.selectedChannel();
    const bool onChannel = channel >= 0;
    const uint8_t ch = onChannel ? static_cast<uint8_t>(channel) : 0;

    MainScreenModel model;
    model.tab = ui.currentTab();
    model.insideTab = (ui.level() == UiController::LEVEL_TAB);
    model.cursor = ui.cursor();
    model.fieldOpen = ui.fieldOpen();
    model.fieldCount = ui.fieldCount();
    model.patternIndex = onChannel ? engine.getSelectedPattern(ch) : -1;
    model.length = onChannel ? engine.getBaseLength(ch) : 0;
    model.subdiv = onChannel ? engine.getSubdiv(ch) : 0;
    model.barLength = onChannel
        ? static_cast<uint8_t>(engine.getBarLength(ch))
        : 0;
    model.mode = onChannel
        ? static_cast<uint8_t>(engine.getChannelMode(ch))
        : static_cast<uint8_t>(DEFAULT_CHANNEL_MODE);
    model.offset = onChannel ? engine.getOffset(ch) : 0;
    model.skipChance = onChannel ? engine.getSkipChance(ch) : 0;
    model.mainField = static_cast<uint8_t>(ui.mainField());
    model.tempo = ui.tempo();
    model.clockSource = ui.clockSource();
    model.headlineWidth = 0;

    return model;
}

}  // namespace flexseq
