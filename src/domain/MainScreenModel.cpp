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
    model.stepTicks = onChannel ? engine.currentStepTicks(ch) : 0;
    switch (ui.mainField()) {
        case UiController::FIELD_TEMPO:       model.mainParameter = MAIN_TEMPO; break;
        case UiController::FIELD_SUBDIV:      model.mainParameter = MAIN_SUBDIV; break;
        case UiController::FIELD_SKIP_CHANCE: model.mainParameter = MAIN_SKIP_CHANCE; break;
        case UiController::FIELD_PATTERN:     model.mainParameter = MAIN_PATTERN; break;
        default:                              model.mainParameter = MAIN_NONE; break;
    }
    model.legacyLayout = ui.isLegacyModeTab();
    model.tempo = ui.tempo();
    model.clockSource = ui.clockSource();
    model.headlineWidth = 0;
    model.mainValueWidth = 0;
    model.mainLabelWidth = 0;
    model.lineLabelWidth[0] = 0;
    model.lineLabelWidth[1] = 0;
    model.lineLabelWidth[2] = 0;
    model.lineValueWidth = 0;

    return model;
}

}  // namespace flexseq
