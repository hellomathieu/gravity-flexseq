#include <stdint.h>
#include <unity.h>

#include <uClock/uClock.h>
#include "NeoHWSerial.h"

// -----------------------------------------------------------------------------
// NeoHWSerial mock
// -----------------------------------------------------------------------------

MockNeoHWSerial NeoSerial;

// -----------------------------------------------------------------------------
// uClock mock
// -----------------------------------------------------------------------------

namespace umodular {
namespace clock {

uClockClass::uClockClass()
    : clock_state(PAUSED) {
}

void uClockClass::init() {
    clock_state = PAUSED;
}

void uClockClass::setOutputPPQN(PPQNResolution resolution) {
    (void)resolution;
}

void uClockClass::setInputPPQN(PPQNResolution resolution) {
    (void)resolution;
}

void uClockClass::setClockMode(ClockMode mode) {
    clock_mode = mode;
}

uClockClass::ClockMode uClockClass::getClockMode() {
    return clock_mode;
}

void uClockClass::setTempo(float tempo) {
    (void)tempo;
}

void uClockClass::start() {
    clock_state = STARTING;
}

void uClockClass::stop() {
    clock_state = PAUSED;
}

void uClockClass::clockMe() {
    if (clock_state == STARTING) {
        clock_state = STARTED;
    }
}

} // namespace clock
} // namespace umodular

// -----------------------------------------------------------------------------
// Global uClock instance
// -----------------------------------------------------------------------------

umodular::clock::uClockClass uClock;

// -----------------------------------------------------------------------------
// Include REAL libGravity implementation
// -----------------------------------------------------------------------------

#include "../../.pio/libdeps/nanoatmega328/libGravity/src/libGravity.cpp"

// -----------------------------------------------------------------------------
// Test lifecycle
// -----------------------------------------------------------------------------

void setUp() {
    ArduinoMock::reset();
}

void tearDown() {
}

// -----------------------------------------------------------------------------
// Gravity::Init()
// -----------------------------------------------------------------------------

void test_gravity_init_configures_encoder_interrupt_registers() {
    Gravity gravityUnderTest;

    gravityUnderTest.Init();

    TEST_ASSERT_EQUAL_UINT8(
        0x06,
        ArduinoMock::pcicr()
    );

    TEST_ASSERT_EQUAL_UINT8(
        0x10,
        ArduinoMock::pcmsk2()
    );

    TEST_ASSERT_EQUAL_UINT8(
        0x08,
        ArduinoMock::pcmsk1()
    );
}

// -----------------------------------------------------------------------------
// G1 investigation
// -----------------------------------------------------------------------------
//
// IMPORTANT:
//
// We deliberately DO NOT call Gravity::Process() here.
//
// Gravity::Process() currently crashes in Encoder::Process() because of the
// independently confirmed Encoder initialization defect.
//
// This test isolates only the output-processing loop found in libGravity:
//
//     for (int i; i < OUTPUT_COUNT; i++) {
//         outputs[i].Process();
//     }
//
// The purpose is to investigate the uninitialized loop variable `i`.
//
// This is an INVESTIGATION TEST, not a functional test.
// -----------------------------------------------------------------------------

void test_gravity_uninitialized_output_loop_is_investigated() {
    Gravity gravityUnderTest;

    gravityUnderTest.Init();

    // Start a trigger on every output.
    for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; ++i) {
        gravityUnderTest.outputs[i].Trigger();

        TEST_ASSERT_TRUE(
            gravityUnderTest.outputs[i].On()
        );
    }

    ArduinoMock::advanceMillis(
        DEFAULT_TRIGGER_DURATION_MS
    );

    // Deliberately reproduce the production loop exactly.
    //
    // DO NOT "fix" i here. The purpose of this test is to observe the
    // consequences of the production implementation.
    for (int i; i < Gravity::OUTPUT_COUNT; ++i) {
        gravityUnderTest.outputs[i].Process();
    }

    // If execution reaches this point, inspect the output states.
    //
    // We do not make this a functional assertion yet because the loop itself
    // contains undefined behaviour. The test is currently intended to
    // characterize the runtime behaviour.
    TEST_PASS();
}

// -----------------------------------------------------------------------------
// Control experiment
// -----------------------------------------------------------------------------
//
// Same operation, but with a correctly initialized loop variable.
//
// This is NOT a test of the production implementation.
// It is a control experiment proving that DigitalOutput processing itself
// behaves correctly when addressed with valid indices.
// -----------------------------------------------------------------------------

void test_gravity_initialized_output_loop_control() {
    Gravity gravityUnderTest;

    gravityUnderTest.Init();

    for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; ++i) {
        gravityUnderTest.outputs[i].Trigger();

        TEST_ASSERT_TRUE(
            gravityUnderTest.outputs[i].On()
        );
    }

    ArduinoMock::advanceMillis(
        DEFAULT_TRIGGER_DURATION_MS
    );

    for (int i = 0; i < Gravity::OUTPUT_COUNT; ++i) {
        gravityUnderTest.outputs[i].Process();
    }

    for (uint8_t i = 0; i < Gravity::OUTPUT_COUNT; ++i) {
        TEST_ASSERT_FALSE(
            gravityUnderTest.outputs[i].On()
        );
    }
}

// -----------------------------------------------------------------------------
// Unity
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Composition de Gravity::Process() — CARACTERISATION du commit epingle
//
// Pourquoi ce test existe. FlexSeq n'appelle PLUS `Gravity::Process()` : il
// echantillonne le convertisseur sous interruption, et cette fonction ferait un
// `analogRead` bloquant qui entrerait en collision (PRD 10.6). Il appelle donc
// ses morceaux — boutons et encodeur — et pilote les sorties lui-meme.
//
// Le risque que cela cree n'est pas de rater une evolution de libGravity, qui est
// FIGEE a un commit du fork par decision de projet (ADR 0008) : c'est d'oublier, au
// prochain changement d'epingle, de re-auditer ce que cette fonction fait. Ce
// test transforme cet oubli possible en ECHEC : il fige la composition observee
// au commit epingle, par ce que la fonction va chercher sur le materiel.
//
// Il n'assertionne donc PAS un comportement souhaitable — il decrit le reel, au
// meme titre que les autres tests de cet environnement.
// -----------------------------------------------------------------------------

void test_gravity_process_reads_exactly_the_two_cv_inputs() {
    ArduinoMock::reset();
    gravity.Init();
    ArduinoMock::reset();   // Init() lit aussi : on part d'une ardoise propre

    gravity.Process();

    // Les deux entrees CV, une fois chacune. C'est precisement ce que FlexSeq ne
    // veut plus voir arriver depuis la boucle principale.
    TEST_ASSERT_EQUAL_UINT16(1, ArduinoMock::analogReads(CV1_PIN));
    TEST_ASSERT_EQUAL_UINT16(1, ArduinoMock::analogReads(CV2_PIN));
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(2, ArduinoMock::totalAnalogReads(),
        "Gravity::Process() lit deux entrees analogiques, pas plus : si ce nombre "
        "change, la composition de la fonction a change avec l'epingle");
}

void test_gravity_process_polls_both_buttons_and_the_encoder_switch() {
    ArduinoMock::reset();
    gravity.Init();
    ArduinoMock::reset();

    gravity.Process();

    // Boutons SCRUTES, non interrompus : aucun n'est sous PCINT (seules les deux
    // broches de l'encodeur le sont, cf. test des registres ci-dessus).
    TEST_ASSERT_GREATER_THAN_UINT16(0, ArduinoMock::digitalReads(SHIFT_BTN_PIN));
    TEST_ASSERT_GREATER_THAN_UINT16(0, ArduinoMock::digitalReads(PLAY_BTN_PIN));
    // L'encodeur consulte son propre bouton a chaque passage.
    TEST_ASSERT_GREATER_THAN_UINT16(0, ArduinoMock::digitalReads(ENCODER_SW_PIN));
}

// Ce que FlexSeq appelle a la place couvre les memes entrees, et AUCUNE lecture
// analogique : c'est l'invariant qui rend le contournement correct.
void test_the_pieces_flexseq_calls_cover_the_inputs_without_touching_the_adc() {
    ArduinoMock::reset();
    gravity.Init();
    ArduinoMock::reset();

    gravity.shift_button.Process();
    gravity.play_button.Process();
    gravity.encoder.Process();

    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, ArduinoMock::totalAnalogReads(),
        "aucune conversion depuis la boucle : le convertisseur appartient a l'ISR");
    TEST_ASSERT_GREATER_THAN_UINT16(0, ArduinoMock::digitalReads(SHIFT_BTN_PIN));
    TEST_ASSERT_GREATER_THAN_UINT16(0, ArduinoMock::digitalReads(PLAY_BTN_PIN));
    TEST_ASSERT_GREATER_THAN_UINT16(0, ArduinoMock::digitalReads(ENCODER_SW_PIN));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(
        test_gravity_init_configures_encoder_interrupt_registers
    );

    RUN_TEST(
        test_gravity_uninitialized_output_loop_is_investigated
    );

    RUN_TEST(
        test_gravity_initialized_output_loop_control
    );

    RUN_TEST(
        test_gravity_process_reads_exactly_the_two_cv_inputs
    );

    RUN_TEST(
        test_gravity_process_polls_both_buttons_and_the_encoder_switch
    );

    RUN_TEST(
        test_the_pieces_flexseq_calls_cover_the_inputs_without_touching_the_adc
    );

    return UNITY_END();
}