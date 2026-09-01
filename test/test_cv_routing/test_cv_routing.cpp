#include <stdint.h>
#include <unity.h>

#include <flexseq/CvDestination.h>
#include <flexseq/SequencerEngine.h>

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
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330);
    TEST_ASSERT_EQUAL_UINT8(18, e.getEffectiveLength(0));
}

void test_the_stopped_transport_never_applies_the_cv() {
    SequencerEngine e;
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330);
    e.advance(STEP * 4); // stopped: advance() returns at once
    TEST_ASSERT_EQUAL_UINT8(18, e.getEffectiveLength(0));
}

void test_the_step_boundary_applies_the_cv() {
    SequencerEngine e;
    e.setBaseLength(0, 18);
    routeLength(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));
}

void test_each_boundary_re_reads_the_pushed_value() {
    SequencerEngine e;
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
    e.setSelectedPattern(0, 3);
    routePattern(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330);
    TEST_ASSERT_EQUAL_INT8(3, e.patternCvIndex(0));
}

void test_the_step_boundary_moves_the_pattern_index() {
    SequencerEngine e;
    e.setSelectedPattern(0, 3);
    routePattern(e, 0, CV_SOURCE_1);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.start();
    e.advance(STEP);
    TEST_ASSERT_EQUAL_INT8(13, e.patternCvIndex(0));
}

void test_the_pattern_index_never_moves_the_selected_pattern() {
    SequencerEngine e;
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

// CARACTERISATION DE L'ETAT ACTUEL, pas une propriete normative. La regle P12
// de la conception E3.1 demande l'inverse : un changement de mode remettra la
// zone a 0. Ce test devra donc etre REMPLACE par le lot qui l'implemente, pas
// complete. Son nom porte 'currently' pour cette raison.
void test_a_change_of_mode_currently_keeps_the_cv_zone() {
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
    TEST_ASSERT_EQUAL_INT8(10, e.lengthCvOffset(0));
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));

    TEST_ASSERT_TRUE(e.setChannelMode(0, MODE_SEQ));
    TEST_ASSERT_EQUAL_INT8(10, e.lengthCvOffset(0));
    TEST_ASSERT_EQUAL_UINT8(28, e.getEffectiveLength(0));
}

/*
 * Famille 10 — ce qu'un changement de base laisse intact
 */

void test_the_length_offset_survives_a_change_of_base() {
    SequencerEngine e;
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
    RUN_TEST(test_a_change_of_mode_currently_keeps_the_cv_zone);

    RUN_TEST(test_the_length_offset_survives_a_change_of_base);
    RUN_TEST(test_the_derived_pattern_index_follows_a_change_of_base_without_losing_the_offset);
    return UNITY_END();
}
