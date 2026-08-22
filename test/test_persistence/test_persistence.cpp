#include <stdint.h>
#include <string.h>
#include <unity.h>

#include <flexseq/Persistence.h>

using flexseq::PatternBank;
using flexseq::Pattern;
using flexseq::PersistenceScheduler;
using flexseq::PersistentImage;
using flexseq::Preferences;
using flexseq::SequencerEngine;
using flexseq::Transport;
using flexseq::UiController;
namespace persist = flexseq::persist;

void setUp() {}
void tearDown() {}

namespace {

const uint8_t SENTINEL = 0x5A;

struct FakeEeprom {
    uint8_t cell[persist::EEPROM_SIZE];
    uint16_t writes;
    uint16_t lowestWrite;
    uint16_t highestWrite;

    FakeEeprom() { reset(); }

    void reset() {
        memset(cell, SENTINEL, sizeof(cell));
        writes = 0;
        lowestWrite = persist::EEPROM_SIZE;
        highestWrite = 0;
    }

    uint8_t read(uint16_t address) const { return cell[address]; }

    void write(uint16_t address, uint8_t value) {
        cell[address] = value;
        ++writes;
        if (address < lowestWrite) lowestWrite = address;
        if (address > highestWrite) highestWrite = address;
    }
};

struct Rig {
    PatternBank bank;
    SequencerEngine engine;
    Transport transport;
    UiController ui;
    Preferences prefs;
    PersistentImage image;
    PersistenceScheduler scheduler;

    Rig() : engine(), transport(engine), ui(engine, bank, transport), prefs(),
            image(bank, engine, ui, prefs) {
        engine.setPatternBank(&bank);
    }
};

// Mene une sauvegarde a son terme. Renvoie le nombre d'appels a advance().
uint16_t finishWrite(Rig& r, FakeEeprom& eeprom, uint32_t nowMs) {
    uint16_t calls = 0;
    while (r.scheduler.advance(eeprom, r.image, nowMs)) {
        ++calls;
    }
    return calls;
}

void fillDistinctState(Rig& r) {
    for (uint8_t index = 0; index < flexseq::PATTERN_COUNT; ++index) {
        Pattern* p = r.bank.getPattern(index);
        p->writeStep(static_cast<uint8_t>(index % 24), true);
        p->writeStep(static_cast<uint8_t>((index * 7 + 3) % 24), true);
        p->setRatchet(static_cast<uint8_t>(index % 24), flexseq::RATCHET_3);
        p->setRatchet(static_cast<uint8_t>((index * 5 + 1) % 24), flexseq::RATCHET_TRIPLET);
    }
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        r.engine.setSelectedPattern(ch, static_cast<uint8_t>(15 - ch));
        r.engine.setEffectiveLength(ch, static_cast<uint8_t>(24 - ch * 3));
        r.engine.setSubdiv(ch, flexseq::subdivAtIndex(static_cast<uint8_t>(ch * 3)));
        r.engine.setBarLength(ch, ch % 2 == 0 ? 3 : 6);
    }
    r.ui.setTempo(287);
    r.ui.setClockSource(4);
    r.prefs.rotateScreen = 0;
    r.prefs.reverseEncoder = 0;
    r.prefs.cvCalibration[0] = -26;
    r.prefs.cvCalibration[1] = 300;
}

bool sameState(const Rig& a, const Rig& b) {
    for (uint16_t index = 0; index < PersistentImage::SIZE; ++index) {
        if (a.image.byteAt(index) != b.image.byteAt(index)) {
            return false;
        }
    }
    return true;
}

FakeEeprom eeprom;

}  // namespace

/*
 * Le format lui-meme
 */

void test_the_layout_is_the_one_the_prd_fixed() {
    TEST_ASSERT_EQUAL_UINT16(384, persist::BASE_ADDRESS);
    TEST_ASSERT_EQUAL_UINT16(286, persist::TOTAL_SIZE);
    TEST_ASSERT_EQUAL_UINT16(1, persist::HEADER_SIZE);
    TEST_ASSERT_EQUAL_UINT16(240, persist::PATTERNS_SIZE);
    TEST_ASSERT_EQUAL_UINT16(36, persist::CHANNELS_SIZE);
    TEST_ASSERT_EQUAL_UINT16(3, persist::GLOBAL_SIZE);
    TEST_ASSERT_EQUAL_UINT16(6, persist::PREFS_SIZE);
    TEST_ASSERT_EQUAL_UINT16(1, persist::PATTERNS_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(241, persist::CHANNELS_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(277, persist::GLOBAL_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(280, persist::PREFS_OFFSET);
}

void test_the_image_ends_below_the_original_memcode() {
    TEST_ASSERT_TRUE(persist::BASE_ADDRESS + persist::TOTAL_SIZE <= 1023);
}

/*
 * Aller-retour
 */

void test_a_round_trip_restores_the_state_byte_for_byte() {
    eeprom.reset();
    Rig saved;
    fillDistinctState(saved);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, persist::QUIET_MS);

    Rig loaded;
    TEST_ASSERT_TRUE(loaded.scheduler.load(eeprom, loaded.image));
    TEST_ASSERT_TRUE_MESSAGE(sameState(saved, loaded), "l'etat relu differe de l'etat ecrit");

    TEST_ASSERT_EQUAL_UINT16(287, loaded.ui.tempo());
    TEST_ASSERT_EQUAL_UINT8(4, loaded.ui.clockSource());
    TEST_ASSERT_EQUAL_UINT8(0, loaded.prefs.rotateScreen);
    TEST_ASSERT_EQUAL_INT16(-26, loaded.prefs.cvCalibration[0]);
    TEST_ASSERT_EQUAL_INT16(300, loaded.prefs.cvCalibration[1]);
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_EQUAL_INT8(15 - ch, loaded.engine.getSelectedPattern(ch));
        TEST_ASSERT_EQUAL_UINT8(24 - ch * 3, loaded.engine.getEffectiveLength(ch));
    }
}

void test_the_patterns_survive_with_their_ratchets() {
    eeprom.reset();
    Rig saved;
    Pattern* p = saved.bank.getPattern(5);
    p->writeStep(0, true);
    p->writeStep(23, true);
    p->setRatchet(0, flexseq::RATCHET_6);
    p->setRatchet(23, flexseq::RATCHET_TRIPLET);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, persist::QUIET_MS);

    Rig loaded;
    loaded.scheduler.load(eeprom, loaded.image);
    const Pattern* q = loaded.bank.getPattern(5);
    bool active = false;
    q->readStep(0, active);
    TEST_ASSERT_TRUE(active);
    q->readStep(23, active);
    TEST_ASSERT_TRUE(active);
    q->readStep(11, active);
    TEST_ASSERT_FALSE(active);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_6, q->getRatchet(0));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET, q->getRatchet(23));
}

/*
 * L'octet de version
 */

void test_a_wrong_version_byte_returns_the_defaults() {
    eeprom.reset();
    Rig saved;
    fillDistinctState(saved);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, persist::QUIET_MS);

    eeprom.cell[persist::BASE_ADDRESS] = static_cast<uint8_t>(persist::FORMAT_VERSION + 1);

    Rig loaded;
    fillDistinctState(loaded);
    TEST_ASSERT_FALSE(loaded.scheduler.load(eeprom, loaded.image));

    Rig fresh;
    TEST_ASSERT_TRUE_MESSAGE(sameState(fresh, loaded),
                             "un octet de version faux doit rendre les defauts");
    TEST_ASSERT_EQUAL_UINT16(UiController::DEFAULT_TEMPO, loaded.ui.tempo());
    TEST_ASSERT_EQUAL_UINT8(0, loaded.ui.clockSource());
}

void test_a_blank_eeprom_returns_the_defaults() {
    eeprom.reset();
    Rig loaded;
    fillDistinctState(loaded);
    TEST_ASSERT_FALSE(loaded.scheduler.load(eeprom, loaded.image));
    Rig fresh;
    TEST_ASSERT_TRUE(sameState(fresh, loaded));
}

void test_the_version_byte_is_written_first_and_kept() {
    eeprom.reset();
    Rig r;
    r.scheduler.markDirty(0);
    finishWrite(r, eeprom, persist::QUIET_MS);
    TEST_ASSERT_EQUAL_UINT8(persist::FORMAT_VERSION, eeprom.cell[persist::BASE_ADDRESS]);
}

/*
 * L'ecriture ne sort JAMAIS de sa zone
 */

void test_no_byte_below_384_or_above_669_is_ever_touched() {
    eeprom.reset();
    Rig r;
    fillDistinctState(r);
    r.scheduler.markDirty(0);
    finishWrite(r, eeprom, persist::QUIET_MS);

    TEST_ASSERT_TRUE_MESSAGE(r.scheduler.isWriting() == false, "la sauvegarde n'est pas finie");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(persist::BASE_ADDRESS, eeprom.lowestWrite,
        "une ecriture est passee SOUS l'adresse 384");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(persist::BASE_ADDRESS + persist::TOTAL_SIZE - 1,
        eeprom.highestWrite, "une ecriture est passee AU-DELA de la zone");

    for (uint16_t address = 0; address < persist::BASE_ADDRESS; ++address) {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(SENTINEL, eeprom.cell[address],
            "un octet du firmware d'origine a ete ecrase");
    }
    for (uint16_t address = persist::BASE_ADDRESS + persist::TOTAL_SIZE;
         address < persist::EEPROM_SIZE; ++address) {
        TEST_ASSERT_EQUAL_UINT8(SENTINEL, eeprom.cell[address]);
    }
}

/*
 * Le delai de calme et l'ecriture etalee
 */

void test_nothing_is_written_before_the_quiet_delay() {
    eeprom.reset();
    Rig r;
    r.scheduler.markDirty(1000);
    TEST_ASSERT_FALSE(r.scheduler.advance(eeprom, r.image, 1000));
    TEST_ASSERT_FALSE(r.scheduler.advance(eeprom, r.image, 1000 + persist::QUIET_MS - 1));
    TEST_ASSERT_EQUAL_UINT16(0, eeprom.writes);
    TEST_ASSERT_TRUE(r.scheduler.advance(eeprom, r.image, 1000 + persist::QUIET_MS));
    TEST_ASSERT_EQUAL_UINT16(1, eeprom.writes);
}

void test_a_change_during_the_delay_restarts_the_countdown() {
    eeprom.reset();
    Rig r;
    r.scheduler.markDirty(0);
    r.scheduler.advance(eeprom, r.image, persist::QUIET_MS - 1);
    TEST_ASSERT_EQUAL_UINT16(0, eeprom.writes);
    r.scheduler.markDirty(persist::QUIET_MS - 1);
    r.scheduler.advance(eeprom, r.image, persist::QUIET_MS);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, eeprom.writes,
        "le compte a rebours n'a pas redemarre a la derniere modification");
    TEST_ASSERT_TRUE(r.scheduler.advance(eeprom, r.image, 2 * persist::QUIET_MS));
}

void test_at_most_one_byte_is_written_per_call() {
    eeprom.reset();
    Rig r;
    fillDistinctState(r);
    r.scheduler.markDirty(0);
    uint16_t previous = 0;
    while (r.scheduler.advance(eeprom, r.image, persist::QUIET_MS)) {
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(previous + 1, eeprom.writes,
            "plus d'une ecriture dans un seul passage de boucle");
        previous = eeprom.writes;
    }
}

void test_an_unchanged_byte_costs_no_write() {
    eeprom.reset();
    Rig r;
    fillDistinctState(r);
    r.scheduler.markDirty(0);
    finishWrite(r, eeprom, persist::QUIET_MS);
    const uint16_t first = eeprom.writes;
    TEST_ASSERT_TRUE(first > 0);

    r.scheduler.markDirty(persist::QUIET_MS);
    finishWrite(r, eeprom, 2 * persist::QUIET_MS);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(first, eeprom.writes,
        "une seconde sauvegarde identique doit n'ecrire AUCUN octet");
}

void test_one_edited_step_costs_a_handful_of_writes_not_the_whole_image() {
    eeprom.reset();
    Rig r;
    r.scheduler.markDirty(0);
    finishWrite(r, eeprom, persist::QUIET_MS);
    const uint16_t baseline = eeprom.writes;

    r.bank.getPattern(0)->writeStep(3, true);
    r.scheduler.markDirty(persist::QUIET_MS);
    finishWrite(r, eeprom, 2 * persist::QUIET_MS);

    const uint16_t added = static_cast<uint16_t>(eeprom.writes - baseline);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(1, added,
        "un step active ne doit couter qu'un octet");
}

void test_the_scheduler_is_clean_once_the_image_is_written() {
    eeprom.reset();
    Rig r;
    fillDistinctState(r);
    r.scheduler.markDirty(0);
    TEST_ASSERT_TRUE(r.scheduler.isDirty());
    finishWrite(r, eeprom, persist::QUIET_MS);
    TEST_ASSERT_FALSE(r.scheduler.isDirty());
    TEST_ASSERT_FALSE(r.scheduler.isWriting());
}

void test_a_power_cut_after_the_write_completes_loses_nothing() {
    eeprom.reset();
    Rig before;
    fillDistinctState(before);
    before.scheduler.markDirty(0);
    finishWrite(before, eeprom, persist::QUIET_MS);

    // La coupure : rien de plus n'est appele, on repart d'un etat neuf.
    Rig after;
    TEST_ASSERT_TRUE(after.scheduler.load(eeprom, after.image));
    TEST_ASSERT_TRUE(sameState(before, after));
}

void test_a_power_cut_in_the_middle_of_a_write_is_caught_by_the_version_byte() {
    eeprom.reset();
    Rig r;
    fillDistinctState(r);
    r.scheduler.markDirty(0);
    // Un seul octet ecrit : c'est l'en-tete. On coupe la, puis on efface
    // l'en-tete comme le ferait un format futur incompatible.
    r.scheduler.advance(eeprom, r.image, persist::QUIET_MS);
    TEST_ASSERT_EQUAL_UINT16(1, eeprom.writes);
    eeprom.cell[persist::BASE_ADDRESS] = 0xFF;

    Rig loaded;
    TEST_ASSERT_FALSE(loaded.scheduler.load(eeprom, loaded.image));
}

/*
 * Robustesse du contenu relu
 */

void test_an_invalid_ratchet_nibble_is_read_back_as_none() {
    eeprom.reset();
    Rig r;
    r.scheduler.markDirty(0);
    finishWrite(r, eeprom, persist::QUIET_MS);
    // Quartets 1 et 5, absents de la table des ratchets (PRD 6.3).
    eeprom.cell[persist::BASE_ADDRESS + persist::PATTERNS_OFFSET
                + persist::PATTERN_STEP_BYTES] = 0x51;

    Rig loaded;
    TEST_ASSERT_TRUE(loaded.scheduler.load(eeprom, loaded.image));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, loaded.bank.getPattern(0)->getRatchet(0));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, loaded.bank.getPattern(0)->getRatchet(1));
}

// Pattern::setRatchet refuse deja un code inconnu, donc sans nettoyage l'ancienne
// valeur SURVIVRAIT au chargement au lieu d'etre remplacee. C'est cette
// difference-la que le nettoyage fait, et rien d'autre.
void test_an_invalid_nibble_replaces_a_previous_ratchet_instead_of_keeping_it() {
    eeprom.reset();
    Rig r;
    r.scheduler.markDirty(0);
    finishWrite(r, eeprom, persist::QUIET_MS);
    eeprom.cell[persist::BASE_ADDRESS + persist::PATTERNS_OFFSET
                + persist::PATTERN_STEP_BYTES] = 0x51;

    Rig loaded;
    loaded.bank.getPattern(0)->setRatchet(0, flexseq::RATCHET_3);
    loaded.bank.getPattern(0)->setRatchet(1, flexseq::RATCHET_6);
    loaded.scheduler.load(eeprom, loaded.image);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(flexseq::RATCHET_NONE,
        loaded.bank.getPattern(0)->getRatchet(0), "l'ancien ratchet a survecu au chargement");
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, loaded.bank.getPattern(0)->getRatchet(1));
}

void test_an_out_of_range_stored_value_is_refused_not_applied() {
    eeprom.reset();
    Rig r;
    r.scheduler.markDirty(0);
    finishWrite(r, eeprom, persist::QUIET_MS);
    eeprom.cell[persist::BASE_ADDRESS + persist::CHANNELS_OFFSET + 1] = 99; // LENGTH
    eeprom.cell[persist::BASE_ADDRESS + persist::GLOBAL_OFFSET + 2] = 99;   // source

    Rig loaded;
    loaded.scheduler.load(eeprom, loaded.image);
    TEST_ASSERT_EQUAL_UINT8(SequencerEngine::DEFAULT_LENGTH, loaded.engine.getEffectiveLength(0));
    TEST_ASSERT_TRUE(loaded.ui.clockSource() < UiController::CLOCK_SOURCE_COUNT);
}

void test_a_stored_subdiv_index_survives_as_its_value() {
    eeprom.reset();
    Rig saved;
    saved.engine.setSubdiv(0, -24);
    saved.engine.setSubdiv(1, 128);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, persist::QUIET_MS);

    Rig loaded;
    loaded.scheduler.load(eeprom, loaded.image);
    TEST_ASSERT_EQUAL_INT16(-24, loaded.engine.getSubdiv(0));
    TEST_ASSERT_EQUAL_INT16(128, loaded.engine.getSubdiv(1));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_the_layout_is_the_one_the_prd_fixed);
    RUN_TEST(test_the_image_ends_below_the_original_memcode);

    RUN_TEST(test_a_round_trip_restores_the_state_byte_for_byte);
    RUN_TEST(test_the_patterns_survive_with_their_ratchets);

    RUN_TEST(test_a_wrong_version_byte_returns_the_defaults);
    RUN_TEST(test_a_blank_eeprom_returns_the_defaults);
    RUN_TEST(test_the_version_byte_is_written_first_and_kept);

    RUN_TEST(test_no_byte_below_384_or_above_669_is_ever_touched);

    RUN_TEST(test_nothing_is_written_before_the_quiet_delay);
    RUN_TEST(test_a_change_during_the_delay_restarts_the_countdown);
    RUN_TEST(test_at_most_one_byte_is_written_per_call);
    RUN_TEST(test_an_unchanged_byte_costs_no_write);
    RUN_TEST(test_one_edited_step_costs_a_handful_of_writes_not_the_whole_image);
    RUN_TEST(test_the_scheduler_is_clean_once_the_image_is_written);
    RUN_TEST(test_a_power_cut_after_the_write_completes_loses_nothing);
    RUN_TEST(test_a_power_cut_in_the_middle_of_a_write_is_caught_by_the_version_byte);

    RUN_TEST(test_an_invalid_ratchet_nibble_is_read_back_as_none);
    RUN_TEST(test_an_invalid_nibble_replaces_a_previous_ratchet_instead_of_keeping_it);
    RUN_TEST(test_an_out_of_range_stored_value_is_refused_not_applied);
    RUN_TEST(test_a_stored_subdiv_index_survives_as_its_value);

    return UNITY_END();
}
