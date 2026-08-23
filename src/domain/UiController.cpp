#include <flexseq/UiController.h>

#include <flexseq/Pattern.h>
#include <flexseq/Subdiv.h>

namespace flexseq {

namespace {

uint8_t barLengthAtIndex(uint8_t index) {
    if (index == 0) return 0;
    if (index == 1) return 2;
    if (index == 2) return 3;
    if (index == 3) return 4;
    return 6;
}

uint8_t ratchetAtIndex(uint8_t index) {
    if (index == 0) return RATCHET_NONE;
    if (index == 1) return RATCHET_2;
    if (index == 2) return RATCHET_3;
    if (index == 3) return RATCHET_4;
    if (index == 4) return RATCHET_6;
    return RATCHET_TRIPLET;
}

uint8_t wrapIndex(uint8_t current, int16_t delta, uint8_t count) {
    if (count == 0) {
        return 0;
    }
    const int16_t span = static_cast<int16_t>(count);
    int16_t value = static_cast<int16_t>(static_cast<int16_t>(current) + delta) % span;
    if (value < 0) {
        value = static_cast<int16_t>(value + span);
    }
    return static_cast<uint8_t>(value);
}

int16_t clampRange(int16_t value, int16_t low, int16_t high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

uint8_t clampIndex(uint8_t current, int16_t delta, uint8_t count) {
    if (count == 0) {
        return 0;
    }
    const int16_t value = clampRange(
        static_cast<int16_t>(static_cast<int16_t>(current) + delta),
        0,
        static_cast<int16_t>(count - 1)
    );
    return static_cast<uint8_t>(value);
}

int8_t oneStep(int8_t delta) {
    if (delta > 0) {
        return 1;
    }
    if (delta < 0) {
        return -1;
    }
    return 0;
}

int8_t indexOfChoice(uint8_t (*choiceAt)(uint8_t), uint8_t count, uint8_t value) {
    for (uint8_t index = 0; index < count; ++index) {
        if (choiceAt(index) == value) {
            return static_cast<int8_t>(index);
        }
    }
    return -1;
}

}  // namespace

UiController::UiController(SequencerEngine& engine, PatternBank& bank, Transport& transport)
    : engine_(engine),
      bank_(bank),
      transport_(transport),
      level_(LEVEL_TAB_BAR),
      currentTab_(TAB_FIRST_CHANNEL),
      cursor_(0),
      stepCursor_(0),
      fieldOpen_(false),
      tempo_(DEFAULT_TEMPO),
      clockSource_(0),
      revision_(0) {}

bool UiController::isChannelTab() const {
    return currentTab_ >= TAB_FIRST_CHANNEL
        && currentTab_ < TAB_FIRST_CHANNEL + SequencerEngine::CHANNEL_COUNT;
}

int8_t UiController::selectedChannel() const {
    if (!isChannelTab()) {
        return -1;
    }
    return static_cast<int8_t>(currentTab_ - TAB_FIRST_CHANNEL);
}

uint8_t UiController::fieldCount() const {
    if (currentTab_ == TAB_CLOCK) {
        return CLOCK_TAB_FIELDS;
    }
    if (isChannelTab()) {
        return CHANNEL_TAB_FIELDS;
    }
    return 0;
}

UiController::Field UiController::fieldAt(uint8_t index) const {
    if (index >= fieldCount()) {
        return FIELD_NONE;
    }
    if (currentTab_ == TAB_CLOCK) {
        return index == 0 ? FIELD_TEMPO : FIELD_CLOCK_SOURCE;
    }
    switch (index) {
        case 0: return FIELD_PATTERN;
        case 1: return FIELD_LENGTH;
        case 2: return FIELD_SUBDIV;
        case 3: return FIELD_BAR_LENGTH;
        default: return FIELD_EDIT_ENTRY;
    }
}

Pattern* UiController::currentPattern() const {
    const int8_t channel = selectedChannel();
    if (channel < 0) {
        return nullptr;
    }
    const int8_t index = engine_.getSelectedPattern(static_cast<uint8_t>(channel));
    if (index < 0) {
        return nullptr;
    }
    return bank_.getPattern(static_cast<uint8_t>(index));
}

void UiController::handle(Event event, int8_t delta) {
    ++revision_;
    if (event == EVENT_PLAY_PRESS) {
        togglePlay();
        return;
    }
    if (event == EVENT_SHIFT_PRESS) {
        return;
    }
    switch (level_) {
        case LEVEL_TAB_BAR: handleTabBar(event, delta); break;
        case LEVEL_TAB: handleTab(event, delta); break;
        case LEVEL_EDIT: handleEdit(event, delta); break;
    }
}

void UiController::handleTabBar(Event event, int8_t delta) {
    switch (event) {
        case EVENT_ROTATE:
            currentTab_ = wrapIndex(currentTab_, oneStep(delta), TAB_COUNT);
            cursor_ = 0;
            fieldOpen_ = false;
            break;
        case EVENT_PRESS:
            if (fieldCount() > 0) {
                level_ = LEVEL_TAB;
                cursor_ = 0;
                fieldOpen_ = false;
            }
            break;
        default:
            break;
    }
}

void UiController::handleTab(Event event, int8_t delta) {
    switch (event) {
        case EVENT_ROTATE:
            if (fieldOpen_) {
                adjustField(delta);
            } else {
                cursor_ = wrapIndex(cursor_, oneStep(delta), fieldCount());
            }
            break;
        case EVENT_SHIFT_ROTATE:
            adjustField(delta);
            break;
        case EVENT_PRESS:
            if (fieldOpen_) {
                fieldOpen_ = false;
            } else if (field() == FIELD_EDIT_ENTRY) {
                level_ = LEVEL_EDIT;
                stepCursor_ = 0;
            } else if (field() != FIELD_NONE) {
                fieldOpen_ = true;
            }
            break;
        case EVENT_LONG_PRESS:
            if (fieldOpen_) {
                fieldOpen_ = false;
            } else {
                level_ = LEVEL_TAB_BAR;
            }
            break;
        default:
            break;
    }
}

void UiController::handleEdit(Event event, int8_t delta) {
    switch (event) {
        case EVENT_ROTATE:
            stepCursor_ = wrapIndex(stepCursor_, oneStep(delta), STEP_COUNT);
            break;
        case EVENT_ROTATE_HELD:
            adjustRatchet(delta);
            break;
        case EVENT_PRESS:
            toggleStep();
            break;
        case EVENT_LONG_PRESS:
            level_ = LEVEL_TAB;
            fieldOpen_ = false;
            break;
        case EVENT_SHIFT_ROTATE: {
            const int8_t channel = selectedChannel();
            if (channel >= 0) {
                const uint8_t next = wrapIndex(
                    static_cast<uint8_t>(channel), oneStep(delta),
                    SequencerEngine::CHANNEL_COUNT
                );
                currentTab_ = static_cast<uint8_t>(TAB_FIRST_CHANNEL + next);
            }
            break;
        }
        case EVENT_SHIFT_LONG_PRESS:
            clearPattern();
            break;
        default:
            break;
    }
}

bool UiController::setTempo(uint16_t bpm) {
    if (bpm < MIN_TEMPO || bpm > MAX_TEMPO) {
        return false;
    }
    tempo_ = bpm;
    ++revision_;
    return true;
}

bool UiController::setClockSource(uint8_t source) {
    if (source >= CLOCK_SOURCE_COUNT) {
        return false;
    }
    clockSource_ = source;
    ++revision_;
    return true;
}

void UiController::adjustField(int8_t accelerated) {
    const int8_t channel = selectedChannel();
    const int8_t delta = field() == FIELD_TEMPO ? accelerated : oneStep(accelerated);
    switch (field()) {
        case FIELD_TEMPO:
            tempo_ = static_cast<uint16_t>(clampRange(
                static_cast<int16_t>(static_cast<int16_t>(tempo_) + delta),
                static_cast<int16_t>(MIN_TEMPO),
                static_cast<int16_t>(MAX_TEMPO)
            ));
            break;
        case FIELD_CLOCK_SOURCE:
            clockSource_ = clampIndex(clockSource_, delta, CLOCK_SOURCE_COUNT);
            break;
        case FIELD_PATTERN: {
            if (channel < 0) {
                break;
            }
            const int8_t current = engine_.getSelectedPattern(static_cast<uint8_t>(channel));
            if (current < 0) {
                break;
            }
            engine_.setSelectedPattern(
                static_cast<uint8_t>(channel),
                clampIndex(static_cast<uint8_t>(current), delta, SequencerEngine::PATTERN_COUNT)
            );
            break;
        }
        case FIELD_LENGTH: {
            if (channel < 0) {
                break;
            }
            const uint8_t current = engine_.getEffectiveLength(static_cast<uint8_t>(channel));
            engine_.setEffectiveLength(
                static_cast<uint8_t>(channel),
                static_cast<uint8_t>(clampRange(
                    static_cast<int16_t>(static_cast<int16_t>(current) + delta),
                    static_cast<int16_t>(SequencerEngine::MIN_LENGTH),
                    static_cast<int16_t>(SequencerEngine::MAX_LENGTH)
                ))
            );
            break;
        }
        case FIELD_SUBDIV: {
            if (channel < 0) {
                break;
            }
            int8_t index = subdivIndexOf(engine_.getSubdiv(static_cast<uint8_t>(channel)));
            if (index < 0) {
                index = static_cast<int8_t>(DEFAULT_SUBDIV_INDEX);
            }
            const uint8_t next = clampIndex(
                static_cast<uint8_t>(index), delta, SUBDIV_CHOICE_COUNT
            );
            engine_.setSubdiv(static_cast<uint8_t>(channel), subdivAtIndex(next));
            break;
        }
        case FIELD_BAR_LENGTH: {
            if (channel < 0) {
                break;
            }
            const int8_t current = engine_.getBarLength(static_cast<uint8_t>(channel));
            if (current < 0) {
                break;
            }
            int8_t index = indexOfChoice(
                barLengthAtIndex, BAR_LENGTH_CHOICE_COUNT, static_cast<uint8_t>(current)
            );
            if (index < 0) {
                index = 0;
            }
            const uint8_t next = clampIndex(
                static_cast<uint8_t>(index), delta, BAR_LENGTH_CHOICE_COUNT
            );
            engine_.setBarLength(static_cast<uint8_t>(channel), barLengthAtIndex(next));
            break;
        }
        default:
            break;
    }
}

void UiController::adjustRatchet(int8_t delta) {
    Pattern* pattern = currentPattern();
    if (pattern == nullptr) {
        return;
    }
    const int8_t channel = selectedChannel();
    if (channel < 0) {
        return;
    }
    const int8_t step = oneStep(delta);
    if (step == 0) {
        return;
    }
    const uint16_t ticks = engine_.getTicksPerStep(static_cast<uint8_t>(channel));

    int8_t index = indexOfChoice(
        ratchetAtIndex, RATCHET_CHOICE_COUNT, pattern->getRatchet(stepCursor_)
    );
    if (index < 0) {
        index = 0;
    }

    uint8_t cursor = static_cast<uint8_t>(index);
    for (uint8_t tried = 0; tried < RATCHET_CHOICE_COUNT; ++tried) {
        const uint8_t candidate = clampIndex(cursor, step, RATCHET_CHOICE_COUNT);
        if (candidate == cursor) {
            return;
        }
        cursor = candidate;
        if (ratchetFitsStep(ratchetAtIndex(cursor), ticks)) {
            pattern->setRatchet(stepCursor_, ratchetAtIndex(cursor));
            engine_.refreshTiming(static_cast<uint8_t>(channel));
            return;
        }
    }
}

void UiController::togglePlay() {
    if (engine_.isRunning()) {
        transport_.stop();
    } else {
        transport_.start();
    }
}

void UiController::toggleStep() {
    Pattern* pattern = currentPattern();
    if (pattern == nullptr) {
        return;
    }
    bool active = false;
    if (!pattern->readStep(stepCursor_, active)) {
        return;
    }
    pattern->writeStep(stepCursor_, !active);
}

void UiController::clearPattern() {
    Pattern* pattern = currentPattern();
    if (pattern == nullptr) {
        return;
    }
    pattern->clear();
    engine_.refreshTiming();
}

}  // namespace flexseq
