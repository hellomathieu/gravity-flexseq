#ifndef FLEXSEQ_UI_CONTROLLER_H
#define FLEXSEQ_UI_CONTROLLER_H

#include <stdint.h>

#include <flexseq/PatternBank.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>

namespace flexseq {

class UiController {
public:
    enum Event : uint8_t {
        EVENT_ROTATE,
        EVENT_ROTATE_HELD,
        EVENT_PRESS,
        EVENT_LONG_PRESS,
        EVENT_SHIFT_ROTATE,
        EVENT_SHIFT_PRESS,
        EVENT_SHIFT_LONG_PRESS,
        EVENT_PLAY_PRESS,
        EVENT_COUNT,
    };

    enum Level : uint8_t {
        LEVEL_TAB_BAR,
        LEVEL_TAB,
        LEVEL_EDIT,
    };

    enum Field : uint8_t {
        FIELD_NONE,
        FIELD_TEMPO,
        FIELD_CLOCK_SOURCE,
        FIELD_PATTERN,
        FIELD_LENGTH,
        FIELD_SUBDIV,
        FIELD_BAR_LENGTH,
        FIELD_EDIT_ENTRY,
    };

    static constexpr uint8_t TAB_COUNT = 8;
    static constexpr uint8_t TAB_CLOCK = 0;
    static constexpr uint8_t TAB_FIRST_CHANNEL = 1;
    static constexpr uint8_t TAB_SETTINGS = 7;

    static constexpr uint8_t CLOCK_TAB_FIELDS = 2;
    static constexpr uint8_t CHANNEL_TAB_FIELDS = 5;

    static constexpr uint8_t CLOCK_SOURCE_COUNT = 6;
    static constexpr uint16_t MIN_TEMPO = 30;
    static constexpr uint16_t MAX_TEMPO = 300;
    static constexpr uint16_t DEFAULT_TEMPO = 120;

    static constexpr uint8_t STEP_COUNT = Pattern::DEFAULT_TOTAL_STEPS;
    static constexpr uint8_t BAR_LENGTH_CHOICE_COUNT = 5;
    static constexpr uint8_t RATCHET_CHOICE_COUNT = 6;

    UiController(SequencerEngine& engine, PatternBank& bank, Transport& transport);

    void handle(Event event, int8_t delta = 0);

    Level level() const { return level_; }
    uint8_t currentTab() const { return currentTab_; }
    bool isChannelTab() const;
    int8_t selectedChannel() const;

    uint8_t fieldCount() const;
    Field fieldAt(uint8_t index) const;
    Field field() const { return fieldAt(cursor_); }
    uint8_t cursor() const { return cursor_; }
    bool fieldOpen() const { return fieldOpen_; }

    uint8_t stepCursor() const { return stepCursor_; }

    uint16_t tempo() const { return tempo_; }
    uint8_t clockSource() const { return clockSource_; }

private:
    void handleTabBar(Event event, int8_t delta);
    void handleTab(Event event, int8_t delta);
    void handleEdit(Event event, int8_t delta);

    void adjustField(int8_t delta);
    void adjustRatchet(int8_t delta);
    void togglePlay();
    void toggleStep();
    void clearPattern();

    Pattern* currentPattern() const;

    SequencerEngine& engine_;
    PatternBank& bank_;
    Transport& transport_;

    Level level_;
    uint8_t currentTab_;
    uint8_t cursor_;
    uint8_t stepCursor_;
    bool fieldOpen_;
    uint16_t tempo_;
    uint8_t clockSource_;
};

}  // namespace flexseq

#endif // FLEXSEQ_UI_CONTROLLER_H
