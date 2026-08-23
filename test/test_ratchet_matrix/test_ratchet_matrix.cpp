#include <stdint.h>
#include <unity.h>

#include <flexseq/Pattern.h>
#include <flexseq/PatternBank.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/Transport.h>
#include <flexseq/TriggerSequencer.h>
#include <flexseq/UiController.h>

using flexseq::Pattern;
using flexseq::PatternBank;
using flexseq::SequencerEngine;
using flexseq::Transport;
using flexseq::TriggerSequencer;
using flexseq::UiController;

void setUp() {}
void tearDown() {}

namespace {

const uint8_t SUBDIV_COUNT = 25;

// La table des cadences, ecrite en clair. Elle ne passe PAS par subdivToTicks :
// une assertion qui se compare a la fonction qu'elle teste se confirme
// elle-meme.
const int16_t SUBDIVS[SUBDIV_COUNT] = {
    -24, -16, -12, -8, -6, -4, -3, -2,
    1,
    2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 24, 32, 64, 128,
};

const uint16_t STEP_TICKS[SUBDIV_COUNT] = {
    4, 6, 8, 12, 16, 24, 32, 48,
    96,
    192, 288, 384, 480, 576, 672, 768, 864, 960, 1056, 1152,
    1536, 2304, 3072, 6144, 12288,
};

const uint8_t CODE_COUNT = 5;
const uint8_t CODES[CODE_COUNT] = {
    flexseq::RATCHET_2, flexseq::RATCHET_3, flexseq::RATCHET_4,
    flexseq::RATCHET_6, flexseq::RATCHET_TRIPLET,
};

// Nombre de declenchements RELEVE sur le firmware du 2026-08-23, cadence par
// cadence. C'est une CARACTERISATION : 13 cases sur 125 valent 1 au lieu de N,
// parce que la duree du step n'est pas divisible par le nombre de
// declenchements. Le lot 21 remplace cette regle ; ces valeurs disent ce qu'elle
// etait, pour que le changement se voie.
const uint8_t TRIGGERS[SUBDIV_COUNT][CODE_COUNT] = {
    {2, 1, 4, 1, 1},  // x24
    {2, 3, 1, 6, 3},  // x16
    {2, 1, 4, 1, 1},  // x12
    {2, 3, 4, 6, 3},  // x8
    {2, 1, 4, 1, 1},  // x6
    {2, 3, 4, 6, 3},  // x4
    {2, 1, 4, 1, 1},  // x3
    {2, 3, 4, 6, 3},  // x2
    {2, 3, 4, 6, 3},  // /1
    {2, 3, 4, 6, 3}, {2, 3, 4, 6, 3}, {2, 3, 4, 6, 3}, {2, 3, 4, 6, 3},
    {2, 3, 4, 6, 3}, {2, 3, 4, 6, 3}, {2, 3, 4, 6, 3}, {2, 3, 4, 6, 3},
    {2, 3, 4, 6, 3}, {2, 3, 4, 6, 3}, {2, 3, 4, 6, 3}, {2, 3, 4, 6, 3},
    {2, 3, 4, 6, 3}, {2, 3, 4, 6, 3}, {2, 3, 4, 6, 3}, {2, 3, 4, 6, 3},
};

struct Rig {
    PatternBank bank;
    SequencerEngine engine;
    Transport transport;
    UiController ui;
    TriggerSequencer seq;

    Rig() : engine(), transport(engine), ui(engine, bank, transport),
            seq(bank, engine) {
        engine.setPatternBank(&bank);
        engine.setChannelMode(0, flexseq::MODE_SEQ);
    }

    Pattern* pattern() { return bank.getPattern(0); }

    void useSubdiv(int16_t subdiv) {
        engine.setSubdiv(0, subdiv);
        engine.refreshTiming();
    }
};

// Ecarts en ticks entre declenchements SORTIS, ceux que la sortie recoit.
// TriggerSequencer filtre sur l'activite du pas, le moteur non : c'est le
// premier qui pilote les broches dans main.cpp.
uint8_t collectGaps(Rig& r, uint16_t ticks, uint16_t* gaps, uint8_t maxGaps) {
    r.engine.start();
    uint16_t last = 0;
    uint8_t seen = 0;
    uint8_t count = 0;
    for (uint16_t t = 1; t <= ticks; ++t) {
        r.engine.advance(1);
        r.seq.update();
        if (r.seq.triggerCount(0) == 0) {
            continue;
        }
        if (seen > 0 && count < maxGaps) {
            gaps[count++] = static_cast<uint16_t>(t - last);
        }
        last = t;
        ++seen;
    }
    return count;
}

}  // namespace

/*
 * La cadence : les 25 valeurs
 */

void test_every_subdiv_gives_its_documented_step_duration() {
    for (uint8_t i = 0; i < SUBDIV_COUNT; ++i) {
        Rig r;
        r.useSubdiv(SUBDIVS[i]);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(STEP_TICKS[i], r.engine.getTicksPerStep(0),
                                         "duree de step inattendue pour cette cadence");
    }
}

void test_the_triplet_spans_two_steps_at_every_subdiv() {
    for (uint8_t i = 0; i < SUBDIV_COUNT; ++i) {
        Rig r;
        r.pattern()->writeStep(0, true);
        r.pattern()->setRatchet(0, flexseq::RATCHET_TRIPLET);
        r.useSubdiv(SUBDIVS[i]);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(
            static_cast<uint16_t>(STEP_TICKS[i] * 2), r.engine.currentStepTicks(0),
            "un triolet doit durer deux steps, a toutes les cadences");
    }
}

/*
 * La matrice : 25 cadences x 5 codes
 */

void test_the_trigger_count_matrix_is_the_audited_one() {
    for (uint8_t i = 0; i < SUBDIV_COUNT; ++i) {
        for (uint8_t c = 0; c < CODE_COUNT; ++c) {
            Rig r;
            r.pattern()->writeStep(0, true);
            r.pattern()->setRatchet(0, CODES[c]);
            r.useSubdiv(SUBDIVS[i]);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(TRIGGERS[i][c],
                                            r.engine.currentStepTriggers(0),
                                            "la matrice ratchet x cadence a change");
        }
    }
}

void test_thirteen_pairs_of_the_matrix_fold_to_a_single_trigger() {
    uint8_t folded = 0;
    for (uint8_t i = 0; i < SUBDIV_COUNT; ++i) {
        for (uint8_t c = 0; c < CODE_COUNT; ++c) {
            if (TRIGGERS[i][c] == 1) {
                ++folded;
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(13, folded,
        "le nombre de couples replies a change : le lot 21 doit le faire, pas un accident");
}

/*
 * Le train de declenchements, en ticks
 */

void test_the_train_is_regular_without_a_ratchet() {
    Rig r;
    for (uint8_t i = 0; i < 4; ++i) {
        r.pattern()->writeStep(i, true);
    }
    r.engine.setEffectiveLength(0, 4);
    r.useSubdiv(1);

    uint16_t gaps[8];
    const uint8_t n = collectGaps(r, 96 * 6, gaps, 8);
    TEST_ASSERT_EQUAL_UINT8(5, n);
    for (uint8_t i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL_UINT16(96, gaps[i]);
    }
}

void test_a_ratchet_3_at_unity_splits_its_step_in_three() {
    Rig r;
    for (uint8_t i = 0; i < 4; ++i) {
        r.pattern()->writeStep(i, true);
    }
    r.pattern()->setRatchet(0, flexseq::RATCHET_3);
    r.engine.setEffectiveLength(0, 4);
    r.useSubdiv(1);

    const uint16_t expected[9] = {32, 32, 96, 96, 96, 32, 32, 32, 96};
    uint16_t gaps[9];
    const uint8_t n = collectGaps(r, 96 * 6, gaps, 9);
    TEST_ASSERT_EQUAL_UINT8(9, n);
    for (uint8_t i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(expected[i], gaps[i],
            "le train d'un ratchet 3 a l'unite a change");
    }
}

void test_a_triplet_at_unity_fires_three_times_over_two_steps() {
    Rig r;
    for (uint8_t i = 0; i < 4; ++i) {
        r.pattern()->writeStep(i, true);
    }
    r.pattern()->setRatchet(0, flexseq::RATCHET_TRIPLET);
    r.engine.setEffectiveLength(0, 4);
    r.useSubdiv(1);

    const uint16_t expected[6] = {64, 64, 96, 96, 96, 64};
    uint16_t gaps[6];
    const uint8_t n = collectGaps(r, 96 * 6, gaps, 6);
    TEST_ASSERT_EQUAL_UINT8(6, n);
    for (uint8_t i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(expected[i], gaps[i],
            "le train d'un triolet a l'unite a change");
    }
}

// CARACTERISATION du repli actuel : a x3 un step vaut 32 ticks, non divisible
// par 3, donc le ratchet est desactive et le train redevient regulier. Le lot 21
// remplace ce comportement.
void test_a_folded_ratchet_leaves_a_regular_train() {
    Rig r;
    for (uint8_t i = 0; i < 4; ++i) {
        r.pattern()->writeStep(i, true);
    }
    r.pattern()->setRatchet(0, flexseq::RATCHET_3);
    r.engine.setEffectiveLength(0, 4);
    r.useSubdiv(-3);

    uint16_t gaps[6];
    const uint8_t n = collectGaps(r, 32 * 6, gaps, 6);
    TEST_ASSERT_EQUAL_UINT8(5, n);
    for (uint8_t i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(32, gaps[i],
            "un ratchet replie doit rendre un train regulier");
    }
}

// CARACTERISATION : le triolet garde ses deux unites meme replie, donc le step
// dure 64 ticks a x3 et n'emet qu'une note.
void test_a_folded_triplet_keeps_its_two_units() {
    Rig r;
    r.pattern()->writeStep(0, true);
    r.pattern()->setRatchet(0, flexseq::RATCHET_TRIPLET);
    r.useSubdiv(-3);
    TEST_ASSERT_EQUAL_UINT16(64, r.engine.currentStepTicks(0));
    TEST_ASSERT_EQUAL_UINT8(1, r.engine.currentStepTriggers(0));
}

/*
 * Les quatre regles fixees par le proprietaire le 2026-08-23
 */

void test_an_inactive_step_emits_nothing_whatever_its_ratchet() {
    for (uint8_t c = 0; c < CODE_COUNT; ++c) {
        Rig r;
        r.pattern()->setRatchet(0, CODES[c]);
        r.engine.setEffectiveLength(0, 1);
        r.useSubdiv(1);
        r.engine.start();

        uint16_t fired = 0;
        for (uint16_t t = 0; t < 96 * 4; ++t) {
            r.engine.advance(1);
            r.seq.update();
            fired = static_cast<uint16_t>(fired + r.seq.triggerCount(0));
        }
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, fired,
            "un pas inactif ne doit rien emettre, quel que soit son ratchet");
    }
}

// Le triolet d'un pas inactif est un SILENCE EN TRIOLET : il n'emet rien et
// garde ses deux unites, donc il repousse la suite du pattern. Mesure : le pas
// muet ajoute 192 ticks a l'ecart qui l'enjambe.
void test_an_inactive_triplet_is_a_triplet_rest() {
    Rig r;
    r.pattern()->writeStep(1, true);
    r.pattern()->writeStep(2, true);
    r.pattern()->writeStep(3, true);
    r.pattern()->setRatchet(0, flexseq::RATCHET_TRIPLET);
    r.engine.setEffectiveLength(0, 4);
    r.useSubdiv(1);

    const uint16_t expected[6] = {96, 96, 288, 96, 96, 288};
    uint16_t gaps[6];
    const uint8_t n = collectGaps(r, 96 * 12, gaps, 6);
    TEST_ASSERT_EQUAL_UINT8(6, n);
    for (uint8_t i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(expected[i], gaps[i],
            "le silence en triolet doit valoir deux unites");
    }
}

void test_a_ratchet_survives_the_step_being_switched_off() {
    Rig r;
    r.ui.handle(UiController::EVENT_ROTATE, 1);   // barre -> onglet du channel 1
    r.pattern()->writeStep(5, true);
    r.pattern()->setRatchet(5, flexseq::RATCHET_6);

    r.pattern()->writeStep(5, false);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(flexseq::RATCHET_6, r.pattern()->getRatchet(5),
        "le ratchet doit survivre a la desactivation du pas");

    r.pattern()->writeStep(5, true);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(flexseq::RATCHET_6, r.pattern()->getRatchet(5),
        "le ratchet doit revenir avec le pas");
}

void test_clearing_the_pattern_wipes_steps_and_ratchets_together() {
    Rig r;
    r.pattern()->writeStep(0, true);
    r.pattern()->writeStep(7, true);
    r.pattern()->setRatchet(0, flexseq::RATCHET_4);
    r.pattern()->setRatchet(7, flexseq::RATCHET_TRIPLET);

    r.pattern()->clear();

    bool active = true;
    r.pattern()->readStep(0, active);
    TEST_ASSERT_FALSE(active);
    r.pattern()->readStep(7, active);
    TEST_ASSERT_FALSE(active);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(flexseq::RATCHET_NONE, r.pattern()->getRatchet(0),
        "l'effacement doit emporter les ratchets");
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, r.pattern()->getRatchet(7));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_every_subdiv_gives_its_documented_step_duration);
    RUN_TEST(test_the_triplet_spans_two_steps_at_every_subdiv);

    RUN_TEST(test_the_trigger_count_matrix_is_the_audited_one);
    RUN_TEST(test_thirteen_pairs_of_the_matrix_fold_to_a_single_trigger);

    RUN_TEST(test_the_train_is_regular_without_a_ratchet);
    RUN_TEST(test_a_ratchet_3_at_unity_splits_its_step_in_three);
    RUN_TEST(test_a_triplet_at_unity_fires_three_times_over_two_steps);
    RUN_TEST(test_a_folded_ratchet_leaves_a_regular_train);
    RUN_TEST(test_a_folded_triplet_keeps_its_two_units);

    RUN_TEST(test_an_inactive_step_emits_nothing_whatever_its_ratchet);
    RUN_TEST(test_an_inactive_triplet_is_a_triplet_rest);
    RUN_TEST(test_a_ratchet_survives_the_step_being_switched_off);
    RUN_TEST(test_clearing_the_pattern_wipes_steps_and_ratchets_together);

    return UNITY_END();
}
