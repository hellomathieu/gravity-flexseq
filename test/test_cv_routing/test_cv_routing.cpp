#include <stdint.h>
#include <unity.h>

#include <flexseq/CvDestination.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/TriggerSequencer.h>

using flexseq::SequencerEngine;
using flexseq::CvDestination;
using flexseq::CV_DEST_NONE;
using flexseq::CV_DEST_PATTERN;
using flexseq::CV_DEST_LENGTH;
using flexseq::CV_DEST_RESET;
using flexseq::CV_DEST_STEP;
using flexseq::CV_SOURCE_1;
using flexseq::CV_SOURCE_2;
using flexseq::MODE_CLOCK;
using flexseq::MODE_RANDOM;
using flexseq::MODE_SEQ;

static const uint16_t STEP = 96; // DEFAULT_SUBDIV = /1

void setUp() {}
void tearDown() {}

static void routeStep(SequencerEngine& e, uint8_t channel, uint8_t source) {
    TEST_ASSERT_TRUE(e.setCvDestination(channel, source, CV_DEST_STEP));
}

static void routeLength(SequencerEngine& e, uint8_t channel, uint8_t source) {
    TEST_ASSERT_TRUE(e.setCvDestination(channel, source, CV_DEST_LENGTH));
}

/*
 * Famille 1 — routage et destinations
 */

void test_the_five_destination_codes_hold_their_persisted_values() {
    TEST_ASSERT_EQUAL_UINT8(0, static_cast<uint8_t>(CV_DEST_NONE));
    TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(CV_DEST_PATTERN));
    TEST_ASSERT_EQUAL_UINT8(2, static_cast<uint8_t>(CV_DEST_LENGTH));
    TEST_ASSERT_EQUAL_UINT8(3, static_cast<uint8_t>(CV_DEST_RESET));
    TEST_ASSERT_EQUAL_UINT8(4, static_cast<uint8_t>(CV_DEST_STEP));
    TEST_ASSERT_EQUAL_UINT8(5, flexseq::CV_DESTINATION_COUNT);
    TEST_ASSERT_EQUAL_UINT8(2, flexseq::CV_SOURCE_COUNT);
}

void test_a_new_engine_routes_nothing() {
    SequencerEngine e;
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_EQUAL_UINT8(CV_DEST_NONE, e.getCvDestination(ch, CV_SOURCE_1));
        TEST_ASSERT_EQUAL_UINT8(CV_DEST_NONE, e.getCvDestination(ch, CV_SOURCE_2));
        TEST_ASSERT_EQUAL_INT8(0, e.lengthCvOffset(ch));
    }
}

void test_an_out_of_range_destination_is_refused_without_changing_the_channel() {
    SequencerEngine e;
    routeLength(e, 0, CV_SOURCE_1);
    TEST_ASSERT_FALSE(e.setCvDestination(0, CV_SOURCE_1, static_cast<CvDestination>(5)));
    TEST_ASSERT_EQUAL_UINT8(CV_DEST_LENGTH, e.getCvDestination(0, CV_SOURCE_1));
    TEST_ASSERT_FALSE(e.setCvDestination(0, 2, CV_DEST_LENGTH));
    TEST_ASSERT_FALSE(e.setCvDestination(6, CV_SOURCE_1, CV_DEST_LENGTH));
}

void test_an_out_of_range_source_pushes_nothing() {
    SequencerEngine e;
    TEST_ASSERT_TRUE(e.setCvInput(CV_SOURCE_1, 330));
    TEST_ASSERT_FALSE(e.setCvInput(2, 330));
    TEST_ASSERT_EQUAL_INT16(330, e.getCvInput(CV_SOURCE_1));
    TEST_ASSERT_EQUAL_INT16(0, e.getCvInput(2));
}

/*
 * Famille 3 — LENGTH CV
 */

void test_a_pushed_value_alone_changes_nothing() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330);
    TEST_ASSERT_EQUAL_UINT8(18, e.getEffectiveLength(0));
}

void test_the_stopped_transport_never_applies_the_cv() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330);
    e.advance(STEP * 4); // stopped: advance() returns at once
    TEST_ASSERT_EQUAL_UINT8(18, e.getEffectiveLength(0));
}

void test_the_step_boundary_applies_the_cv() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));
}

void test_each_boundary_re_reads_the_pushed_value() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.start();

    e.setCvInput(CV_SOURCE_1, 330);
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));

    e.setCvInput(CV_SOURCE_1, 0);
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0)); // pas encore
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(18, e.getEffectiveLength(0));
}

void test_the_hysteresis_state_is_per_channel() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setChannelMode(1, MODE_SEQ);
    e.setBaseLength(0, 18);
    e.setBaseLength(1, 18);
    routeLength(e, 0, CV_SOURCE_1);
    routeLength(e, 1, CV_SOURCE_1);
    e.setSubdiv(1, 2); // channel 1 steps half as often
    e.start();

    e.setCvInput(CV_SOURCE_1, 330);
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));
    TEST_ASSERT_EQUAL_UINT8(18, e.getEffectiveLength(1)); // pas encore de frontiere

    e.setCvInput(CV_SOURCE_1, 310); // dans la bande de la zone 10
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0)); // garde 10 par hysteresis
    TEST_ASSERT_EQUAL_UINT8(27, e.getEffectiveLength(1)); // part de 0 -> zone 9
}

/*
 * Famille 4 — CV1 et CV2 simultanes
 */

void test_either_source_alone_drives_the_length() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setChannelMode(1, MODE_SEQ);
    e.setBaseLength(0, 18);
    e.setBaseLength(1, 18);
    routeLength(e, 0, CV_SOURCE_1);
    routeLength(e, 1, CV_SOURCE_2);
    e.setCvInput(CV_SOURCE_1, 330);
    e.setCvInput(CV_SOURCE_2, -330);
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));
    TEST_ASSERT_EQUAL_UINT8(8, e.getEffectiveLength(1));
}

void test_two_sources_on_one_length_add_up() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 10);
    routeLength(e, 0, CV_SOURCE_1);
    routeLength(e, 0, CV_SOURCE_2);
    e.setCvInput(CV_SOURCE_1, 330);  // +10
    e.setCvInput(CV_SOURCE_2, 165);  // +5
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(15, e.lengthCvOffset(0));
    TEST_ASSERT_EQUAL_UINT8(25, e.getEffectiveLength(0));
}

void test_the_sum_is_clamped_once_and_not_twice() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 36);
    routeLength(e, 0, CV_SOURCE_1);
    routeLength(e, 0, CV_SOURCE_2);
    e.setCvInput(CV_SOURCE_1, 512);   // +15
    e.setCvInput(CV_SOURCE_2, -512);  // -15
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(0, e.lengthCvOffset(0));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(36, e.getEffectiveLength(0),
        "somme PUIS clamp : deux ecretages successifs rendraient 21");
}

void test_a_source_routed_elsewhere_does_not_contribute() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.setCvDestination(0, CV_SOURCE_2, CV_DEST_PATTERN);
    e.setCvInput(CV_SOURCE_1, 330);
    e.setCvInput(CV_SOURCE_2, 512);
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(10, e.lengthCvOffset(0));
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));
}

void test_an_unrouted_channel_keeps_its_base_length() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 18);
    e.setCvDestination(0, CV_SOURCE_1, CV_DEST_PATTERN);
    e.setCvDestination(0, CV_SOURCE_2, CV_DEST_STEP);
    e.setCvInput(CV_SOURCE_1, 512);
    e.setCvInput(CV_SOURCE_2, -512);
    e.start();
    e.advance(STEP * 4);
    TEST_ASSERT_EQUAL_INT8(0, e.lengthCvOffset(0));
    TEST_ASSERT_EQUAL_UINT8(18, e.getEffectiveLength(0));
}

/*
 * Famille 5 — transitions
 */

void test_removing_the_routing_returns_to_the_base_at_the_next_boundary() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));

    e.setCvDestination(0, CV_SOURCE_1, CV_DEST_NONE);
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0)); // rien en milieu de step
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(18, e.getEffectiveLength(0));
}

void test_changing_the_destination_clears_the_hysteresis_of_that_source() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.start();

    e.setCvInput(CV_SOURCE_1, 330);
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0)); // zone +10

    e.setCvDestination(0, CV_SOURCE_1, CV_DEST_PATTERN);
    routeLength(e, 0, CV_SOURCE_1);

    e.setCvInput(CV_SOURCE_1, 310);
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(27, e.getEffectiveLength(0),
        "la zone repart de 0 : 310 quantifie a 9, et non 10 garde par hysteresis");
}

void test_stop_and_play_keep_the_zones() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));

    e.stop();
    e.reset();
    e.start();
    e.setCvInput(CV_SOURCE_1, 310); // dans la bande de la zone +10
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(28, e.getEffectiveLength(0),
        "un arret ne remet pas la zone a zero");
}

void test_a_reset_never_folds_the_playhead() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 30);
    routeLength(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, -512); // -15
    e.start();
    e.advance(STEP * 6);
    TEST_ASSERT_EQUAL_UINT8(15, e.getEffectiveLength(0));
    e.reset();
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_UINT8(15, e.getEffectiveLength(0));
}

/*
 * Famille 6 — invariants structurels
 */

void test_reset_preserves_the_cv_state() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));

    e.reset();
    TEST_ASSERT_EQUAL_UINT8(CV_DEST_LENGTH, e.getCvDestination(0, CV_SOURCE_1));
    TEST_ASSERT_EQUAL_INT8(10, e.lengthCvOffset(0));
}

void test_the_playhead_folds_at_most_once_per_boundary() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 30);
    routeLength(e, 0, CV_SOURCE_1);
    e.start();
    e.advance(STEP * 26);
    TEST_ASSERT_EQUAL_INT8(26, e.effectiveStep(0));

    e.setCvInput(CV_SOURCE_1, -512); // -15 -> longueur 15
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(15, e.getEffectiveLength(0));
    TEST_ASSERT_EQUAL_INT8(27 % 15, e.effectiveStep(0)); // 12, un seul modulo
}

/*
 * Famille 8 — PATTERN CV, l'index effectif
 */

static void routePattern(SequencerEngine& e, uint8_t channel, uint8_t source) {
    TEST_ASSERT_TRUE(e.setCvDestination(channel, source, CV_DEST_PATTERN));
}

void test_a_new_engine_reports_the_selected_pattern_as_the_index() {
    SequencerEngine e;
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_EQUAL_INT8(0, e.patternCvIndex(ch));
    }
    e.setSelectedPattern(2, 9);
    TEST_ASSERT_EQUAL_INT8(9, e.patternCvIndex(2));
    TEST_ASSERT_EQUAL_INT8(-1, e.patternCvIndex(SequencerEngine::CHANNEL_COUNT));
}

void test_a_pushed_value_alone_does_not_move_the_index() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setSelectedPattern(0, 3);
    routePattern(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330);
    TEST_ASSERT_EQUAL_INT8(3, e.patternCvIndex(0));
}

void test_the_step_boundary_moves_the_pattern_index() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setSelectedPattern(0, 3);
    routePattern(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(13, e.patternCvIndex(0));
}

void test_the_pattern_index_never_moves_the_selected_pattern() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setSelectedPattern(0, 10);
    routePattern(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10, donc 20 ecrete a 15
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(15, e.patternCvIndex(0));
    TEST_ASSERT_EQUAL_INT8(10, e.getSelectedPattern(0));
}

void test_two_sources_on_the_pattern_are_clamped_once_and_not_twice() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setSelectedPattern(0, 15);
    routePattern(e, 0, CV_SOURCE_1);
    routePattern(e, 0, CV_SOURCE_2);
    e.setCvInput(CV_SOURCE_1, 330);  // +10
    e.setCvInput(CV_SOURCE_2, -330); // -10
    e.start();
    e.advance(STEP);
    // Deux ecretages successifs rendraient 5. La somme d'abord rend 15.
    TEST_ASSERT_EQUAL_INT8(15, e.patternCvIndex(0));
}

void test_a_source_routed_to_the_length_does_not_move_the_index() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setSelectedPattern(0, 3);
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(3, e.patternCvIndex(0));
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));
}

void test_a_source_routed_to_the_pattern_does_not_move_the_length() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setSelectedPattern(0, 3);
    e.setBaseLength(0, 18);
    routePattern(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(13, e.patternCvIndex(0));
    TEST_ASSERT_EQUAL_UINT8(18, e.getEffectiveLength(0));
    TEST_ASSERT_EQUAL_INT8(0, e.lengthCvOffset(0));
}

void test_removing_the_pattern_routing_returns_to_the_base_index() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setSelectedPattern(0, 3);
    routePattern(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(13, e.patternCvIndex(0));

    TEST_ASSERT_TRUE(e.setCvDestination(0, CV_SOURCE_1, CV_DEST_NONE));
    TEST_ASSERT_EQUAL_INT8(3, e.patternCvIndex(0));
}

/*
 * Famille 9 — ce qu'un changement de mode laisse intact
 */

void test_the_routing_survives_a_change_of_mode() {
    SequencerEngine e;
    routeLength(e, 0, CV_SOURCE_1);
    routePattern(e, 0, CV_SOURCE_2);

    static const flexseq::ChannelMode COURSE[] = {MODE_CLOCK, MODE_SEQ, MODE_RANDOM,
                                                  MODE_SEQ};
    for (uint8_t step = 0; step < 4; ++step) {
        TEST_ASSERT_TRUE(e.setChannelMode(0, COURSE[step]));
        TEST_ASSERT_EQUAL_UINT8(COURSE[step], e.getChannelMode(0));
        TEST_ASSERT_EQUAL_UINT8(CV_DEST_LENGTH, e.getCvDestination(0, CV_SOURCE_1));
        TEST_ASSERT_EQUAL_UINT8(CV_DEST_PATTERN, e.getCvDestination(0, CV_SOURCE_2));
    }
}

void test_the_bases_survive_a_change_of_mode() {
    SequencerEngine e;
    e.setBaseLength(0, 18);
    e.setSelectedPattern(0, 7);

    static const flexseq::ChannelMode COURSE[] = {MODE_CLOCK, MODE_SEQ, MODE_RANDOM,
                                                  MODE_SEQ};
    for (uint8_t step = 0; step < 4; ++step) {
        TEST_ASSERT_TRUE(e.setChannelMode(0, COURSE[step]));
        TEST_ASSERT_EQUAL_UINT8(18, e.getBaseLength(0));
        TEST_ASSERT_EQUAL_INT8(7, e.getSelectedPattern(0));
    }
}

/*
 * Famille 11 — le gating par mode (E3.7-F2)
 */

void test_a_length_routing_outside_seq_keeps_the_base_length() {
    static const flexseq::ChannelMode OUTSIDE[] = {MODE_CLOCK, MODE_RANDOM};
    for (uint8_t m = 0; m < 2; ++m) {
        SequencerEngine e;
        e.setBaseLength(0, 18);
        e.setChannelMode(0, OUTSIDE[m]);
        routeLength(e, 0, CV_SOURCE_1);
        e.setCvInput(CV_SOURCE_1, 330); // zone +10 si elle etait lue
        e.start();
        e.advance(STEP * 2);
        TEST_ASSERT_EQUAL_INT8(0, e.lengthCvOffset(0));
        TEST_ASSERT_EQUAL_UINT8(18, e.getEffectiveLength(0));
    }
}

void test_a_change_of_mode_resets_the_cv_zone() {
    SequencerEngine e;
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.setChannelMode(0, MODE_SEQ);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(10, e.lengthCvOffset(0));
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));

    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_CLOCK));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(18, e.getEffectiveLength(0),
        "le retour a la base est immediat, jamais differe a une frontiere");

    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_SEQ));
    TEST_ASSERT_EQUAL_INT8_MESSAGE(0, e.lengthCvOffset(0),
        "la zone fut remise a zero : 10 serait la modulation heritee");
    TEST_ASSERT_EQUAL_UINT8(18, e.getEffectiveLength(0));
}

void test_the_return_to_seq_applies_the_cv_at_the_first_boundary_only() {
    SequencerEngine e;
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.setChannelMode(0, MODE_SEQ);
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(18, e.getEffectiveLength(0));

    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_CLOCK));
    e.setCvInput(CV_SOURCE_1, 330); // zone +10 une fois lue en SEQ
    e.advance(STEP * 2);
    TEST_ASSERT_EQUAL_INT8(0, e.lengthCvOffset(0));
    TEST_ASSERT_EQUAL_UINT8(18, e.getEffectiveLength(0));

    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_SEQ));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(18, e.getEffectiveLength(0),
        "avant la premiere frontiere le canal joue sa base");
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(10, e.lengthCvOffset(0));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(28, e.getEffectiveLength(0),
        "la premiere frontiere reapplique le CV courant");
}

/*
 * Famille 10 — ce qu'un changement de base laisse intact
 */

void test_the_length_offset_survives_a_change_of_base() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(10, e.lengthCvOffset(0));
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));

    TEST_ASSERT_TRUE(e.setBaseLength(0, 20));
    TEST_ASSERT_EQUAL_INT8(10, e.lengthCvOffset(0));
    TEST_ASSERT_EQUAL_UINT8(30, e.getEffectiveLength(0));
    TEST_ASSERT_EQUAL_UINT8(20, e.getBaseLength(0));
}

// La stabilite de l'offset PATTERN s'observe INDIRECTEMENT : l'API publique
// n'expose pas d'offset, seulement l'index derive. L'index qui suit la base
// d'exactement +10 est la preuve que la zone n'a pas bouge. Les bases 3 et 4
// gardent de la marge sous 15 : un ecretage rendrait la meme valeur pour un
// offset conserve et pour un offset plus grand.
void test_the_derived_pattern_index_follows_a_change_of_base_without_losing_the_offset() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setSelectedPattern(0, 3);
    routePattern(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(13, e.patternCvIndex(0));

    TEST_ASSERT_TRUE(e.setSelectedPattern(0, 4));
    TEST_ASSERT_EQUAL_INT8(14, e.patternCvIndex(0));
    TEST_ASSERT_EQUAL_INT8(4, e.getSelectedPattern(0));
}

void test_the_step_boundary_moves_the_step_offset() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 12);
    routeStep(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(10, e.stepCvOffset(0));
}

void test_the_step_cv_shifts_the_read_without_moving_the_local_step() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 12);
    routeStep(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.start();
    for (uint8_t i = 0; i < 4; ++i) {
        e.advance(STEP);
    }
    TEST_ASSERT_EQUAL_INT8(4, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(2, e.currentReadStep(0));
}

void test_a_change_of_length_keeps_the_step_offset_and_moves_the_read() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 12);
    routeStep(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.start();
    for (uint8_t i = 0; i < 5; ++i) {
        e.advance(STEP);
    }
    TEST_ASSERT_EQUAL_INT8(10, e.stepCvOffset(0));
    TEST_ASSERT_EQUAL_INT8(3, e.currentReadStep(0));
    e.setBaseLength(0, 8);
    TEST_ASSERT_EQUAL_UINT8(8, e.getEffectiveLength(0));
    TEST_ASSERT_EQUAL_INT8(10, e.stepCvOffset(0));
    TEST_ASSERT_EQUAL_INT8(7, e.currentReadStep(0));
}

void test_two_sources_on_the_step_add_before_the_modulo() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 36);
    routeStep(e, 0, CV_SOURCE_1);
    routeStep(e, 0, CV_SOURCE_2);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.setCvInput(CV_SOURCE_2, 330); // zone +10
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(20, e.stepCvOffset(0));
    TEST_ASSERT_EQUAL_INT8(21, e.currentReadStep(0));
}

void test_a_triplet_on_the_read_step_stretches_the_step() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 12);
    e.instanceForChannel(0)->setRatchet(4, flexseq::RATCHET_TRIPLET);
    routeStep(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 99); // zone +3
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT16(2 * STEP, e.currentStepTicks(0));
    TEST_ASSERT_EQUAL_UINT8(3, e.currentStepTriggers(0));
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(4, e.currentReadStep(0));
}

void test_a_triplet_on_the_local_step_alone_does_not_stretch_the_step() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 12);
    e.instanceForChannel(0)->setRatchet(1, flexseq::RATCHET_TRIPLET);
    routeStep(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 99); // zone +3
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT16(STEP, e.currentStepTicks(0));
    TEST_ASSERT_EQUAL_UINT8(1, e.currentStepTriggers(0));
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(4, e.currentReadStep(0));
}

// STEP-8b.4: the trigger decision reads the CONTENT at readStep, PRD 10.2
// E3.4-1 and 10.3 point 6. The first boundary consumes the armed onset of
// step 0 (D79) and is discarded; the second boundary emits one onset on
// local step 2, read step 5 under a zone of +3. No ratchet anywhere, so the
// count is the content decision alone.
static void advanceToLocalStepTwoUnderStepOffsetThree(SequencerEngine& e,
                                                      flexseq::TriggerSequencer& t) {
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 12);
    routeStep(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 99); // zone +3
    e.start();
    e.advance(STEP);
    t.update();
    e.advance(STEP);
    t.update();
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(5, e.currentReadStep(0));
}

void test_the_trigger_fires_on_a_step_active_at_the_read_step_only() {
    SequencerEngine e;
    flexseq::TriggerSequencer t(e);
    e.instanceForChannel(0)->writeStep(5, true);
    advanceToLocalStepTwoUnderStepOffsetThree(e, t);
    TEST_ASSERT_EQUAL_UINT8(1, t.triggerCount(0));
}

void test_the_trigger_stays_silent_on_a_step_active_at_the_local_step_only() {
    SequencerEngine e;
    flexseq::TriggerSequencer t(e);
    e.instanceForChannel(0)->writeStep(2, true);
    advanceToLocalStepTwoUnderStepOffsetThree(e, t);
    TEST_ASSERT_EQUAL_UINT8(0, t.triggerCount(0));
}

// STEP-8b.5: LENGTH and STEP change at the same boundary, PRD 10.3 E3.4-4.
// CV1 takes the length from 12 to 8 on the boundary that lands on local
// step 6; CV2 holds a read shift of +3. The read step moves from 9 to 1, the
// offset does not move (P33), and the content and the ratchet are read at
// the read step of the NEW length: step 1 is active with a TRIPLET, steps 6
// and 9 are inactive with no ratchet.
void test_a_length_change_at_the_boundary_reads_the_content_and_the_ratchet_at_the_new_read_step() {
    SequencerEngine e;
    flexseq::TriggerSequencer t(e);
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 12);
    e.instanceForChannel(0)->writeStep(1, true);
    e.instanceForChannel(0)->setRatchet(1, flexseq::RATCHET_TRIPLET);
    routeLength(e, 0, CV_SOURCE_1);
    routeStep(e, 0, CV_SOURCE_2);
    e.setCvInput(CV_SOURCE_2, 99); // zone +3
    e.start();
    for (uint8_t i = 0; i < 5; ++i) {
        e.advance(STEP);
        t.update();
    }
    TEST_ASSERT_EQUAL_INT8(5, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(8, e.currentReadStep(0));
    TEST_ASSERT_EQUAL_UINT8(12, e.getEffectiveLength(0));
    TEST_ASSERT_EQUAL_UINT8(1, e.currentStepTriggers(0));

    e.setCvInput(CV_SOURCE_1, -132); // zone -4
    e.advance(STEP);
    t.update();
    TEST_ASSERT_EQUAL_UINT8(8, e.getEffectiveLength(0));
    TEST_ASSERT_EQUAL_INT8(6, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(3, e.stepCvOffset(0));
    TEST_ASSERT_EQUAL_INT8(1, e.currentReadStep(0));
    TEST_ASSERT_EQUAL_UINT16(2 * STEP, e.currentStepTicks(0));
    TEST_ASSERT_EQUAL_UINT8(3, e.currentStepTriggers(0));
    TEST_ASSERT_EQUAL_UINT8(1, t.triggerCount(0));
}

// STEP-9.3 G1: a CV reset puts the local step back to 0 and keeps the STEP
// zone (PRD 10.3, RESET is immediate; the zones survive a reset). The armed
// onset of step 0 is therefore read at the offset, and the first boundary
// reads one step further.
void test_a_cv_reset_keeps_the_step_zone_and_reads_from_the_offset() {
    SequencerEngine e;
    flexseq::TriggerSequencer t(e);
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 12);
    e.instanceForChannel(0)->writeStep(3, true);
    TEST_ASSERT_TRUE(e.setCvDestination(0, CV_SOURCE_1, CV_DEST_RESET));
    routeStep(e, 0, CV_SOURCE_2);
    e.setCvInput(CV_SOURCE_2, 99); // zone +3
    e.start();
    e.advance(STEP);
    t.update();
    e.advance(STEP);
    t.update();
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(5, e.currentReadStep(0));

    e.applyCvResetEvents(static_cast<uint8_t>(1u << CV_SOURCE_1));
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(3, e.stepCvOffset(0));
    TEST_ASSERT_EQUAL_INT8(3, e.currentReadStep(0));

    e.advance(1);
    t.update();
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    TEST_ASSERT_EQUAL_UINT8(1, t.triggerCount(0));

    e.advance(STEP - 1);
    t.update();
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(4, e.currentReadStep(0));
    TEST_ASSERT_EQUAL_UINT8(0, t.triggerCount(0));
}

// STEP-9.3 G3: a negative zone wraps the read backward, PRD 10.2 P30.
void test_a_negative_step_zone_wraps_the_read_backward() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 12);
    routeStep(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, -99); // zone -3
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(-3, e.stepCvOffset(0));
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(10, e.currentReadStep(0));
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(11, e.currentReadStep(0));
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(3, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(0, e.currentReadStep(0));
}

// STEP-9.3 G4: the two bounds of the length under a STEP routing.
void test_a_length_of_one_under_a_step_routing_always_reads_step_zero() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 1);
    routeStep(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.start();
    for (uint8_t i = 0; i < 3; ++i) {
        e.advance(STEP);
        TEST_ASSERT_EQUAL_INT8(10, e.stepCvOffset(0));
        TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
        TEST_ASSERT_EQUAL_INT8(0, e.currentReadStep(0));
    }
}

void test_a_length_of_thirty_six_under_a_step_routing_wraps_at_thirty_six() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 36);
    routeStep(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 99); // zone +3
    e.start();
    for (uint8_t i = 0; i < 34; ++i) {
        e.advance(STEP);
    }
    TEST_ASSERT_EQUAL_INT8(34, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(1, e.currentReadStep(0));
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(35, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(2, e.currentReadStep(0));
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(0, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(3, e.currentReadStep(0));
}

// STEP-9.3 G5: outside SEQ the STEP zone holds 0 (PRD 10.2, matrix F0 and A3),
// and the return to SEQ reads the base until the next boundary re-applies the CV.
void test_a_change_of_mode_resets_the_step_zone_and_the_return_to_seq_reapplies_it_at_the_next_boundary() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 12);
    routeStep(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(10, e.stepCvOffset(0));
    TEST_ASSERT_EQUAL_INT8(11, e.currentReadStep(0));

    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_CLOCK));
    TEST_ASSERT_EQUAL_INT8(0, e.stepCvOffset(0));
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(1, e.currentReadStep(0));
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(0, e.stepCvOffset(0));
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(2, e.currentReadStep(0));

    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_SEQ));
    TEST_ASSERT_EQUAL_INT8(0, e.stepCvOffset(0));
    TEST_ASSERT_EQUAL_INT8(2, e.currentReadStep(0));
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(10, e.stepCvOffset(0));
    TEST_ASSERT_EQUAL_INT8(3, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(1, e.currentReadStep(0));
}

// STEP-9.3 G6: a ratchet 6 is read at the read step. At SUBDIV /1 a step is
// 96 ticks, a slot is 16 ticks, so R6 is admitted by ratchetFitsStep() (the
// matrix of test_ratchet_matrix): a miss here is the coordinate, never the rate.
void test_a_ratchet_six_on_the_read_step_gives_six_triggers() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 12);
    e.instanceForChannel(0)->setRatchet(4, flexseq::RATCHET_6);
    routeStep(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 99); // zone +3
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(4, e.currentReadStep(0));
    TEST_ASSERT_EQUAL_UINT8(6, e.currentStepTriggers(0));
    TEST_ASSERT_EQUAL_UINT16(STEP, e.currentStepTicks(0));
}

void test_a_ratchet_six_on_the_local_step_alone_gives_one_trigger() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 12);
    e.instanceForChannel(0)->setRatchet(1, flexseq::RATCHET_6);
    routeStep(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 99); // zone +3
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(1, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(4, e.currentReadStep(0));
    TEST_ASSERT_EQUAL_UINT8(1, e.currentStepTriggers(0));
    TEST_ASSERT_EQUAL_UINT16(STEP, e.currentStepTicks(0));
}

// STEP-9.3 G10: a source routed to STEP feeds neither the length nor the index.
void test_a_source_routed_to_the_step_moves_neither_the_length_nor_the_pattern_index() {
    SequencerEngine e;
    e.setChannelMode(0, MODE_SEQ);
    e.setBaseLength(0, 12);
    e.setSelectedPattern(0, 5);
    routeStep(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(10, e.stepCvOffset(0));
    TEST_ASSERT_EQUAL_INT8(0, e.lengthCvOffset(0));
    TEST_ASSERT_EQUAL_UINT8(12, e.getEffectiveLength(0));
    TEST_ASSERT_EQUAL_INT8(5, e.patternCvIndex(0));
    TEST_ASSERT_EQUAL_INT8(5, e.getSelectedPattern(0));
}

// STEP-9.3 G11: in CLOCK a STEP routing stays inert, the zone holds 0, and the
// channel keeps one onset and one trigger per step.
void test_a_step_routing_in_clock_keeps_a_null_zone_and_one_trigger_per_step() {
    SequencerEngine e;
    flexseq::TriggerSequencer t(e);
    e.setChannelMode(0, MODE_CLOCK);
    e.setBaseLength(0, 12);
    routeStep(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10 if it were read
    e.start();
    e.advance(STEP);
    t.update();
    e.advance(STEP);
    t.update();
    TEST_ASSERT_EQUAL_INT8(0, e.stepCvOffset(0));
    TEST_ASSERT_EQUAL_INT8(2, e.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(2, e.currentReadStep(0));
    TEST_ASSERT_EQUAL_UINT8(1, e.onsetCount(0));
    TEST_ASSERT_EQUAL_UINT8(1, t.triggerCount(0));
}

static char modInitial(flexseq::CvDestination d) {
    switch (d) {
        case flexseq::CV_DEST_PATTERN: return 'P';
        case flexseq::CV_DEST_LENGTH:  return 'L';
        case flexseq::CV_DEST_RESET:   return 'R';
        case flexseq::CV_DEST_STEP:    return 'S';
        default:                       return '-';
    }
}

void test_the_mod_cycle_holds_the_twenty_one_values_of_the_prd() {
    static const char* attendu[21] = {
        "-/-",
        "P/-", "L/-", "R/-", "S/-",
        "-/P", "-/L", "-/R", "-/S",
        "P/L", "P/R", "P/S",
        "L/P", "L/R", "L/S",
        "R/P", "R/L", "R/S",
        "S/P", "S/L", "S/R"
    };
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(21, flexseq::MOD_CHOICE_COUNT,
        "le cycle porte vingt et une valeurs");
    for (uint8_t index = 0; index < 21; ++index) {
        flexseq::CvDestination a = flexseq::CV_DEST_NONE;
        flexseq::CvDestination b = flexseq::CV_DEST_NONE;
        flexseq::modChoiceAt(index, &a, &b);
        char lu[4] = { modInitial(a), '/', modInitial(b), '\0' };
        TEST_ASSERT_EQUAL_STRING_MESSAGE(attendu[index], lu,
            "l ordre du cycle est celui du PRD 10.2");
        TEST_ASSERT_EQUAL_INT8_MESSAGE(static_cast<int8_t>(index),
            flexseq::modIndexOf(a, b), "l index se retrouve depuis la paire");
    }
}

void test_the_cycle_never_puts_two_sources_on_the_same_destination() {
    for (uint8_t d = 1; d < flexseq::CV_DESTINATION_COUNT; ++d) {
        const flexseq::CvDestination dest = static_cast<flexseq::CvDestination>(d);
        TEST_ASSERT_EQUAL_INT8_MESSAGE(-1, flexseq::modIndexOf(dest, dest),
            "la rotation ne peut pas produire deux entrees sur la meme cible");
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_the_five_destination_codes_hold_their_persisted_values);
    RUN_TEST(test_a_new_engine_routes_nothing);
    RUN_TEST(test_an_out_of_range_destination_is_refused_without_changing_the_channel);
    RUN_TEST(test_an_out_of_range_source_pushes_nothing);
    RUN_TEST(test_a_pushed_value_alone_changes_nothing);
    RUN_TEST(test_the_stopped_transport_never_applies_the_cv);
    RUN_TEST(test_the_step_boundary_applies_the_cv);
    RUN_TEST(test_each_boundary_re_reads_the_pushed_value);
    RUN_TEST(test_the_hysteresis_state_is_per_channel);
    RUN_TEST(test_either_source_alone_drives_the_length);
    RUN_TEST(test_two_sources_on_one_length_add_up);
    RUN_TEST(test_the_sum_is_clamped_once_and_not_twice);
    RUN_TEST(test_a_source_routed_elsewhere_does_not_contribute);
    RUN_TEST(test_an_unrouted_channel_keeps_its_base_length);
    RUN_TEST(test_removing_the_routing_returns_to_the_base_at_the_next_boundary);
    RUN_TEST(test_changing_the_destination_clears_the_hysteresis_of_that_source);
    RUN_TEST(test_stop_and_play_keep_the_zones);
    RUN_TEST(test_a_reset_never_folds_the_playhead);
    RUN_TEST(test_reset_preserves_the_cv_state);
    RUN_TEST(test_the_playhead_folds_at_most_once_per_boundary);

    RUN_TEST(test_a_new_engine_reports_the_selected_pattern_as_the_index);
    RUN_TEST(test_a_pushed_value_alone_does_not_move_the_index);
    RUN_TEST(test_the_step_boundary_moves_the_pattern_index);
    RUN_TEST(test_the_pattern_index_never_moves_the_selected_pattern);
    RUN_TEST(test_two_sources_on_the_pattern_are_clamped_once_and_not_twice);
    RUN_TEST(test_a_source_routed_to_the_length_does_not_move_the_index);
    RUN_TEST(test_a_source_routed_to_the_pattern_does_not_move_the_length);
    RUN_TEST(test_removing_the_pattern_routing_returns_to_the_base_index);

    RUN_TEST(test_the_routing_survives_a_change_of_mode);
    RUN_TEST(test_the_bases_survive_a_change_of_mode);

    RUN_TEST(test_a_length_routing_outside_seq_keeps_the_base_length);
    RUN_TEST(test_a_change_of_mode_resets_the_cv_zone);
    RUN_TEST(test_the_return_to_seq_applies_the_cv_at_the_first_boundary_only);

    RUN_TEST(test_the_length_offset_survives_a_change_of_base);
    RUN_TEST(test_the_derived_pattern_index_follows_a_change_of_base_without_losing_the_offset);
    RUN_TEST(test_the_step_boundary_moves_the_step_offset);
    RUN_TEST(test_the_step_cv_shifts_the_read_without_moving_the_local_step);
    RUN_TEST(test_a_change_of_length_keeps_the_step_offset_and_moves_the_read);
    RUN_TEST(test_two_sources_on_the_step_add_before_the_modulo);
    RUN_TEST(test_a_triplet_on_the_read_step_stretches_the_step);
    RUN_TEST(test_a_triplet_on_the_local_step_alone_does_not_stretch_the_step);
    RUN_TEST(test_the_trigger_fires_on_a_step_active_at_the_read_step_only);
    RUN_TEST(test_the_trigger_stays_silent_on_a_step_active_at_the_local_step_only);
    RUN_TEST(test_a_length_change_at_the_boundary_reads_the_content_and_the_ratchet_at_the_new_read_step);
    RUN_TEST(test_a_cv_reset_keeps_the_step_zone_and_reads_from_the_offset);
    RUN_TEST(test_a_negative_step_zone_wraps_the_read_backward);
    RUN_TEST(test_a_length_of_one_under_a_step_routing_always_reads_step_zero);
    RUN_TEST(test_a_length_of_thirty_six_under_a_step_routing_wraps_at_thirty_six);
    RUN_TEST(test_a_change_of_mode_resets_the_step_zone_and_the_return_to_seq_reapplies_it_at_the_next_boundary);
    RUN_TEST(test_a_ratchet_six_on_the_read_step_gives_six_triggers);
    RUN_TEST(test_a_ratchet_six_on_the_local_step_alone_gives_one_trigger);
    RUN_TEST(test_a_source_routed_to_the_step_moves_neither_the_length_nor_the_pattern_index);
    RUN_TEST(test_a_step_routing_in_clock_keeps_a_null_zone_and_one_trigger_per_step);
    RUN_TEST(test_the_mod_cycle_holds_the_twenty_one_values_of_the_prd);
    RUN_TEST(test_the_cycle_never_puts_two_sources_on_the_same_destination);
    return UNITY_END();
}
