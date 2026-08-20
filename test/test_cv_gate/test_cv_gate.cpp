#include <stdint.h>
#include <unity.h>

#include <flexseq/CvGate.h>

using flexseq::CvGate;
using flexseq::calibratedFromRaw;
using flexseq::rawFromCalibrated;

// Seuils par defaut de libGravity : +1 V -> 634, +0,5 V -> 585 (PRD 10.5).
static const uint16_t ARM = 634;
static const uint16_t REARM = 586;
static const uint16_t IDLE = 538;   // le zero mesure de l'entree bipolaire

static CvGate gate;

void setUp() {
    gate = CvGate();
    gate.configure(ARM, REARM);
}
void tearDown() {}

void test_a_new_gate_is_low_and_has_no_event(void) {
    TEST_ASSERT_FALSE(gate.high());
    TEST_ASSERT_FALSE(gate.pending());
    TEST_ASSERT_FALSE(gate.takeEdge());
}

// Le faux front au demarrage : l'etat precedent est initialise a BAS, donc un
// premier echantillon DEJA haut compte comme un front — c'est correct, l'entree
// vient de passer haute du point de vue du firmware. Ce qui est ecarte, c'est le
// front fantome de libGravity sur un `old_read_` non initialise.
void test_first_sample_below_the_threshold_is_not_an_edge(void) {
    TEST_ASSERT_FALSE(gate.update(IDLE));
    TEST_ASSERT_FALSE(gate.pending());
}

void test_crossing_the_arm_threshold_is_an_edge(void) {
    TEST_ASSERT_FALSE(gate.update(IDLE));
    TEST_ASSERT_TRUE(gate.update(ARM + 1));
    TEST_ASSERT_TRUE(gate.high());
    TEST_ASSERT_TRUE(gate.pending());
}

void test_the_threshold_itself_is_not_a_crossing(void) {
    gate.update(IDLE);
    TEST_ASSERT_FALSE(gate.update(ARM));  // il faut DEPASSER
}

// Front, jamais niveau : une gate maintenue haute ne produit qu'un evenement.
void test_a_held_gate_produces_one_event_only(void) {
    gate.update(IDLE);
    TEST_ASSERT_TRUE(gate.update(1000));
    for (uint8_t i = 0; i < 50; ++i) {
        TEST_ASSERT_FALSE(gate.update(1000));
    }
}

// L'hysteresis : redescendre sous le seuil HAUT ne rearme pas. C'est ce qui
// dispense d'anti-rebond.
void test_falling_between_the_thresholds_does_not_rearm(void) {
    gate.update(IDLE);
    gate.update(ARM + 1);
    gate.takeEdge();

    gate.update(REARM + 10);            // entre les deux seuils
    TEST_ASSERT_TRUE(gate.high());
    TEST_ASSERT_FALSE(gate.update(ARM + 1));  // donc pas de nouveau front
}

void test_falling_below_the_rearm_threshold_rearms(void) {
    gate.update(IDLE);
    gate.update(ARM + 1);
    gate.takeEdge();

    gate.update(REARM - 1);
    TEST_ASSERT_FALSE(gate.high());
    TEST_ASSERT_TRUE(gate.update(ARM + 1));   // un second front
}

// Le verrou survit a un retour au repos : c'est ce qui fait qu'une impulsion
// courte n'est pas perdue si la boucle principale met du temps a la consommer.
void test_the_latch_survives_the_pulse_going_away(void) {
    gate.update(IDLE);
    gate.update(ARM + 1);
    gate.update(IDLE);                  // l'impulsion est deja repartie
    TEST_ASSERT_TRUE(gate.pending());
    TEST_ASSERT_TRUE(gate.takeEdge());
    TEST_ASSERT_FALSE(gate.takeEdge()); // consomme une seule fois
}

void test_two_pulses_before_consumption_latch_once(void) {
    gate.update(IDLE);
    gate.update(ARM + 1);
    gate.update(IDLE);
    gate.update(ARM + 1);
    TEST_ASSERT_TRUE(gate.takeEdge());
    TEST_ASSERT_FALSE(gate.takeEdge());
}

// L'aller-retour, plutot que des nombres calcules a la main. `map()` tronquant
// des deux cotes, l'inverse est ambigu d'un pas — d'ou la tolerance, qui EST la
// propriete : un pas vaut ~10 mV de CV.
void test_raw_thresholds_round_trip_to_their_calibrated_value(void) {
    const int16_t targets[] = {102, 51, 0, -102, 400};
    for (uint8_t i = 0; i < 5; ++i) {
        const uint16_t raw = rawFromCalibrated(targets[i], -566, 512, 0);
        const int16_t back = calibratedFromRaw(raw, -566, 512, 0);
        const int16_t err = static_cast<int16_t>(back - targets[i]);
        TEST_ASSERT_TRUE_MESSAGE(err >= -1 && err <= 1, "l'aller-retour derive de plus d'un pas");
    }
}

// Les deux seuils que le firmware utilisera vraiment, et leur sens exact : au
// point d'armement la tension vaut +1 V, donc « au-dessus de +1 V » est bien
// `raw > arm`. Au point de rearmement elle vaut +0,5 V, donc « en dessous de
// +0,5 V » est bien `raw < rearm` (PRD 10.5).
void test_the_two_schmitt_thresholds_are_exact(void) {
    const uint16_t arm = rawFromCalibrated(102, -566, 512, 0);
    const uint16_t rearm = rawFromCalibrated(51, -566, 512, 0);

    TEST_ASSERT_EQUAL_UINT16(634, arm);
    TEST_ASSERT_EQUAL_UINT16(586, rearm);
    TEST_ASSERT_EQUAL_INT16(102, calibratedFromRaw(arm, -566, 512, 0));
    TEST_ASSERT_EQUAL_INT16(51, calibratedFromRaw(rearm, -566, 512, 0));
    TEST_ASSERT_TRUE(arm > rearm);
}

void test_calibration_shifts_the_raw_thresholds(void) {
    const uint16_t centred = rawFromCalibrated(102, -566, 512, 0);
    const uint16_t shifted = rawFromCalibrated(102, -566, 512, 50);
    TEST_ASSERT_TRUE(shifted > centred);
}

void test_raw_conversion_stays_in_range(void) {
    TEST_ASSERT_EQUAL_UINT16(0, rawFromCalibrated(-2000, -566, 512, 0));
    TEST_ASSERT_EQUAL_UINT16(1023, rawFromCalibrated(2000, -566, 512, 0));
    TEST_ASSERT_EQUAL_UINT16(0, rawFromCalibrated(102, 512, 512, 0)); // plage nulle
}

// --- entree INVERSEE : le sens vient de l'ordre des seuils -------------------
//
// libGravity nie `read_` si `SetAttenuation()` recoit un pourcentage negatif, et
// n'expose aucun accesseur pour le savoir. Plutot que de porter cette hypothese
// en commentaire, la porte prend les deux sens : il suffit de lui donner un
// `arm` INFERIEUR au `rearm`.

static const uint16_t INV_ARM = 390;    // la tension monte quand le brut descend
static const uint16_t INV_REARM = 438;

void test_inverted_input_arms_below_its_threshold(void) {
    gate = CvGate();
    gate.configure(INV_ARM, INV_REARM);

    TEST_ASSERT_FALSE(gate.update(IDLE));          // repos, au-dessus des deux
    TEST_ASSERT_TRUE(gate.update(INV_ARM - 1));    // franchit vers le BAS
    TEST_ASSERT_TRUE(gate.high());
}

void test_inverted_input_keeps_its_hysteresis(void) {
    gate = CvGate();
    gate.configure(INV_ARM, INV_REARM);
    gate.update(IDLE);
    gate.update(INV_ARM - 1);
    gate.takeEdge();

    gate.update(INV_REARM - 10);                   // entre les deux seuils
    TEST_ASSERT_TRUE(gate.high());
    TEST_ASSERT_FALSE(gate.update(INV_ARM - 1));   // donc pas de nouveau front

    gate.update(INV_REARM + 1);                    // repasse au-dela
    TEST_ASSERT_FALSE(gate.high());
    TEST_ASSERT_TRUE(gate.update(INV_ARM - 1));    // un second front
}

void test_inverted_input_ignores_a_rise(void) {
    gate = CvGate();
    gate.configure(INV_ARM, INV_REARM);
    gate.update(IDLE);
    TEST_ASSERT_FALSE(gate.update(1000));          // vers le haut : rien
    TEST_ASSERT_FALSE(gate.pending());
}

// Deux seuils egaux suppriment l'hysteresis : la porte reste inerte plutot que
// de claquer sur le bruit.
void test_equal_thresholds_leave_the_gate_inert(void) {
    gate = CvGate();
    gate.configure(600, 600);
    for (uint16_t v = 0; v < 1024; v = static_cast<uint16_t>(v + 37)) {
        TEST_ASSERT_FALSE(gate.update(v));
    }
    TEST_ASSERT_FALSE(gate.pending());
}

// Une porte NON configuree ne produit rien : `usable_` est faux tant que
// configure() n'a pas ete appele.
void test_an_unconfigured_gate_never_fires(void) {
    gate = CvGate();
    for (uint16_t v = 0; v < 1024; v = static_cast<uint16_t>(v + 37)) {
        TEST_ASSERT_FALSE(gate.update(v));
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_new_gate_is_low_and_has_no_event);
    RUN_TEST(test_first_sample_below_the_threshold_is_not_an_edge);
    RUN_TEST(test_crossing_the_arm_threshold_is_an_edge);
    RUN_TEST(test_the_threshold_itself_is_not_a_crossing);
    RUN_TEST(test_a_held_gate_produces_one_event_only);
    RUN_TEST(test_falling_between_the_thresholds_does_not_rearm);
    RUN_TEST(test_falling_below_the_rearm_threshold_rearms);
    RUN_TEST(test_the_latch_survives_the_pulse_going_away);
    RUN_TEST(test_two_pulses_before_consumption_latch_once);
    RUN_TEST(test_raw_thresholds_round_trip_to_their_calibrated_value);
    RUN_TEST(test_the_two_schmitt_thresholds_are_exact);
    RUN_TEST(test_calibration_shifts_the_raw_thresholds);
    RUN_TEST(test_raw_conversion_stays_in_range);

    RUN_TEST(test_inverted_input_arms_below_its_threshold);
    RUN_TEST(test_inverted_input_keeps_its_hysteresis);
    RUN_TEST(test_inverted_input_ignores_a_rise);
    RUN_TEST(test_equal_thresholds_leave_the_gate_inert);
    RUN_TEST(test_an_unconfigured_gate_never_fires);
    return UNITY_END();
}
