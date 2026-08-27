#include <stdint.h>
#include <unity.h>

#include <flexseq/Pattern.h>
#include <flexseq/PatternBank.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>
#include <flexseq/UiController.h>

using flexseq::PatternBank;
using flexseq::SequencerEngine;
using flexseq::Transport;
using flexseq::UiController;

void setUp() {}
void tearDown() {}

static const uint16_t STEP = SequencerEngine::PPQN; // 96 = default ticksPerStep (/1)

void test_tick_advances_only_while_running() {
    SequencerEngine e;
    Transport t(e);

    t.tick(STEP);
    TEST_ASSERT_EQUAL_UINT32(0, e.masterPhase()); // not running yet

    t.resume(); // run without reset
    t.tick(STEP);
    TEST_ASSERT_EQUAL_UINT32(STEP, e.masterPhase());
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
}

void test_start_resets_then_runs() {
    SequencerEngine e;
    Transport t(e);

    t.resume();
    t.tick(STEP * 5);
    TEST_ASSERT_EQUAL_UINT32(STEP * 5, e.masterPhase());

    t.start(); // MIDI Start = reset + run
    TEST_ASSERT_EQUAL_UINT32(0, e.masterPhase());
    TEST_ASSERT_TRUE(e.isRunning());
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
}

void test_stop_preserves_phase_then_resume_continues() {
    SequencerEngine e;
    Transport t(e);

    t.resume();
    t.tick(STEP * 3);
    t.stop();
    TEST_ASSERT_FALSE(e.isRunning());
    TEST_ASSERT_EQUAL_UINT32(STEP * 3, e.masterPhase());

    t.tick(STEP); // ignored while stopped
    TEST_ASSERT_EQUAL_UINT32(STEP * 3, e.masterPhase());

    t.resume(); // continue at current phase (no reset)
    t.tick(STEP);
    TEST_ASSERT_EQUAL_UINT32(STEP * 4, e.masterPhase());
    TEST_ASSERT_EQUAL_INT8(4, e.effectiveStep(0));
}

void test_reset_zeroes_phase_without_stopping() {
    SequencerEngine e;
    Transport t(e);

    t.resume();
    t.tick(STEP * 7);
    t.reset(); // external reset (global)
    TEST_ASSERT_EQUAL_UINT32(0, e.masterPhase());
    TEST_ASSERT_TRUE(e.isRunning());

    t.tick(STEP); // still running -> progresses
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
}

void test_batched_ticks_cross_multiple_steps() {
    SequencerEngine e;
    Transport t(e);

    t.resume();
    t.tick(STEP * 4); // 4 output ticks worth in one drain
    TEST_ASSERT_EQUAL_INT8(4, e.effectiveStep(0));
}

/*
 * Lot 5 — ce que PLAY et le changement de source doivent garantir
 */

namespace {

struct Wired {
    PatternBank bank;
    SequencerEngine engine;
    Transport transport;
    UiController ui;

    Wired() : engine(), transport(engine), ui(engine, transport) {
        for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
            engine.setChannelMode(ch, flexseq::MODE_SEQ);
        }
        engine.setPatternBank(&bank);
    }
};

}  // namespace

// Un TRIOLET etire le temps d'un seul channel (PRD 6.3), donc les six channels
// derivent les uns par rapport aux autres. PLAY doit tous les realigner : c'est
// la seule chose qui le fait.
void test_play_realigns_channels_that_a_triplet_had_pulled_apart() {
    Wired w;
    // Le triolet va dans un pattern que SEUL le channel 0 selectionne : sur le
    // pattern 0, que les six channels partagent par defaut, ils s'etireraient
    // tous pareil et rien ne deriverait.
    w.bank.getPattern(1)->setRatchet(0, flexseq::RATCHET_TRIPLET);
    w.engine.setSelectedPattern(0, 1);
    w.engine.refreshTiming();
    w.engine.start();

    // Le channel 0 tient son premier step deux fois plus longtemps.
    w.engine.advance(6 * SequencerEngine::PPQN);
    TEST_ASSERT_TRUE_MESSAGE(w.engine.effectiveStep(0) != w.engine.effectiveStep(1),
                             "les channels devaient avoir derive");

    w.ui.handle(UiController::EVENT_PLAY_PRESS);  // arret
    w.ui.handle(UiController::EVENT_PLAY_PRESS);  // marche = reset + run
    TEST_ASSERT_TRUE(w.engine.isRunning());
    TEST_ASSERT_EQUAL_UINT32(0, w.engine.masterPhase());
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_EQUAL_INT8_MESSAGE(0, w.engine.effectiveStep(ch),
                                       "PLAY n'a pas realigne tous les channels");
    }
}

// Le tempo appartient a l'etat d'interface, pas a la source : un aller-retour
// interne -> externe -> interne doit le laisser intact.
void test_a_source_round_trip_leaves_the_tempo_untouched() {
    Wired w;
    w.ui.setTempo(174);
    for (uint8_t source = 0; source < UiController::CLOCK_SOURCE_COUNT; ++source) {
        TEST_ASSERT_TRUE(w.ui.setClockSource(source));
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(174, w.ui.tempo(),
                                         "le changement de source a touche au tempo");
    }
    TEST_ASSERT_TRUE(w.ui.setClockSource(0));
    TEST_ASSERT_EQUAL_UINT16(174, w.ui.tempo());
    TEST_ASSERT_EQUAL_UINT8(0, w.ui.clockSource());
}

// Changer de source ne doit pas arreter le moteur : seul PLAY le fait.
void test_changing_the_source_does_not_stop_the_engine() {
    Wired w;
    w.engine.start();
    w.engine.advance(SequencerEngine::PPQN);
    const uint32_t phase = w.engine.masterPhase();
    for (uint8_t source = 0; source < UiController::CLOCK_SOURCE_COUNT; ++source) {
        w.ui.setClockSource(source);
        TEST_ASSERT_TRUE_MESSAGE(w.engine.isRunning(),
                                 "un changement de source a arrete le moteur");
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(phase, w.engine.masterPhase(),
                                     "un changement de source a bouge la phase");
}

void test_the_source_field_never_leaves_the_six_valid_values() {
    Wired w;
    TEST_ASSERT_FALSE(w.ui.setClockSource(UiController::CLOCK_SOURCE_COUNT));
    TEST_ASSERT_FALSE(w.ui.setClockSource(255));
    TEST_ASSERT_EQUAL_UINT8(0, w.ui.clockSource());
}

void test_the_tempo_bounds_are_refused_not_clamped_by_the_setter() {
    Wired w;
    TEST_ASSERT_FALSE(w.ui.setTempo(UiController::MIN_TEMPO - 1));
    TEST_ASSERT_FALSE(w.ui.setTempo(UiController::MAX_TEMPO + 1));
    TEST_ASSERT_EQUAL_UINT16(UiController::DEFAULT_TEMPO, w.ui.tempo());
    TEST_ASSERT_TRUE(w.ui.setTempo(UiController::MIN_TEMPO));
    TEST_ASSERT_TRUE(w.ui.setTempo(UiController::MAX_TEMPO));
    TEST_ASSERT_EQUAL_UINT16(UiController::MAX_TEMPO, w.ui.tempo());
}

// Le drainage groupe : plusieurs ticks externes accumules entre deux passages
// doivent avancer exactement comme des ticks isoles.
void test_external_ticks_drained_in_a_batch_land_where_single_ticks_would() {
    Wired single;
    Wired batched;
    single.engine.start();
    batched.engine.start();
    for (uint16_t i = 0; i < 4 * SequencerEngine::PPQN; ++i) {
        single.transport.tick(1);
    }
    batched.transport.tick(4 * SequencerEngine::PPQN);
    TEST_ASSERT_EQUAL_UINT32(single.engine.masterPhase(), batched.engine.masterPhase());
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_EQUAL_INT8(single.engine.effectiveStep(ch), batched.engine.effectiveStep(ch));
    }
}

// Transport rapporte l'etat de marche pour que l'ADAPTATEUR puisse le refleter
// sur l'horloge de libGravity sans que le domaine connaisse le materiel.
void test_transport_reports_the_running_state() {
    SequencerEngine engine;
    Transport transport(engine);
    TEST_ASSERT_FALSE(transport.isRunning());
    transport.start();
    TEST_ASSERT_TRUE(transport.isRunning());
    transport.stop();
    TEST_ASSERT_FALSE(transport.isRunning());
    transport.resume();
    TEST_ASSERT_TRUE(transport.isRunning());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_tick_advances_only_while_running);
    RUN_TEST(test_start_resets_then_runs);
    RUN_TEST(test_stop_preserves_phase_then_resume_continues);
    RUN_TEST(test_reset_zeroes_phase_without_stopping);
    RUN_TEST(test_batched_ticks_cross_multiple_steps);

    RUN_TEST(test_play_realigns_channels_that_a_triplet_had_pulled_apart);
    RUN_TEST(test_a_source_round_trip_leaves_the_tempo_untouched);
    RUN_TEST(test_changing_the_source_does_not_stop_the_engine);
    RUN_TEST(test_the_source_field_never_leaves_the_six_valid_values);
    RUN_TEST(test_the_tempo_bounds_are_refused_not_clamped_by_the_setter);
    RUN_TEST(test_external_ticks_drained_in_a_batch_land_where_single_ticks_would);
    RUN_TEST(test_transport_reports_the_running_state);
    return UNITY_END();
}
