#include <stdint.h>
#include <string.h>
#include <unity.h>

#include <flexseq/Persistence.h>
#include <flexseq/TriggerSequencer.h>

using flexseq::PatternBank;
using flexseq::Pattern;
using flexseq::PersistenceScheduler;
using flexseq::PersistentImage;
using flexseq::PersistentImageV4;
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
    bool busyFlag;

    FakeEeprom() { reset(); }

    void reset() {
        memset(cell, SENTINEL, sizeof(cell));
        writes = 0;
        lowestWrite = persist::EEPROM_SIZE;
        highestWrite = 0;
        busyFlag = false;
    }

    uint8_t read(uint16_t address) const { return cell[address]; }

    bool busy() const { return busyFlag; }

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

    Rig() : engine(), transport(engine), ui(engine, transport), prefs(),
            image(bank, engine, ui, prefs) {
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
        r.engine.setBaseLength(ch, static_cast<uint8_t>(24 - ch * 3));
        r.engine.setSubdiv(ch, flexseq::subdivAtIndex(static_cast<uint8_t>(ch * 3)));
        r.engine.setBarLength(ch, ch % 2 == 0 ? 3 : 6);
        r.engine.setChannelMode(ch, static_cast<flexseq::ChannelMode>(ch % 3));
        r.engine.setOffset(ch, static_cast<uint16_t>(ch * 2 + 1));
        r.engine.setSkipChance(ch, static_cast<uint8_t>(ch + 2));
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
    TEST_ASSERT_EQUAL_UINT16(304, persist::TOTAL_SIZE);
    TEST_ASSERT_EQUAL_UINT16(1, persist::HEADER_SIZE);
    TEST_ASSERT_EQUAL_UINT16(240, persist::PATTERNS_SIZE);
    TEST_ASSERT_EQUAL_UINT16(54, persist::CHANNELS_SIZE);
    TEST_ASSERT_EQUAL_UINT16(9, persist::CHANNEL_RECORD);
    TEST_ASSERT_EQUAL_UINT16(3, persist::GLOBAL_SIZE);
    TEST_ASSERT_EQUAL_UINT16(6, persist::PREFS_SIZE);
    TEST_ASSERT_EQUAL_UINT16(1, persist::PATTERNS_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(241, persist::CHANNELS_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(295, persist::GLOBAL_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(298, persist::PREFS_OFFSET);
}

void test_the_format_version_is_two() {
    TEST_ASSERT_EQUAL_UINT8(2, persist::FORMAT_VERSION);
}

void test_the_image_ends_below_the_original_memcode() {
    TEST_ASSERT_TRUE(persist::BASE_ADDRESS + persist::TOTAL_SIZE <= 1023);
}


/*
 * Les accesseurs octet bruts de Pattern
 */

void test_the_pattern_keeps_its_twenty_three_bytes() {
    TEST_ASSERT_EQUAL_UINT8(23, sizeof(Pattern));
}

void test_a_raw_step_byte_refuses_an_index_past_the_pattern() {
    Pattern pattern;
    pattern.setStepByte(5, 0xFF);
    pattern.setStepByte(200, 0xFF);
    TEST_ASSERT_EQUAL_UINT8(0, pattern.stepByte(5));
    TEST_ASSERT_EQUAL_UINT8(0, pattern.stepByte(200));
    for (uint8_t i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, pattern.stepByte(i));
    }
}

void test_a_raw_ratchet_byte_refuses_an_index_past_the_pattern() {
    Pattern pattern;
    pattern.setRatchetByte(18, 0xFF);
    pattern.setRatchetByte(200, 0xFF);
    TEST_ASSERT_EQUAL_UINT8(0, pattern.ratchetByte(18));
    TEST_ASSERT_EQUAL_UINT8(0, pattern.ratchetByte(200));
    for (uint8_t i = 0; i < 18; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, pattern.ratchetByte(i));
    }
}

void test_a_raw_step_byte_stores_what_it_is_given_without_canonicalising() {
    Pattern pattern;
    pattern.setStepByte(4, 0xF0);
    TEST_ASSERT_EQUAL_UINT8(0xF0, pattern.stepByte(4));
}

void test_the_v4_content_codec_round_trips_the_twenty_three_bytes() {
    Pattern source;
    const uint8_t activeSteps[] = {0, 7, 8, 23, 31, 32, 35};
    for (uint8_t i = 0; i < sizeof(activeSteps); ++i) {
        source.writeStep(activeSteps[i], true);
    }
    source.setRatchet(0, flexseq::RATCHET_3);
    source.setRatchet(1, flexseq::RATCHET_6);
    source.setRatchet(34, flexseq::RATCHET_TRIPLET);
    source.setRatchet(35, flexseq::RATCHET_4);

    uint8_t image[persist::v4::CONTENT_BYTES];
    for (uint8_t offset = 0; offset < persist::v4::CONTENT_BYTES; ++offset) {
        image[offset] = persist::v4::contentByte(source, offset);
    }

    Pattern loaded;
    for (uint8_t offset = 0; offset < persist::v4::CONTENT_BYTES; ++offset) {
        persist::v4::applyContentByte(loaded, offset, image[offset]);
    }

    for (uint8_t step = 0; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
        bool wanted = false;
        bool got = false;
        TEST_ASSERT_TRUE(source.readStep(step, wanted));
        TEST_ASSERT_TRUE(loaded.readStep(step, got));
        TEST_ASSERT_EQUAL_INT(wanted, got);
        TEST_ASSERT_EQUAL_UINT8(source.getRatchet(step), loaded.getRatchet(step));
    }

    for (uint8_t offset = 0; offset < persist::v4::CONTENT_BYTES; ++offset) {
        TEST_ASSERT_EQUAL_UINT8(image[offset], persist::v4::contentByte(loaded, offset));
    }
}

void test_the_v4_codec_drops_the_four_bits_above_the_last_step_on_load() {
    Pattern pattern;
    persist::v4::applyContentByte(pattern, 4, 0xFF);

    TEST_ASSERT_EQUAL_UINT8(0x0F, pattern.stepByte(4));

    for (uint8_t step = 32; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
        bool active = false;
        TEST_ASSERT_TRUE(pattern.readStep(step, active));
        TEST_ASSERT_TRUE(active);
    }
}

void test_the_v4_codec_drops_the_four_bits_above_the_last_step_on_emit() {
    Pattern pattern;
    pattern.setStepByte(4, 0xF0);
    TEST_ASSERT_EQUAL_UINT8(0, persist::v4::contentByte(pattern, 4));

    pattern.setStepByte(4, 0xF5);
    TEST_ASSERT_EQUAL_UINT8(0x05, persist::v4::contentByte(pattern, 4));
}

void test_the_v4_codec_leaves_the_other_step_bytes_whole() {
    Pattern pattern;
    for (uint8_t offset = 0; offset < 4; ++offset) {
        persist::v4::applyContentByte(pattern, offset, 0xFF);
        TEST_ASSERT_EQUAL_UINT8(0xFF, pattern.stepByte(offset));
        TEST_ASSERT_EQUAL_UINT8(0xFF, persist::v4::contentByte(pattern, offset));
    }
}

void test_the_v4_codec_normalises_an_invalid_ratchet_nibble() {
    Pattern pattern;
    persist::v4::applyContentByte(pattern, 5, 0x53);

    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_3, pattern.getRatchet(0));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(1));
    TEST_ASSERT_EQUAL_UINT8(0x03, persist::v4::contentByte(pattern, 5));
}

void test_an_invalid_v4_nibble_replaces_a_previous_ratchet_instead_of_keeping_it() {
    Pattern pattern;
    persist::v4::applyContentByte(pattern, 5, 0x36);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_6, pattern.getRatchet(0));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_3, pattern.getRatchet(1));

    persist::v4::applyContentByte(pattern, 5, 0x55);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(0));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(1));
    TEST_ASSERT_EQUAL_UINT8(0, persist::v4::contentByte(pattern, 5));
}

void test_the_v4_codec_ignores_an_offset_past_the_record() {
    Pattern pattern;
    for (uint8_t offset = 0; offset < persist::v4::CONTENT_BYTES; ++offset) {
        persist::v4::applyContentByte(pattern, offset, static_cast<uint8_t>(0x11 * offset));
    }

    uint8_t before[persist::v4::CONTENT_BYTES];
    for (uint8_t offset = 0; offset < persist::v4::CONTENT_BYTES; ++offset) {
        before[offset] = persist::v4::contentByte(pattern, offset);
    }

    persist::v4::applyContentByte(pattern, persist::v4::CONTENT_BYTES, 0xFF);
    persist::v4::applyContentByte(pattern, 200, 0xFF);

    for (uint8_t offset = 0; offset < persist::v4::CONTENT_BYTES; ++offset) {
        TEST_ASSERT_EQUAL_UINT8(before[offset], persist::v4::contentByte(pattern, offset));
    }

    TEST_ASSERT_EQUAL_UINT8(0, persist::v4::contentByte(pattern, persist::v4::CONTENT_BYTES));
    TEST_ASSERT_EQUAL_UINT8(0, persist::v4::contentByte(pattern, 200));
}

namespace {

void fillDistinctPattern(Pattern& pattern) {
    const uint8_t activeSteps[] = {0, 7, 8, 23, 31, 32, 35};
    for (uint8_t i = 0; i < sizeof(activeSteps); ++i) {
        pattern.writeStep(activeSteps[i], true);
    }
    pattern.setRatchet(0, flexseq::RATCHET_3);
    pattern.setRatchet(1, flexseq::RATCHET_6);
    pattern.setRatchet(34, flexseq::RATCHET_TRIPLET);
    pattern.setRatchet(35, flexseq::RATCHET_4);
}

}  // namespace


/*
 * Aller-retour
 */

void test_a_round_trip_restores_the_cv_destinations() {
    eeprom.reset();
    Rig saved;
    TEST_ASSERT_TRUE(saved.engine.setCvDestination(0, flexseq::CV_SOURCE_1,
                                                   flexseq::CV_DEST_LENGTH));
    TEST_ASSERT_TRUE(saved.engine.setCvDestination(0, flexseq::CV_SOURCE_2,
                                                   flexseq::CV_DEST_STEP));
    TEST_ASSERT_TRUE(saved.engine.setCvDestination(3, flexseq::CV_SOURCE_1,
                                                   flexseq::CV_DEST_RESET));
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, persist::QUIET_MS);

    Rig loaded;
    TEST_ASSERT_TRUE(loaded.scheduler.load(eeprom, loaded.image));
    TEST_ASSERT_EQUAL_UINT8(flexseq::CV_DEST_LENGTH,
                            loaded.engine.getCvDestination(0, flexseq::CV_SOURCE_1));
    TEST_ASSERT_EQUAL_UINT8(flexseq::CV_DEST_STEP,
                            loaded.engine.getCvDestination(0, flexseq::CV_SOURCE_2));
    TEST_ASSERT_EQUAL_UINT8(flexseq::CV_DEST_RESET,
                            loaded.engine.getCvDestination(3, flexseq::CV_SOURCE_1));
    TEST_ASSERT_EQUAL_UINT8(flexseq::CV_DEST_NONE,
                            loaded.engine.getCvDestination(3, flexseq::CV_SOURCE_2));
}

void test_a_round_trip_never_restores_a_hysteresis_zone() {
    eeprom.reset();
    Rig saved;
    TEST_ASSERT_TRUE(saved.engine.setChannelMode(0, flexseq::MODE_SEQ));
    TEST_ASSERT_TRUE(saved.engine.setBaseLength(0, 18));
    TEST_ASSERT_TRUE(saved.engine.setCvDestination(0, flexseq::CV_SOURCE_1,
                                                   flexseq::CV_DEST_LENGTH));
    saved.engine.setCvInput(flexseq::CV_SOURCE_1, 330);
    saved.engine.start();
    saved.engine.advance(96);
    TEST_ASSERT_EQUAL_INT8(10, saved.engine.lengthCvOffset(0));
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, persist::QUIET_MS);

    Rig loaded;
    TEST_ASSERT_TRUE(loaded.scheduler.load(eeprom, loaded.image));
    TEST_ASSERT_EQUAL_UINT8(flexseq::CV_DEST_LENGTH,
                            loaded.engine.getCvDestination(0, flexseq::CV_SOURCE_1));
    TEST_ASSERT_EQUAL_INT8_MESSAGE(0, loaded.engine.lengthCvOffset(0),
        "les zones ne sont pas persistees : seule la destination revient");
    TEST_ASSERT_EQUAL_UINT8(18, loaded.engine.getEffectiveLength(0));
}

void test_a_round_trip_keeps_a_base_length_of_thirty_six() {
    eeprom.reset();
    Rig saved;
    TEST_ASSERT_TRUE(saved.engine.setBaseLengthFromStorage(0, 36));
    TEST_ASSERT_EQUAL_UINT8(36, saved.engine.getBaseLength(0));
    TEST_ASSERT_EQUAL_UINT8(36, saved.engine.getEffectiveLength(0));
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, persist::QUIET_MS);

    Rig loaded;
    TEST_ASSERT_TRUE(loaded.scheduler.load(eeprom, loaded.image));
    TEST_ASSERT_EQUAL_UINT8(36, loaded.engine.getBaseLength(0));
    TEST_ASSERT_EQUAL_UINT8(36, loaded.engine.getEffectiveLength(0));
}

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

void test_no_byte_below_384_or_above_687_is_ever_touched() {
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
    // Quartet 5, absent de la table des ratchets (PRD 6.3).
    eeprom.cell[persist::BASE_ADDRESS + persist::PATTERNS_OFFSET
                + persist::PATTERN_STEP_BYTES] = 0x55;

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
                + persist::PATTERN_STEP_BYTES] = 0x55;

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

/*
 * Format v2 — l'enregistrement par channel passe de 6 a 9 octets
 */

void test_the_channel_record_carries_the_mode_the_offset_and_the_skip_chance() {
    eeprom.reset();
    Rig saved;
    saved.engine.setChannelMode(2, flexseq::MODE_SEQ);
    saved.engine.setOffset(2, 7);
    saved.engine.setSkipChance(2, 9);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, persist::QUIET_MS);

    Rig loaded;
    TEST_ASSERT_TRUE(loaded.scheduler.load(eeprom, loaded.image));
    TEST_ASSERT_EQUAL_UINT8(flexseq::MODE_SEQ, loaded.engine.getChannelMode(2));
    TEST_ASSERT_EQUAL_UINT16(7, loaded.engine.getOffset(2));
    TEST_ASSERT_EQUAL_UINT8(9, loaded.engine.getSkipChance(2));
}

void test_the_three_new_fields_sit_at_their_fixed_place_in_the_record() {
    eeprom.reset();
    Rig r;
    r.engine.setChannelMode(0, flexseq::MODE_RANDOM);
    r.engine.setOffset(0, 13);
    r.engine.setSkipChance(0, 4);
    const uint16_t base = persist::CHANNELS_OFFSET;
    TEST_ASSERT_EQUAL_UINT8(flexseq::MODE_RANDOM, r.image.byteAt(base + 4));
    TEST_ASSERT_EQUAL_UINT8(13, r.image.byteAt(base + 5));
    TEST_ASSERT_EQUAL_UINT8(4, r.image.byteAt(base + 6));
}

void test_a_version_one_image_returns_the_defaults() {
    eeprom.reset();
    Rig saved;
    fillDistinctState(saved);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, persist::QUIET_MS);
    eeprom.cell[persist::BASE_ADDRESS] = 1;

    Rig loaded;
    fillDistinctState(loaded);
    TEST_ASSERT_FALSE_MESSAGE(loaded.scheduler.load(eeprom, loaded.image),
        "une image en version 1 doit etre refusee");
    Rig fresh;
    TEST_ASSERT_TRUE(sameState(fresh, loaded));
}

void test_a_bad_mode_byte_is_refused_while_the_next_record_still_loads() {
    eeprom.reset();
    Rig saved;
    saved.engine.setChannelMode(1, flexseq::MODE_SEQ);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, persist::QUIET_MS);
    eeprom.cell[persist::BASE_ADDRESS + persist::CHANNELS_OFFSET + 4] = 3;

    Rig loaded;
    loaded.scheduler.load(eeprom, loaded.image);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(flexseq::MODE_SEQ, loaded.engine.getChannelMode(1),
        "l'enregistrement suivant n'a pas ete lu");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(flexseq::DEFAULT_CHANNEL_MODE,
        loaded.engine.getChannelMode(0), "un mode hors plage a ete applique");
}

void test_a_bad_skip_chance_byte_is_refused_while_the_next_record_still_loads() {
    eeprom.reset();
    Rig saved;
    saved.engine.setSkipChance(1, 7);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, persist::QUIET_MS);
    eeprom.cell[persist::BASE_ADDRESS + persist::CHANNELS_OFFSET + 6] = 99;

    Rig loaded;
    loaded.scheduler.load(eeprom, loaded.image);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(7, loaded.engine.getSkipChance(1),
        "l'enregistrement suivant n'a pas ete lu");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, loaded.engine.getSkipChance(0),
        "une chance de saut hors plage a ete appliquee");
}

void test_the_offset_never_exceeds_the_single_byte_the_format_gives_it() {
    eeprom.reset();
    Rig saved;
    saved.engine.setSubdiv(0, 128);
    saved.engine.setOffset(0, 300);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(255, saved.engine.getOffset(0),
        "l'offset doit tenir dans l'octet que le format lui donne");
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, persist::QUIET_MS);

    Rig loaded;
    loaded.scheduler.load(eeprom, loaded.image);
    TEST_ASSERT_EQUAL_UINT16(255, loaded.engine.getOffset(0));
}

void test_the_two_cv_target_bytes_are_reserved_and_read_as_zero() {
    Rig r;
    fillDistinctState(r);
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        const uint16_t base =
            static_cast<uint16_t>(persist::CHANNELS_OFFSET + ch * persist::CHANNEL_RECORD);
        TEST_ASSERT_EQUAL_UINT8(0, r.image.byteAt(base + 7));
        TEST_ASSERT_EQUAL_UINT8(0, r.image.byteAt(base + 8));
    }
}

void test_a_stored_cv_target_is_ignored_without_disturbing_the_record() {
    eeprom.reset();
    Rig saved;
    fillDistinctState(saved);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, persist::QUIET_MS);
    eeprom.cell[persist::BASE_ADDRESS + persist::CHANNELS_OFFSET + 7] = 0xFF;
    eeprom.cell[persist::BASE_ADDRESS + persist::CHANNELS_OFFSET + 8] = 0xFF;

    Rig loaded;
    TEST_ASSERT_TRUE(loaded.scheduler.load(eeprom, loaded.image));
    TEST_ASSERT_EQUAL_INT8(saved.engine.getSelectedPattern(0), loaded.engine.getSelectedPattern(0));
    TEST_ASSERT_EQUAL_UINT8(saved.engine.getEffectiveLength(0),
                            loaded.engine.getEffectiveLength(0));
    TEST_ASSERT_EQUAL_UINT8(saved.engine.getSkipChance(0), loaded.engine.getSkipChance(0));
    TEST_ASSERT_EQUAL_UINT8(0, loaded.image.byteAt(persist::CHANNELS_OFFSET + 7));
}


struct JournalEeprom {
    uint8_t cell[persist::EEPROM_SIZE];
    uint16_t order[persist::EEPROM_SIZE];
    uint16_t count;
    bool overflowed;

    JournalEeprom() { reset(); }

    void reset() {
        memset(cell, SENTINEL, sizeof(cell));
        count = 0;
        overflowed = false;
    }

    uint8_t read(uint16_t address) const { return cell[address]; }

    void write(uint16_t address, uint8_t value) {
        cell[address] = value;
        if (count < persist::EEPROM_SIZE) {
            order[count++] = address;
        } else {
            overflowed = true;
        }
    }
};

JournalEeprom journal;


void test_the_v2_scan_still_writes_the_version_first() {
    journal.reset();
    Rig r;
    for (uint16_t index = 0; index < flexseq::PersistentImage::SIZE; ++index) {
        journal.cell[r.image.addressAt(index)] = static_cast<uint8_t>(~r.image.byteAt(index));
    }
    journal.count = 0;
    r.scheduler.markDirty(0);
    while (r.scheduler.advance(journal, r.image, persist::QUIET_MS)) {
    }
    TEST_ASSERT_EQUAL_UINT16(304, journal.count);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(384, journal.order[0],
        "le contrat v2 ecrit la version en PREMIER, et il ne change pas");
}


void test_the_v2_mapping_is_the_identity_the_scheduler_used_to_inline() {
    Rig r;
    for (uint16_t index = 0; index < flexseq::PersistentImage::SIZE; ++index) {
        TEST_ASSERT_EQUAL_UINT16(384 + index, r.image.addressAt(index));
    }
}

void test_the_v2_version_sits_at_the_first_logical_index() {
    TEST_ASSERT_EQUAL_UINT16(0, flexseq::PersistentImage::VERSION_INDEX);
    Rig r;
    TEST_ASSERT_EQUAL_UINT16(384, r.image.addressAt(flexseq::PersistentImage::VERSION_INDEX));
    TEST_ASSERT_EQUAL_UINT8(2, r.image.byteAt(flexseq::PersistentImage::VERSION_INDEX));
}


// ---------------------------------------------------------------------------
// STEP-8.1, P35 normative: PRD 10.3 points 6 and 7, ADR 0011.
// OLD carries a TRIPLET on step 1, NEW carries none. After the boundary
// that moves the index to NEW, the content read is NEW, so the cached
// ratchet must be NEW too: one trigger, a step of one unit.

// STEP-8.1, the refused path is a characterization, green before B6 and
// after it: the loader validates the length before it writes one byte, so
// a refused record publishes nothing and the cache has nothing to follow.

// STEP-8b.5, P35 under a STEP offset: PRD 10.2 E3.4-1, 10.3 points 6 and 7,
// ADR 0011. CV1 moves the PATTERN index, CV2 shifts the read by +3. On the
// boundary that swaps OLD for NEW, local step 2 is read at step 5, and the
// content and the ratchet of that step must both come from NEW.
//
//            step 2                 step 5 (the read step)
//   OLD      inactive, NONE         ACTIVE, TRIPLET      -> 3 triggers, 192 ticks
//   NEW      ACTIVE, TRIPLET        INACTIVE, RATCHET_2  -> 2 triggers,  96 ticks
//
// NEW differs from OLD on both dimensions at step 5, and NEW differs from
// itself between step 5 and step 2: a reader left on localStep, or a ratchet
// left on OLD, changes the observation.

// STEP-9.3 G2: the PATTERN index and the STEP offset change on the SAME
// boundary. CV1 moves the index from 5 to 0 and CV2 shifts the read by +3 in
// the same advance(). The musical decision must read the NEW template at the
// NEW read step: NEW carries an active step 5 with RATCHET_2, so the sub-onset
// at half a step fires exactly once. Every other pairing is silent: OLD has
// nothing at step 5, NEW has nothing at local step 2.

// Entre dans l ecran EDIT d un canal. Le mode SEQ est obligatoire : EDIT n est
// atteignable que de la, decision du proprietaire du 2026-09-05.


// PRD 5.0 amendement 1quater : les six copies partent PROPRES, decision du
// proprietaire. Consequence acceptee et nommee : apres une coupure, le premier
// chargement d un canal ecrase une copie editee sans rien demander.


// Decision du proprietaire du 2026-09-14 : le record de template stocke une
// longueur, donc un canal dont la longueur a change ne correspond plus au
// template charge.


// Le bit vit dans un octet : un index hors des six deborderait sur un voisin.


// L oracle est le COMPORTEMENT : le template change apres le premier service, et
// un second passage ne doit pas aller le relire.

// Le gel de A1 a A8 porte sur l ECRITURE, jamais sur la lecture.

// Une EEPROM occupee GARDE la demande : la consommer sans charger la perdrait
// en silence.

// Le chargement remet la copie a l identique du template : le drapeau tombe.

/*
 * Version 4 — the shared bank, PRD 11.1 and ADR 0014
 */

namespace {

struct RigV4 {
    PatternBank bank;
    SequencerEngine engine;
    Transport transport;
    UiController ui;
    Preferences prefs;
    PersistentImageV4 image;
    PersistenceScheduler scheduler;

    RigV4() : engine(), transport(engine), ui(engine, transport), prefs(),
              image(engine, ui, prefs) {
        engine.setPatternBank(&bank);
    }
};

uint16_t finishWriteV4(RigV4& r, FakeEeprom& ee, uint32_t nowMs) {
    uint16_t calls = 0;
    while (r.scheduler.advance(ee, r.image, nowMs)) {
        ++calls;
    }
    return calls;
}

uint64_t bankStepMask(const Pattern& pattern) {
    uint64_t mask = 0;
    for (uint8_t step = 0; step < 36; ++step) {
        bool active = false;
        pattern.readStep(step, active);
        if (active) {
            mask |= (static_cast<uint64_t>(1) << step);
        }
    }
    return mask;
}

void fillDistinctStateV4(RigV4& r) {
    for (uint8_t index = 0; index < flexseq::PATTERN_COUNT; ++index) {
        Pattern* p = r.bank.getPattern(index);
        p->writeStep(static_cast<uint8_t>(index % 36), true);
        p->writeStep(static_cast<uint8_t>((index * 7 + 3) % 36), true);
        p->writeStep(35, index % 2 == 0);
        p->setRatchet(static_cast<uint8_t>(index % 36), flexseq::RATCHET_3);
        p->setRatchet(static_cast<uint8_t>((index * 5 + 1) % 36), flexseq::RATCHET_TRIPLET);
    }
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        r.engine.setSelectedPattern(ch, static_cast<uint8_t>(15 - ch));
        r.engine.setBaseLength(ch, static_cast<uint8_t>(36 - ch * 3));
        r.engine.setSubdiv(ch, flexseq::subdivAtIndex(static_cast<uint8_t>(ch * 3)));
        r.engine.setBarLength(ch, ch % 2 == 0 ? 3 : 6);
        r.engine.setChannelMode(ch, static_cast<flexseq::ChannelMode>(ch % 3));
        r.engine.setOffset(ch, static_cast<uint16_t>(ch * 2 + 1));
        r.engine.setSkipChance(ch, static_cast<uint8_t>(ch + 2));
        r.engine.setCvDestination(ch, flexseq::CV_SOURCE_1,
                                  static_cast<flexseq::CvDestination>((ch + 1) % 5));
        r.engine.setCvDestination(ch, flexseq::CV_SOURCE_2,
                                  static_cast<flexseq::CvDestination>((ch + 3) % 5));
    }
    r.ui.setTempo(287);
    r.ui.setClockSource(4);
    r.prefs.rotateScreen = 0;
    r.prefs.reverseEncoder = 0;
    r.prefs.cvCalibration[0] = -26;
    r.prefs.cvCalibration[1] = 300;
}

bool sameStateV4(const RigV4& a, const RigV4& b) {
    for (uint16_t index = 0; index < PersistentImageV4::SIZE; ++index) {
        if (a.image.byteAt(index) != b.image.byteAt(index)) {
            return false;
        }
    }
    return true;
}

}  // namespace

void test_the_v4_layout_is_the_one_the_prd_fixed() {
    TEST_ASSERT_EQUAL_UINT8(4, persist::v4::FORMAT_VERSION);
    TEST_ASSERT_EQUAL_UINT8(2, persist::FORMAT_VERSION);
    TEST_ASSERT_EQUAL_UINT16(1, persist::v4::HEADER_SIZE);
    TEST_ASSERT_EQUAL_UINT16(1, persist::v4::BANK_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(368, persist::v4::BANK_SIZE);
    TEST_ASSERT_EQUAL_UINT8(23, persist::v4::BANK_RECORD);
    TEST_ASSERT_EQUAL_UINT8(16, persist::v4::BANK_COUNT);
    TEST_ASSERT_EQUAL_UINT16(369, persist::v4::CHANNELS_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(54, persist::v4::CHANNELS_SIZE);
    TEST_ASSERT_EQUAL_UINT16(423, persist::v4::GLOBAL_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(5, persist::v4::GLOBAL_SIZE);
    TEST_ASSERT_EQUAL_UINT16(428, persist::v4::PREFS_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(6, persist::v4::PREFS_SIZE);
    TEST_ASSERT_EQUAL_UINT16(434, persist::v4::TOTAL_SIZE);
    TEST_ASSERT_EQUAL_UINT16(817, persist::v4::LAST_ADDRESS);
}

void test_the_v4_records_carry_thirty_six_steps_and_no_length() {
    TEST_ASSERT_EQUAL_UINT8(5, persist::v4::STEP_BYTES);
    TEST_ASSERT_EQUAL_UINT8(18, persist::v4::RATCHET_BYTES);
    TEST_ASSERT_EQUAL_UINT8(23, persist::v4::CONTENT_BYTES);
    TEST_ASSERT_EQUAL_UINT8(0, persist::v4::RECORD_STEPS_AT);
    TEST_ASSERT_EQUAL_UINT8(5, persist::v4::RECORD_RATCHETS_AT);
    TEST_ASSERT_EQUAL_UINT8(0x0F, persist::v4::LAST_STEP_BYTE_MASK);
}

void test_the_v4_image_is_scanned_whole_and_ends_on_its_version() {
    TEST_ASSERT_EQUAL_UINT16(434, PersistentImageV4::SIZE);
    TEST_ASSERT_EQUAL_UINT16(433, PersistentImageV4::VERSION_INDEX);
    TEST_ASSERT_EQUAL_UINT16(0, persist::v4::IMAGE_BANK_AT);
    TEST_ASSERT_EQUAL_UINT16(368, persist::v4::IMAGE_CHANNELS_AT);
    TEST_ASSERT_EQUAL_UINT16(422, persist::v4::IMAGE_GLOBAL_AT);
    TEST_ASSERT_EQUAL_UINT16(427, persist::v4::IMAGE_PREFS_AT);
    TEST_ASSERT_EQUAL_UINT16(433, persist::v4::IMAGE_VERSION_AT);
}

void test_the_v4_image_leaves_the_original_memcode_alone() {
    TEST_ASSERT_TRUE(persist::BASE_ADDRESS + persist::v4::TOTAL_SIZE <= 1023);
    TEST_ASSERT_EQUAL_UINT16(817, persist::BASE_ADDRESS + persist::v4::TOTAL_SIZE - 1);
}

void test_the_v4_version_holds_the_last_logical_index_at_the_first_address() {
    RigV4 r;
    TEST_ASSERT_EQUAL_UINT16(384, r.image.addressAt(433));
    TEST_ASSERT_EQUAL_UINT8(4, r.image.byteAt(433));
    TEST_ASSERT_EQUAL_UINT16(385, r.image.addressAt(0));
    TEST_ASSERT_EQUAL_UINT16(817, r.image.addressAt(432));
}

void test_the_v4_mapping_covers_the_format_exactly_once() {
    RigV4 r;
    bool seen[1024];
    memset(seen, 0, sizeof(seen));
    for (uint16_t index = 0; index < PersistentImageV4::SIZE; ++index) {
        const uint16_t address = r.image.addressAt(index);
        TEST_ASSERT_FALSE_MESSAGE(seen[address], "two logical indexes map to one address");
        seen[address] = true;
    }
    for (uint16_t address = 384; address <= 817; ++address) {
        TEST_ASSERT_TRUE_MESSAGE(seen[address], "an address of the format is never scanned");
    }
    TEST_ASSERT_FALSE(seen[383]);
    TEST_ASSERT_FALSE(seen[818]);
}

void test_the_v4_scan_writes_the_whole_image_and_the_version_last() {
    journal.reset();
    RigV4 r;
    for (uint16_t index = 0; index < PersistentImageV4::SIZE; ++index) {
        journal.cell[r.image.addressAt(index)] = static_cast<uint8_t>(~r.image.byteAt(index));
    }
    journal.count = 0;
    r.scheduler.markDirty(0);
    while (r.scheduler.advance(journal, r.image, persist::QUIET_MS)) {
    }
    TEST_ASSERT_FALSE(journal.overflowed);
    TEST_ASSERT_EQUAL_UINT16(434, journal.count);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(384, journal.order[journal.count - 1],
        "the version must be the last byte written");
    for (uint16_t i = 0; i + 1 < journal.count; ++i) {
        TEST_ASSERT_TRUE_MESSAGE(journal.order[i] >= 385 && journal.order[i] <= 817,
            "a data byte fell outside the format, or the version came early");
    }
}

void test_the_v4_global_zone_reads_mod_and_range_as_zero_and_normalises_them() {
    RigV4 r;
    TEST_ASSERT_EQUAL_UINT8(0, r.image.byteAt(422 + 3));
    TEST_ASSERT_EQUAL_UINT8(0, r.image.byteAt(422 + 4));
    r.image.applyByte(422 + 3, 0x5A);
    r.image.applyByte(422 + 4, 0xA5);
    TEST_ASSERT_EQUAL_UINT8(0, r.image.byteAt(422 + 3));
    TEST_ASSERT_EQUAL_UINT8(0, r.image.byteAt(422 + 4));
    TEST_ASSERT_EQUAL_UINT16(UiController::DEFAULT_TEMPO, r.ui.tempo());
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.clockSource());
}

void test_a_v4_round_trip_restores_the_bank_and_the_channels_byte_for_byte() {
    eeprom.reset();
    RigV4 saved;
    fillDistinctStateV4(saved);
    saved.scheduler.markDirty(0);
    finishWriteV4(saved, eeprom, persist::QUIET_MS);

    RigV4 loaded;
    TEST_ASSERT_TRUE(loaded.scheduler.load(eeprom, loaded.image));
    TEST_ASSERT_TRUE(sameStateV4(saved, loaded));
    for (uint8_t index = 0; index < flexseq::PATTERN_COUNT; ++index) {
        TEST_ASSERT_EQUAL_UINT64(bankStepMask(*saved.bank.getPattern(index)),
                                 bankStepMask(*loaded.bank.getPattern(index)));
        TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_3,
                                loaded.bank.getPattern(index)->getRatchet(index % 36));
    }
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_EQUAL_INT8(15 - ch, loaded.engine.getSelectedPattern(ch));
        TEST_ASSERT_EQUAL_UINT8(36 - ch * 3, loaded.engine.getBaseLength(ch));
        TEST_ASSERT_EQUAL_UINT8((ch + 1) % 5, loaded.engine.getCvDestination(ch, flexseq::CV_SOURCE_1));
        TEST_ASSERT_EQUAL_UINT8((ch + 3) % 5, loaded.engine.getCvDestination(ch, flexseq::CV_SOURCE_2));
    }
    TEST_ASSERT_EQUAL_UINT16(287, loaded.ui.tempo());
    TEST_ASSERT_EQUAL_INT16(-26, loaded.prefs.cvCalibration[0]);
}

void test_the_v4_defaults_seed_the_factory_bank_and_clear_the_free_slots() {
    RigV4 r;
    r.bank.getPattern(9)->writeStep(2, true);
    r.bank.getPattern(0)->writeStep(1, true);
    r.bank.getPattern(0)->setRatchet(0, flexseq::RATCHET_6);
    r.image.resetToDefaults();
    TEST_ASSERT_EQUAL_UINT64(0x9111, bankStepMask(*r.bank.getPattern(0)));
    TEST_ASSERT_EQUAL_UINT64(0x0810, bankStepMask(*r.bank.getPattern(1)));
    TEST_ASSERT_EQUAL_UINT64(0xB733, bankStepMask(*r.bank.getPattern(7)));
    TEST_ASSERT_EQUAL_UINT8(0, r.bank.getPattern(0)->getRatchet(0));
    for (uint8_t index = 8; index < 16; ++index) {
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(0, bankStepMask(*r.bank.getPattern(index)),
            "a free slot must come back empty");
    }
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_EQUAL_INT8(0, r.engine.getSelectedPattern(ch));
    }
    TEST_ASSERT_EQUAL_UINT16(UiController::DEFAULT_TEMPO, r.ui.tempo());
}

void test_the_v4_defaults_clear_the_cv_routing_and_the_three_settings() {
    RigV4 r;
    r.engine.setCvDestination(2, flexseq::CV_SOURCE_1, flexseq::CV_DEST_PATTERN);
    r.engine.setCvDestination(2, flexseq::CV_SOURCE_2, flexseq::CV_DEST_RESET);
    r.engine.setCvDestination(5, flexseq::CV_SOURCE_2, flexseq::CV_DEST_STEP);
    r.prefs.rotateScreen = 0;
    r.prefs.reverseEncoder = 0;
    r.prefs.cvCalibration[0] = -39;
    r.prefs.cvCalibration[1] = 41;
    r.ui.setTempo(77);
    r.ui.setClockSource(3);

    r.image.resetToDefaults();

    for (uint8_t channel = 0; channel < 6; ++channel) {
        TEST_ASSERT_EQUAL(flexseq::CV_DEST_NONE,
                          r.engine.getCvDestination(channel, flexseq::CV_SOURCE_1));
        TEST_ASSERT_EQUAL(flexseq::CV_DEST_NONE,
                          r.engine.getCvDestination(channel, flexseq::CV_SOURCE_2));
    }
    TEST_ASSERT_EQUAL_UINT8(1, r.prefs.rotateScreen);
    TEST_ASSERT_EQUAL_UINT8(1, r.prefs.reverseEncoder);
    TEST_ASSERT_EQUAL_INT16(0, r.prefs.cvCalibration[0]);
    TEST_ASSERT_EQUAL_INT16(0, r.prefs.cvCalibration[1]);
    TEST_ASSERT_EQUAL_UINT16(120, r.ui.tempo());
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.clockSource());
}

void test_the_v4_defaults_reset_the_mode_the_offset_and_the_skip_chance() {
    eeprom.reset();
    eeprom.cell[384] = 0xFF;
    RigV4 r;
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_TRUE(r.engine.setChannelMode(ch, flexseq::MODE_RANDOM));
        TEST_ASSERT_TRUE(r.engine.setOffset(ch, 7));
        TEST_ASSERT_TRUE(r.engine.setSkipChance(ch, 9));
    }
    TEST_ASSERT_FALSE(flexseq::bootstrap(eeprom, r.image, r.scheduler, 0));
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_EQUAL_UINT8(flexseq::MODE_CLOCK, r.engine.getChannelMode(ch));
        TEST_ASSERT_EQUAL_UINT8(0, r.engine.getOffset(ch));
        TEST_ASSERT_EQUAL_UINT8(0, r.engine.getSkipChance(ch));
    }
}

void test_the_first_boot_seeds_the_bank_in_ram_and_writes_the_eeprom_after_the_quiet_delay() {
    journal.reset();
    RigV4 r;
    const bool restored = flexseq::bootstrap(journal, r.image, r.scheduler, 0);

    TEST_ASSERT_FALSE_MESSAGE(restored, "a blank EEPROM cannot be restored");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, journal.count,
        "the boot must write no byte: the scan does, after the quiet delay");
    TEST_ASSERT_TRUE(r.scheduler.isDirty());
    TEST_ASSERT_EQUAL_UINT64(0x9111, bankStepMask(*r.engine.patternForChannel(0)));
    for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
        TEST_ASSERT_EQUAL_INT8(0, r.engine.getSelectedPattern(channel));
    }

    TEST_ASSERT_FALSE(r.scheduler.advance(journal, r.image, persist::QUIET_MS - 1));
    TEST_ASSERT_EQUAL_UINT16(0, journal.count);
    while (r.scheduler.advance(journal, r.image, persist::QUIET_MS)) {
    }
    TEST_ASSERT_EQUAL_UINT16(434, journal.count);
    TEST_ASSERT_EQUAL_UINT8(4, journal.cell[384]);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x11, journal.cell[385], "the low byte of the A1 mask");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x91, journal.cell[386], "the high byte of the A1 mask");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, journal.cell[385 + 8 * 23], "B1 is empty");
    TEST_ASSERT_EQUAL_UINT16(384, journal.order[journal.count - 1]);
}

void test_a_nominal_boot_restores_the_bank_and_writes_nothing() {
    eeprom.reset();
    RigV4 saved;
    saved.image.resetToDefaults();
    saved.bank.getPattern(3)->writeStep(30, true);
    saved.bank.getPattern(0)->writeStep(2, true);
    saved.engine.setSelectedPattern(2, 3);
    saved.scheduler.markDirty(0);
    finishWriteV4(saved, eeprom, persist::QUIET_MS);
    const uint16_t written = eeprom.writes;

    RigV4 loaded;
    const bool restored = flexseq::bootstrap(eeprom, loaded.image, loaded.scheduler, 0);
    TEST_ASSERT_TRUE_MESSAGE(restored, "a valid version 4 image must be restored");
    TEST_ASSERT_FALSE(loaded.scheduler.isDirty());
    TEST_ASSERT_EQUAL_UINT16(written, eeprom.writes);
    TEST_ASSERT_EQUAL_UINT64(0x9111 | (static_cast<uint64_t>(1) << 2),
                             bankStepMask(*loaded.bank.getPattern(0)));
    TEST_ASSERT_EQUAL_UINT64(0xCCCC | (static_cast<uint64_t>(1) << 30),
                             bankStepMask(*loaded.bank.getPattern(3)));
    TEST_ASSERT_EQUAL_INT8(3, loaded.engine.getSelectedPattern(2));
    TEST_ASSERT_EQUAL_PTR(loaded.bank.getPattern(3), loaded.engine.patternForChannel(2));
}

void test_a_valid_version_two_image_is_refused_without_migration() {
    eeprom.reset();
    Rig legacy;
    legacy.scheduler.markDirty(0);
    finishWrite(legacy, eeprom, persist::QUIET_MS);
    TEST_ASSERT_EQUAL_UINT8(2, eeprom.cell[384]);

    RigV4 r;
    TEST_ASSERT_FALSE(flexseq::bootstrap(eeprom, r.image, r.scheduler, 0));
    TEST_ASSERT_EQUAL_UINT64(0x9111, bankStepMask(*r.bank.getPattern(0)));
}

void test_a_valid_version_three_image_is_refused_without_migration() {
    eeprom.reset();
    for (uint16_t address = 384; address <= 971; ++address) {
        eeprom.cell[address] = 0;
    }
    eeprom.cell[384] = 3;
    eeprom.cell[385] = 0x11;
    eeprom.cell[386] = 0x91;

    RigV4 r;
    r.bank.getPattern(0)->writeStep(20, true);
    TEST_ASSERT_FALSE_MESSAGE(flexseq::bootstrap(eeprom, r.image, r.scheduler, 0),
        "no migration from version 3: the defaults are taken");
    TEST_ASSERT_EQUAL_UINT64(0x9111, bankStepMask(*r.bank.getPattern(0)));
    TEST_ASSERT_TRUE(r.scheduler.isDirty());
}

void test_every_unknown_version_byte_falls_back_to_the_defaults() {
    const uint8_t versions[3] = { 0xFF, 1, 5 };
    for (uint8_t index = 0; index < 3; ++index) {
        eeprom.reset();
        eeprom.cell[384] = versions[index];
        RigV4 r;
        r.bank.getPattern(0)->writeStep(20, true);
        TEST_ASSERT_FALSE(flexseq::bootstrap(eeprom, r.image, r.scheduler, 0));
        TEST_ASSERT_EQUAL_UINT64(0x9111, bankStepMask(*r.bank.getPattern(0)));
    }
}

void test_one_edited_step_of_a_shared_pattern_costs_one_write() {
    eeprom.reset();
    RigV4 r;
    r.image.resetToDefaults();
    r.scheduler.markDirty(0);
    finishWriteV4(r, eeprom, persist::QUIET_MS);
    const uint16_t before = eeprom.writes;

    r.bank.getPattern(9)->writeStep(3, true);
    r.scheduler.markDirty(1000);
    finishWriteV4(r, eeprom, 1000 + persist::QUIET_MS);
    TEST_ASSERT_EQUAL_UINT16(1, eeprom.writes - before);
    TEST_ASSERT_EQUAL_UINT8(0x08, eeprom.cell[385 + 9 * 23]);
}

void test_the_v4_write_never_leaves_its_zone() {
    eeprom.reset();
    RigV4 r;
    fillDistinctStateV4(r);
    r.scheduler.markDirty(0);
    finishWriteV4(r, eeprom, persist::QUIET_MS);
    TEST_ASSERT_TRUE(eeprom.lowestWrite >= 384);
    TEST_ASSERT_TRUE(eeprom.highestWrite <= 817);
    for (uint16_t address = 0; address < 384; ++address) {
        TEST_ASSERT_EQUAL_UINT8(SENTINEL, eeprom.cell[address]);
    }
    for (uint16_t address = 818; address < persist::EEPROM_SIZE; ++address) {
        TEST_ASSERT_EQUAL_UINT8(SENTINEL, eeprom.cell[address]);
    }
}

void test_a_v4_power_cut_in_the_middle_of_a_write_is_caught_by_the_version_byte() {
    eeprom.reset();
    RigV4 r;
    fillDistinctStateV4(r);
    r.scheduler.markDirty(0);
    for (uint8_t i = 0; i < 100; ++i) {
        TEST_ASSERT_TRUE(r.scheduler.advance(eeprom, r.image, persist::QUIET_MS));
    }
    TEST_ASSERT_NOT_EQUAL(4, eeprom.cell[384]);

    RigV4 loaded;
    TEST_ASSERT_FALSE(loaded.scheduler.load(eeprom, loaded.image));
    TEST_ASSERT_EQUAL_UINT64(0x9111, bankStepMask(*loaded.bank.getPattern(0)));
}

void test_an_image_without_a_bank_reads_zero_and_applies_nothing() {
    SequencerEngine engine;
    Transport transport(engine);
    UiController ui(engine, transport);
    Preferences prefs;
    PersistentImageV4 image(engine, ui, prefs);
    for (uint16_t index = 0; index < 368; ++index) {
        TEST_ASSERT_EQUAL_UINT8(0, image.byteAt(index));
    }
    image.applyByte(0, 0xFF);
    image.resetToDefaults();
    TEST_ASSERT_EQUAL_UINT8(0, image.byteAt(0));
    TEST_ASSERT_EQUAL_UINT8(4, image.byteAt(433));
}

void test_a_v4_round_trip_never_restores_a_hysteresis_zone() {
    eeprom.reset();
    RigV4 saved;
    saved.engine.setChannelMode(0, flexseq::MODE_SEQ);
    saved.engine.setCvDestination(0, flexseq::CV_SOURCE_1, flexseq::CV_DEST_LENGTH);
    saved.engine.setCvInput(flexseq::CV_SOURCE_1, 330);
    saved.engine.start();
    saved.engine.advance(96);
    TEST_ASSERT_EQUAL_UINT8(26, saved.engine.getEffectiveLength(0));
    saved.scheduler.markDirty(0);
    finishWriteV4(saved, eeprom, persist::QUIET_MS);

    RigV4 loaded;
    TEST_ASSERT_TRUE(loaded.scheduler.load(eeprom, loaded.image));
    TEST_ASSERT_EQUAL_UINT8(flexseq::CV_DEST_LENGTH,
                            loaded.engine.getCvDestination(0, flexseq::CV_SOURCE_1));
    TEST_ASSERT_EQUAL_UINT8(16, loaded.engine.getBaseLength(0));
    TEST_ASSERT_EQUAL_UINT8(16, loaded.engine.getEffectiveLength(0));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_the_layout_is_the_one_the_prd_fixed);
    RUN_TEST(test_the_format_version_is_two);
    RUN_TEST(test_the_image_ends_below_the_original_memcode);
    RUN_TEST(test_the_pattern_keeps_its_twenty_three_bytes);
    RUN_TEST(test_a_raw_step_byte_refuses_an_index_past_the_pattern);
    RUN_TEST(test_a_raw_ratchet_byte_refuses_an_index_past_the_pattern);
    RUN_TEST(test_a_raw_step_byte_stores_what_it_is_given_without_canonicalising);
    RUN_TEST(test_the_v4_content_codec_round_trips_the_twenty_three_bytes);
    RUN_TEST(test_the_v4_codec_drops_the_four_bits_above_the_last_step_on_load);
    RUN_TEST(test_the_v4_codec_drops_the_four_bits_above_the_last_step_on_emit);
    RUN_TEST(test_the_v4_codec_leaves_the_other_step_bytes_whole);
    RUN_TEST(test_the_v4_codec_normalises_an_invalid_ratchet_nibble);
    RUN_TEST(test_an_invalid_v4_nibble_replaces_a_previous_ratchet_instead_of_keeping_it);
    RUN_TEST(test_the_v4_codec_ignores_an_offset_past_the_record);
    RUN_TEST(test_a_round_trip_restores_the_cv_destinations);
    RUN_TEST(test_a_round_trip_never_restores_a_hysteresis_zone);
    RUN_TEST(test_a_round_trip_keeps_a_base_length_of_thirty_six);
    RUN_TEST(test_a_round_trip_restores_the_state_byte_for_byte);
    RUN_TEST(test_the_patterns_survive_with_their_ratchets);
    RUN_TEST(test_a_wrong_version_byte_returns_the_defaults);
    RUN_TEST(test_a_blank_eeprom_returns_the_defaults);
    RUN_TEST(test_the_v4_defaults_clear_the_cv_routing_and_the_three_settings);
    RUN_TEST(test_the_version_byte_is_written_first_and_kept);
    RUN_TEST(test_no_byte_below_384_or_above_687_is_ever_touched);
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
    RUN_TEST(test_the_channel_record_carries_the_mode_the_offset_and_the_skip_chance);
    RUN_TEST(test_the_three_new_fields_sit_at_their_fixed_place_in_the_record);
    RUN_TEST(test_a_version_one_image_returns_the_defaults);
    RUN_TEST(test_a_bad_mode_byte_is_refused_while_the_next_record_still_loads);
    RUN_TEST(test_a_bad_skip_chance_byte_is_refused_while_the_next_record_still_loads);
    RUN_TEST(test_the_offset_never_exceeds_the_single_byte_the_format_gives_it);
    RUN_TEST(test_the_two_cv_target_bytes_are_reserved_and_read_as_zero);
    RUN_TEST(test_a_stored_cv_target_is_ignored_without_disturbing_the_record);
    RUN_TEST(test_the_v2_scan_still_writes_the_version_first);
    RUN_TEST(test_the_v2_mapping_is_the_identity_the_scheduler_used_to_inline);
    RUN_TEST(test_the_v2_version_sits_at_the_first_logical_index);

    RUN_TEST(test_the_v4_layout_is_the_one_the_prd_fixed);
    RUN_TEST(test_the_v4_records_carry_thirty_six_steps_and_no_length);
    RUN_TEST(test_the_v4_image_is_scanned_whole_and_ends_on_its_version);
    RUN_TEST(test_the_v4_image_leaves_the_original_memcode_alone);
    RUN_TEST(test_the_v4_version_holds_the_last_logical_index_at_the_first_address);
    RUN_TEST(test_the_v4_mapping_covers_the_format_exactly_once);
    RUN_TEST(test_the_v4_scan_writes_the_whole_image_and_the_version_last);
    RUN_TEST(test_the_v4_global_zone_reads_mod_and_range_as_zero_and_normalises_them);
    RUN_TEST(test_a_v4_round_trip_restores_the_bank_and_the_channels_byte_for_byte);
    RUN_TEST(test_the_v4_defaults_seed_the_factory_bank_and_clear_the_free_slots);
    RUN_TEST(test_the_v4_defaults_reset_the_mode_the_offset_and_the_skip_chance);
    RUN_TEST(test_the_first_boot_seeds_the_bank_in_ram_and_writes_the_eeprom_after_the_quiet_delay);
    RUN_TEST(test_a_nominal_boot_restores_the_bank_and_writes_nothing);
    RUN_TEST(test_a_valid_version_two_image_is_refused_without_migration);
    RUN_TEST(test_a_valid_version_three_image_is_refused_without_migration);
    RUN_TEST(test_every_unknown_version_byte_falls_back_to_the_defaults);
    RUN_TEST(test_one_edited_step_of_a_shared_pattern_costs_one_write);
    RUN_TEST(test_the_v4_write_never_leaves_its_zone);
    RUN_TEST(test_a_v4_power_cut_in_the_middle_of_a_write_is_caught_by_the_version_byte);
    RUN_TEST(test_an_image_without_a_bank_reads_zero_and_applies_nothing);
    RUN_TEST(test_a_v4_round_trip_never_restores_a_hysteresis_zone);
    return UNITY_END();
}
