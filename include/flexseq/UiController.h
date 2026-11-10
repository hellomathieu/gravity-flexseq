#ifndef FLEXSEQ_UI_CONTROLLER_H
#define FLEXSEQ_UI_CONTROLLER_H

#include <stdint.h>

#include <flexseq/ClockSource.h>
#include <flexseq/Pattern.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>

namespace flexseq {

class UiController {
public:
    enum Event : uint8_t {
        EVENT_ROTATE,
        EVENT_PRESS,
        EVENT_LONG_PRESS,
        EVENT_SHIFT_ROTATE,
        EVENT_SHIFT_PRESS,
        EVENT_SHIFT_LONG_PRESS,
        EVENT_PLAY_PRESS,
        EVENT_SHIFT_PLAY_PRESS,
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
        FIELD_CLOCK_MODE,
        FIELD_PATTERN,
        FIELD_LENGTH,
        FIELD_SUBDIV,
        FIELD_BAR_LENGTH,
        FIELD_EDIT_ENTRY,
        FIELD_SKIP_CHANCE,
        FIELD_MODE,
        FIELD_OFFSET,
        FIELD_MOD,
        FIELD_CONFIG,
        FIELD_PPQN,
        FIELD_CALIBRATE,
        FIELD_ROTATE_SCREEN,
        FIELD_REVERSE_ENCODER,
        FIELD_FACTORY_RESET,
    };

    enum SettingsAction : uint8_t {
        SETTINGS_NONE,
        SETTINGS_CALIBRATE,
        SETTINGS_ROTATE_SCREEN,
        SETTINGS_REVERSE_ENCODER,
        SETTINGS_FACTORY_RESET,
    };

    static constexpr uint8_t TAB_COUNT = 8;
    static constexpr uint8_t TAB_CLOCK = 0;
    static constexpr uint8_t TAB_FIRST_CHANNEL = 1;
    static constexpr uint8_t TAB_SETTINGS = 7;

    static constexpr uint8_t CLOCK_TAB_FIELDS_INT = 2;
    static constexpr uint8_t CLOCK_TAB_FIELDS_EXT = 2;
    static constexpr uint8_t CLOCK_TAB_FIELDS_MIDI = 1;
    static constexpr uint8_t CHANNEL_TAB_FIELDS = 3;
    static constexpr uint8_t SEQ_CHANNEL_TAB_FIELDS = 4;
    static constexpr uint8_t CONFIG_PAGE_FIELDS = 3;
    static constexpr uint8_t SETTINGS_TAB_FIELDS = 4;

    // PRD 5.0 amendement 1quater : la grande valeur reprend la PREMIERE
    // position, et elle porte le NOM du template. Un outil qui lit ces index les
    // lit par leur NOM.
    static constexpr uint8_t SEQ_FIELD_INDEX_PATTERN = 0;
    static constexpr uint8_t SEQ_FIELD_INDEX_MODE = 1;
    static constexpr uint8_t SEQ_FIELD_INDEX_EDIT_ENTRY = 2;
    static constexpr uint8_t SEQ_FIELD_INDEX_CONFIG = 3;

    static constexpr uint8_t CONFIG_FIELD_INDEX_LENGTH = 0;
    static constexpr uint8_t CONFIG_FIELD_INDEX_SUBDIV = 1;
    static constexpr uint8_t CONFIG_FIELD_INDEX_MOD = 2;

    static constexpr uint8_t CLOCK_FIELD_INDEX_MODE_INT = 1;
    static constexpr uint8_t CLOCK_FIELD_INDEX_MODE_EXTERNAL = 0;

    static constexpr uint8_t SETTINGS_FIELD_INDEX_CALIBRATE = 0;
    static constexpr uint8_t SETTINGS_FIELD_INDEX_ROTATE_SCREEN = 1;
    static constexpr uint8_t SETTINGS_FIELD_INDEX_REVERSE_ENCODER = 2;
    static constexpr uint8_t SETTINGS_FIELD_INDEX_FACTORY_RESET = 3;


    static constexpr uint8_t CLOCK_SOURCE_COUNT = CLOCK_SOURCE_VALUE_COUNT;
    static constexpr uint8_t CLOCK_SOURCE_INTERNAL = CLOCK_SOURCE_INT;
    static constexpr uint16_t MIN_TEMPO = 20;
    static constexpr uint16_t MAX_TEMPO = 300;
#ifndef FLEXSEQ_DEFAULT_TEMPO
#define FLEXSEQ_DEFAULT_TEMPO 120
#endif
    static constexpr uint16_t DEFAULT_TEMPO = FLEXSEQ_DEFAULT_TEMPO;

    static constexpr uint8_t STEP_COUNT = 36;
    static constexpr uint8_t BAR_LENGTH_CHOICE_COUNT = 5;
    static constexpr uint8_t RATCHET_CHOICE_COUNT = flexseq::RATCHET_CHOICE_COUNT;

    UiController(SequencerEngine& engine, Transport& transport);

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

    // PRD 5.0 amendement 1quater. Le nom AFFICHE est un choix : SHIFT plus une
    // rotation le change, et la rotation dans le champ ouvert aussi. Il
    // n atteint selectedPattern qu au chargement, ce qui permet a NO de rendre
    // son nom au template joue.
    //
    // ⚠️ La sentinelle -1 dit « aucun choix en cours », donc l ecran montre ce
    // que le canal joue. Elle evite un candidat PAR CANAL : un seul octet suffit
    // parce que changer d onglet la repose.
    static constexpr int8_t NO_BROWSE = -1;
    int8_t displayedPattern() const;

    bool editIsReadOnly() const;

    uint8_t stepCursor() const { return stepCursor_; }
    bool isOnHeader() const { return onHeader_; }
    bool isOnHeaderLength() const { return onHeaderLength_; }
    bool isOnConfigPage() const { return onConfigPage_; }

    bool doneShown() const { return doneShown_; }
    bool confirmYes() const { return confirmYes_; }
    SettingsAction takeSettingsAction();

    uint8_t revision() const { return revision_; }

    uint16_t tempo() const { return tempo_; }
    uint8_t clockSource() const { return clockSource_; }

    bool setTempo(uint16_t bpm);
    bool setClockSource(uint8_t source);

    Field mainField() const;
    bool isLegacyModeTab() const;

private:
    void handleTabBar(Event event, int8_t delta);
    void handleTab(Event event, int8_t delta);
    void handleEdit(Event event, int8_t delta);

    void adjustField(int8_t delta);
    void adjustFieldValue(Field target, int8_t delta);
    void adjustRatchet(int8_t delta);
    void togglePlay();
    void toggleStep();
    void clearPattern();
    void postPatternLoad();
    void browsePattern(int8_t delta);
    void pressSettingsField();

    Pattern* currentPattern() const;

    SequencerEngine& engine_;
    Transport& transport_;

    Level level_;
    uint8_t currentTab_;
    uint8_t cursor_;
    uint8_t stepCursor_;
    bool onHeader_;
    bool onHeaderLength_;
    bool onConfigPage_;
    bool fieldOpen_;
    int8_t patternBrowse_;
    uint16_t tempo_;
    uint8_t clockSource_;
    uint8_t revision_;
    uint8_t pendingSettingsAction_;
    bool doneShown_;
    bool confirmYes_;
};

}  // namespace flexseq

#endif // FLEXSEQ_UI_CONTROLLER_H
