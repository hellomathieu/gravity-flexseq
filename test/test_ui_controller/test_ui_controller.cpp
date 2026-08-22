#include <stdint.h>
#include <unity.h>

#include <flexseq/Pattern.h>
#include <flexseq/PatternBank.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Subdiv.h>
#include <flexseq/Transport.h>
#include <flexseq/UiController.h>

using flexseq::Pattern;
using flexseq::PatternBank;
using flexseq::SequencerEngine;
using flexseq::Transport;
using flexseq::UiController;

void setUp() {}
void tearDown() {}

namespace {

struct Rig {
    PatternBank bank;
    SequencerEngine engine;
    Transport transport;
    UiController ui;

    Rig() : engine(), transport(engine), ui(engine, bank, transport) {
        engine.setPatternBank(&bank);
    }

    void enterTab() { ui.handle(UiController::EVENT_PRESS); }

    void gotoTab(uint8_t tab) {
        while (ui.currentTab() != tab) {
            ui.handle(UiController::EVENT_ROTATE, 1);
        }
    }

    void gotoField(UiController::Field field) {
        for (uint8_t guard = 0; guard < UiController::CHANNEL_TAB_FIELDS; ++guard) {
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

void test_rotate_honours_an_accelerated_delta() {
    Rig r;
    r.ui.handle(UiController::EVENT_ROTATE, 3);
    TEST_ASSERT_EQUAL_UINT8(4, r.ui.currentTab());
    r.ui.handle(UiController::EVENT_ROTATE, -3);
    TEST_ASSERT_EQUAL_UINT8(1, r.ui.currentTab());
}

void test_press_enters_a_tab_that_has_fields() {
    Rig r;
    r.enterTab();
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB, r.ui.level());
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.cursor());
    TEST_ASSERT_EQUAL(UiController::FIELD_PATTERN, r.ui.field());
}

void test_press_does_nothing_on_the_settings_tab_while_it_is_deferred() {
    Rig r;
    r.gotoTab(UiController::TAB_SETTINGS);
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.fieldCount());
    r.enterTab();
    TEST_ASSERT_EQUAL(UiController::LEVEL_TAB_BAR, r.ui.level());
}

void test_the_clock_tab_exposes_tempo_then_source() {
    Rig r;
    r.gotoTab(UiController::TAB_CLOCK);
    TEST_ASSERT_EQUAL_UINT8(UiController::CLOCK_TAB_FIELDS, r.ui.fieldCount());
    TEST_ASSERT_EQUAL(UiController::FIELD_TEMPO, r.ui.fieldAt(0));
    TEST_ASSERT_EQUAL(UiController::FIELD_CLOCK_SOURCE, r.ui.fieldAt(1));
    TEST_ASSERT_EQUAL_INT8(-1, r.ui.selectedChannel());
}

void test_the_other_gestures_do_nothing_on_the_tab_bar() {
    Rig r;
    const UiController::Event inert[] = {
        UiController::EVENT_ROTATE_HELD,
        UiController::EVENT_LONG_PRESS,
        UiController::EVENT_SHIFT_ROTATE,
        UiController::EVENT_SHIFT_PRESS,
        UiController::EVENT_SHIFT_LONG_PRESS,
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
    TEST_ASSERT_EQUAL(UiController::FIELD_LENGTH, r.ui.field());
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL(UiController::FIELD_PATTERN, r.ui.field());
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL(UiController::FIELD_EDIT_ENTRY, r.ui.field());
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL(UiController::FIELD_PATTERN, r.ui.field());
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

void test_the_clock_source_field_never_reaches_the_sentinel() {
    Rig r;
    r.gotoTab(UiController::TAB_CLOCK);
    r.enterTab();
    r.gotoField(UiController::FIELD_CLOCK_SOURCE);
    for (uint8_t i = 0; i < UiController::CLOCK_SOURCE_COUNT + 5; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
        TEST_ASSERT_TRUE(r.ui.clockSource() < UiController::CLOCK_SOURCE_COUNT);
    }
    TEST_ASSERT_EQUAL_UINT8(UiController::CLOCK_SOURCE_COUNT - 1, r.ui.clockSource());
    for (uint8_t i = 0; i < UiController::CLOCK_SOURCE_COUNT + 5; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.clockSource());
}

void test_the_pattern_field_is_clamped_to_the_bank() {
    Rig r;
    r.enterTab();
    for (uint8_t i = 0; i < SequencerEngine::PATTERN_COUNT + 5; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_INT8(SequencerEngine::PATTERN_COUNT - 1, r.engine.getSelectedPattern(0));
    for (uint8_t i = 0; i < SequencerEngine::PATTERN_COUNT + 5; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_INT8(0, r.engine.getSelectedPattern(0));
}

void test_the_length_field_is_clamped_to_one_and_twenty_four() {
    Rig r;
    r.enterTab();
    r.gotoField(UiController::FIELD_LENGTH);
    for (uint8_t i = 0; i < SequencerEngine::MAX_LENGTH + 5; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT8(SequencerEngine::MAX_LENGTH, r.engine.getEffectiveLength(0));
    for (uint8_t i = 0; i < SequencerEngine::MAX_LENGTH + 5; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_UINT8(SequencerEngine::MIN_LENGTH, r.engine.getEffectiveLength(0));
}

void test_the_subdiv_field_walks_the_libgravity_list_and_clamps() {
    Rig r;
    r.enterTab();
    r.gotoField(UiController::FIELD_SUBDIV);
    TEST_ASSERT_EQUAL_INT16(flexseq::DEFAULT_SUBDIV, r.engine.getSubdiv(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT16(2, r.engine.getSubdiv(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -2);
    TEST_ASSERT_EQUAL_INT16(-2, r.engine.getSubdiv(0));
    for (uint8_t i = 0; i < flexseq::SUBDIV_CHOICE_COUNT + 5; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_INT16(128, r.engine.getSubdiv(0));
    for (uint8_t i = 0; i < flexseq::SUBDIV_CHOICE_COUNT + 5; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_INT16(-24, r.engine.getSubdiv(0));
}

void test_the_bar_length_field_walks_only_the_allowed_values() {
    Rig r;
    r.enterTab();
    r.gotoField(UiController::FIELD_BAR_LENGTH);
    TEST_ASSERT_EQUAL_INT8(SequencerEngine::DEFAULT_BAR_LENGTH, r.engine.getBarLength(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT8(6, r.engine.getBarLength(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT8(6, r.engine.getBarLength(0));
    for (uint8_t i = 0; i < UiController::BAR_LENGTH_CHOICE_COUNT + 3; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_INT8(SequencerEngine::BAR_NONE, r.engine.getBarLength(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT8(2, r.engine.getBarLength(0));
}

void test_an_accelerated_turn_lands_on_the_bound_instead_of_being_refused() {
    Rig r;
    r.enterTab();
    r.gotoField(UiController::FIELD_LENGTH);
    for (uint8_t i = 0; i < 7; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT8(23, r.engine.getEffectiveLength(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 3);
    TEST_ASSERT_EQUAL_UINT8(SequencerEngine::MAX_LENGTH, r.engine.getEffectiveLength(0));

    for (uint8_t i = 0; i < 22; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_UINT8(2, r.engine.getEffectiveLength(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -3);
    TEST_ASSERT_EQUAL_UINT8(SequencerEngine::MIN_LENGTH, r.engine.getEffectiveLength(0));

    r.gotoField(UiController::FIELD_PATTERN);
    for (uint8_t i = 0; i < 14; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_INT8(14, r.engine.getSelectedPattern(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 3);
    TEST_ASSERT_EQUAL_INT8(SequencerEngine::PATTERN_COUNT - 1, r.engine.getSelectedPattern(0));
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

void test_rotate_moves_the_step_cursor_and_wraps_at_twenty_four() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_UINT8(UiController::STEP_COUNT - 1, r.ui.stepCursor());
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.stepCursor());
    r.ui.handle(UiController::EVENT_ROTATE, 5);
    TEST_ASSERT_EQUAL_UINT8(5, r.ui.stepCursor());
}

void test_press_toggles_the_step_under_the_cursor() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_ROTATE, 7);
    bool active = true;
    r.bank.getPattern(0)->readStep(7, active);
    TEST_ASSERT_FALSE(active);
    r.ui.handle(UiController::EVENT_PRESS);
    r.bank.getPattern(0)->readStep(7, active);
    TEST_ASSERT_TRUE(active);
    r.ui.handle(UiController::EVENT_PRESS);
    r.bank.getPattern(0)->readStep(7, active);
    TEST_ASSERT_FALSE(active);
}

void test_rotate_held_sets_the_ratchet_and_clamps_at_both_ends() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_ROTATE_HELD, 1);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_2, r.bank.getPattern(0)->getRatchet(0));
    r.ui.handle(UiController::EVENT_ROTATE_HELD, 1);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_3, r.bank.getPattern(0)->getRatchet(0));
    for (uint8_t i = 0; i < UiController::RATCHET_CHOICE_COUNT + 3; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE_HELD, 1);
    }
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, r.bank.getPattern(0)->getRatchet(0));
    for (uint8_t i = 0; i < UiController::RATCHET_CHOICE_COUNT + 3; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE_HELD, -1);
    }
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, r.bank.getPattern(0)->getRatchet(0));
}

void test_a_ratchet_edit_takes_effect_on_the_current_step_immediately() {
    Rig r;
    r.enterEdit();
    r.engine.start();
    TEST_ASSERT_EQUAL_UINT16(SequencerEngine::PPQN, r.engine.currentStepTicks(0));
    r.ui.handle(UiController::EVENT_ROTATE_HELD, 5);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, r.bank.getPattern(0)->getRatchet(0));
    TEST_ASSERT_EQUAL_UINT16(2 * SequencerEngine::PPQN, r.engine.currentStepTicks(0));
}

void test_shift_rotate_changes_channel_and_wraps() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT8(1, r.ui.selectedChannel());
    TEST_ASSERT_EQUAL(UiController::LEVEL_EDIT, r.ui.level());
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    TEST_ASSERT_EQUAL_INT8(0, r.ui.selectedChannel());
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    TEST_ASSERT_EQUAL_INT8(SequencerEngine::CHANNEL_COUNT - 1, r.ui.selectedChannel());
}

void test_the_grid_follows_the_pattern_of_the_channel_selected_in_edit() {
    Rig r;
    r.engine.setSelectedPattern(1, 5);
    r.enterEdit();
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    r.ui.handle(UiController::EVENT_PRESS);
    bool active = false;
    r.bank.getPattern(5)->readStep(0, active);
    TEST_ASSERT_TRUE(active);
    r.bank.getPattern(0)->readStep(0, active);
    TEST_ASSERT_FALSE(active);
}

void test_shift_long_press_clears_the_pattern_steps_and_ratchets() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_ROTATE_HELD, 3);
    r.ui.handle(UiController::EVENT_ROTATE, 4);
    r.ui.handle(UiController::EVENT_PRESS);
    r.ui.handle(UiController::EVENT_SHIFT_LONG_PRESS);
    for (uint8_t step = 0; step < UiController::STEP_COUNT; ++step) {
        bool active = true;
        r.bank.getPattern(0)->readStep(step, active);
        TEST_ASSERT_FALSE(active);
        TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, r.bank.getPattern(0)->getRatchet(step));
    }
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

void test_shift_press_is_deliberately_free_and_changes_nothing() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_ROTATE, 3);
    r.ui.handle(UiController::EVENT_SHIFT_PRESS);
    TEST_ASSERT_EQUAL(UiController::LEVEL_EDIT, r.ui.level());
    TEST_ASSERT_EQUAL_UINT8(3, r.ui.stepCursor());
    bool active = true;
    r.bank.getPattern(0)->readStep(3, active);
    TEST_ASSERT_FALSE(active);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_starts_on_the_tab_bar_over_the_first_channel);
    RUN_TEST(test_rotate_changes_tab);
    RUN_TEST(test_rotate_wraps_at_both_ends_of_the_tab_bar);
    RUN_TEST(test_rotate_honours_an_accelerated_delta);
    RUN_TEST(test_press_enters_a_tab_that_has_fields);
    RUN_TEST(test_press_does_nothing_on_the_settings_tab_while_it_is_deferred);
    RUN_TEST(test_the_clock_tab_exposes_tempo_then_source);
    RUN_TEST(test_the_other_gestures_do_nothing_on_the_tab_bar);

    RUN_TEST(test_rotate_moves_the_field_cursor_and_wraps);
    RUN_TEST(test_press_opens_a_value_field_and_press_closes_it);
    RUN_TEST(test_rotate_changes_the_value_while_the_field_is_open);
    RUN_TEST(test_long_press_closes_the_open_field_without_leaving_the_tab);
    RUN_TEST(test_long_press_on_a_closed_field_returns_to_the_tab_bar);
    RUN_TEST(test_shift_rotate_changes_the_value_without_opening_the_field);
    RUN_TEST(test_tempo_is_clamped_to_the_musical_range);
    RUN_TEST(test_the_clock_source_field_never_reaches_the_sentinel);
    RUN_TEST(test_the_pattern_field_is_clamped_to_the_bank);
    RUN_TEST(test_the_length_field_is_clamped_to_one_and_twenty_four);
    RUN_TEST(test_the_subdiv_field_walks_the_libgravity_list_and_clamps);
    RUN_TEST(test_the_bar_length_field_walks_only_the_allowed_values);
    RUN_TEST(test_an_accelerated_turn_lands_on_the_bound_instead_of_being_refused);
    RUN_TEST(test_a_field_edit_applies_to_the_channel_of_the_current_tab);
    RUN_TEST(test_the_edit_entry_is_not_a_value);

    RUN_TEST(test_press_on_the_edit_entry_enters_the_grid);
    RUN_TEST(test_rotate_moves_the_step_cursor_and_wraps_at_twenty_four);
    RUN_TEST(test_press_toggles_the_step_under_the_cursor);
    RUN_TEST(test_rotate_held_sets_the_ratchet_and_clamps_at_both_ends);
    RUN_TEST(test_a_ratchet_edit_takes_effect_on_the_current_step_immediately);
    RUN_TEST(test_shift_rotate_changes_channel_and_wraps);
    RUN_TEST(test_the_grid_follows_the_pattern_of_the_channel_selected_in_edit);
    RUN_TEST(test_shift_long_press_clears_the_pattern_steps_and_ratchets);
    RUN_TEST(test_long_press_returns_from_the_grid_to_the_tab);

    RUN_TEST(test_play_toggles_the_transport_at_every_level);
    RUN_TEST(test_play_realigns_the_channels_when_it_starts);
    RUN_TEST(test_shift_press_is_deliberately_free_and_changes_nothing);

    return UNITY_END();
}
