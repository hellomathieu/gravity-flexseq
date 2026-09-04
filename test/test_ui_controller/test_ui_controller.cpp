#include <stdint.h>
#include <unity.h>

#include <flexseq/Pattern.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Subdiv.h>
#include <flexseq/Transport.h>
#include <flexseq/CvDestination.h>
#include <flexseq/UiController.h>

using flexseq::Pattern;
using flexseq::SequencerEngine;
using flexseq::Transport;
using flexseq::UiController;

void setUp() {}
void tearDown() {}

namespace {

struct Rig {
    SequencerEngine engine;
    Transport transport;
    UiController ui;

    Rig() : engine(), transport(engine), ui(engine, transport) {
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

// libGravity accelere une rotation rapide (x3 sous 16 ms). Sur une navigation
// discrete cela fait sauter des selections — constate sur le module le
// 2026-08-22. Un cran vaut donc UN onglet, quelle que soit l'acceleration.
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
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MODE, r.ui.field(),
        "MODE est la ligne 1, comme dans l original");
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
    TEST_ASSERT_EQUAL(UiController::FIELD_PATTERN, r.ui.field());
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL(UiController::FIELD_MODE, r.ui.field());
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL(UiController::FIELD_EDIT_ENTRY, r.ui.field());
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MODE, r.ui.field(),
        "la liste boucle sur son premier champ, qui est MODE");
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
    // SHIFT plus rotation ajuste le champ SELECTIONNE, et le curseur demarre
    // sur MODE depuis l etape 5b : il faut donc aller sur PATTERN d abord.
    r.gotoField(UiController::FIELD_PATTERN);
    for (uint8_t i = 0; i < SequencerEngine::PATTERN_COUNT + 5; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_INT8(SequencerEngine::PATTERN_COUNT - 1, r.engine.getSelectedPattern(0));
    for (uint8_t i = 0; i < SequencerEngine::PATTERN_COUNT + 5; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_INT8(0, r.engine.getSelectedPattern(0));
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

void test_the_subdiv_field_walks_the_libgravity_list_and_clamps() {
    Rig r;
    r.enterTab();
    r.gotoField(UiController::FIELD_SUBDIV);
    TEST_ASSERT_EQUAL_INT16(flexseq::DEFAULT_SUBDIV, r.engine.getSubdiv(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT16(2, r.engine.getSubdiv(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -2);  // ecrete a un pas
    TEST_ASSERT_EQUAL_INT16(flexseq::DEFAULT_SUBDIV, r.engine.getSubdiv(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
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

// LE TEMPO EST LA SEULE EXCEPTION : sa plage compte 271 valeurs, trop pour se
// parcourir cran par cran. Il garde donc l'acceleration de libGravity, et c'est
// la qu'un cran accelere doit atterrir sur la borne plutot que d'etre refuse.
void test_no_field_keeps_the_acceleration_not_even_the_tempo() {
    Rig r;
    r.gotoTab(UiController::TAB_CLOCK);
    r.enterTab();
    TEST_ASSERT_EQUAL(UiController::FIELD_TEMPO, r.ui.field());
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 3);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(UiController::DEFAULT_TEMPO + 1, r.ui.tempo(),
        "un cran vaut un BPM, l acceleration a disparu du tempo aussi");

    r.gotoField(UiController::FIELD_CLOCK_SOURCE);
    const uint8_t before = r.ui.clockSource();
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 3);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(before + 1, r.ui.clockSource(),
        "la source est une liste courte : un cran vaut un pas");
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

// Les listes courtes n'accelerent pas : un cran, un pas, quelle que soit
// l'amplitude que la dependance rapporte.
void test_short_lists_never_accelerate() {
    Rig r;
    r.enterTab();
    r.gotoField(UiController::FIELD_LENGTH);
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 3);
    TEST_ASSERT_EQUAL_UINT8(SequencerEngine::DEFAULT_LENGTH + 1, r.engine.getEffectiveLength(0));
    r.gotoField(UiController::FIELD_PATTERN);
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 3);
    TEST_ASSERT_EQUAL_INT8(1, r.engine.getSelectedPattern(0));
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
    TEST_ASSERT_EQUAL_UINT8(35, r.ui.stepCursor());
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
    r.engine.instanceForChannel(0)->readStep(7, active);
    TEST_ASSERT_FALSE(active);
    r.ui.handle(UiController::EVENT_PRESS);
    r.engine.instanceForChannel(0)->readStep(7, active);
    TEST_ASSERT_TRUE(active);
    r.ui.handle(UiController::EVENT_PRESS);
    r.engine.instanceForChannel(0)->readStep(7, active);
    TEST_ASSERT_FALSE(active);
}

void test_the_editor_writes_into_the_instance_and_never_into_the_buffer() {
    Rig r;
    flexseq::ModulatedPatternState state;
    r.engine.setModulatedPatterns(&state);
    state.loaded[0] = 5;

    r.enterEdit();
    for (uint8_t i = 0; i < 7; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    r.ui.handle(UiController::EVENT_PRESS);

    bool active = false;
    r.engine.instanceForChannel(0)->readStep(7, active);
    TEST_ASSERT_TRUE(active);
    state.pattern[0].readStep(7, active);
    TEST_ASSERT_FALSE(active);
}

void test_clearing_a_pattern_clears_the_instance_and_never_the_buffer() {
    Rig r;
    flexseq::ModulatedPatternState state;
    r.engine.setModulatedPatterns(&state);
    state.loaded[0] = 5;
    r.engine.instanceForChannel(0)->writeStep(3, true);
    state.pattern[0].writeStep(3, true);

    r.enterEdit();
    r.ui.handle(UiController::EVENT_SHIFT_LONG_PRESS);

    bool active = true;
    r.engine.instanceForChannel(0)->readStep(3, active);
    TEST_ASSERT_FALSE(active);
    state.pattern[0].readStep(3, active);
    TEST_ASSERT_TRUE(active);
}

void test_shift_rotate_sets_the_ratchet_of_an_active_step_and_clamps() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_PRESS);  // le pas 0 devient actif
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_2, r.engine.instanceForChannel(0)->getRatchet(0));
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_3, r.engine.instanceForChannel(0)->getRatchet(0));
    for (uint8_t i = 0; i < UiController::RATCHET_CHOICE_COUNT + 3; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, r.engine.instanceForChannel(0)->getRatchet(0));
    for (uint8_t i = 0; i < UiController::RATCHET_CHOICE_COUNT + 3; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, -1);
    }
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, r.engine.instanceForChannel(0)->getRatchet(0));
}

void test_a_ratchet_edit_takes_effect_on_the_current_step_immediately() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_PRESS);  // le pas 0 devient actif
    r.engine.start();
    TEST_ASSERT_EQUAL_UINT16(SequencerEngine::PPQN, r.engine.currentStepTicks(0));
    for (uint8_t i = 0; i < 5; ++i) {
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 5);
    }
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, r.engine.instanceForChannel(0)->getRatchet(0));
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
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_2, r.engine.instanceForChannel(0)->getRatchet(0));
}

void test_shift_rotate_on_an_inactive_step_does_nothing() {
    Rig r;
    r.enterEdit();
    bool active = true;
    r.engine.instanceForChannel(0)->readStep(0, active);
    TEST_ASSERT_FALSE(active);
    r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(flexseq::RATCHET_NONE, r.engine.instanceForChannel(0)->getRatchet(0),
        "le geste ne regle le ratchet que sur un pas actif");
}

void test_the_grid_follows_the_pattern_of_the_channel_selected_in_edit() {
    Rig r;
    r.engine.setSelectedPattern(1, 5);
    r.gotoTab(UiController::TAB_FIRST_CHANNEL + 1);
    r.enterEdit();
    r.ui.handle(UiController::EVENT_PRESS);
    bool active = false;
    r.engine.instanceForChannel(1)->readStep(0, active);
    TEST_ASSERT_TRUE(active);
    r.engine.instanceForChannel(0)->readStep(0, active);
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
        r.engine.instanceForChannel(0)->readStep(step, active);
        TEST_ASSERT_FALSE(active);
        TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, r.engine.instanceForChannel(0)->getRatchet(step));
    }
}

// 35 et 36 sont ECRITS EN TOUTES LETTRES. Une boucle bornee par STEP_COUNT
// suivrait la constante et ne prouverait rien de sa valeur.
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
        "le 36e cran doit ramener le curseur a 0");
}

void test_the_step_cursor_wraps_backwards_from_0_to_35() {
    Rig r;
    r.enterEdit();
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_EQUAL_UINT8(35, r.ui.stepCursor());
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

// L'original ne demarre et n'arrete que l'horloge INTERNE : `if (masterClockMode
// == 0)` (Interactions.ino:372). En source externe ou MIDI, PLAY ne fait rien —
// c'est la source qui commande. Decide par le proprietaire le 2026-08-24.
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

// Le compteur de revisions est ce qui declenche un redessin ET une sauvegarde
// differee : s'il ne bougeait pas, une edition resterait invisible et non
// persistee. Une revision de trop ne coute rien, une manquante coute l'edition.
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
    r.engine.instanceForChannel(0)->readStep(3, active);
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

void test_shift_rotate_on_the_bar_changes_the_main_parameter() {
    {
        Rig r;  // SEQ : le pattern
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
        TEST_ASSERT_EQUAL_INT8(1, r.engine.getSelectedPattern(0));
    }
    {
        Rig r;  // CLOCK : la SUBDIV
        r.engine.setChannelMode(0, flexseq::MODE_CLOCK);
        r.ui.handle(UiController::EVENT_SHIFT_ROTATE, 1);
        TEST_ASSERT_EQUAL_INT16(2, r.engine.getSubdiv(0));
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
    SequencerEngine engine;
    Transport transport;
    UiController ui;

    explicit ModeRig(flexseq::ChannelMode mode)
        : engine(), transport(engine), ui(engine, transport) {
        for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
            engine.setChannelMode(ch, mode);
        }
        ui.handle(UiController::EVENT_PRESS);
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

void test_a_seq_tab_keeps_its_fields_and_gains_the_mode() {
    ModeRig r(flexseq::MODE_SEQ);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(6, r.ui.fieldCount(),
        "SEQ garde ses cinq champs jusqu au lot 12, et gagne MODE");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MODE, r.ui.fieldAt(0),
        "MODE est la ligne 1 dans les TROIS modes : sans lui, SEQ serait un aller simple");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_PATTERN, r.ui.fieldAt(1), "2");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_LENGTH, r.ui.fieldAt(2), "3");
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_EDIT_ENTRY, r.ui.fieldAt(5), "6");
}

void test_the_mode_can_always_be_changed_back_out_of_seq() {
    // Le defaut que deux mutants survivants ont revele : sans MODE en SEQ, on
    // n'en sort plus.
    ModeRig r(flexseq::MODE_SEQ);
    TEST_ASSERT_EQUAL_MESSAGE(UiController::FIELD_MODE, r.ui.field(), "MODE est atteignable");
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
    for (uint8_t i = 0; i < 4; ++i) {
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(4, r.ui.cursor(), "curseur au dernier champ de SEQ");
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

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_clock_tab_holds_the_three_lines_of_the_original);
    RUN_TEST(test_a_random_tab_puts_the_subdivision_on_the_second_line);
    RUN_TEST(test_a_seq_tab_keeps_its_fields_and_gains_the_mode);
    RUN_TEST(test_the_mode_can_always_be_changed_back_out_of_seq);
    RUN_TEST(test_the_mode_field_cycles_the_three_modes);
    RUN_TEST(test_the_cursor_never_designates_a_field_that_does_not_exist);
    RUN_TEST(test_the_offset_field_moves_the_offset);
    RUN_TEST(test_the_offset_never_goes_below_zero);
    RUN_TEST(test_the_mod_field_is_navigable_and_does_nothing_yet);

    RUN_TEST(test_starts_on_the_tab_bar_over_the_first_channel);
    RUN_TEST(test_rotate_changes_tab);
    RUN_TEST(test_rotate_wraps_at_both_ends_of_the_tab_bar);
    RUN_TEST(test_an_accelerated_delta_moves_one_tab_not_three);
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
    RUN_TEST(test_the_tempo_range_is_the_one_the_module_announces);
    RUN_TEST(test_tempo_is_clamped_to_the_musical_range);
    RUN_TEST(test_the_clock_source_field_never_reaches_the_sentinel);
    RUN_TEST(test_the_pattern_field_is_clamped_to_the_bank);
    RUN_TEST(test_the_length_field_edits_the_base_and_never_the_derived_value);
    RUN_TEST(test_the_length_field_leaves_the_modulation_in_place);
    RUN_TEST(test_the_length_field_is_clamped_to_one_and_thirty_six);
    RUN_TEST(test_the_subdiv_field_walks_the_libgravity_list_and_clamps);
    RUN_TEST(test_the_bar_length_field_walks_only_the_allowed_values);
    RUN_TEST(test_no_field_keeps_the_acceleration_not_even_the_tempo);
    RUN_TEST(test_a_long_turn_on_the_tempo_lands_on_the_bound);
    RUN_TEST(test_short_lists_never_accelerate);
    RUN_TEST(test_a_field_edit_applies_to_the_channel_of_the_current_tab);
    RUN_TEST(test_the_edit_entry_is_not_a_value);

    RUN_TEST(test_press_on_the_edit_entry_enters_the_grid);
    RUN_TEST(test_rotate_moves_the_step_cursor_and_wraps_at_thirty_six);
    RUN_TEST(test_press_toggles_the_step_under_the_cursor);
    RUN_TEST(test_the_editor_writes_into_the_instance_and_never_into_the_buffer);
    RUN_TEST(test_clearing_a_pattern_clears_the_instance_and_never_the_buffer);
    RUN_TEST(test_shift_rotate_sets_the_ratchet_of_an_active_step_and_clamps);
    RUN_TEST(test_a_ratchet_edit_takes_effect_on_the_current_step_immediately);
    RUN_TEST(test_shift_rotate_in_edit_no_longer_changes_channel);
    RUN_TEST(test_shift_rotate_on_an_inactive_step_does_nothing);
    RUN_TEST(test_the_grid_follows_the_pattern_of_the_channel_selected_in_edit);
    RUN_TEST(test_shift_long_press_clears_the_pattern_steps_and_ratchets);
    RUN_TEST(test_the_step_cursor_reaches_35_and_wraps_at_36);
    RUN_TEST(test_the_step_cursor_wraps_backwards_from_0_to_35);
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
    RUN_TEST(test_shift_rotate_on_the_bar_changes_the_main_parameter);
    RUN_TEST(test_shift_rotate_on_the_bar_moves_nothing_else);
    RUN_TEST(test_shift_rotate_on_the_settings_tab_changes_nothing);
    RUN_TEST(test_shift_play_is_reserved_and_does_not_toggle_the_transport);

    return UNITY_END();
}
