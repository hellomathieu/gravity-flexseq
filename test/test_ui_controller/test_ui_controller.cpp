#include <stdint.h>
#include <unity.h>

#include <flexseq/Pattern.h>
#include <flexseq/PatternBank.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Subdiv.h>
#include <flexseq/Transport.h>
#include <flexseq/CvDestination.h>
#include <flexseq/UiController.h>
#include <flexseq/Persistence.h>
#include <flexseq/UiFrame.h>

using flexseq::Pattern;
using flexseq::SequencerEngine;
using flexseq::Transport;
using flexseq::UiController;

void setUp() {}
void tearDown() {}

namespace {

struct Rig {
    flexseq::PatternBank bank;
    SequencerEngine engine;
    Transport transport;
    UiController ui;

    Rig() : engine(), transport(engine), ui(engine, transport) {
        engine.setPatternBank(&bank);
        for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
            engine.setChannelMode(ch, flexseq::MODE_SEQ);
        }
    }

    void enterTab() { ui.handle(UiController::EVENT_PRESS); }

    void gotoTab(uint8_t tab) {
        for (uint8_t guard = 0; guard < UiController::TAB_COUNT; ++guard) {
            if (ui.currentTab() == tab) {
                return;
            }
            ui.handle(UiController::EVENT_ROTATE, 1);
        }
        TEST_FAIL_MESSAGE("tab never reached");
    }

    // LENGTH, SUBDIV et MOD ont quitte l'onglet pour la page CONFIG au lot 12 :
    // le harnais y entre de lui-meme, sinon chaque test le repeterait.
    void gotoField(UiController::Field field) {
        const bool onConfig = (field == UiController::FIELD_LENGTH
                               || field == UiController::FIELD_SUBDIV
                               || field == UiController::FIELD_MOD);
        if (onConfig && !ui.isOnConfigPage()) {
            gotoField(UiController::FIELD_CONFIG);
            ui.handle(UiController::EVENT_PRESS);
        }
        for (uint8_t guard = 0; guard <= UiController::CHANNEL_TAB_FIELDS; ++guard) {
            if (ui.field() == field) {
                return;
            }
            ui.handle(UiController::EVENT_ROTATE, 1);
        }
        TEST_FAIL_MESSAGE("field never reached");
    }

    void enterEdit() {
        enterTab();
        gotoField(UiController::FIELD_EDIT_ENTRY);
        ui.handle(UiController::EVENT_PRESS);
    }
};

}  // namespace

/*
 * Tab bar
 */

void test_starts_on_the_tab_bar_over_the_first_channel() {
    Rig r;
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB_BAR, r.ui.level());
    TEST_ASSERT_EQUAL_UINT8(UiController::TAB_FIRST_CHANNEL, r.ui.currentTab());
    TEST_ASSERT_EQUAL_INT8(0, r.ui.selectedChannel());
    TEST_ASSERT_FALSE(r.ui.fieldOpen());
}

void test_rotate_changes_tab() {
    Rig r;
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(2, r.ui.currentTab());
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_UINT8(1, r.ui.currentTab());
}

void test_rotate_wraps_at_both_ends_of_the_tab_bar() {
    Rig r;
    r.gotoTab(UiController::TAB_SETTINGS);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(UiController::TAB_CLOCK, r.ui.currentTab());
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_UINT8(UiController::TAB_SETTINGS, r.ui.currentTab());
}

void test_an_accelerated_delta_moves_one_tab_not_three() {
    Rig r;
    r.ui.handle(UiController::EVENT_ROTATE, 3);
    TEST_ASSERT_EQUAL_UINT8(2, r.ui.currentTab());
    r.ui.handle(UiController::EVENT_ROTATE, -3);
    TEST_ASSERT_EQUAL_UINT8(1, r.ui.currentTab());
}

void test_press_enters_a_tab_that_has_fields() {
    Rig r;
    r.enterTab();
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB, r.ui.level());
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.cursor());
    // Le rig met les six canaux en SEQ, donc le premier champ est la grande
    // valeur — PRD 5.0 amendement 1quater. MODE suit, un cran plus loin.
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_PATTERN, r.ui.field(),
        "on choisit le pattern avant de l editer");
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MODE, r.ui.field(),
        "MODE est la ligne 1 des trois");
}

void test_the_settings_tab_holds_the_four_entries_of_the_original() {
    Rig r;
    r.gotoTab(UiController::TAB_SETTINGS);
    TEST_ASSERT_EQUAL_UINT8(4, r.ui.fieldCount());
    TEST_ASSERT_EQUAL(UiController::FIELD_CALIBRATE, r.ui.fieldAt(0));
    TEST_ASSERT_EQUAL(UiController::FIELD_ROTATE_SCREEN, r.ui.fieldAt(1));
    TEST_ASSERT_EQUAL(UiController::FIELD_REVERSE_ENCODER, r.ui.fieldAt(2));
    TEST_ASSERT_EQUAL(UiController::FIELD_FACTORY_RESET, r.ui.fieldAt(3));
    TEST_ASSERT_EQUAL(UiController::FIELD_NONE, r.ui.fieldAt(4));
    r.enterTab();
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB, r.ui.level());
    TEST_ASSERT_EQUAL(UiController::SETTINGS_NONE, r.ui.takeSettingsAction());
}

namespace {
void gotoSettingsField(Rig& r, uint8_t index) {
    r.gotoTab(UiController::TAB_SETTINGS);
    r.enterTab();
    for (uint8_t i = 0; i < index; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT8(index, r.ui.cursor());
}
}  // namespace

void test_calibrate_asks_for_the_action_once_and_shows_done() {
    Rig r;
    gotoSettingsField(r, 0);
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_TRUE(r.ui.doneShown());
    TEST_ASSERT_FALSE(r.ui.fieldOpen());
    TEST_ASSERT_EQUAL(UiController::SETTINGS_CALIBRATE, r.ui.takeSettingsAction());
    TEST_ASSERT_EQUAL(UiController::SETTINGS_NONE, r.ui.takeSettingsAction());
}

void test_rotate_screen_asks_for_the_action_and_shows_no_done() {
    Rig r;
    gotoSettingsField(r, 1);
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_FALSE(r.ui.doneShown());
    TEST_ASSERT_FALSE(r.ui.fieldOpen());
    TEST_ASSERT_EQUAL(UiController::SETTINGS_ROTATE_SCREEN, r.ui.takeSettingsAction());
}

void test_reverse_encoder_asks_for_the_action_and_shows_done() {
    Rig r;
    gotoSettingsField(r, 2);
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_TRUE(r.ui.doneShown());
    TEST_ASSERT_EQUAL(UiController::SETTINGS_REVERSE_ENCODER, r.ui.takeSettingsAction());
}

void test_the_press_that_clears_done_does_nothing_else() {
    Rig r;
    gotoSettingsField(r, 0);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.takeSettingsAction();
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_TRUE(r.ui.doneShown());
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.cursor());
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_FALSE(r.ui.doneShown());
    TEST_ASSERT_EQUAL(UiController::SETTINGS_NONE, r.ui.takeSettingsAction());
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB, r.ui.level());
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_TRUE(r.ui.doneShown());
    r.ui.handle(UiController::EVENT_LONG_PRESS);
    TEST_ASSERT_FALSE(r.ui.doneShown());
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB, r.ui.level());
}

void test_factory_reset_asks_first_and_no_comes_first() {
    Rig r;
    gotoSettingsField(r, 3);
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_TRUE(r.ui.fieldOpen());
    TEST_ASSERT_FALSE(r.ui.confirmYes());
    TEST_ASSERT_EQUAL(UiController::SETTINGS_NONE, r.ui.takeSettingsAction());
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_FALSE(r.ui.fieldOpen());
    TEST_ASSERT_FALSE(r.ui.doneShown());
    TEST_ASSERT_EQUAL(UiController::SETTINGS_NONE, r.ui.takeSettingsAction());
}

void test_factory_reset_runs_on_yes_only() {
    Rig r;
    gotoSettingsField(r, 3);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_TRUE(r.ui.confirmYes());
    TEST_ASSERT_EQUAL_UINT8(3, r.ui.cursor());
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_TRUE(r.ui.confirmYes());
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_FALSE(r.ui.confirmYes());
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_FALSE(r.ui.fieldOpen());
    TEST_ASSERT_TRUE(r.ui.doneShown());
    TEST_ASSERT_EQUAL(UiController::SETTINGS_FACTORY_RESET, r.ui.takeSettingsAction());
}

void test_a_long_press_cancels_the_factory_reset_question() {
    Rig r;
    gotoSettingsField(r, 3);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    r.ui.handle(UiController::EVENT_LONG_PRESS);
    TEST_ASSERT_FALSE(r.ui.fieldOpen());
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB, r.ui.level());
    TEST_ASSERT_EQUAL(UiController::SETTINGS_NONE, r.ui.takeSettingsAction());
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_FALSE_MESSAGE(r.ui.confirmYes(), "the question starts again at NO");
}

void test_shift_rotation_never_answers_the_factory_reset_question() {
    Rig r;
    gotoSettingsField(r, 3);
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_FALSE(r.ui.confirmYes());
    TEST_ASSERT_FALSE(r.ui.fieldOpen());
}


void test_the_internal_clock_exposes_the_tempo_then_the_mode() {
    Rig r;
    r.gotoTab(UiController::TAB_CLOCK);
    TEST_ASSERT_EQUAL_UINT8(2, r.ui.fieldCount());
    TEST_ASSERT_EQUAL(UiController::FIELD_TEMPO, r.ui.fieldAt(0));
    TEST_ASSERT_EQUAL(UiController::FIELD_CLOCK_MODE, r.ui.fieldAt(1));
    TEST_ASSERT_EQUAL(UiController::FIELD_NONE, r.ui.fieldAt(2));
    TEST_ASSERT_EQUAL(UiController::FIELD_TEMPO, r.ui.mainField());
    TEST_ASSERT_EQUAL_INT8(-1, r.ui.selectedChannel());
}

void test_the_external_clock_exposes_the_mode_then_the_ppqn() {
    for (uint8_t source = 1; source <= 4; ++source) {
        Rig r;
        r.gotoTab(UiController::TAB_CLOCK);
        TEST_ASSERT_TRUE(r.ui.setClockSource(source));
        TEST_ASSERT_EQUAL_UINT8(2, r.ui.fieldCount());
        TEST_ASSERT_EQUAL(UiController::FIELD_CLOCK_MODE, r.ui.fieldAt(0));
        TEST_ASSERT_EQUAL(UiController::FIELD_PPQN, r.ui.fieldAt(1));
        TEST_ASSERT_EQUAL(UiController::FIELD_NONE, r.ui.fieldAt(2));
        TEST_ASSERT_EQUAL(UiController::FIELD_NONE, r.ui.mainField());
    }
}

void test_the_midi_clock_exposes_the_mode_alone() {
    Rig r;
    r.gotoTab(UiController::TAB_CLOCK);
    TEST_ASSERT_TRUE(r.ui.setClockSource(5));
    TEST_ASSERT_EQUAL_UINT8(1, r.ui.fieldCount());
    TEST_ASSERT_EQUAL(UiController::FIELD_CLOCK_MODE, r.ui.fieldAt(0));
    TEST_ASSERT_EQUAL(UiController::FIELD_NONE, r.ui.fieldAt(1));
    TEST_ASSERT_EQUAL(UiController::FIELD_NONE, r.ui.mainField());
}

void test_the_mode_and_the_ppqn_are_two_views_of_the_source_byte() {
    const uint8_t modes[] = {0, 1, 1, 1, 1, 2};
    const uint8_t rates[] = {0, 24, 4, 2, 1, 0};
    for (uint8_t source = 0; source < 6; ++source) {
        TEST_ASSERT_EQUAL_UINT8(modes[source], flexseq::clockModeOf(source));
        if (modes[source] == 1) {
            TEST_ASSERT_EQUAL_UINT8(rates[source],
                flexseq::ppqnAtIndex(flexseq::ppqnIndexOf(source)));
            TEST_ASSERT_EQUAL_UINT8(source, flexseq::clockSourceOf(
                flexseq::CLOCK_MODE_EXT, flexseq::ppqnIndexOf(source)));
        } else {
            TEST_ASSERT_EQUAL_UINT8(0, flexseq::ppqnIndexOf(source));
        }
    }
    TEST_ASSERT_EQUAL_UINT8(0, flexseq::clockSourceOf(flexseq::CLOCK_MODE_INT, 3));
    TEST_ASSERT_EQUAL_UINT8(5, flexseq::clockSourceOf(flexseq::CLOCK_MODE_MIDI, 3));
    TEST_ASSERT_EQUAL_UINT8(1, flexseq::clockSourceOf(flexseq::CLOCK_MODE_EXT, 4));
    TEST_ASSERT_EQUAL_UINT8(6, UiController::CLOCK_SOURCE_COUNT);
}

void test_the_mode_field_walks_int_ext_midi_and_stops_at_both_ends() {
    Rig r;
    r.gotoTab(UiController::TAB_CLOCK);
    r.enterTab();
    r.gotoField(UiController::FIELD_CLOCK_MODE);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.clockSource());
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(1, r.ui.clockSource());
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(5, r.ui.clockSource());
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(5, r.ui.clockSource());
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_UINT8(1, r.ui.clockSource());
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.clockSource());
}

void test_the_cursor_follows_the_mode_field_when_the_mode_moves_it() {
    Rig r;
    r.gotoTab(UiController::TAB_CLOCK);
    r.enterTab();
    r.gotoField(UiController::FIELD_CLOCK_MODE);
    TEST_ASSERT_EQUAL_UINT8(1, r.ui.cursor());
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.cursor());
    TEST_ASSERT_EQUAL(UiController::FIELD_CLOCK_MODE, r.ui.field());
    TEST_ASSERT_TRUE(r.ui.fieldOpen());
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.cursor());
    TEST_ASSERT_EQUAL(UiController::FIELD_CLOCK_MODE, r.ui.field());
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.clockSource());
    TEST_ASSERT_EQUAL_UINT8(1, r.ui.cursor());
    TEST_ASSERT_EQUAL(UiController::FIELD_CLOCK_MODE, r.ui.field());
}

void test_the_ppqn_field_walks_24_4_2_1_and_never_leaves_the_external_mode() {
    Rig r;
    r.gotoTab(UiController::TAB_CLOCK);
    TEST_ASSERT_TRUE(r.ui.setClockSource(1));
    r.enterTab();
    r.gotoField(UiController::FIELD_PPQN);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_UINT8(1, r.ui.clockSource());
    for (uint8_t expected = 2; expected <= 4; ++expected) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
        TEST_ASSERT_EQUAL_UINT8(expected, r.ui.clockSource());
    }
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(4, r.ui.clockSource());
    TEST_ASSERT_EQUAL(UiController::FIELD_PPQN, r.ui.field());
}

void test_a_mode_that_does_not_change_keeps_the_ppqn() {
    Rig r;
    r.gotoTab(UiController::TAB_CLOCK);
    TEST_ASSERT_TRUE(r.ui.setClockSource(3));
    r.enterTab();
    r.gotoField(UiController::FIELD_CLOCK_MODE);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, 0);
    TEST_ASSERT_EQUAL_UINT8(3, r.ui.clockSource());
}

void test_entering_the_external_mode_starts_at_24_ppqn() {
    Rig r;
    r.gotoTab(UiController::TAB_CLOCK);
    TEST_ASSERT_TRUE(r.ui.setClockSource(5));
    r.enterTab();
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_UINT8(1, r.ui.clockSource());
}

void test_shift_rotation_on_the_bar_leaves_the_tempo_alone_off_the_internal_clock() {
    const uint8_t sources[] = {1, 5};
    for (uint8_t i = 0; i < 2; ++i) {
        Rig r;
        r.gotoTab(UiController::TAB_CLOCK);
        TEST_ASSERT_TRUE(r.ui.setClockSource(sources[i]));
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
        TEST_ASSERT_EQUAL_UINT16(120, r.ui.tempo());
        TEST_ASSERT_EQUAL_UINT8(sources[i], r.ui.clockSource());
    }
}

void test_the_other_gestures_do_nothing_on_the_tab_bar() {
    Rig r;
    const UiController::Event inert[] = {
        UiController::EVENT_LONG_PRESS,
        UiController::EVENT_SHIFT_PRESS,
        UiController::EVENT_SHIFT_LONG_PRESS,
        UiController::EVENT_SHIFT_PLAY_PRESS,
    };
    for (uint8_t i = 0; i < sizeof(inert) / sizeof(inert[0]); ++i) {
        r.ui.handle(inert[i], 1);
        TEST_ASSERT_EQUAL(UiController::LEVEL_TAB_BAR, r.ui.level());
        TEST_ASSERT_EQUAL_UINT8(UiController::TAB_FIRST_CHANNEL, r.ui.currentTab());
    }
}

/*
 * Inside a tab
 */

void test_rotate_moves_the_field_cursor_and_wraps() {
    Rig r;
    r.enterTab();
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL(UiController::FIELD_MODE, r.ui.field());
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL(UiController::FIELD_PATTERN, r.ui.field());
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL(UiController::FIELD_CONFIG, r.ui.field());
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_PATTERN, r.ui.field(),
        "la liste boucle sur son premier champ, la grande valeur");
}

void test_press_opens_a_value_field_and_press_closes_it() {
    Rig r;
    r.enterTab();
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_TRUE(r.ui.fieldOpen());
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_FALSE(r.ui.fieldOpen());
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB, r.ui.level());
}

void test_rotate_changes_the_value_while_the_field_is_open() {
    Rig r;
    r.enterTab();
    r.gotoField(UiController::FIELD_LENGTH);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(SequencerEngine::DEFAULT_LENGTH + 1, r.engine.getEffectiveLength(0));
    TEST_ASSERT_EQUAL(UiController::FIELD_LENGTH, r.ui.field());
}

void test_long_press_closes_the_open_field_without_leaving_the_tab() {
    Rig r;
    r.enterTab();
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_TRUE(r.ui.fieldOpen());
    r.ui.handle(UiController::EVENT_LONG_PRESS);
    TEST_ASSERT_FALSE(r.ui.fieldOpen());
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB, r.ui.level());
}

void test_long_press_on_a_closed_field_returns_to_the_tab_bar() {
    Rig r;
    r.enterTab();
    r.ui.handle(UiController::EVENT_LONG_PRESS);
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB_BAR, r.ui.level());
    TEST_ASSERT_EQUAL_UINT8(UiController::TAB_FIRST_CHANNEL, r.ui.currentTab());
}

void test_shift_rotate_changes_the_value_without_opening_the_field() {
    Rig r;
    r.enterTab();
    r.gotoField(UiController::FIELD_LENGTH);
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    TEST_ASSERT_FALSE(r.ui.fieldOpen());
    TEST_ASSERT_EQUAL_UINT8(SequencerEngine::DEFAULT_LENGTH - 1, r.engine.getEffectiveLength(0));
}

void test_the_tempo_range_is_the_one_the_module_announces() {
    TEST_ASSERT_EQUAL_UINT16(20, UiController::MIN_TEMPO);
    TEST_ASSERT_EQUAL_UINT16(300, UiController::MAX_TEMPO);
    Rig r;
    TEST_ASSERT_FALSE(r.ui.setTempo(19));
    TEST_ASSERT_TRUE(r.ui.setTempo(20));
    TEST_ASSERT_TRUE(r.ui.setTempo(300));
    TEST_ASSERT_FALSE(r.ui.setTempo(301));
}

void test_tempo_is_clamped_to_the_musical_range() {
    Rig r;
    r.gotoTab(UiController::TAB_CLOCK);
    r.enterTab();
    TEST_ASSERT_EQUAL_UINT16(UiController::DEFAULT_TEMPO, r.ui.tempo());
    for (uint16_t i = 0; i < UiController::MAX_TEMPO + 10; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT16(UiController::MAX_TEMPO, r.ui.tempo());
    for (uint16_t i = 0; i < UiController::MAX_TEMPO + 10; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_UINT16(UiController::MIN_TEMPO, r.ui.tempo());
}

void test_the_clock_fields_never_reach_the_sentinel() {
    Rig r;
    r.gotoTab(UiController::TAB_CLOCK);
    r.enterTab();
    r.gotoField(UiController::FIELD_CLOCK_MODE);
    for (uint8_t i = 0; i < 11; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
        TEST_ASSERT_TRUE(r.ui.clockSource() < 6);
    }
    TEST_ASSERT_EQUAL_UINT8(5, r.ui.clockSource());
    for (uint8_t i = 0; i < 11; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.clockSource());
}

// ⚠️ SHIFT plus rotation CHOISIT le nom et ne charge pas — PRD 5.0 amendement
// 1quater. Ce qui se deplace est donc le nom AFFICHE, et le canal garde le sien
// tant qu un appui court dans le champ n a pas charge.
void test_the_displayed_name_is_clamped_to_the_bank() {
    Rig r;
    for (uint8_t i = 0; i < SequencerEngine::PATTERN_COUNT + 5; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_INT8(15, r.ui.displayedPattern());
    for (uint8_t i = 0; i < SequencerEngine::PATTERN_COUNT + 5; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_INT8(0, r.ui.displayedPattern());
    TEST_ASSERT_EQUAL_INT8_MESSAGE(0, r.engine.getSelectedPattern(0),
        "et le canal n a jamais charge");
}

void test_the_length_field_edits_the_base_and_never_the_derived_value() {
    Rig r;
    r.enterTab();
    r.gotoField(UiController::FIELD_LENGTH);
    TEST_ASSERT_TRUE(r.engine.setBaseLength(0, 18));
    TEST_ASSERT_TRUE(r.engine.setCvDestination(0, flexseq::CV_SOURCE_1,
                                               flexseq::CV_DEST_LENGTH));
    r.engine.setCvInput(flexseq::CV_SOURCE_1, 330); // zone +10
    r.engine.start();
    r.engine.advance(96);
    TEST_ASSERT_EQUAL_UINT8(18, r.engine.getBaseLength(0));
    TEST_ASSERT_EQUAL_UINT8(28, r.engine.getEffectiveLength(0));

    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(19, r.engine.getBaseLength(0),
        "un cran part de la BASE : partir de la derivee donnerait 29");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(29, r.engine.getEffectiveLength(0),
        "la derivee suit la nouvelle base : partir de la derivee donnerait 36");
}

void test_the_length_field_leaves_the_modulation_in_place() {
    Rig r;
    r.enterTab();
    r.gotoField(UiController::FIELD_LENGTH);
    TEST_ASSERT_TRUE(r.engine.setBaseLength(0, 18));
    TEST_ASSERT_TRUE(r.engine.setCvDestination(0, flexseq::CV_SOURCE_1,
                                               flexseq::CV_DEST_LENGTH));
    r.engine.setCvInput(flexseq::CV_SOURCE_1, -330); // zone -10
    r.engine.start();
    r.engine.advance(96);
    TEST_ASSERT_EQUAL_UINT8(8, r.engine.getEffectiveLength(0));

    for (uint8_t i = 0; i < 3; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_UINT8(15, r.engine.getBaseLength(0));
    TEST_ASSERT_EQUAL_UINT8(5, r.engine.getEffectiveLength(0));
}

void test_the_length_field_is_clamped_to_one_and_thirty_six() {
    Rig r;
    r.enterTab();
    r.gotoField(UiController::FIELD_LENGTH);
    for (uint8_t i = 0; i < 40; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT8(36, r.engine.getEffectiveLength(0));
    for (uint8_t i = 0; i < 40; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_UINT8(1, r.engine.getEffectiveLength(0));
}

void test_a_turn_to_the_right_multiplies_the_subdiv_and_a_turn_to_the_left_divides_it() {
    Rig r;
    r.enterTab();
    r.gotoField(UiController::FIELD_SUBDIV);
    TEST_ASSERT_EQUAL_INT16(1, r.engine.getSubdiv(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT16_MESSAGE(-2, r.engine.getSubdiv(0), "un cran a droite : x2");
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -2);
    TEST_ASSERT_EQUAL_INT16_MESSAGE(1, r.engine.getSubdiv(0), "un geste vaut un cran");
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    TEST_ASSERT_EQUAL_INT16_MESSAGE(2, r.engine.getSubdiv(0), "un cran a gauche : /2");
    for (uint8_t i = 0; i < 30; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_INT16_MESSAGE(-24, r.engine.getSubdiv(0), "butee a droite : x24");
    for (uint8_t i = 0; i < 30; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_INT16_MESSAGE(128, r.engine.getSubdiv(0), "butee a gauche : /128");
}

// ⚠️ SEP a quitte l'onglet pour l'EN-TETE de l'ecran EDIT au lot 12 : un cran a
// gauche depuis le step 0 l'y atteint, et la rotation la fait defiler.
void test_the_bar_length_field_walks_only_the_allowed_values() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_EQUAL_INT8(SequencerEngine::DEFAULT_BAR_LENGTH, r.engine.getBarLength(0));
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT8(6, r.engine.getBarLength(0));
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT8(6, r.engine.getBarLength(0));
    for (uint8_t i = 0; i < UiController::BAR_LENGTH_CHOICE_COUNT + 3; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_INT8(SequencerEngine::BAR_NONE, r.engine.getBarLength(0));
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT8(2, r.engine.getBarLength(0));
}

void test_no_field_keeps_the_acceleration_not_even_the_tempo() {
    Rig r;
    r.gotoTab(UiController::TAB_CLOCK);
    r.enterTab();
    TEST_ASSERT_EQUAL(UiController::FIELD_TEMPO, r.ui.field());
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 3);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(UiController::DEFAULT_TEMPO + 1, r.ui.tempo(),
        "un cran vaut un BPM, l acceleration a disparu du tempo aussi");

    r.gotoField(UiController::FIELD_CLOCK_MODE);
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 3);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, r.ui.clockSource(),
        "le mode est une liste courte : un cran vaut un pas");
}

void test_a_long_turn_on_the_tempo_lands_on_the_bound() {
    Rig r;
    r.gotoTab(UiController::TAB_CLOCK);
    r.enterTab();
    for (uint16_t i = 0; i < 200; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 3);
    }
    TEST_ASSERT_EQUAL_UINT16(UiController::MAX_TEMPO, r.ui.tempo());
    for (uint16_t i = 0; i < 300; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -3);
    }
    TEST_ASSERT_EQUAL_UINT16(UiController::MIN_TEMPO, r.ui.tempo());
}

void test_short_lists_never_accelerate() {
    Rig r;
    r.enterTab();
    r.gotoField(UiController::FIELD_LENGTH);
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 3);
    TEST_ASSERT_EQUAL_UINT8(SequencerEngine::DEFAULT_LENGTH + 1, r.engine.getEffectiveLength(0));
    // et le nom du pattern, parametre principal, depuis la barre
    r.ui.handle(UiController::EVENT_LONG_PRESS);
    r.ui.handle(UiController::EVENT_LONG_PRESS);
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 3);
    TEST_ASSERT_EQUAL_INT8(1, r.ui.displayedPattern());
}

void test_a_field_edit_applies_to_the_channel_of_the_current_tab() {
    Rig r;
    r.gotoTab(3);
    r.enterTab();
    r.gotoField(UiController::FIELD_LENGTH);
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(SequencerEngine::DEFAULT_LENGTH + 1, r.engine.getEffectiveLength(2));
    TEST_ASSERT_EQUAL_UINT8(SequencerEngine::DEFAULT_LENGTH, r.engine.getEffectiveLength(0));
}

void test_the_edit_entry_is_not_a_value() {
    Rig r;
    r.enterTab();
    r.gotoField(UiController::FIELD_EDIT_ENTRY);
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB, r.ui.level());
    TEST_ASSERT_FALSE(r.ui.fieldOpen());
    TEST_ASSERT_EQUAL_UINT8(SequencerEngine::DEFAULT_LENGTH, r.engine.getEffectiveLength(0));
}

/*
 * EDIT PATTERN
 */

void test_press_on_the_edit_entry_enters_the_grid() {
    Rig r;
    r.enterEdit();
    TEST_ASSERT_EQUAL(UiController::LEVEL_EDIT, r.ui.level());
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.stepCursor());
}

void test_rotate_moves_the_step_cursor_and_wraps_at_thirty_six() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, r.ui.isOnHeader() ? 1 : 0,
        "un cran a gauche depuis le step 0 monte dans l en-tete");
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.stepCursor());
    for (uint8_t i = 0; i < 5; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 5);  // accelere, mais un pas chacun
    }
    TEST_ASSERT_EQUAL_UINT8(5, r.ui.stepCursor());
}

void test_press_toggles_the_step_under_the_cursor() {
    Rig r;
    r.enterEdit();
    for (uint8_t i = 0; i < 7; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    bool active = true;
    r.engine.patternForChannel(0)->readStep(7, active);
    TEST_ASSERT_FALSE(active);
    r.ui.handle(UiController::EVENT_PRESS);
    r.engine.patternForChannel(0)->readStep(7, active);
    TEST_ASSERT_TRUE(active);
    r.ui.handle(UiController::EVENT_PRESS);
    r.engine.patternForChannel(0)->readStep(7, active);
    TEST_ASSERT_FALSE(active);
}

void test_an_edit_on_one_channel_reaches_every_channel_on_the_same_pattern() {
    Rig r;
    TEST_ASSERT_TRUE(r.engine.setSelectedPattern(0, 2));
    TEST_ASSERT_TRUE(r.engine.setSelectedPattern(1, 2));
    TEST_ASSERT_TRUE(r.engine.setSelectedPattern(2, 3));

    r.enterEdit();
    for (uint8_t i = 0; i < 7; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    r.ui.handle(UiController::EVENT_PRESS);

    bool active = false;
    r.engine.patternForChannel(0)->readStep(7, active);
    TEST_ASSERT_TRUE(active);
    r.engine.patternForChannel(1)->readStep(7, active);
    TEST_ASSERT_TRUE_MESSAGE(active, "channel 1 plays the same pattern, so it hears the edit");
    r.engine.patternForChannel(2)->readStep(7, active);
    TEST_ASSERT_FALSE_MESSAGE(active, "channel 2 plays another pattern, so it does not");
}

void test_clearing_a_pattern_clears_it_for_every_channel_that_plays_it() {
    Rig r;
    TEST_ASSERT_TRUE(r.engine.setSelectedPattern(0, 2));
    TEST_ASSERT_TRUE(r.engine.setSelectedPattern(1, 2));
    TEST_ASSERT_TRUE(r.engine.setSelectedPattern(2, 3));
    r.engine.patternForChannel(0)->writeStep(3, true);
    r.engine.patternForChannel(2)->writeStep(3, true);

    r.enterEdit();
    r.ui.handle(UiController::EVENT_SHIFT_LONG_PRESS);

    bool active = true;
    r.engine.patternForChannel(0)->readStep(3, active);
    TEST_ASSERT_FALSE(active);
    r.engine.patternForChannel(1)->readStep(3, active);
    TEST_ASSERT_FALSE_MESSAGE(active, "the clear reaches channel 1, which plays the same pattern");
    r.engine.patternForChannel(2)->readStep(3, active);
    TEST_ASSERT_TRUE_MESSAGE(active, "the clear leaves the pattern of channel 2 alone");
}

void test_shift_rotate_sets_the_ratchet_of_an_active_step_and_clamps() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_PRESS);  // le pas 0 devient actif
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(8, r.engine.patternForChannel(0)->getRatchet(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_2, r.engine.patternForChannel(0)->getRatchet(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_3, r.engine.patternForChannel(0)->getRatchet(0));
    for (uint8_t i = 0; i < UiController::RATCHET_CHOICE_COUNT + 3; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, r.engine.patternForChannel(0)->getRatchet(0));
    for (uint8_t i = 0; i < UiController::RATCHET_CHOICE_COUNT + 3; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, r.engine.patternForChannel(0)->getRatchet(0));
}

void test_a_ratchet_edit_takes_effect_on_the_current_step_immediately() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_PRESS);  // le pas 0 devient actif
    r.engine.start();
    TEST_ASSERT_EQUAL_UINT16(SequencerEngine::PPQN, r.engine.currentStepTicks(0));
    for (uint8_t i = 0; i < 6; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 5);
    }
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, r.engine.patternForChannel(0)->getRatchet(0));
    TEST_ASSERT_EQUAL_UINT16(2 * SequencerEngine::PPQN, r.engine.currentStepTicks(0));
}

void test_shift_rotate_in_edit_no_longer_changes_channel() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_PRESS);  // le pas 0 devient actif
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT8_MESSAGE(0, r.ui.selectedChannel(),
        "rien dans l original ne change de channel depuis l editeur");
    TEST_ASSERT_EQUAL(UiController::LEVEL_EDIT, r.ui.level());
    TEST_ASSERT_EQUAL_UINT8(8, r.engine.patternForChannel(0)->getRatchet(0));
}

void test_shift_rotate_on_an_inactive_step_does_nothing() {
    Rig r;
    r.enterEdit();
    bool active = true;
    r.engine.patternForChannel(0)->readStep(0, active);
    TEST_ASSERT_FALSE(active);
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(flexseq::RATCHET_NONE, r.engine.patternForChannel(0)->getRatchet(0),
        "le geste ne regle le ratchet que sur un pas actif");
}

void test_the_grid_follows_the_pattern_of_the_channel_selected_in_edit() {
    Rig r;
    r.engine.setSelectedPattern(1, 5);
    r.gotoTab(UiController::TAB_FIRST_CHANNEL + 1);
    r.enterEdit();
    r.ui.handle(UiController::EVENT_PRESS);
    bool active = false;
    r.engine.patternForChannel(1)->readStep(0, active);
    TEST_ASSERT_TRUE(active);
    r.engine.patternForChannel(0)->readStep(0, active);
    TEST_ASSERT_FALSE(active);
}

void test_shift_long_press_clears_the_pattern_steps_and_ratchets() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 3);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_SHIFT_LONG_PRESS);
    for (uint8_t step = 0; step < UiController::STEP_COUNT; ++step) {
        bool active = true;
        r.engine.patternForChannel(0)->readStep(step, active);
        TEST_ASSERT_FALSE(active);
        TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, r.engine.patternForChannel(0)->getRatchet(step));
    }
}

void test_the_step_cursor_reaches_35_and_wraps_at_36() {
    Rig r;
    r.enterEdit();
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.stepCursor());
    for (uint8_t i = 0; i < 35; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(35, r.ui.stepCursor(),
        "le curseur doit atteindre le step 35");
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, r.ui.stepCursor(),
        "le 36e cran boucle sur le step 0, il ne passe PAS par l en-tete");
    TEST_ASSERT_FALSE_MESSAGE(r.ui.isOnHeader(), "l en-tete ne s atteint que par la gauche");
}

// ⚠️ Le bouclage ARRIERE a disparu le 2026-09-04, decision du proprietaire :
// un cran a gauche depuis le step 0 monte dans l en-tete, il ne boucle plus sur
// le dernier step. Le bouclage AVANT, 35 -> 0, est conserve tel quel.
void test_no_backward_wrap_the_header_takes_that_detent() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_TRUE_MESSAGE(r.ui.isOnHeader(), "l en-tete prend ce cran");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, r.ui.stepCursor(),
        "et le curseur de pas ne bouge pas");
}

void test_the_header_field_cycles_the_measure_separation() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_TRUE_MESSAGE(r.ui.isOnHeader(), "sur l en-tete");
    TEST_ASSERT_FALSE_MESSAGE(r.ui.fieldOpen(), "le champ n est pas encore ouvert");
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_TRUE_MESSAGE(r.ui.fieldOpen(), "un appui court l ouvre");
    const int8_t before = r.engine.getBarLength(0);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(before, r.engine.getBarLength(0),
        "la rotation fait defiler SEP, et le curseur ne bouge plus");
    TEST_ASSERT_TRUE_MESSAGE(r.ui.isOnHeader(), "on reste sur l en-tete");
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_FALSE_MESSAGE(r.ui.fieldOpen(), "un second appui court valide");
}

void test_a_second_detent_to_the_left_selects_the_length_and_it_is_the_stop() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_TRUE_MESSAGE(r.ui.isOnHeader(), "le premier cran selectionne SEP");
    TEST_ASSERT_FALSE_MESSAGE(r.ui.isOnHeaderLength(), "et pas encore la longueur");
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_TRUE_MESSAGE(r.ui.isOnHeaderLength(), "le second cran selectionne la longueur");
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_TRUE_MESSAGE(r.ui.isOnHeaderLength(), "la longueur est la butee de gauche");
    TEST_ASSERT_TRUE(r.ui.isOnHeader());
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_FALSE_MESSAGE(r.ui.isOnHeaderLength(), "un cran a droite revient sur SEP");
    TEST_ASSERT_TRUE_MESSAGE(r.ui.isOnHeader(), "et reste dans l en-tete");
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_FALSE_MESSAGE(r.ui.isOnHeader(), "un cran de plus rend le premier pas");
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.stepCursor());
}

void test_the_open_length_of_the_header_edits_the_base_length_and_leaves_sep_alone() {
    Rig r;
    r.enterEdit();
    r.engine.setBaseLength(0, 12);
    const int8_t sep = r.engine.getBarLength(0);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(12, r.engine.getBaseLength(0),
        "un champ ferme ne change pas sa valeur");
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_TRUE(r.ui.fieldOpen());
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(13, r.engine.getBaseLength(0), "un cran a droite : 13");
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(11, r.engine.getBaseLength(0), "deux crans a gauche : 11");
    TEST_ASSERT_EQUAL_INT8_MESSAGE(sep, r.engine.getBarLength(0), "SEP ne bouge pas");
    TEST_ASSERT_TRUE_MESSAGE(r.ui.isOnHeaderLength(), "la rotation ne deplace pas la selection");
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_FALSE(r.ui.fieldOpen());
}

void test_the_open_sep_does_not_edit_the_length() {
    Rig r;
    r.enterEdit();
    r.engine.setBaseLength(0, 12);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(12, r.engine.getBaseLength(0));
}

void test_a_long_press_on_the_header_length_returns_to_the_first_step() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_LONG_PRESS);
    TEST_ASSERT_FALSE(r.ui.isOnHeader());
    TEST_ASSERT_FALSE(r.ui.isOnHeaderLength());
    TEST_ASSERT_FALSE(r.ui.fieldOpen());
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_FALSE_MESSAGE(r.ui.isOnHeaderLength(), "le retour dans l en-tete repart de SEP");
}

void test_the_header_length_stays_adjustable_under_a_pattern_routing() {
    Rig r;
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_PATTERN);
    r.enterEdit();
    r.engine.setBaseLength(0, 12);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(13, r.engine.getBaseLength(0),
        "la longueur appartient au canal, pas au pattern");
}

void test_a_long_press_on_the_header_returns_to_the_first_step() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_LONG_PRESS);
    TEST_ASSERT_FALSE_MESSAGE(r.ui.isOnHeader(), "on quitte l en-tete");
    TEST_ASSERT_FALSE_MESSAGE(r.ui.fieldOpen(), "et le champ se referme");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, r.ui.stepCursor(), "sur le step 0");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::LEVEL_EDIT, r.ui.level(),
        "et on reste dans EDIT : l appui long n en sort pas depuis l en-tete");
}

void test_a_press_on_the_header_does_not_toggle_a_step() {
    Rig r;
    r.enterEdit();
    flexseq::Pattern* p = r.engine.patternForChannel(0);
    bool before = false;
    p->readStep(0, before);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_PRESS);
    bool after = false;
    p->readStep(0, after);
    TEST_ASSERT_EQUAL_MESSAGE(before, after, "le step 0 n est pas bascule");
}

void test_shift_rotate_on_the_header_changes_no_ratchet() {
    Rig r;
    r.enterEdit();
    flexseq::Pattern* p = r.engine.patternForChannel(0);
    p->writeStep(0, true);
    const uint8_t before = p->getRatchet(0);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(before, p->getRatchet(0),
        "le ratchet n a pas de sens sur l en-tete");
}

void test_long_press_returns_from_the_grid_to_the_tab() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_LONG_PRESS);
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB, r.ui.level());
    TEST_ASSERT_EQUAL(UiController::FIELD_EDIT_ENTRY, r.ui.field());
    r.ui.handle(UiController::EVENT_LONG_PRESS);
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB_BAR, r.ui.level());
}

/*
 * PLAY, and the gesture left free
 */

void test_play_toggles_the_transport_at_every_level() {
    Rig r;
    r.ui.handle(UiController::EVENT_PLAY_PRESS);
    TEST_ASSERT_TRUE(r.engine.isRunning());
    r.ui.handle(UiController::EVENT_PLAY_PRESS);
    TEST_ASSERT_FALSE(r.engine.isRunning());

    r.enterTab();
    r.ui.handle(UiController::EVENT_PLAY_PRESS);
    TEST_ASSERT_TRUE(r.engine.isRunning());
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB, r.ui.level());

    r.gotoField(UiController::FIELD_EDIT_ENTRY);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_PLAY_PRESS);
    TEST_ASSERT_FALSE(r.engine.isRunning());
    TEST_ASSERT_EQUAL(UiController::LEVEL_EDIT, r.ui.level());
}

void test_play_does_nothing_when_the_clock_is_not_internal() {
    Rig r;
    TEST_ASSERT_TRUE(r.ui.setClockSource(1));  // externe, 24 PPQN
    r.ui.handle(UiController::EVENT_PLAY_PRESS);
    TEST_ASSERT_FALSE_MESSAGE(r.engine.isRunning(),
        "PLAY ne demarre pas en source externe");

    r.engine.start();
    r.ui.handle(UiController::EVENT_PLAY_PRESS);
    TEST_ASSERT_TRUE_MESSAGE(r.engine.isRunning(),
        "PLAY n arrete pas non plus en source externe");

    TEST_ASSERT_TRUE(r.ui.setClockSource(0));  // retour a l'interne
    r.ui.handle(UiController::EVENT_PLAY_PRESS);
    TEST_ASSERT_FALSE(r.engine.isRunning());
}

void test_play_realigns_the_channels_when_it_starts() {
    Rig r;
    r.engine.start();
    r.engine.advance(3 * SequencerEngine::PPQN);
    TEST_ASSERT_EQUAL_UINT32(3 * SequencerEngine::PPQN, r.engine.masterPhase());
    r.ui.handle(UiController::EVENT_PLAY_PRESS);
    TEST_ASSERT_FALSE(r.engine.isRunning());
    r.ui.handle(UiController::EVENT_PLAY_PRESS);
    TEST_ASSERT_TRUE(r.engine.isRunning());
    TEST_ASSERT_EQUAL_UINT32(0, r.engine.masterPhase());
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_EQUAL_INT8(0, r.engine.effectiveStep(ch));
    }
}

void test_every_handled_gesture_moves_the_revision() {
    Rig r;
    const uint8_t start = r.ui.revision();
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_NOT_EQUAL(start, r.ui.revision());
    const uint8_t afterRotate = r.ui.revision();
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_NOT_EQUAL(afterRotate, r.ui.revision());
}

void test_setting_the_tempo_or_the_source_moves_the_revision() {
    Rig r;
    const uint8_t start = r.ui.revision();
    TEST_ASSERT_TRUE(r.ui.setTempo(174));
    TEST_ASSERT_NOT_EQUAL(start, r.ui.revision());
    const uint8_t afterTempo = r.ui.revision();
    TEST_ASSERT_TRUE(r.ui.setClockSource(3));
    TEST_ASSERT_NOT_EQUAL(afterTempo, r.ui.revision());
}

void test_a_refused_setting_leaves_the_revision_alone() {
    Rig r;
    const uint8_t start = r.ui.revision();
    TEST_ASSERT_FALSE(r.ui.setTempo(UiController::MAX_TEMPO + 1));
    TEST_ASSERT_FALSE(r.ui.setClockSource(UiController::CLOCK_SOURCE_COUNT));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(start, r.ui.revision(),
        "un reglage refuse ne doit pas provoquer de sauvegarde");
}

void test_the_revision_wraps_without_ever_matching_its_neighbour() {
    Rig r;
    uint8_t previous = r.ui.revision();
    for (uint16_t i = 0; i < 600; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
        TEST_ASSERT_NOT_EQUAL_MESSAGE(previous, r.ui.revision(),
            "deux revisions consecutives ne doivent jamais etre egales");
        previous = r.ui.revision();
    }
}

void test_shift_press_is_deliberately_free_and_changes_nothing() {
    Rig r;
    r.enterEdit();
    for (uint8_t i = 0; i < 3; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    r.ui.handle(UiController::EVENT_SHIFT_PRESS);
    TEST_ASSERT_EQUAL(UiController::LEVEL_EDIT, r.ui.level());
    TEST_ASSERT_EQUAL_UINT8(3, r.ui.stepCursor());
    bool active = true;
    r.engine.patternForChannel(0)->readStep(3, active);
    TEST_ASSERT_FALSE(active);
}

/*
 * The tab's main parameter, edited from the bar with SHIFT
 */

void test_the_main_parameter_follows_the_channel_mode() {
    Rig r;
    TEST_ASSERT_EQUAL(UiController::FIELD_PATTERN, r.ui.mainField());
    r.engine.setChannelMode(0, flexseq::MODE_CLOCK);
    TEST_ASSERT_EQUAL(UiController::FIELD_SUBDIV, r.ui.mainField());
    r.engine.setChannelMode(0, flexseq::MODE_RANDOM);
    TEST_ASSERT_EQUAL(UiController::FIELD_SKIP_CHANCE, r.ui.mainField());

    r.gotoTab(UiController::TAB_CLOCK);
    TEST_ASSERT_EQUAL(UiController::FIELD_TEMPO, r.ui.mainField());
    r.gotoTab(UiController::TAB_SETTINGS);
    TEST_ASSERT_EQUAL(UiController::FIELD_NONE, r.ui.mainField());
}

void test_the_interface_caps_the_skip_chance_at_nine() {
    Rig r;
    r.engine.setChannelMode(0, flexseq::MODE_RANDOM);
    for (uint8_t i = 0; i < 20; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT8(9, r.engine.getSkipChance(0));
}

void test_the_engine_still_accepts_a_skip_chance_of_ten() {
    Rig r;
    TEST_ASSERT_TRUE(r.engine.setSkipChance(0, 10));
    TEST_ASSERT_EQUAL_UINT8(10, r.engine.getSkipChance(0));
    TEST_ASSERT_FALSE(r.engine.setSkipChance(0, 11));
    TEST_ASSERT_EQUAL_UINT8(10, r.engine.getSkipChance(0));
}

void test_shift_rotate_on_the_bar_changes_the_main_parameter() {
    {
        Rig r;  // SEQ : le NOM du pattern, et le chargement reste a l appui court
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
        TEST_ASSERT_EQUAL_INT8(1, r.ui.displayedPattern());
        TEST_ASSERT_EQUAL_INT8(0, r.engine.getSelectedPattern(0));
    }
    {
        Rig r;  // CLOCK : la SUBDIV
        r.engine.setChannelMode(0, flexseq::MODE_CLOCK);
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
        TEST_ASSERT_EQUAL_INT16(-2, r.engine.getSubdiv(0));
    }
    {
        Rig r;  // RANDOM : la chance de saut
        r.engine.setChannelMode(0, flexseq::MODE_RANDOM);
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
        TEST_ASSERT_EQUAL_UINT8(1, r.engine.getSkipChance(0));
    }
    {
        Rig r;  // l onglet horloge : le tempo
        r.gotoTab(UiController::TAB_CLOCK);
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
        TEST_ASSERT_EQUAL_UINT16(UiController::DEFAULT_TEMPO + 1, r.ui.tempo());
    }
}

void test_shift_rotate_on_the_bar_moves_nothing_else() {
    Rig r;
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB_BAR, r.ui.level());
    TEST_ASSERT_EQUAL_UINT8(UiController::TAB_FIRST_CHANNEL, r.ui.currentTab());
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.cursor());
    TEST_ASSERT_FALSE(r.ui.fieldOpen());
    TEST_ASSERT_EQUAL_UINT8(SequencerEngine::DEFAULT_LENGTH, r.engine.getEffectiveLength(0));
}

void test_shift_rotate_on_the_settings_tab_changes_nothing() {
    Rig r;
    r.gotoTab(UiController::TAB_SETTINGS);
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT16(UiController::DEFAULT_TEMPO, r.ui.tempo());
    TEST_ASSERT_EQUAL_INT8(0, r.engine.getSelectedPattern(0));
}

void test_shift_play_is_reserved_and_does_not_toggle_the_transport() {
    Rig r;
    TEST_ASSERT_FALSE(r.engine.isRunning());
    r.ui.handle(UiController::EVENT_SHIFT_PLAY_PRESS);
    TEST_ASSERT_FALSE_MESSAGE(r.engine.isRunning(),
        "SHIFT + PLAY est reserve a RECORDING, le transport garde PLAY seul");
    r.ui.handle(UiController::EVENT_PLAY_PRESS);
    TEST_ASSERT_TRUE(r.engine.isRunning());
    r.ui.handle(UiController::EVENT_SHIFT_PLAY_PRESS);
    TEST_ASSERT_TRUE(r.engine.isRunning());
}

namespace {

struct ModeRig {
    flexseq::PatternBank bank;
    SequencerEngine engine;
    Transport transport;
    UiController ui;

    explicit ModeRig(flexseq::ChannelMode mode)
        : engine(), transport(engine), ui(engine, transport) {
        engine.setPatternBank(&bank);
        for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
            engine.setChannelMode(ch, mode);
        }
        ui.handle(UiController::EVENT_PRESS);
    }

    void gotoField(UiController::Field target) {
        for (uint8_t guard = 0; guard <= UiController::CHANNEL_TAB_FIELDS; ++guard) {
            if (ui.field() == target) return;
            ui.handle(UiController::EVENT_ROTATE, 1);
        }
    }
};

}  // namespace

void test_a_clock_tab_holds_the_three_lines_of_the_original() {
    ModeRig r(flexseq::MODE_CLOCK);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, r.ui.fieldCount(),
        "MODE, OFFSET et MOD : trois lignes, comme l original");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MODE, r.ui.fieldAt(0), "ligne 1");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_OFFSET, r.ui.fieldAt(1), "ligne 2 en CLOCK");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MOD, r.ui.fieldAt(2), "ligne 3");
}

void test_a_random_tab_puts_the_subdivision_on_the_second_line() {
    ModeRig r(flexseq::MODE_RANDOM);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, r.ui.fieldCount(), "trois lignes aussi");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MODE, r.ui.fieldAt(0), "ligne 1");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_SUBDIV, r.ui.fieldAt(1),
        "ligne 2 en RANDOM : la SUBDIVISION, et non l offset");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MOD, r.ui.fieldAt(2), "ligne 3");
}

// ⚠️ QUATRE POSITIONS DE CURSEUR, mais TOUJOURS TROIS LIGNES. La grande valeur
// n est pas une ligne : c est le grand nom a gauche. La conformite a l original
// porte sur les lignes, et elle est intacte.
void test_a_seq_tab_takes_the_three_lines_of_the_original() {
    ModeRig r(flexseq::MODE_SEQ);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(4, r.ui.fieldCount(),
        "trois lignes, plus la grande valeur");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_PATTERN, r.ui.fieldAt(0),
        "la grande valeur vient en premier : on choisit avant d editer");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MODE, r.ui.fieldAt(1),
        "MODE est la ligne 1 dans les TROIS modes : sans lui, SEQ serait un aller simple");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_EDIT_ENTRY, r.ui.fieldAt(2), "ligne 2 : EDIT");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_CONFIG, r.ui.fieldAt(3), "ligne 3 : CONFIG");
}

void test_the_published_field_indices_agree_with_fieldAt() {
    ModeRig r(flexseq::MODE_SEQ);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, UiController::SEQ_FIELD_INDEX_PATTERN, "PATTERN vaut 0");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, UiController::SEQ_FIELD_INDEX_MODE, "MODE vaut 1");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(2, UiController::SEQ_FIELD_INDEX_EDIT_ENTRY, "EDIT vaut 2");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, UiController::SEQ_FIELD_INDEX_CONFIG, "CONFIG vaut 3");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MODE,
        r.ui.fieldAt(UiController::SEQ_FIELD_INDEX_MODE), "index et fieldAt : MODE");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_EDIT_ENTRY,
        r.ui.fieldAt(UiController::SEQ_FIELD_INDEX_EDIT_ENTRY), "EDIT_ENTRY");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_CONFIG,
        r.ui.fieldAt(UiController::SEQ_FIELD_INDEX_CONFIG), "CONFIG");
}

void test_the_config_page_carries_length_subdiv_and_mod() {
    ModeRig r(flexseq::MODE_SEQ);
    r.gotoField(UiController::FIELD_CONFIG);
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_TRUE_MESSAGE(r.ui.isOnConfigPage(), "un appui court ouvre la page");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, r.ui.fieldCount(), "trois champs");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, UiController::CONFIG_FIELD_INDEX_LENGTH, "LENGTH vaut 0");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, UiController::CONFIG_FIELD_INDEX_SUBDIV, "SUBDIV vaut 1");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(2, UiController::CONFIG_FIELD_INDEX_MOD, "MOD vaut 2");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_LENGTH,
        r.ui.fieldAt(UiController::CONFIG_FIELD_INDEX_LENGTH), "ligne 1");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_SUBDIV,
        r.ui.fieldAt(UiController::CONFIG_FIELD_INDEX_SUBDIV), "ligne 2");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MOD,
        r.ui.fieldAt(UiController::CONFIG_FIELD_INDEX_MOD), "ligne 3");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_LENGTH, r.ui.field(),
        "le curseur se pose sur LENGTH en entrant");
}

void test_a_rotation_of_mod_sets_both_destinations_and_clamps() {
    ModeRig r(flexseq::MODE_SEQ);
    r.gotoField(UiController::FIELD_CONFIG);
    r.ui.handle(UiController::EVENT_PRESS);
    r.gotoField(UiController::FIELD_MOD);
    r.ui.handle(UiController::EVENT_PRESS);

    for (int i = 0; i < 9; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::CV_DEST_PATTERN,
        r.engine.getCvDestination(0, flexseq::CV_SOURCE_1),
        "la neuvieme valeur du cycle est P/L");
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::CV_DEST_LENGTH,
        r.engine.getCvDestination(0, flexseq::CV_SOURCE_2),
        "et elle pose les DEUX destinations");

    for (int i = 0; i < 40; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::CV_DEST_STEP,
        r.engine.getCvDestination(0, flexseq::CV_SOURCE_1), "le cycle ecrete sur S/R");
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::CV_DEST_RESET,
        r.engine.getCvDestination(0, flexseq::CV_SOURCE_2), "la vingt et unieme valeur");

    for (int i = 0; i < 40; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::CV_DEST_NONE,
        r.engine.getCvDestination(0, flexseq::CV_SOURCE_1), "et ecrete sur OFF");
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::CV_DEST_NONE,
        r.engine.getCvDestination(0, flexseq::CV_SOURCE_2), "les deux entrees liberees");
}

void test_a_single_value_of_mod_frees_the_second_input() {
    ModeRig r(flexseq::MODE_SEQ);
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_PATTERN);
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_2, flexseq::CV_DEST_LENGTH);
    r.gotoField(UiController::FIELD_CONFIG);
    r.ui.handle(UiController::EVENT_PRESS);
    r.gotoField(UiController::FIELD_MOD);
    r.ui.handle(UiController::EVENT_PRESS);

    for (int i = 0; i < 8; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::CV_DEST_PATTERN,
        r.engine.getCvDestination(0, flexseq::CV_SOURCE_1), "P reste sur CV1");
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::CV_DEST_NONE,
        r.engine.getCvDestination(0, flexseq::CV_SOURCE_2),
        "une valeur simple LIBERE la seconde entree");
}

void test_a_clock_channel_does_not_edit_its_routing() {
    ModeRig r(flexseq::MODE_CLOCK);
    r.gotoField(UiController::FIELD_MOD);
    r.ui.handle(UiController::EVENT_PRESS);
    for (int i = 0; i < 5; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::CV_DEST_NONE,
        r.engine.getCvDestination(0, flexseq::CV_SOURCE_1),
        "en CLOCK la destination du PRD 10.2 n existe pas encore : rien ne bouge");
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::CV_DEST_NONE,
        r.engine.getCvDestination(0, flexseq::CV_SOURCE_2), "ni sur la seconde entree");
}

void test_a_long_press_leaves_the_config_page_for_the_tab() {
    ModeRig r(flexseq::MODE_SEQ);
    r.gotoField(UiController::FIELD_CONFIG);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_LONG_PRESS);
    TEST_ASSERT_FALSE_MESSAGE(r.ui.isOnConfigPage(), "on quitte la page");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::LEVEL_TAB, r.ui.level(),
        "et on reste dans l onglet : l appui long monte d UN niveau");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_CONFIG, r.ui.field(),
        "sur le champ par lequel on etait entre");
    r.ui.handle(UiController::EVENT_LONG_PRESS);
    TEST_ASSERT_EQUAL_MESSAGE(UiController::LEVEL_TAB_BAR, r.ui.level(),
        "le second appui long atteint la barre");
}

void test_the_config_page_is_left_behind_when_a_tab_is_entered_again() {
    ModeRig r(flexseq::MODE_SEQ);
    r.gotoField(UiController::FIELD_CONFIG);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_LONG_PRESS);
    r.ui.handle(UiController::EVENT_LONG_PRESS);
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_FALSE_MESSAGE(r.ui.isOnConfigPage(),
        "entrer dans un onglet part de ses propres champs, jamais de CONFIG");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_PATTERN, r.ui.field(),
        "donc sur son premier champ, la grande valeur");
}

void test_the_length_is_edited_on_the_config_page() {
    ModeRig r(flexseq::MODE_SEQ);
    r.gotoField(UiController::FIELD_CONFIG);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_PRESS);
    const uint8_t before = r.engine.getBaseLength(0);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(before + 1, r.engine.getBaseLength(0),
        "la longueur s edite sur la page, et plus sur l onglet");
}


void test_the_mode_can_always_be_changed_back_out_of_seq() {
    ModeRig r(flexseq::MODE_SEQ);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MODE, r.ui.field(),
        "MODE reste atteignable, a un cran de la grande valeur");
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::MODE_RANDOM, r.engine.getChannelMode(0),
        "on redescend de SEQ vers RANDOM");
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::MODE_CLOCK, r.engine.getChannelMode(0),
        "puis vers CLOCK");
}

void test_the_mode_field_cycles_the_three_modes() {
    ModeRig r(flexseq::MODE_CLOCK);
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MODE, r.ui.field(), "on demarre sur MODE");
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::MODE_RANDOM, r.engine.getChannelMode(0),
        "CLOCK puis RANDOM");
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::MODE_SEQ, r.engine.getChannelMode(0), "puis SEQ");
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_MESSAGE(flexseq::MODE_SEQ, r.engine.getChannelMode(0),
        "et la borne tient : pas de retour a CLOCK par debordement");
}

void test_the_cursor_never_designates_a_field_that_does_not_exist() {
    ModeRig r(flexseq::MODE_SEQ);
    for (uint8_t i = 0; i < 2; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(2, r.ui.cursor(), "curseur au dernier champ de SEQ");
    r.engine.setChannelMode(0, flexseq::MODE_CLOCK);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_TRUE_MESSAGE(r.ui.cursor() < r.ui.fieldCount(),
        "le curseur doit rester dans la liste");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(UiController::FIELD_NONE, r.ui.field(),
        "et le champ designe doit exister");
}

void test_the_offset_field_moves_the_offset() {
    ModeRig r(flexseq::MODE_CLOCK);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_OFFSET, r.ui.field(), "sur OFFSET");
    r.ui.handle(UiController::EVENT_PRESS);
    const uint8_t avant = r.engine.getOffset(0);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(avant + 1, r.engine.getOffset(0), "un cran, un pas");
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(avant, r.engine.getOffset(0), "et retour");
}

void test_the_offset_never_goes_below_zero() {
    ModeRig r(flexseq::MODE_CLOCK);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    r.ui.handle(UiController::EVENT_PRESS);
    for (uint8_t i = 0; i < 5; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, r.engine.getOffset(0), "la borne basse tient");
}

void test_the_offset_stops_at_the_last_tick_of_the_step() {
    ModeRig r(flexseq::MODE_CLOCK);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    r.ui.handle(UiController::EVENT_PRESS);
    for (uint16_t i = 0; i < 200; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(95, r.engine.getOffset(0),
        "SUBDIV /1 fait 96 ticks, donc le dernier offset est 95");
}

void test_a_step_longer_than_255_ticks_keeps_the_full_offset_range() {
    ModeRig r(flexseq::MODE_CLOCK);
    r.engine.setSubdiv(0, 4);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    r.ui.handle(UiController::EVENT_PRESS);
    for (uint16_t i = 0; i < 400; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(255, r.engine.getOffset(0),
        "SUBDIV /4 fait 384 ticks : la borne est plafonnee a 255, jamais repliee a 0");
}

void test_the_mod_field_is_navigable_and_does_nothing_yet() {
    ModeRig r(flexseq::MODE_CLOCK);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MOD, r.ui.field(), "sur MOD");
    const flexseq::CvDestination a1 = r.engine.getCvDestination(0, flexseq::CV_SOURCE_1);
    const flexseq::CvDestination a2 = r.engine.getCvDestination(0, flexseq::CV_SOURCE_2);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_MESSAGE(a1, r.engine.getCvDestination(0, flexseq::CV_SOURCE_1),
        "la source 1 ne bouge pas : le mecanisme est au lot 13");
    TEST_ASSERT_EQUAL_MESSAGE(a2, r.engine.getCvDestination(0, flexseq::CV_SOURCE_2),
        "ni la source 2");
}

/*
 * Le curseur d un onglet en SEQ — PRD 5.0 amendement 1quater
 *
 * La grande valeur reprend la premiere position : c est le nom qu on choisit
 * avant d editer. MODE, EDIT et CONFIG suivent en 1, 2, 3.
 */

void test_the_published_seq_indices_are_zero_to_three() {
    TEST_ASSERT_EQUAL_UINT8(0, UiController::SEQ_FIELD_INDEX_PATTERN);
    TEST_ASSERT_EQUAL_UINT8(1, UiController::SEQ_FIELD_INDEX_MODE);
    TEST_ASSERT_EQUAL_UINT8(2, UiController::SEQ_FIELD_INDEX_EDIT_ENTRY);
    TEST_ASSERT_EQUAL_UINT8(3, UiController::SEQ_FIELD_INDEX_CONFIG);
}

// Les deux autres modes ne lisent aucun pattern : la position n existe pas.
void test_a_clock_channel_keeps_three_positions_and_mode_first() {
    Rig r;
    r.engine.setChannelMode(0, flexseq::MODE_CLOCK);
    r.gotoTab(UiController::TAB_FIRST_CHANNEL);
    r.enterTab();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, r.ui.fieldCount(), "trois positions");
    TEST_ASSERT_EQUAL(UiController::FIELD_MODE, r.ui.fieldAt(0));
}

// ⚠️ Le seul endroit du firmware ou un geste deplace ce que le curseur designe :
// la position 0 change de sens avec le mode.
void test_turning_a_channel_to_seq_puts_the_cursor_back_on_mode() {
    Rig r;
    r.engine.setChannelMode(0, flexseq::MODE_CLOCK);
    r.gotoTab(UiController::TAB_FIRST_CHANNEL);
    r.enterTab();
    TEST_ASSERT_EQUAL(UiController::FIELD_MODE, r.ui.field());
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);   // CLOCK -> RANDOM
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);   // RANDOM -> SEQ
    TEST_ASSERT_EQUAL(flexseq::MODE_SEQ, r.engine.getChannelMode(0));
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MODE, r.ui.field(),
                              "le curseur reste sur MODE, il ne glisse pas sur la grande valeur");
}

void test_turning_a_channel_back_to_clock_keeps_the_cursor_on_mode() {
    Rig r;
    r.gotoTab(UiController::TAB_FIRST_CHANNEL);
    r.engine.setChannelMode(0, flexseq::MODE_SEQ);
    r.enterTab();
    while (r.ui.field() != UiController::FIELD_CONFIG) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    r.ui.handle(UiController::EVENT_ROTATE, 1);         // revient sur la grande valeur
    while (r.ui.field() != UiController::FIELD_MODE) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);   // SEQ -> CLOCK
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MODE, r.ui.field(),
                              "et le curseur ne sort jamais des bornes");
}

/*
 * Le choix de l image — lot 16E etape 4, correction du 2026-09-13
 *
 * Le declencheur de redessin et le selecteur d ecran lisaient des etats
 * DIFFERENTS, et l ecran restait fige. Ils lisent desormais cette decision-ci.
 */

void test_the_frame_choice_names_the_main_screen_by_default() {
    Rig r;
    const flexseq::UiFrameChoice choice = flexseq::uiFrameChoiceOf(r.ui);
    TEST_ASSERT_EQUAL(flexseq::UI_FRAME_MAIN, choice.kind);
    TEST_ASSERT_EQUAL_INT8(-1, choice.channel);
}

void test_the_frame_choice_names_the_channel_editor_and_its_channel() {
    Rig r;
    r.enterEdit();
    const flexseq::UiFrameChoice choice = flexseq::uiFrameChoiceOf(r.ui);
    TEST_ASSERT_EQUAL(flexseq::UI_FRAME_CHANNEL_EDIT, choice.kind);
    TEST_ASSERT_EQUAL_INT8(0, choice.channel);
}

/*
 * PRD 5.0, amendment of 2026-09-15 — the pattern is chosen in its own field, a
 * short press loads it, and nothing ever asks: the patterns are shared.
 */

namespace {

void seqChannelOnPattern(Rig& r) {
    r.gotoTab(UiController::TAB_FIRST_CHANNEL);
    r.engine.setChannelMode(0, flexseq::MODE_SEQ);
    r.enterTab();
    TEST_ASSERT_EQUAL(UiController::FIELD_PATTERN, r.ui.field());
}

bool stepActive(const flexseq::Pattern* p, uint8_t step) {
    bool active = false;
    return p != nullptr && p->readStep(step, active) && active;
}

}  // namespace

void test_a_seq_tab_puts_the_big_value_first() {
    Rig r;
    seqChannelOnPattern(r);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(4, r.ui.fieldCount(), "quatre positions");
    TEST_ASSERT_EQUAL(UiController::FIELD_PATTERN, r.ui.fieldAt(0));
    TEST_ASSERT_EQUAL(UiController::FIELD_MODE, r.ui.fieldAt(1));
    TEST_ASSERT_EQUAL(UiController::FIELD_EDIT_ENTRY, r.ui.fieldAt(2));
    TEST_ASSERT_EQUAL(UiController::FIELD_CONFIG, r.ui.fieldAt(3));
}

void test_the_displayed_name_starts_on_the_played_pattern() {
    Rig r;
    seqChannelOnPattern(r);
    r.engine.setSelectedPattern(0, 6);
    TEST_ASSERT_EQUAL_INT8(6, r.ui.displayedPattern());
}

void test_shift_rotate_names_the_pattern_and_loads_nothing() {
    Rig r;
    seqChannelOnPattern(r);
    r.engine.setSelectedPattern(0, 6);

    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);

    TEST_ASSERT_EQUAL_INT8_MESSAGE(7, r.ui.displayedPattern(), "le nom avance");
    TEST_ASSERT_EQUAL_INT8_MESSAGE(6, r.engine.getSelectedPattern(0),
        "et le canal joue toujours le sien");
    TEST_ASSERT_EQUAL_PTR(r.bank.getPattern(6), r.engine.patternForChannel(0));
}

void test_the_open_field_names_the_pattern_too() {
    Rig r;
    seqChannelOnPattern(r);
    r.engine.setSelectedPattern(0, 6);

    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_TRUE_MESSAGE(r.ui.fieldOpen(), "l appui court ouvre le champ");
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    r.ui.handle(UiController::EVENT_ROTATE, 1);

    TEST_ASSERT_EQUAL_INT8(8, r.ui.displayedPattern());
    TEST_ASSERT_EQUAL_INT8_MESSAGE(6, r.engine.getSelectedPattern(0),
        "tourner ne charge pas");
}

void test_the_name_stops_at_both_ends() {
    Rig r;
    seqChannelOnPattern(r);
    r.engine.setSelectedPattern(0, 0);

    for (uint8_t i = 0; i < 20; ++i) r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT8_MESSAGE(15, r.ui.displayedPattern(), "B8 est le dernier");
    for (uint8_t i = 0; i < 20; ++i) r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    TEST_ASSERT_EQUAL_INT8_MESSAGE(0, r.ui.displayedPattern(), "A1 est le premier");
}

void test_a_short_press_loads_the_shown_pattern_and_the_channel_plays_it() {
    Rig r;
    seqChannelOnPattern(r);
    r.engine.setSelectedPattern(0, 6);
    r.bank.getPattern(7)->writeStep(2, true);

    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    r.ui.handle(UiController::EVENT_PRESS);

    TEST_ASSERT_FALSE_MESSAGE(r.ui.fieldOpen(), "le champ se ferme");
    TEST_ASSERT_EQUAL_INT8(7, r.engine.getSelectedPattern(0));
    TEST_ASSERT_EQUAL_PTR(r.bank.getPattern(7), r.engine.patternForChannel(0));
    TEST_ASSERT_TRUE(stepActive(r.engine.patternForChannel(0), 2));
    TEST_ASSERT_EQUAL_INT8_MESSAGE(7, r.ui.displayedPattern(),
        "sans choix en cours, l ecran nomme ce que le canal joue");
}

void test_an_edited_pattern_loads_without_a_question_and_keeps_its_edit() {
    Rig r;
    seqChannelOnPattern(r);
    r.engine.setSelectedPattern(0, 6);
    r.bank.getPattern(6)->writeStep(4, true);

    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    r.ui.handle(UiController::EVENT_PRESS);

    TEST_ASSERT_FALSE(r.ui.fieldOpen());
    TEST_ASSERT_EQUAL_INT8_MESSAGE(7, r.engine.getSelectedPattern(0),
        "un seul appui charge : rien ne demande");
    TEST_ASSERT_TRUE_MESSAGE(stepActive(r.bank.getPattern(6), 4),
        "le pattern quitte garde son edition : rien n est detruit");
}

void test_a_long_press_closes_without_loading() {
    Rig r;
    seqChannelOnPattern(r);
    r.engine.setSelectedPattern(0, 6);

    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT8(7, r.ui.displayedPattern());
    r.ui.handle(UiController::EVENT_LONG_PRESS);

    TEST_ASSERT_FALSE(r.ui.fieldOpen());
    TEST_ASSERT_EQUAL_INT8(6, r.engine.getSelectedPattern(0));
    TEST_ASSERT_EQUAL_INT8_MESSAGE(6, r.ui.displayedPattern(),
        "l ecran revient sur le pattern joue");
}

void test_a_change_of_tab_drops_the_choice() {
    Rig r;
    seqChannelOnPattern(r);
    r.engine.setChannelMode(1, flexseq::MODE_SEQ);
    r.engine.setSelectedPattern(0, 6);
    r.engine.setSelectedPattern(1, 2);

    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT8(7, r.ui.displayedPattern());

    r.ui.handle(UiController::EVENT_LONG_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT8_MESSAGE(2, r.ui.displayedPattern(),
        "le canal suivant nomme le sien");
}

/*
 * PRD 5.0, amendment of 2026-09-15 — EDIT is read only while a source of the
 * channel is routed to PATTERN.
 */

void test_an_unrouted_channel_is_editable() {
    Rig r;
    TEST_ASSERT_FALSE(r.ui.editIsReadOnly());
    r.enterEdit();
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_TRUE(stepActive(r.engine.patternForChannel(0), 0));
}

void test_a_pattern_routing_on_the_first_source_makes_edit_read_only() {
    Rig r;
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_PATTERN);
    r.engine.patternForChannel(0)->writeStep(3, true);
    r.enterEdit();
    TEST_ASSERT_TRUE(r.ui.editIsReadOnly());

    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_FALSE_MESSAGE(stepActive(r.engine.patternForChannel(0), 0),
        "un appui court n active aucun pas");

    for (uint8_t i = 0; i < 3; ++i) r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(3, r.ui.stepCursor());
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(flexseq::RATCHET_NONE,
        r.engine.patternForChannel(0)->getRatchet(3), "aucun ratchet n est pose");

    r.ui.handle(UiController::EVENT_SHIFT_LONG_PRESS);
    TEST_ASSERT_TRUE_MESSAGE(stepActive(r.engine.patternForChannel(0), 3),
        "l effacement ne touche pas le pattern");
}

void test_a_pattern_routing_on_the_second_source_is_read_only_too() {
    Rig r;
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_2, flexseq::CV_DEST_PATTERN);
    r.enterEdit();
    TEST_ASSERT_TRUE(r.ui.editIsReadOnly());
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_FALSE(stepActive(r.engine.patternForChannel(0), 0));
}

void test_another_routing_does_not_block_the_edit() {
    Rig r;
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_LENGTH);
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_2, flexseq::CV_DEST_STEP);
    TEST_ASSERT_FALSE(r.ui.editIsReadOnly());
    r.enterEdit();
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_TRUE(stepActive(r.engine.patternForChannel(0), 0));
}

void test_sep_stays_adjustable_under_a_pattern_routing() {
    Rig r;
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_PATTERN);
    r.enterEdit();
    const int8_t before = r.engine.getBarLength(0);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_TRUE(r.ui.isOnHeader());
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(before, r.engine.getBarLength(0),
        "SEP appartient au canal, pas au pattern");
}

void test_read_only_follows_the_channel_of_the_tab() {
    Rig r;
    r.engine.setCvDestination(1, flexseq::CV_SOURCE_1, flexseq::CV_DEST_PATTERN);
    r.gotoTab(UiController::TAB_FIRST_CHANNEL);
    TEST_ASSERT_FALSE(r.ui.editIsReadOnly());
    r.gotoTab(static_cast<uint8_t>(UiController::TAB_FIRST_CHANNEL + 1));
    TEST_ASSERT_TRUE(r.ui.editIsReadOnly());
    r.gotoTab(UiController::TAB_CLOCK);
    TEST_ASSERT_FALSE(r.ui.editIsReadOnly());
}

void test_removing_the_routing_gives_the_edit_back() {
    Rig r;
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_PATTERN);
    r.enterEdit();
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_FALSE(stepActive(r.engine.patternForChannel(0), 0));
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_NONE);
    TEST_ASSERT_FALSE(r.ui.editIsReadOnly());
    r.ui.handle(UiController::EVENT_PRESS);
    TEST_ASSERT_TRUE(stepActive(r.engine.patternForChannel(0), 0));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_seq_tab_puts_the_big_value_first);
    RUN_TEST(test_the_displayed_name_starts_on_the_played_pattern);
    RUN_TEST(test_shift_rotate_names_the_pattern_and_loads_nothing);
    RUN_TEST(test_the_open_field_names_the_pattern_too);
    RUN_TEST(test_a_short_press_loads_the_shown_pattern_and_the_channel_plays_it);
    RUN_TEST(test_an_edited_pattern_loads_without_a_question_and_keeps_its_edit);
    RUN_TEST(test_an_unrouted_channel_is_editable);
    RUN_TEST(test_a_pattern_routing_on_the_first_source_makes_edit_read_only);
    RUN_TEST(test_a_pattern_routing_on_the_second_source_is_read_only_too);
    RUN_TEST(test_another_routing_does_not_block_the_edit);
    RUN_TEST(test_sep_stays_adjustable_under_a_pattern_routing);
    RUN_TEST(test_read_only_follows_the_channel_of_the_tab);
    RUN_TEST(test_removing_the_routing_gives_the_edit_back);
    RUN_TEST(test_the_name_stops_at_both_ends);
    RUN_TEST(test_a_long_press_closes_without_loading);
    RUN_TEST(test_a_change_of_tab_drops_the_choice);
    RUN_TEST(test_a_clock_tab_holds_the_three_lines_of_the_original);
    RUN_TEST(test_a_random_tab_puts_the_subdivision_on_the_second_line);
    RUN_TEST(test_a_seq_tab_takes_the_three_lines_of_the_original);
    RUN_TEST(test_the_published_field_indices_agree_with_fieldAt);
    RUN_TEST(test_the_config_page_carries_length_subdiv_and_mod);
    RUN_TEST(test_a_rotation_of_mod_sets_both_destinations_and_clamps);
    RUN_TEST(test_a_single_value_of_mod_frees_the_second_input);
    RUN_TEST(test_a_clock_channel_does_not_edit_its_routing);
    RUN_TEST(test_a_long_press_leaves_the_config_page_for_the_tab);
    RUN_TEST(test_the_config_page_is_left_behind_when_a_tab_is_entered_again);
    RUN_TEST(test_the_length_is_edited_on_the_config_page);
    RUN_TEST(test_the_mode_can_always_be_changed_back_out_of_seq);
    RUN_TEST(test_the_mode_field_cycles_the_three_modes);
    RUN_TEST(test_the_cursor_never_designates_a_field_that_does_not_exist);
    RUN_TEST(test_the_offset_field_moves_the_offset);
    RUN_TEST(test_the_offset_never_goes_below_zero);
    RUN_TEST(test_the_offset_stops_at_the_last_tick_of_the_step);
    RUN_TEST(test_a_step_longer_than_255_ticks_keeps_the_full_offset_range);
    RUN_TEST(test_the_mod_field_is_navigable_and_does_nothing_yet);

    RUN_TEST(test_starts_on_the_tab_bar_over_the_first_channel);
    RUN_TEST(test_rotate_changes_tab);
    RUN_TEST(test_rotate_wraps_at_both_ends_of_the_tab_bar);
    RUN_TEST(test_an_accelerated_delta_moves_one_tab_not_three);
    RUN_TEST(test_press_enters_a_tab_that_has_fields);
    RUN_TEST(test_the_settings_tab_holds_the_four_entries_of_the_original);
    RUN_TEST(test_calibrate_asks_for_the_action_once_and_shows_done);
    RUN_TEST(test_rotate_screen_asks_for_the_action_and_shows_no_done);
    RUN_TEST(test_reverse_encoder_asks_for_the_action_and_shows_done);
    RUN_TEST(test_the_press_that_clears_done_does_nothing_else);
    RUN_TEST(test_factory_reset_asks_first_and_no_comes_first);
    RUN_TEST(test_factory_reset_runs_on_yes_only);
    RUN_TEST(test_a_long_press_cancels_the_factory_reset_question);
    RUN_TEST(test_shift_rotation_never_answers_the_factory_reset_question);
    RUN_TEST(test_the_internal_clock_exposes_the_tempo_then_the_mode);
    RUN_TEST(test_the_external_clock_exposes_the_mode_then_the_ppqn);
    RUN_TEST(test_the_midi_clock_exposes_the_mode_alone);
    RUN_TEST(test_the_mode_and_the_ppqn_are_two_views_of_the_source_byte);
    RUN_TEST(test_the_mode_field_walks_int_ext_midi_and_stops_at_both_ends);
    RUN_TEST(test_the_cursor_follows_the_mode_field_when_the_mode_moves_it);
    RUN_TEST(test_the_ppqn_field_walks_24_4_2_1_and_never_leaves_the_external_mode);
    RUN_TEST(test_a_mode_that_does_not_change_keeps_the_ppqn);
    RUN_TEST(test_entering_the_external_mode_starts_at_24_ppqn);
    RUN_TEST(test_shift_rotation_on_the_bar_leaves_the_tempo_alone_off_the_internal_clock);
    RUN_TEST(test_the_other_gestures_do_nothing_on_the_tab_bar);

    RUN_TEST(test_rotate_moves_the_field_cursor_and_wraps);
    RUN_TEST(test_press_opens_a_value_field_and_press_closes_it);
    RUN_TEST(test_rotate_changes_the_value_while_the_field_is_open);
    RUN_TEST(test_long_press_closes_the_open_field_without_leaving_the_tab);
    RUN_TEST(test_long_press_on_a_closed_field_returns_to_the_tab_bar);
    RUN_TEST(test_shift_rotate_changes_the_value_without_opening_the_field);
    RUN_TEST(test_the_tempo_range_is_the_one_the_module_announces);
    RUN_TEST(test_tempo_is_clamped_to_the_musical_range);
    RUN_TEST(test_the_clock_fields_never_reach_the_sentinel);
    RUN_TEST(test_the_displayed_name_is_clamped_to_the_bank);
    RUN_TEST(test_the_length_field_edits_the_base_and_never_the_derived_value);
    RUN_TEST(test_the_length_field_leaves_the_modulation_in_place);
    RUN_TEST(test_the_length_field_is_clamped_to_one_and_thirty_six);
    RUN_TEST(test_a_turn_to_the_right_multiplies_the_subdiv_and_a_turn_to_the_left_divides_it);
    RUN_TEST(test_the_bar_length_field_walks_only_the_allowed_values);
    RUN_TEST(test_no_field_keeps_the_acceleration_not_even_the_tempo);
    RUN_TEST(test_a_long_turn_on_the_tempo_lands_on_the_bound);
    RUN_TEST(test_short_lists_never_accelerate);
    RUN_TEST(test_a_field_edit_applies_to_the_channel_of_the_current_tab);
    RUN_TEST(test_the_edit_entry_is_not_a_value);

    RUN_TEST(test_press_on_the_edit_entry_enters_the_grid);
    RUN_TEST(test_rotate_moves_the_step_cursor_and_wraps_at_thirty_six);
    RUN_TEST(test_press_toggles_the_step_under_the_cursor);
    RUN_TEST(test_an_edit_on_one_channel_reaches_every_channel_on_the_same_pattern);
    RUN_TEST(test_clearing_a_pattern_clears_it_for_every_channel_that_plays_it);
    RUN_TEST(test_shift_rotate_sets_the_ratchet_of_an_active_step_and_clamps);
    RUN_TEST(test_a_ratchet_edit_takes_effect_on_the_current_step_immediately);
    RUN_TEST(test_shift_rotate_in_edit_no_longer_changes_channel);
    RUN_TEST(test_shift_rotate_on_an_inactive_step_does_nothing);
    RUN_TEST(test_the_grid_follows_the_pattern_of_the_channel_selected_in_edit);
    RUN_TEST(test_shift_long_press_clears_the_pattern_steps_and_ratchets);
    RUN_TEST(test_the_step_cursor_reaches_35_and_wraps_at_36);
    RUN_TEST(test_no_backward_wrap_the_header_takes_that_detent);
    RUN_TEST(test_the_header_field_cycles_the_measure_separation);
    RUN_TEST(test_a_long_press_on_the_header_returns_to_the_first_step);
    RUN_TEST(test_a_second_detent_to_the_left_selects_the_length_and_it_is_the_stop);
    RUN_TEST(test_the_open_length_of_the_header_edits_the_base_length_and_leaves_sep_alone);
    RUN_TEST(test_the_open_sep_does_not_edit_the_length);
    RUN_TEST(test_a_long_press_on_the_header_length_returns_to_the_first_step);
    RUN_TEST(test_the_header_length_stays_adjustable_under_a_pattern_routing);
    RUN_TEST(test_a_press_on_the_header_does_not_toggle_a_step);
    RUN_TEST(test_shift_rotate_on_the_header_changes_no_ratchet);
    RUN_TEST(test_long_press_returns_from_the_grid_to_the_tab);

    RUN_TEST(test_play_toggles_the_transport_at_every_level);
    RUN_TEST(test_play_does_nothing_when_the_clock_is_not_internal);
    RUN_TEST(test_play_realigns_the_channels_when_it_starts);
    RUN_TEST(test_every_handled_gesture_moves_the_revision);
    RUN_TEST(test_setting_the_tempo_or_the_source_moves_the_revision);
    RUN_TEST(test_a_refused_setting_leaves_the_revision_alone);
    RUN_TEST(test_the_revision_wraps_without_ever_matching_its_neighbour);
    RUN_TEST(test_shift_press_is_deliberately_free_and_changes_nothing);

    RUN_TEST(test_the_main_parameter_follows_the_channel_mode);
    RUN_TEST(test_the_interface_caps_the_skip_chance_at_nine);
    RUN_TEST(test_the_engine_still_accepts_a_skip_chance_of_ten);
    RUN_TEST(test_shift_rotate_on_the_bar_changes_the_main_parameter);
    RUN_TEST(test_shift_rotate_on_the_bar_moves_nothing_else);
    RUN_TEST(test_shift_rotate_on_the_settings_tab_changes_nothing);
    RUN_TEST(test_shift_play_is_reserved_and_does_not_toggle_the_transport);
    RUN_TEST(test_the_published_seq_indices_are_zero_to_three);
    RUN_TEST(test_a_clock_channel_keeps_three_positions_and_mode_first);
    RUN_TEST(test_turning_a_channel_to_seq_puts_the_cursor_back_on_mode);
    RUN_TEST(test_turning_a_channel_back_to_clock_keeps_the_cursor_on_mode);
    RUN_TEST(test_the_frame_choice_names_the_main_screen_by_default);
    RUN_TEST(test_the_frame_choice_names_the_channel_editor_and_its_channel);

    return UNITY_END();
}
