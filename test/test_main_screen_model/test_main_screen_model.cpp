#include <string.h>
#include <unity.h>

#include <flexseq/EditTitle.h>
#include <flexseq/MainScreenModel.h>
#include <flexseq/PatternBank.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>
#include <flexseq/UiController.h>

using flexseq::MainScreenModel;
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
    }

    // Entre dans l'onglet du channel demande, depuis la barre.
    void enterChannel(uint8_t channel) {
        const uint8_t tab = static_cast<uint8_t>(channel + UiController::TAB_FIRST_CHANNEL);
        for (uint8_t guard = 0; guard < 2 * UiController::TAB_COUNT; ++guard) {
            if (ui.currentTab() == tab) {
                break;
            }
            ui.handle(UiController::EVENT_ROTATE, 1);
        }
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(tab, ui.currentTab(), "l onglet vise");
        ui.handle(UiController::EVENT_PRESS);
    }

    MainScreenModel model() { return flexseq::mainScreenModelOf(ui, engine); }
};

}  // namespace

void test_the_model_carries_the_mode_of_the_selected_channel() {
    Rig r;
    r.engine.setChannelMode(2, flexseq::MODE_RANDOM);
    r.enterChannel(2);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        static_cast<uint8_t>(flexseq::MODE_RANDOM), r.model().mode,
        "le mode du channel selectionne, pas celui du channel 0");
}

void test_the_model_reads_the_selected_channel_and_not_the_first() {
    // Chaque channel porte une valeur DIFFERENTE : un cablage qui lirait
    // toujours le channel 0 passerait si toutes se ressemblaient.
    Rig r;
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        r.engine.setChannelMode(ch, flexseq::MODE_CLOCK);
        r.engine.setBaseLength(ch, static_cast<uint8_t>(ch + 4));
        r.engine.setOffset(ch, static_cast<uint8_t>(ch + 1));
    }
    r.enterChannel(4);
    const MainScreenModel m = r.model();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(8, m.length, "la longueur du channel 4");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(5, m.offset, "l offset du channel 4");
}

void test_the_main_parameter_is_never_derived_twice() {
    // Le modele ne recalcule PAS la regle : il porte ce que mainField() dit.
    Rig r;
    const flexseq::ChannelMode modes[3] = {
        flexseq::MODE_CLOCK, flexseq::MODE_RANDOM, flexseq::MODE_SEQ};
    for (uint8_t i = 0; i < 3; ++i) {
        Rig fresh;
        fresh.engine.setChannelMode(1, modes[i]);
        fresh.enterChannel(1);
        {
            const uint8_t attendu =
                modes[i] == flexseq::MODE_CLOCK  ? flexseq::MAIN_SUBDIV
              : modes[i] == flexseq::MODE_RANDOM ? flexseq::MAIN_SKIP_CHANCE
                                                 : flexseq::MAIN_PATTERN;
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(attendu, fresh.model().mainParameter,
                "la traduction de mainField() doit suivre le mode");
        }
    }
    (void)r;
}

void test_clock_makes_the_subdivision_the_main_parameter() {
    Rig r;
    r.engine.setChannelMode(0, flexseq::MODE_CLOCK);
    r.engine.setSubdiv(0, 4);
    r.enterChannel(0);
    const MainScreenModel m = r.model();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        flexseq::MAIN_SUBDIV, m.mainParameter,
        "en CLOCK le parametre principal est la SUBDIVISION");
    TEST_ASSERT_EQUAL_INT16_MESSAGE(4, m.subdiv, "la valeur suit");
}

void test_random_makes_the_skip_chance_the_main_parameter() {
    Rig r;
    r.engine.setChannelMode(0, flexseq::MODE_RANDOM);
    r.engine.setSkipChance(0, 3);
    r.enterChannel(0);
    const MainScreenModel m = r.model();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        flexseq::MAIN_SKIP_CHANCE, m.mainParameter,
        "en RANDOM le parametre principal est la CHANCE DE SAUT");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, m.skipChance, "la valeur suit");
}

void test_seq_makes_the_pattern_the_main_parameter() {
    Rig r;
    r.engine.setChannelMode(0, flexseq::MODE_SEQ);
    r.engine.setSelectedPattern(0, 9);
    r.enterChannel(0);
    const MainScreenModel m = r.model();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        flexseq::MAIN_PATTERN, m.mainParameter,
        "en SEQ le parametre principal est le PATTERN");
    TEST_ASSERT_EQUAL_INT8_MESSAGE(9, m.patternIndex, "la valeur suit");
}

void test_the_clock_tab_makes_the_tempo_the_main_parameter() {
    Rig r;
    // ⚠️ L'interface demarre sur l'onglet 1, le premier channel, et NON sur
    // l'onglet d'horloge. Mesure du 2026-09-04 : ce test l'a etabli.
    for (uint8_t guard = 0; guard < 2 * UiController::TAB_COUNT; ++guard) {
        if (r.ui.currentTab() == UiController::TAB_CLOCK) {
            break;
        }
        r.ui.handle(UiController::EVENT_ROTATE, 1);
    }
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(UiController::TAB_CLOCK, r.ui.currentTab(),
        "l onglet d horloge est atteignable en tournant");
    const MainScreenModel m = r.model();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        flexseq::MAIN_TEMPO, m.mainParameter,
        "sur l onglet d horloge le parametre principal est le TEMPO");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(UiController::DEFAULT_TEMPO, m.tempo, "le tempo suit");
    TEST_ASSERT_EQUAL_INT8_MESSAGE(-1, m.patternIndex,
        "hors d un channel, l index de pattern vaut -1 et non 0");
}

void test_the_mode_defaults_to_clock_outside_a_channel() {
    Rig r;
    const MainScreenModel m = r.model();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        static_cast<uint8_t>(flexseq::DEFAULT_CHANNEL_MODE), m.mode,
        "hors d un channel le mode vaut le defaut du domaine");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, m.offset, "et l offset vaut zero");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, m.skipChance, "et la chance de saut vaut zero");
}

void test_the_model_shows_the_editable_length_and_not_the_derived_one() {
    // ⚠️ Sans modulation, baseLength et effectiveLength valent la MEME chose,
    // et ce test ne prouverait rien : un mutant qui lit la derivee a d'abord
    // SURVECU pour cette raison. Le CV les separe, 18 contre 28.
    Rig r;
    r.engine.setChannelMode(0, flexseq::MODE_SEQ);
    r.engine.setBaseLength(0, 18);
    TEST_ASSERT_TRUE(r.engine.setCvDestination(0, flexseq::CV_SOURCE_1,
                                              flexseq::CV_DEST_LENGTH));
    TEST_ASSERT_TRUE(r.engine.setCvInput(flexseq::CV_SOURCE_1, 330));
    r.engine.start();
    r.engine.advance(96);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(28, r.engine.getEffectiveLength(0),
        "precondition : la modulation doit avoir separe les deux longueurs");

    r.enterChannel(0);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(18, r.model().length,
        "l ecran montre la longueur EDITABLE, jamais la longueur modulee");
}


/*
 * PRD 5.0, amendment of 2026-09-15 — the EDIT title names the pattern the
 * channel PLAYS, and it reads CV PATTERN while a source is routed to PATTERN.
 */

namespace {

struct EditRig : Rig {
    char title[flexseq::EDIT_TITLE_CAPACITY];

    EditRig() : Rig() {
        memset(title, 'x', sizeof(title));
        for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
            engine.setChannelMode(ch, flexseq::MODE_SEQ);
        }
    }

    void enterEditOf(uint8_t channel) {
        enterChannel(channel);
        while (ui.field() != UiController::FIELD_EDIT_ENTRY) {
            ui.handle(UiController::EVENT_ROTATE, 1);
        }
        ui.handle(UiController::EVENT_PRESS);
        TEST_ASSERT_EQUAL(UiController::LEVEL_EDIT, ui.level());
    }
};

}  // namespace

void test_the_edit_title_names_the_selected_pattern_without_a_routing() {
    EditRig r;
    r.engine.setSelectedPattern(0, 4);
    r.enterEditOf(0);
    TEST_ASSERT_TRUE(flexseq::editTitleFor(r.ui, r.engine, 0, r.title));
    TEST_ASSERT_EQUAL_STRING("EDIT PATTERN A5", r.title);
    r.engine.setSelectedPattern(0, 13);
    TEST_ASSERT_TRUE(flexseq::editTitleFor(r.ui, r.engine, 0, r.title));
    TEST_ASSERT_EQUAL_STRING("EDIT PATTERN B6", r.title);
}

void test_the_cv_title_names_the_played_pattern_and_not_the_selected_one() {
    EditRig r;
    r.engine.setSelectedPattern(0, 4);
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_PATTERN);
    r.engine.setCvInput(flexseq::CV_SOURCE_1, 330);
    r.engine.start();
    r.engine.advance(96);
    TEST_ASSERT_EQUAL_INT8(14, r.engine.patternCvIndex(0));
    r.enterEditOf(0);
    TEST_ASSERT_TRUE(flexseq::editTitleFor(r.ui, r.engine, 0, r.title));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("CV PATTERN B7", r.title,
        "the title names the pattern the channel plays, B7, not the selected A5");
}

void test_the_cv_title_appears_as_soon_as_a_source_is_routed_to_pattern() {
    EditRig r;
    r.engine.setSelectedPattern(0, 0);
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_2, flexseq::CV_DEST_PATTERN);
    r.enterEditOf(0);
    TEST_ASSERT_TRUE(flexseq::editTitleFor(r.ui, r.engine, 0, r.title));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("CV PATTERN A1", r.title,
        "the routing decides, not the zone: at zone zero the word already changes");
}

void test_another_routing_keeps_the_edit_title() {
    EditRig r;
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_LENGTH);
    r.enterEditOf(0);
    TEST_ASSERT_TRUE(flexseq::editTitleFor(r.ui, r.engine, 0, r.title));
    TEST_ASSERT_EQUAL_STRING("EDIT PATTERN A1", r.title);
}

void test_the_title_refuses_an_invalid_channel_and_leaves_the_buffer_alone() {
    EditRig r;
    TEST_ASSERT_FALSE(flexseq::editTitleFor(r.ui, r.engine, 6, r.title));
    TEST_ASSERT_EQUAL_CHAR('x', r.title[0]);
}

void test_the_edit_cursor_follows_the_step_and_hides_on_the_header() {
    EditRig r;
    r.enterEditOf(0);
    TEST_ASSERT_EQUAL_INT8(0, flexseq::editCursorFor(r.ui));
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_INT8(2, flexseq::editCursorFor(r.ui));
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    r.ui.handle(UiController::EVENT_ROTATE, -1);
    TEST_ASSERT_TRUE(r.ui.isOnHeader());
    TEST_ASSERT_EQUAL_INT8(-1, flexseq::editCursorFor(r.ui));
}

void test_the_edit_cursor_hides_under_a_pattern_routing() {
    EditRig r;
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_PATTERN);
    r.enterEditOf(0);
    r.ui.handle(UiController::EVENT_ROTATE, 1);
    TEST_ASSERT_EQUAL_UINT8(1, r.ui.stepCursor());
    TEST_ASSERT_EQUAL_INT8_MESSAGE(-1, flexseq::editCursorFor(r.ui),
        "nothing to edit, so no cursor is drawn");
    r.engine.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_NONE);
    TEST_ASSERT_EQUAL_INT8(1, flexseq::editCursorFor(r.ui));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_the_model_shows_the_editable_length_and_not_the_derived_one);
    RUN_TEST(test_the_model_carries_the_mode_of_the_selected_channel);
    RUN_TEST(test_the_model_reads_the_selected_channel_and_not_the_first);
    RUN_TEST(test_the_main_parameter_is_never_derived_twice);
    RUN_TEST(test_clock_makes_the_subdivision_the_main_parameter);
    RUN_TEST(test_random_makes_the_skip_chance_the_main_parameter);
    RUN_TEST(test_seq_makes_the_pattern_the_main_parameter);
    RUN_TEST(test_the_clock_tab_makes_the_tempo_the_main_parameter);
    RUN_TEST(test_the_mode_defaults_to_clock_outside_a_channel);
    RUN_TEST(test_the_edit_title_names_the_selected_pattern_without_a_routing);
    RUN_TEST(test_the_cv_title_names_the_played_pattern_and_not_the_selected_one);
    RUN_TEST(test_the_cv_title_appears_as_soon_as_a_source_is_routed_to_pattern);
    RUN_TEST(test_another_routing_keeps_the_edit_title);
    RUN_TEST(test_the_title_refuses_an_invalid_channel_and_leaves_the_buffer_alone);
    RUN_TEST(test_the_edit_cursor_follows_the_step_and_hides_on_the_header);
    RUN_TEST(test_the_edit_cursor_hides_under_a_pattern_routing);
    return UNITY_END();
}
