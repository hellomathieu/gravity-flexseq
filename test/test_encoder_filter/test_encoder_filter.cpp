#include <stdint.h>
#include <unity.h>

#include <flexseq/EncoderFilter.h>

using flexseq::EncoderFilter;

void setUp() {}
void tearDown() {}

namespace {

// Consomme le faux premier mouvement de libGravity, pour partir d'un filtre
// deja arme dans les tests qui n'etudient pas ce cas.
void armed(EncoderFilter& f, uint32_t atMs = 0) {
    f.filter(1, atMs);
}

}  // namespace

/*
 * Le faux premier mouvement (previous_pos_ non initialise, anomalie auditee)
 */

void test_the_first_reported_movement_is_swallowed() {
    EncoderFilter f;
    TEST_ASSERT_FALSE(f.sawFirstMovement());
    TEST_ASSERT_EQUAL_INT8(0, f.filter(1, 0));
    TEST_ASSERT_TRUE(f.sawFirstMovement());
}

void test_the_movement_after_the_first_goes_through() {
    EncoderFilter f;
    f.filter(1, 0);
    TEST_ASSERT_EQUAL_INT8(1, f.filter(1, 1000));
}

void test_a_first_movement_of_either_sign_is_swallowed() {
    EncoderFilter down;
    TEST_ASSERT_EQUAL_INT8(0, down.filter(-3, 0));
    TEST_ASSERT_EQUAL_INT8(-3, down.filter(-3, 1000));
}

void test_reset_re_arms_the_first_movement_guard() {
    EncoderFilter f;
    armed(f);
    TEST_ASSERT_EQUAL_INT8(1, f.filter(1, 100));
    f.reset();
    TEST_ASSERT_FALSE(f.sawFirstMovement());
    TEST_ASSERT_EQUAL_INT8(0, f.filter(1, 200));
}

/*
 * Le rebond de cran : c'est l'inversion RAPIDE qui est un rebond
 */

void test_a_reversal_inside_the_window_is_a_bounce_and_is_swallowed() {
    EncoderFilter f;
    armed(f, 0);
    TEST_ASSERT_EQUAL_INT8(1, f.filter(1, 100));
    TEST_ASSERT_EQUAL_INT8_MESSAGE(0, f.filter(-1, 105), "un rebond a 5 ms doit etre absorbe");
}

void test_a_reversal_outside_the_window_is_a_real_change_of_direction() {
    EncoderFilter f;
    armed(f, 0);
    f.filter(1, 100);
    TEST_ASSERT_EQUAL_INT8(-1, f.filter(-1, 100 + EncoderFilter::DEFAULT_REVERSAL_WINDOW_MS));
}

void test_the_window_boundary_is_exclusive() {
    const uint16_t w = EncoderFilter::DEFAULT_REVERSAL_WINDOW_MS;
    EncoderFilter inside;
    armed(inside, 0);
    inside.filter(1, 100);
    TEST_ASSERT_EQUAL_INT8(0, inside.filter(-1, 100 + w - 1));

    EncoderFilter atBoundary;
    armed(atBoundary, 0);
    atBoundary.filter(1, 100);
    TEST_ASSERT_EQUAL_INT8(-1, atBoundary.filter(-1, 100 + w));
}

// LE cas observe sur le module : deux crans rapides ne produisaient AUCUN
// evenement, la position revenant a son point de depart. Le filtre doit en
// rendre deux.
void test_two_fast_detents_yield_two_events_not_none() {
    EncoderFilter f;
    armed(f, 0);
    int16_t total = 0;
    // Sequence rapportee par libGravity pour deux crans qui rebondissent.
    const int8_t reported[] = {1, -1, 1, -1};
    const uint32_t at[] = {100, 103, 106, 109};
    for (uint8_t i = 0; i < 4; ++i) {
        total = static_cast<int16_t>(total + f.filter(reported[i], at[i]));
    }
    TEST_ASSERT_EQUAL_INT16_MESSAGE(2, total, "deux crans doivent rendre deux evenements");
    TEST_ASSERT_EQUAL_UINT16(2, f.suppressed() - 1); // -1 : le faux premier mouvement
}

void test_a_same_direction_burst_is_never_swallowed() {
    EncoderFilter f;
    armed(f, 0);
    int16_t total = 0;
    for (uint8_t i = 0; i < 10; ++i) {
        total = static_cast<int16_t>(total + f.filter(1, 100 + i));
    }
    TEST_ASSERT_EQUAL_INT16(10, total);
}

void test_a_bounce_does_not_delay_the_next_real_detent() {
    EncoderFilter f;
    armed(f, 0);
    f.filter(1, 100);
    f.filter(-1, 102);                       // rebond absorbe
    // Le cran suivant est mesure depuis le DERNIER cran accepte, pas depuis le
    // rebond : sinon un rebond repousserait indefiniment la fenetre.
    TEST_ASSERT_EQUAL_INT8(-1, f.filter(-1, 100 + EncoderFilter::DEFAULT_REVERSAL_WINDOW_MS));
}

/*
 * L'acceleration de libGravity traverse le filtre
 */

void test_the_acceleration_of_the_dependency_is_preserved() {
    EncoderFilter f;
    armed(f, 0);
    TEST_ASSERT_EQUAL_INT8(3, f.filter(3, 100));
    TEST_ASSERT_EQUAL_INT8(-2, f.filter(-2, 1000));
}

void test_a_reversal_is_judged_on_the_sign_not_the_magnitude() {
    EncoderFilter f;
    armed(f, 0);
    f.filter(3, 100);
    TEST_ASSERT_EQUAL_INT8(0, f.filter(-3, 102));
}

void test_an_absurd_delta_is_clamped_to_the_ui_range() {
    EncoderFilter f;
    armed(f, 0);
    TEST_ASSERT_EQUAL_INT8(EncoderFilter::MAX_DELTA, f.filter(30000, 100));
    TEST_ASSERT_EQUAL_INT8(-EncoderFilter::MAX_DELTA, f.filter(-30000, 1000));
}

void test_a_zero_delta_changes_nothing() {
    EncoderFilter f;
    armed(f, 0);
    const uint16_t before = f.suppressed();
    TEST_ASSERT_EQUAL_INT8(0, f.filter(0, 100));
    TEST_ASSERT_EQUAL_UINT16(before, f.suppressed());
    TEST_ASSERT_EQUAL_INT8(1, f.filter(1, 101));
}

// Un delta nul ne doit pas non plus DEPLACER la fenetre : sinon il masquerait le
// rebond qui le suit. Ce n'est pas visible sur la valeur rendue, seulement sur
// l'etat, d'ou ce test-ci en plus du precedent.
void test_a_zero_delta_does_not_move_the_window() {
    const uint16_t w = EncoderFilter::DEFAULT_REVERSAL_WINDOW_MS;
    EncoderFilter f;
    armed(f, 0);
    TEST_ASSERT_EQUAL_INT8(1, f.filter(1, 100));
    f.filter(0, 100 + w - 2);
    TEST_ASSERT_EQUAL_INT8_MESSAGE(-1, f.filter(-1, 100 + w),
        "la fenetre doit se compter depuis le dernier cran REEL");
}

/*
 * Le seuil est REGLABLE, parce qu'il doit etre mesure sur le module
 */

void test_the_window_is_adjustable_because_it_has_not_been_measured() {
    EncoderFilter f(40);
    TEST_ASSERT_EQUAL_UINT16(40, f.reversalWindowMs());
    armed(f, 0);
    f.filter(1, 100);
    TEST_ASSERT_EQUAL_INT8(0, f.filter(-1, 130));
    f.setReversalWindowMs(5);
    TEST_ASSERT_EQUAL_UINT16(5, f.reversalWindowMs());
    TEST_ASSERT_EQUAL_INT8(-1, f.filter(-1, 140));
}

void test_a_window_of_zero_lets_everything_through() {
    EncoderFilter f(0);
    armed(f, 0);
    f.filter(1, 100);
    TEST_ASSERT_EQUAL_INT8(-1, f.filter(-1, 100));
}

// Ce que le firmware de diagnostic affichera pour choisir le seuil : l'intervalle
// entre les deux derniers mouvements rapportes.
void test_the_last_interval_is_exposed_for_the_measurement() {
    EncoderFilter f;
    armed(f, 0);
    f.filter(1, 100);
    TEST_ASSERT_EQUAL_UINT16(100, f.lastIntervalMs());
    f.filter(1, 137);
    TEST_ASSERT_EQUAL_UINT16(37, f.lastIntervalMs());
    f.filter(-1, 140);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(3, f.lastIntervalMs(),
        "un rebond absorbe doit tout de meme etre mesure");
}

void test_the_millis_wrap_does_not_open_the_window() {
    EncoderFilter f;
    f.filter(1, 0xFFFFFFF0u);
    f.filter(1, 0xFFFFFFF8u);
    // Franchissement du debordement de millis() : 4 ms plus tard en temps reel.
    TEST_ASSERT_EQUAL_INT8_MESSAGE(0, f.filter(-1, 2u),
        "l'arithmetique non signee doit encaisser le debordement de millis()");
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_the_first_reported_movement_is_swallowed);
    RUN_TEST(test_the_movement_after_the_first_goes_through);
    RUN_TEST(test_a_first_movement_of_either_sign_is_swallowed);
    RUN_TEST(test_reset_re_arms_the_first_movement_guard);

    RUN_TEST(test_a_reversal_inside_the_window_is_a_bounce_and_is_swallowed);
    RUN_TEST(test_a_reversal_outside_the_window_is_a_real_change_of_direction);
    RUN_TEST(test_the_window_boundary_is_exclusive);
    RUN_TEST(test_two_fast_detents_yield_two_events_not_none);
    RUN_TEST(test_a_same_direction_burst_is_never_swallowed);
    RUN_TEST(test_a_bounce_does_not_delay_the_next_real_detent);

    RUN_TEST(test_the_acceleration_of_the_dependency_is_preserved);
    RUN_TEST(test_a_reversal_is_judged_on_the_sign_not_the_magnitude);
    RUN_TEST(test_an_absurd_delta_is_clamped_to_the_ui_range);
    RUN_TEST(test_a_zero_delta_changes_nothing);
    RUN_TEST(test_a_zero_delta_does_not_move_the_window);

    RUN_TEST(test_the_window_is_adjustable_because_it_has_not_been_measured);
    RUN_TEST(test_a_window_of_zero_lets_everything_through);
    RUN_TEST(test_the_last_interval_is_exposed_for_the_measurement);
    RUN_TEST(test_the_millis_wrap_does_not_open_the_window);

    return UNITY_END();
}
