#include <stdint.h>
#include <string.h>
#include <unity.h>

#include <flexseq/Persistence.h>

using flexseq::PatternBank;
using flexseq::Pattern;
using flexseq::PersistenceScheduler;
using flexseq::PersistentImage;
using flexseq::PersistentImageV3;
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
 * Le format cible, version 3 — declare, pas encore en service
 */

void test_the_v3_layout_is_the_one_the_prd_fixed() {
    TEST_ASSERT_EQUAL_UINT16(1, persist::v3::HEADER_SIZE);
    TEST_ASSERT_EQUAL_UINT16(1, persist::v3::TEMPLATES_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(384, persist::v3::TEMPLATES_SIZE);
    TEST_ASSERT_EQUAL_UINT16(385, persist::v3::INSTANCES_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(138, persist::v3::INSTANCES_SIZE);
    TEST_ASSERT_EQUAL_UINT16(523, persist::v3::CHANNELS_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(54, persist::v3::CHANNELS_SIZE);
    TEST_ASSERT_EQUAL_UINT16(577, persist::v3::GLOBAL_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(5, persist::v3::GLOBAL_SIZE);
    TEST_ASSERT_EQUAL_UINT16(582, persist::v3::PREFS_OFFSET);
    TEST_ASSERT_EQUAL_UINT16(6, persist::v3::PREFS_SIZE);
    TEST_ASSERT_EQUAL_UINT16(588, persist::v3::TOTAL_SIZE);
    TEST_ASSERT_EQUAL_UINT16(971, persist::v3::LAST_ADDRESS);
}

void test_the_v3_format_version_is_three() {
    TEST_ASSERT_EQUAL_UINT8(3, persist::v3::FORMAT_VERSION);
    TEST_ASSERT_EQUAL_UINT8(2, persist::FORMAT_VERSION);
}

void test_the_v3_records_carry_thirty_six_steps() {
    TEST_ASSERT_EQUAL_UINT8(5, persist::v3::STEP_BYTES);
    TEST_ASSERT_EQUAL_UINT8(18, persist::v3::RATCHET_BYTES);
    TEST_ASSERT_EQUAL_UINT8(23, persist::v3::CONTENT_BYTES);
    TEST_ASSERT_EQUAL_UINT8(1, persist::v3::LENGTH_BYTES);
    TEST_ASSERT_EQUAL_UINT8(24, persist::v3::TEMPLATE_RECORD);
    TEST_ASSERT_EQUAL_UINT8(23, persist::v3::INSTANCE_RECORD);
    TEST_ASSERT_EQUAL_UINT8(16, persist::v3::TEMPLATE_COUNT);
    TEST_ASSERT_EQUAL_UINT8(6, persist::v3::INSTANCE_COUNT);
    TEST_ASSERT_EQUAL_UINT8(9, persist::v3::CHANNEL_RECORD);
    TEST_ASSERT_EQUAL_UINT8(0, persist::v3::RECORD_STEPS_AT);
    TEST_ASSERT_EQUAL_UINT8(5, persist::v3::RECORD_RATCHETS_AT);
    TEST_ASSERT_EQUAL_UINT8(23, persist::v3::RECORD_LENGTH_AT);
}

void test_the_v3_global_zone_reserves_mod_and_range() {
    TEST_ASSERT_EQUAL_UINT8(0, persist::v3::GLOBAL_TEMPO_LO_AT);
    TEST_ASSERT_EQUAL_UINT8(1, persist::v3::GLOBAL_TEMPO_HI_AT);
    TEST_ASSERT_EQUAL_UINT8(2, persist::v3::GLOBAL_CLOCK_SOURCE_AT);
    TEST_ASSERT_EQUAL_UINT8(3, persist::v3::GLOBAL_MOD_AT);
    TEST_ASSERT_EQUAL_UINT8(4, persist::v3::GLOBAL_RANGE_AT);
}

void test_the_v3_image_leaves_the_original_memcode_alone() {
    TEST_ASSERT_TRUE(persist::BASE_ADDRESS + persist::v3::TOTAL_SIZE <= 1023);
    TEST_ASSERT_EQUAL_UINT16(51, 1022 - persist::v3::LAST_ADDRESS);
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

/*
 * Le codec de contenu de la version 3
 */

void test_the_v3_content_codec_round_trips_the_twenty_three_bytes() {
    Pattern source;
    const uint8_t activeSteps[] = {0, 7, 8, 23, 31, 32, 35};
    for (uint8_t i = 0; i < sizeof(activeSteps); ++i) {
        source.writeStep(activeSteps[i], true);
    }
    source.setRatchet(0, flexseq::RATCHET_3);
    source.setRatchet(1, flexseq::RATCHET_6);
    source.setRatchet(34, flexseq::RATCHET_TRIPLET);
    source.setRatchet(35, flexseq::RATCHET_4);

    uint8_t image[persist::v3::CONTENT_BYTES];
    for (uint8_t offset = 0; offset < persist::v3::CONTENT_BYTES; ++offset) {
        image[offset] = persist::v3::contentByte(source, offset);
    }

    Pattern loaded;
    for (uint8_t offset = 0; offset < persist::v3::CONTENT_BYTES; ++offset) {
        persist::v3::applyContentByte(loaded, offset, image[offset]);
    }

    for (uint8_t step = 0; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
        bool wanted = false;
        bool got = false;
        TEST_ASSERT_TRUE(source.readStep(step, wanted));
        TEST_ASSERT_TRUE(loaded.readStep(step, got));
        TEST_ASSERT_EQUAL_INT(wanted, got);
        TEST_ASSERT_EQUAL_UINT8(source.getRatchet(step), loaded.getRatchet(step));
    }

    for (uint8_t offset = 0; offset < persist::v3::CONTENT_BYTES; ++offset) {
        TEST_ASSERT_EQUAL_UINT8(image[offset], persist::v3::contentByte(loaded, offset));
    }
}

void test_the_v3_codec_drops_the_four_bits_above_the_last_step_on_load() {
    Pattern pattern;
    persist::v3::applyContentByte(pattern, 4, 0xFF);

    TEST_ASSERT_EQUAL_UINT8(0x0F, pattern.stepByte(4));

    for (uint8_t step = 32; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
        bool active = false;
        TEST_ASSERT_TRUE(pattern.readStep(step, active));
        TEST_ASSERT_TRUE(active);
    }
}

void test_the_v3_codec_drops_the_four_bits_above_the_last_step_on_emit() {
    Pattern pattern;
    pattern.setStepByte(4, 0xF0);
    TEST_ASSERT_EQUAL_UINT8(0, persist::v3::contentByte(pattern, 4));

    pattern.setStepByte(4, 0xF5);
    TEST_ASSERT_EQUAL_UINT8(0x05, persist::v3::contentByte(pattern, 4));
}

void test_the_v3_codec_leaves_the_other_step_bytes_whole() {
    Pattern pattern;
    for (uint8_t offset = 0; offset < 4; ++offset) {
        persist::v3::applyContentByte(pattern, offset, 0xFF);
        TEST_ASSERT_EQUAL_UINT8(0xFF, pattern.stepByte(offset));
        TEST_ASSERT_EQUAL_UINT8(0xFF, persist::v3::contentByte(pattern, offset));
    }
}

void test_the_v3_codec_normalises_an_invalid_ratchet_nibble() {
    Pattern pattern;
    persist::v3::applyContentByte(pattern, 5, 0x53);

    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_3, pattern.getRatchet(0));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(1));
    TEST_ASSERT_EQUAL_UINT8(0x03, persist::v3::contentByte(pattern, 5));
}

void test_an_invalid_v3_nibble_replaces_a_previous_ratchet_instead_of_keeping_it() {
    Pattern pattern;
    persist::v3::applyContentByte(pattern, 5, 0x36);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_6, pattern.getRatchet(0));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_3, pattern.getRatchet(1));

    persist::v3::applyContentByte(pattern, 5, 0x51);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(0));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE, pattern.getRatchet(1));
    TEST_ASSERT_EQUAL_UINT8(0, persist::v3::contentByte(pattern, 5));
}

void test_the_v3_codec_ignores_an_offset_past_the_record() {
    Pattern pattern;
    for (uint8_t offset = 0; offset < persist::v3::CONTENT_BYTES; ++offset) {
        persist::v3::applyContentByte(pattern, offset, static_cast<uint8_t>(0x11 * offset));
    }

    uint8_t before[persist::v3::CONTENT_BYTES];
    for (uint8_t offset = 0; offset < persist::v3::CONTENT_BYTES; ++offset) {
        before[offset] = persist::v3::contentByte(pattern, offset);
    }

    persist::v3::applyContentByte(pattern, persist::v3::CONTENT_BYTES, 0xFF);
    persist::v3::applyContentByte(pattern, 200, 0xFF);

    for (uint8_t offset = 0; offset < persist::v3::CONTENT_BYTES; ++offset) {
        TEST_ASSERT_EQUAL_UINT8(before[offset], persist::v3::contentByte(pattern, offset));
    }

    TEST_ASSERT_EQUAL_UINT8(0, persist::v3::contentByte(pattern, persist::v3::CONTENT_BYTES));
    TEST_ASSERT_EQUAL_UINT8(0, persist::v3::contentByte(pattern, 200));
}

/*
 * Le record de modele de la version 3 — 23 octets de contenu, plus la longueur
 */

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

void test_the_v3_template_record_round_trips_its_twenty_four_bytes() {
    Pattern source;
    fillDistinctPattern(source);
    const uint8_t sourceLength = 20;

    uint8_t image[persist::v3::TEMPLATE_RECORD];
    for (uint8_t offset = 0; offset < persist::v3::TEMPLATE_RECORD; ++offset) {
        image[offset] = persist::v3::templateByte(source, sourceLength, offset);
    }

    Pattern loaded;
    uint8_t loadedLength = 1;
    for (uint8_t offset = 0; offset < persist::v3::TEMPLATE_RECORD; ++offset) {
        TEST_ASSERT_TRUE(persist::v3::applyTemplateByte(loaded, loadedLength, offset,
                                                        image[offset]));
    }

    TEST_ASSERT_EQUAL_UINT8(20, loadedLength);
    for (uint8_t step = 0; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
        bool wanted = false;
        bool got = false;
        TEST_ASSERT_TRUE(source.readStep(step, wanted));
        TEST_ASSERT_TRUE(loaded.readStep(step, got));
        TEST_ASSERT_EQUAL_INT(wanted, got);
        TEST_ASSERT_EQUAL_UINT8(source.getRatchet(step), loaded.getRatchet(step));
    }
    for (uint8_t offset = 0; offset < persist::v3::TEMPLATE_RECORD; ++offset) {
        TEST_ASSERT_EQUAL_UINT8(image[offset],
                                persist::v3::templateByte(loaded, loadedLength, offset));
    }
}

void test_the_v3_template_record_accepts_every_length_in_range() {
    const uint8_t wanted[] = {1, 16, 24, 35, 36};
    for (uint8_t i = 0; i < sizeof(wanted); ++i) {
        Pattern pattern;
        uint8_t length = 8;
        TEST_ASSERT_TRUE(persist::v3::applyTemplateByte(pattern, length, 23, wanted[i]));
        TEST_ASSERT_EQUAL_UINT8(wanted[i], length);
        TEST_ASSERT_EQUAL_UINT8(wanted[i], persist::v3::templateByte(pattern, length, 23));
    }
}

void test_the_v3_template_record_refuses_a_length_out_of_range() {
    const uint8_t refused[] = {0, 37, 255};
    for (uint8_t i = 0; i < sizeof(refused); ++i) {
        Pattern pattern;
        uint8_t length = 12;
        TEST_ASSERT_FALSE(persist::v3::applyTemplateByte(pattern, length, 23, refused[i]));
        TEST_ASSERT_EQUAL_UINT8(12, length);
    }
}

void test_a_refused_length_leaves_the_loaded_content_intact() {
    Pattern source;
    fillDistinctPattern(source);

    Pattern loaded;
    uint8_t length = 12;
    for (uint8_t offset = 0; offset < persist::v3::CONTENT_BYTES; ++offset) {
        TEST_ASSERT_TRUE(persist::v3::applyTemplateByte(
            loaded, length, offset, persist::v3::contentByte(source, offset)));
    }

    TEST_ASSERT_FALSE(persist::v3::applyTemplateByte(loaded, length, 23, 0));
    TEST_ASSERT_EQUAL_UINT8(12, length);

    for (uint8_t offset = 0; offset < persist::v3::CONTENT_BYTES; ++offset) {
        TEST_ASSERT_EQUAL_UINT8(persist::v3::contentByte(source, offset),
                                persist::v3::contentByte(loaded, offset));
    }
}

void test_the_v3_template_record_clamps_the_length_it_emits() {
    Pattern pattern;
    TEST_ASSERT_EQUAL_UINT8(1, persist::v3::templateByte(pattern, 0, 23));
    TEST_ASSERT_EQUAL_UINT8(36, persist::v3::templateByte(pattern, 37, 23));
    TEST_ASSERT_EQUAL_UINT8(36, persist::v3::templateByte(pattern, 255, 23));
    TEST_ASSERT_EQUAL_UINT8(1, persist::v3::templateByte(pattern, 1, 23));
    TEST_ASSERT_EQUAL_UINT8(36, persist::v3::templateByte(pattern, 36, 23));
    TEST_ASSERT_EQUAL_UINT8(20, persist::v3::templateByte(pattern, 20, 23));
}

void test_the_length_byte_touches_no_content_byte() {
    Pattern pattern;
    fillDistinctPattern(pattern);

    for (uint8_t offset = 0; offset < persist::v3::CONTENT_BYTES; ++offset) {
        TEST_ASSERT_EQUAL_UINT8(persist::v3::templateByte(pattern, 4, offset),
                                persist::v3::templateByte(pattern, 33, offset));
        TEST_ASSERT_EQUAL_UINT8(persist::v3::contentByte(pattern, offset),
                                persist::v3::templateByte(pattern, 4, offset));
    }
    TEST_ASSERT_EQUAL_UINT8(4, persist::v3::templateByte(pattern, 4, 23));
    TEST_ASSERT_EQUAL_UINT8(33, persist::v3::templateByte(pattern, 33, 23));
}

void test_the_v3_instance_record_carries_no_length() {
    Pattern source;
    fillDistinctPattern(source);

    Pattern loaded;
    for (uint8_t offset = 0; offset < persist::v3::INSTANCE_RECORD; ++offset) {
        persist::v3::applyContentByte(loaded, offset, persist::v3::contentByte(source, offset));
    }

    for (uint8_t step = 0; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
        bool wanted = false;
        bool got = false;
        TEST_ASSERT_TRUE(source.readStep(step, wanted));
        TEST_ASSERT_TRUE(loaded.readStep(step, got));
        TEST_ASSERT_EQUAL_INT(wanted, got);
        TEST_ASSERT_EQUAL_UINT8(source.getRatchet(step), loaded.getRatchet(step));
    }
}

void test_the_v3_length_bound_is_the_pattern_capacity_not_the_engine_cap() {
    TEST_ASSERT_EQUAL_UINT8(1, persist::v3::MIN_TEMPLATE_LENGTH);
    TEST_ASSERT_EQUAL_UINT8(36, persist::v3::MAX_TEMPLATE_LENGTH);
    TEST_ASSERT_EQUAL_UINT8(36, SequencerEngine::MAX_LENGTH);

    Pattern pattern;
    uint8_t length = 1;
    TEST_ASSERT_TRUE(persist::v3::applyTemplateByte(pattern, length, 23, 36));
    TEST_ASSERT_EQUAL_UINT8(36, length);
}

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

struct RigV3 {
    SequencerEngine engine;
    Transport transport;
    UiController ui;
    Preferences prefs;
    PersistentImageV3 image;
    PersistenceScheduler scheduler;

    RigV3() : engine(), transport(engine), ui(engine, transport), prefs(),
              image(engine, ui, prefs) {}
};

uint16_t finishWriteV3(RigV3& r, FakeEeprom& ee, uint32_t nowMs) {
    uint16_t calls = 0;
    while (r.scheduler.advance(ee, r.image, nowMs)) {
        ++calls;
    }
    return calls;
}

void writeTemplateRecord(FakeEeprom& ee, uint8_t index, const Pattern& content,
                         uint8_t length) {
    for (uint8_t offset = 0; offset < persist::v3::TEMPLATE_RECORD; ++offset) {
        ee.write(persist::v3::templateAddress(index, offset),
                 persist::v3::templateByte(content, length, offset));
    }
}

Pattern distinctContent(uint8_t seed) {
    Pattern p;
    p.writeStep(seed % Pattern::DEFAULT_TOTAL_STEPS, true);
    p.writeStep((seed + 7) % Pattern::DEFAULT_TOTAL_STEPS, true);
    p.setRatchet(seed % Pattern::DEFAULT_TOTAL_STEPS, flexseq::RATCHET_3);
    return p;
}

bool sameContent(const Pattern& a, const Pattern& b) {
    for (uint8_t i = 0; i < Pattern::DEFAULT_TOTAL_STEPS; ++i) {
        bool left = false;
        bool right = false;
        a.readStep(i, left);
        b.readStep(i, right);
        if (left != right) return false;
        if (a.getRatchet(i) != b.getRatchet(i)) return false;
    }
    return true;
}

void test_an_all_zero_template_is_empty() {
    FakeEeprom ee;
    RigV3 r;
    Pattern silent;
    writeTemplateRecord(ee, 10, silent, 16);
    TEST_ASSERT_TRUE(r.image.isTemplateEmpty(ee, 10));
}

void test_one_active_step_makes_a_template_occupied() {
    FakeEeprom ee;
    RigV3 r;
    for (uint8_t step = 0; step < Pattern::DEFAULT_TOTAL_STEPS; ++step) {
        Pattern one;
        one.writeStep(step, true);
        writeTemplateRecord(ee, 10, one, 16);
        TEST_ASSERT_FALSE(r.image.isTemplateEmpty(ee, 10));
    }
}

void test_the_thirty_sixth_step_alone_makes_a_template_occupied() {
    FakeEeprom ee;
    RigV3 r;
    Pattern last;
    last.writeStep(35, true);
    writeTemplateRecord(ee, 10, last, 16);
    TEST_ASSERT_FALSE(r.image.isTemplateEmpty(ee, 10));
}

void test_ratchets_alone_leave_a_template_empty() {
    FakeEeprom ee;
    RigV3 r;
    Pattern silent;
    writeTemplateRecord(ee, 10, silent, 16);
    for (uint8_t offset = 0; offset < persist::v3::RATCHET_BYTES; ++offset) {
        ee.write(persist::v3::templateAddress(
                     10, static_cast<uint8_t>(persist::v3::RECORD_RATCHETS_AT + offset)),
                 0x66);
    }
    TEST_ASSERT_TRUE(r.image.isTemplateEmpty(ee, 10));
}

void test_the_four_bits_above_the_last_step_do_not_occupy_a_template() {
    FakeEeprom ee;
    RigV3 r;
    Pattern silent;
    writeTemplateRecord(ee, 10, silent, 16);
    ee.write(persist::v3::templateAddress(
                 10, static_cast<uint8_t>(persist::v3::RECORD_STEPS_AT
                                          + persist::v3::STEP_BYTES - 1)),
             0xF0);
    TEST_ASSERT_TRUE(r.image.isTemplateEmpty(ee, 10));
}

void test_an_empty_query_reads_five_bytes_and_writes_none() {
    FakeEeprom ee;
    RigV3 r;
    Pattern silent;
    writeTemplateRecord(ee, 10, silent, 16);
    const uint16_t before = ee.writes;
    TEST_ASSERT_TRUE(r.image.isTemplateEmpty(ee, 10));
    TEST_ASSERT_EQUAL_UINT16(before, ee.writes);
}

void test_an_out_of_range_index_is_not_empty() {
    FakeEeprom ee;
    RigV3 r;
    TEST_ASSERT_FALSE(r.image.isTemplateEmpty(ee, 16));
}

void test_a_template_request_arms_and_reports_itself() {
    FakeEeprom ee;
    RigV3 r;
    TEST_ASSERT_FALSE(r.scheduler.isWritingTemplate());
    TEST_ASSERT_TRUE(r.scheduler.requestTemplateWrite(r.image, 0, 9));
    TEST_ASSERT_TRUE(r.scheduler.isWritingTemplate());
}

void test_a_template_request_writes_one_byte_per_advance() {
    FakeEeprom ee;
    RigV3 r;
    TEST_ASSERT_TRUE(r.scheduler.requestTemplateWrite(r.image, 0, 9));
    for (uint8_t written = 0; written < persist::v3::TEMPLATE_RECORD; ++written) {
        const uint16_t before = ee.writes;
        TEST_ASSERT_TRUE(r.scheduler.advance(ee, r.image, 0));
        TEST_ASSERT_EQUAL_UINT16(before + 1, ee.writes);
    }
}

void test_a_template_request_ends_after_its_record() {
    FakeEeprom ee;
    RigV3 r;
    const Pattern wanted = distinctContent(4);
    *r.engine.instanceForChannel(0) = wanted;
    TEST_ASSERT_TRUE(r.engine.setBaseLengthFromStorage(0, 21));
    TEST_ASSERT_TRUE(r.scheduler.requestTemplateWrite(r.image, 0, 11));
    for (uint8_t i = 0; i < persist::v3::TEMPLATE_RECORD; ++i) {
        r.scheduler.advance(ee, r.image, 0);
    }
    TEST_ASSERT_FALSE(r.scheduler.isWritingTemplate());
    for (uint8_t offset = 0; offset < persist::v3::CONTENT_BYTES; ++offset) {
        TEST_ASSERT_EQUAL_UINT8(persist::v3::contentByte(wanted, offset),
                                ee.read(persist::v3::templateAddress(11, offset)));
    }
    TEST_ASSERT_EQUAL_UINT8(
        21, ee.read(persist::v3::templateAddress(11, persist::v3::RECORD_LENGTH_AT)));
}

void test_a_template_request_on_a_frozen_slot_arms_nothing() {
    FakeEeprom ee;
    RigV3 r;
    for (uint8_t index = 0; index < 8; ++index) {
        TEST_ASSERT_FALSE(r.scheduler.requestTemplateWrite(r.image, 0, index));
    }
    TEST_ASSERT_FALSE(r.scheduler.isWritingTemplate());
}

void test_a_second_template_request_is_refused_while_one_is_in_flight() {
    FakeEeprom ee;
    RigV3 r;
    TEST_ASSERT_TRUE(r.scheduler.requestTemplateWrite(r.image, 0, 9));
    TEST_ASSERT_TRUE(r.scheduler.advance(ee, r.image, 0));
    TEST_ASSERT_FALSE(r.scheduler.requestTemplateWrite(r.image, 1, 10));
}

void test_the_template_request_goes_before_the_image_scan() {
    FakeEeprom ee;
    RigV3 r;
    r.scheduler.markDirty(0);
    TEST_ASSERT_TRUE(r.scheduler.requestTemplateWrite(r.image, 0, 9));
    TEST_ASSERT_TRUE(r.scheduler.advance(ee, r.image, persist::QUIET_MS));
    TEST_ASSERT_TRUE(ee.lowestWrite >= persist::v3::templateAddress(9, 0));
    TEST_ASSERT_TRUE(r.scheduler.isDirty());
}

void test_the_image_scan_resumes_after_the_template() {
    FakeEeprom ee;
    RigV3 r;
    r.scheduler.markDirty(0);
    TEST_ASSERT_TRUE(r.scheduler.requestTemplateWrite(r.image, 0, 9));
    for (uint8_t i = 0; i < persist::v3::TEMPLATE_RECORD; ++i) {
        r.scheduler.advance(ee, r.image, persist::QUIET_MS);
    }
    TEST_ASSERT_FALSE(r.scheduler.isWritingTemplate());
    TEST_ASSERT_TRUE(r.scheduler.advance(ee, r.image, persist::QUIET_MS));
    // The template zone ends at 768; the scanned image starts at 769.
    TEST_ASSERT_TRUE(ee.highestWrite >= 769);
    TEST_ASSERT_TRUE(r.scheduler.isDirty());
}

void test_no_advance_ever_writes_more_than_one_byte() {
    FakeEeprom ee;
    RigV3 r;
    r.scheduler.markDirty(0);
    TEST_ASSERT_TRUE(r.scheduler.requestTemplateWrite(r.image, 0, 13));
    for (uint16_t pass = 0; pass < 400; ++pass) {
        const uint16_t before = ee.writes;
        if (!r.scheduler.advance(ee, r.image, persist::QUIET_MS)) {
            break;
        }
        TEST_ASSERT_EQUAL_UINT16(before + 1, ee.writes);
    }
}

void test_save_template_writes_the_instance_content_into_the_record() {
    FakeEeprom ee;
    RigV3 r;
    const Pattern wanted = distinctContent(3);
    *r.engine.instanceForChannel(1) = wanted;
    TEST_ASSERT_TRUE(r.image.saveTemplate(ee, 1, 9));
    for (uint8_t offset = 0; offset < persist::v3::CONTENT_BYTES; ++offset) {
        TEST_ASSERT_EQUAL_UINT8(persist::v3::contentByte(wanted, offset),
                                ee.read(persist::v3::templateAddress(9, offset)));
    }
}

void test_save_template_writes_the_base_not_the_modulated_length() {
    FakeEeprom ee;
    RigV3 r;
    TEST_ASSERT_TRUE(r.engine.setChannelMode(1, flexseq::MODE_SEQ));
    TEST_ASSERT_TRUE(r.engine.setBaseLength(1, 18));
    TEST_ASSERT_TRUE(r.engine.setCvDestination(1, flexseq::CV_SOURCE_1,
                                               flexseq::CV_DEST_LENGTH));
    r.engine.setCvInput(flexseq::CV_SOURCE_1, 330);
    r.engine.start();
    r.engine.advance(96);
    TEST_ASSERT_EQUAL_UINT8(18, r.engine.getBaseLength(1));
    TEST_ASSERT_EQUAL_UINT8(28, r.engine.getEffectiveLength(1));

    TEST_ASSERT_TRUE(r.image.saveTemplate(ee, 1, 9));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        18, ee.read(persist::v3::templateAddress(9, persist::v3::RECORD_LENGTH_AT)),
        "ADR 0009 : SAVE ecrit la base, jamais la longueur modulee par le CV");
}

void test_save_template_writes_the_channel_length_into_the_record() {
    FakeEeprom ee;
    RigV3 r;
    TEST_ASSERT_TRUE(r.engine.setBaseLengthFromStorage(1, 36));
    TEST_ASSERT_EQUAL_UINT8(36, r.engine.getBaseLength(1));
    TEST_ASSERT_EQUAL_UINT8(36, r.engine.getEffectiveLength(1));
    TEST_ASSERT_TRUE(r.image.saveTemplate(ee, 1, 9));
    TEST_ASSERT_EQUAL_UINT8(
        36, ee.read(persist::v3::templateAddress(9, persist::v3::RECORD_LENGTH_AT)));
}

void test_save_template_refuses_the_eight_frozen_slots_without_writing() {
    FakeEeprom ee;
    RigV3 r;
    for (uint8_t index = 0; index < 8; ++index) {
        const uint16_t before = ee.writes;
        TEST_ASSERT_FALSE(r.image.saveTemplate(ee, 0, index));
        TEST_ASSERT_EQUAL_UINT16(before, ee.writes);
    }
}

void test_save_template_accepts_the_eight_writable_slots() {
    FakeEeprom ee;
    RigV3 r;
    for (uint8_t index = 8; index < 16; ++index) {
        TEST_ASSERT_TRUE(r.image.saveTemplate(ee, 0, index));
    }
}

void test_save_template_refuses_an_invalid_channel_or_index_without_writing() {
    FakeEeprom ee;
    RigV3 r;
    const uint16_t before = ee.writes;
    TEST_ASSERT_FALSE(r.image.saveTemplate(ee, 6, 9));
    TEST_ASSERT_FALSE(r.image.saveTemplate(ee, 0, 16));
    TEST_ASSERT_EQUAL_UINT16(before, ee.writes);
}

void test_save_then_load_returns_the_same_pattern_and_length() {
    FakeEeprom ee;
    RigV3 r;
    const Pattern wanted = distinctContent(6);
    *r.engine.instanceForChannel(2) = wanted;
    TEST_ASSERT_TRUE(r.engine.setBaseLengthFromStorage(2, 30));
    TEST_ASSERT_TRUE(r.image.saveTemplate(ee, 2, 12));

    TEST_ASSERT_TRUE(r.image.loadTemplate(ee, 5, 12));
    TEST_ASSERT_TRUE(sameContent(wanted, *r.engine.instanceForChannel(5)));
    TEST_ASSERT_EQUAL_UINT8(30, r.engine.getBaseLength(5));
    TEST_ASSERT_EQUAL_UINT8(30, r.engine.getEffectiveLength(5));
}

void test_load_template_copies_the_content_into_the_channel_instance() {
    FakeEeprom ee;
    RigV3 r;
    const Pattern wanted = distinctContent(5);
    writeTemplateRecord(ee, 5, wanted, 12);
    TEST_ASSERT_TRUE(r.image.loadTemplate(ee, 2, 5));
    TEST_ASSERT_TRUE(sameContent(wanted, *r.engine.instanceForChannel(2)));
}

void test_load_template_leaves_the_five_other_instances_untouched() {
    FakeEeprom ee;
    RigV3 r;
    writeTemplateRecord(ee, 5, distinctContent(5), 12);
    Pattern before;
    TEST_ASSERT_TRUE(r.image.loadTemplate(ee, 2, 5));
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        if (ch == 2) continue;
        TEST_ASSERT_TRUE(sameContent(before, *r.engine.instanceForChannel(ch)));
    }
}

void test_load_template_keeps_a_length_of_thirty_six_in_the_base() {
    FakeEeprom ee;
    RigV3 r;
    writeTemplateRecord(ee, 9, distinctContent(3), 36);
    TEST_ASSERT_TRUE(r.image.loadTemplate(ee, 0, 9));
    TEST_ASSERT_EQUAL_UINT8(36, r.engine.getBaseLength(0));
    TEST_ASSERT_EQUAL_UINT8(36, r.engine.getEffectiveLength(0));
    TEST_ASSERT_EQUAL_INT8(9, r.engine.getSelectedPattern(0));
}

void test_load_template_needs_no_clamp_below_the_ceiling() {
    FakeEeprom ee;
    RigV3 r;
    writeTemplateRecord(ee, 4, distinctContent(1), 12);
    TEST_ASSERT_TRUE(r.image.loadTemplate(ee, 1, 4));
    TEST_ASSERT_EQUAL_UINT8(12, r.engine.getBaseLength(1));
    TEST_ASSERT_EQUAL_UINT8(12, r.engine.getEffectiveLength(1));
}

void test_load_template_refuses_an_invalid_length_without_losing_the_content() {
    FakeEeprom ee;
    RigV3 r;
    const Pattern wanted = distinctContent(2);
    writeTemplateRecord(ee, 6, wanted, 20);
    ee.write(persist::v3::templateAddress(6, persist::v3::RECORD_LENGTH_AT), 37);
    TEST_ASSERT_TRUE(r.engine.setBaseLength(3, 8));
    TEST_ASSERT_TRUE(r.image.loadTemplate(ee, 3, 6));
    TEST_ASSERT_TRUE(sameContent(wanted, *r.engine.instanceForChannel(3)));
    TEST_ASSERT_EQUAL_UINT8(8, r.engine.getBaseLength(3));
}

void test_load_template_accepts_a_frozen_factory_slot() {
    FakeEeprom ee;
    RigV3 r;
    const Pattern wanted = distinctContent(8);
    writeTemplateRecord(ee, 0, wanted, 16);
    TEST_ASSERT_TRUE(r.image.loadTemplate(ee, 4, 0));
    TEST_ASSERT_TRUE(sameContent(wanted, *r.engine.instanceForChannel(4)));
    TEST_ASSERT_EQUAL_INT8(0, r.engine.getSelectedPattern(4));
}

void test_load_template_refuses_an_invalid_channel_or_index() {
    FakeEeprom ee;
    RigV3 r;
    writeTemplateRecord(ee, 1, distinctContent(4), 10);
    TEST_ASSERT_FALSE(r.image.loadTemplate(ee, 6, 1));
    TEST_ASSERT_FALSE(r.image.loadTemplate(ee, 0, 16));
    Pattern before;
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_TRUE(sameContent(before, *r.engine.instanceForChannel(ch)));
    }
}

void test_load_template_writes_nothing_to_the_eeprom() {
    FakeEeprom ee;
    RigV3 r;
    writeTemplateRecord(ee, 7, distinctContent(6), 18);
    const uint16_t before = ee.writes;
    TEST_ASSERT_TRUE(r.image.loadTemplate(ee, 5, 7));
    TEST_ASSERT_EQUAL_UINT16(before, ee.writes);
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

void test_the_v3_scan_visits_and_writes_the_two_hundred_and_four_logical_bytes() {
    journal.reset();
    RigV3 r;
    for (uint16_t index = 0; index < flexseq::PersistentImageV3::SIZE; ++index) {
        journal.cell[r.image.addressAt(index)] = static_cast<uint8_t>(~r.image.byteAt(index));
    }
    journal.count = 0;
    r.scheduler.markDirty(0);
    while (r.scheduler.advance(journal, r.image, persist::QUIET_MS)) {
    }
    TEST_ASSERT_FALSE(journal.overflowed);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(204, journal.count,
        "le parcours doit ecrire les 204 octets logiques quand tous different");
}

void test_the_v3_scan_touches_only_the_version_and_the_data_zone() {
    journal.reset();
    RigV3 r;
    for (uint16_t index = 0; index < flexseq::PersistentImageV3::SIZE; ++index) {
        journal.cell[r.image.addressAt(index)] = static_cast<uint8_t>(~r.image.byteAt(index));
    }
    journal.count = 0;
    r.scheduler.markDirty(0);
    while (r.scheduler.advance(journal, r.image, persist::QUIET_MS)) {
    }
    for (uint16_t i = 0; i < journal.count; ++i) {
        const uint16_t address = journal.order[i];
        const bool isVersion = (address == 384);
        const bool isData = (address >= 769 && address <= 971);
        TEST_ASSERT_TRUE_MESSAGE(isVersion || isData,
            "une ecriture est tombee hors de la version et de la zone de donnees");
        TEST_ASSERT_TRUE_MESSAGE(address < 385 || address > 768,
            "une ecriture est tombee dans la zone des templates");
        TEST_ASSERT_TRUE_MESSAGE(address < 972,
            "une ecriture a depasse la fin du format");
    }
}

void test_the_v3_version_byte_is_the_last_address_written() {
    journal.reset();
    RigV3 r;
    for (uint16_t index = 0; index < flexseq::PersistentImageV3::SIZE; ++index) {
        journal.cell[r.image.addressAt(index)] = static_cast<uint8_t>(~r.image.byteAt(index));
    }
    journal.count = 0;
    r.scheduler.markDirty(0);
    while (r.scheduler.advance(journal, r.image, persist::QUIET_MS)) {
    }
    TEST_ASSERT_EQUAL_UINT16(204, journal.count);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(384, journal.order[journal.count - 1],
        "la version doit etre le dernier octet ecrit");
    for (uint16_t i = 0; i + 1 < journal.count; ++i) {
        TEST_ASSERT_TRUE_MESSAGE(journal.order[i] >= 769,
            "un octet de donnees doit preceder la version");
    }
}

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

uint32_t instanceStepMask(const Pattern& pattern) {
    uint32_t mask = 0;
    for (uint8_t step = 0; step < 36; ++step) {
        bool active = false;
        pattern.readStep(step, active);
        if (active) {
            mask |= (static_cast<uint32_t>(1) << step);
        }
    }
    return mask;
}

void test_the_first_boot_seeds_the_templates_and_fills_the_instances_from_a1() {
    journal.reset();
    RigV3 r;
    const bool restored = flexseq::bootstrap(journal, r.image, r.scheduler, 0);

    TEST_ASSERT_FALSE_MESSAGE(restored, "une EEPROM vierge ne peut pas etre restauree");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x11, journal.cell[385],
        "l octet bas du masque de A1 doit etre seme");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x91, journal.cell[386],
        "l octet haut du masque de A1 doit etre seme");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(16, journal.cell[385 + 23],
        "la longueur du record de template d usine vaut 16");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(384, journal.count,
        "le semis ecrit les seize records de vingt-quatre octets");

    for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
        TEST_ASSERT_EQUAL_INT8(0, r.engine.getSelectedPattern(channel));
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0x9111,
            instanceStepMask(*r.engine.instanceForChannel(channel)),
            "le CONTENU de l instance doit etre celui de A1, pas seulement son index");
    }
}

void test_a_nominal_boot_restores_the_instances_and_never_overwrites_them() {
    eeprom.reset();
    RigV3 saved;
    for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
        saved.engine.instanceForChannel(channel)->writeStep(
            static_cast<uint8_t>(30 + channel), true);
    }
    saved.scheduler.markDirty(0);
    finishWriteV3(saved, eeprom, persist::QUIET_MS);

    RigV3 loaded;
    const bool restored = flexseq::bootstrap(eeprom, loaded.image, loaded.scheduler, 0);

    TEST_ASSERT_TRUE_MESSAGE(restored, "une image v3 valide doit etre restauree");
    for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
        const uint32_t mask = instanceStepMask(*loaded.engine.instanceForChannel(channel));
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            static_cast<uint32_t>(1) << (30 + channel), mask,
            "l instance restauree ne doit JAMAIS etre ecrasee par un template");
    }
}

void test_a_valid_version_two_image_is_refused_without_migration() {
    eeprom.reset();
    Rig legacy;
    legacy.scheduler.markDirty(0);
    finishWrite(legacy, eeprom, persist::QUIET_MS);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(2, eeprom.cell[384],
        "le rig v2 doit avoir ecrit une image valide");

    RigV3 r;
    const bool restored = flexseq::bootstrap(eeprom, r.image, r.scheduler, 0);
    TEST_ASSERT_FALSE_MESSAGE(restored, "aucune migration de v2 vers v3 : QB2");
    for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
        TEST_ASSERT_EQUAL_UINT32(0x9111,
            instanceStepMask(*r.engine.instanceForChannel(channel)));
    }
}

void test_every_unknown_version_byte_falls_back_to_the_defaults() {
    const uint8_t versions[3] = { 0xFF, 1, 4 };
    for (uint8_t index = 0; index < 3; ++index) {
        eeprom.reset();
        eeprom.cell[384] = versions[index];
        RigV3 r;
        TEST_ASSERT_FALSE_MESSAGE(
            flexseq::bootstrap(eeprom, r.image, r.scheduler, 0),
            "une version inconnue doit etre refusee");
        TEST_ASSERT_EQUAL_UINT32(0x9111,
            instanceStepMask(*r.engine.instanceForChannel(0)));
    }
}

void test_the_v3_defaults_reset_the_mode_the_offset_and_the_skip_chance() {
    eeprom.reset();
    eeprom.cell[384] = 0xFF;
    RigV3 r;
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

void test_the_v3_logical_image_is_two_hundred_and_four_bytes() {
    TEST_ASSERT_EQUAL_UINT16(204, flexseq::PersistentImageV3::SIZE);
    TEST_ASSERT_EQUAL_UINT16(203, flexseq::PersistentImageV3::VERSION_INDEX);
    TEST_ASSERT_EQUAL_UINT16(588, persist::v3::TOTAL_SIZE);
    TEST_ASSERT_EQUAL_UINT16(384, persist::v3::TEMPLATES_SIZE);
}

void test_the_v3_version_holds_the_last_logical_index_at_the_first_address() {
    RigV3 r;
    TEST_ASSERT_EQUAL_UINT16(384, r.image.addressAt(203));
    TEST_ASSERT_EQUAL_UINT8(3, r.image.byteAt(203));
}

void test_the_v3_mapping_never_lands_in_the_template_zone() {
    RigV3 r;
    for (uint16_t index = 0; index < flexseq::PersistentImageV3::SIZE; ++index) {
        const uint16_t address = r.image.addressAt(index);
        TEST_ASSERT_TRUE_MESSAGE(address < 385 || address > 768,
            "un octet logique est mappe dans la zone des templates");
    }
}

void test_the_v3_mapping_covers_the_version_and_the_data_zone_exactly() {
    RigV3 r;
    bool seen[1024];
    memset(seen, 0, sizeof(seen));
    for (uint16_t index = 0; index < flexseq::PersistentImageV3::SIZE; ++index) {
        const uint16_t address = r.image.addressAt(index);
        TEST_ASSERT_FALSE_MESSAGE(seen[address], "deux index logiques visent la meme adresse");
        seen[address] = true;
    }
    TEST_ASSERT_TRUE(seen[384]);
    for (uint16_t address = 385; address <= 768; ++address) {
        TEST_ASSERT_FALSE(seen[address]);
    }
    for (uint16_t address = 769; address <= 971; ++address) {
        TEST_ASSERT_TRUE(seen[address]);
    }
    TEST_ASSERT_FALSE(seen[972]);
}

void test_the_v3_global_zone_reads_mod_and_range_as_zero() {
    RigV3 r;
    TEST_ASSERT_EQUAL_UINT8(0, r.image.byteAt(192 + 3));
    TEST_ASSERT_EQUAL_UINT8(0, r.image.byteAt(192 + 4));
}

void test_a_stored_mod_or_range_is_normalised_back_to_zero() {
    RigV3 r;
    r.image.applyByte(192 + 3, 0x5A);
    r.image.applyByte(192 + 4, 0xA5);
    TEST_ASSERT_EQUAL_UINT8(0, r.image.byteAt(192 + 3));
    TEST_ASSERT_EQUAL_UINT8(0, r.image.byteAt(192 + 4));
    TEST_ASSERT_EQUAL_UINT16(UiController::DEFAULT_TEMPO, r.ui.tempo());
    TEST_ASSERT_EQUAL_UINT8(0, r.ui.clockSource());
}

void test_the_v3_image_round_trips_the_six_instances() {
    eeprom.reset();
    RigV3 saved;
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        Pattern* instance = saved.engine.instanceForChannel(ch);
        instance->writeStep(static_cast<uint8_t>(ch), true);
        instance->setRatchet(static_cast<uint8_t>(ch), flexseq::RATCHET_3);
    }
    saved.ui.setTempo(143);
    saved.scheduler.markDirty(0);
    finishWriteV3(saved, eeprom, persist::QUIET_MS);

    RigV3 loaded;
    TEST_ASSERT_TRUE(loaded.scheduler.load(eeprom, loaded.image));
    TEST_ASSERT_EQUAL_UINT16(143, loaded.ui.tempo());
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        const Pattern* instance = loaded.engine.instanceForChannel(ch);
        bool active = false;
        instance->readStep(static_cast<uint8_t>(ch), active);
        TEST_ASSERT_TRUE_MESSAGE(active, "un step d instance n a pas survecu au tour complet");
        TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_3, instance->getRatchet(static_cast<uint8_t>(ch)));
    }
}

void test_the_v3_reset_clears_the_six_instances_without_a_bank() {
    RigV3 r;
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        r.engine.instanceForChannel(ch)->writeStep(0, true);
    }
    r.image.resetToDefaults();
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        bool active = true;
        r.engine.instanceForChannel(ch)->readStep(0, active);
        TEST_ASSERT_FALSE_MESSAGE(active, "une instance n a pas ete videe");
    }
    TEST_ASSERT_EQUAL_UINT16(UiController::DEFAULT_TEMPO, r.ui.tempo());
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
// E3.6.4.2 : le chargeur template -> tampon de modulation.
// Il ne touche ni l'instance, ni baseLength, ni selectedPattern. Son bool dit
// que le record a ete accepte, PAS que le remplacement fut atomique.
// ---------------------------------------------------------------------------

namespace {

void writeTemplateRecord(FakeEeprom& ee, uint8_t index, const uint8_t* content,
                         uint8_t length) {
    for (uint8_t offset = 0; offset < persist::v3::CONTENT_BYTES; ++offset) {
        ee.cell[persist::v3::templateAddress(index, offset)] = content[offset];
    }
    ee.cell[persist::v3::templateAddress(index, persist::v3::RECORD_LENGTH_AT)] = length;
}

}  // namespace

void test_the_loader_refuses_a_channel_out_of_range() {
    FakeEeprom ee;
    flexseq::ModulatedPatternState state;
    state.length[0] = 7;
    TEST_ASSERT_FALSE(flexseq::loadTemplateIntoModulationBuffer(ee, state, 6, 0));
    TEST_ASSERT_EQUAL_UINT8(7, state.length[0]);
}

void test_the_loader_refuses_a_template_out_of_range() {
    FakeEeprom ee;
    // Le garde d'index doit etre DISCRIMINANT. Sans cette preparation, retirer le
    // garde faisait lire la sentinelle 0x5A, donc une longueur de 90, refusee par
    // applyTemplateByte : la fonction rendait false pour la mauvaise raison et le
    // test passait quand meme. On ecrit donc un record VALIDE a l'adresse que
    // l'index 16 atteindrait.
    //
    // Cette adresse appartient normalement a une AUTRE zone de l'image, celle des
    // instances. On ne l'ecrit ici que dans le FakeEeprom, et uniquement pour cette
    // contre-epreuve. Le firmware n'ecrit jamais un record de template la.
    uint8_t content[persist::v3::CONTENT_BYTES];
    memset(content, 0x7F, sizeof(content));
    writeTemplateRecord(ee, 16, content, 24);

    flexseq::ModulatedPatternState state;
    state.length[0] = 7;
    state.pattern[0].setStepByte(0, 0x11);

    TEST_ASSERT_FALSE(flexseq::loadTemplateIntoModulationBuffer(ee, state, 0, 16));
    TEST_ASSERT_EQUAL_UINT8(7, state.length[0]);
    TEST_ASSERT_EQUAL_UINT8(0x11, state.pattern[0].stepByte(0));
}

void test_the_loader_brings_the_content_and_the_length() {
    FakeEeprom ee;
    uint8_t content[persist::v3::CONTENT_BYTES];
    memset(content, 0, sizeof(content));
    content[0] = 0x81;   // steps 0 et 7
    content[4] = 0x0F;   // steps 32 a 35
    content[persist::v3::STEP_BYTES] = 0x23;  // ratchets des steps 0 et 1
    writeTemplateRecord(ee, 9, content, 12);

    flexseq::ModulatedPatternState state;
    TEST_ASSERT_TRUE(flexseq::loadTemplateIntoModulationBuffer(ee, state, 3, 9));

    TEST_ASSERT_EQUAL_UINT8(12, state.length[3]);
    bool active = false;
    TEST_ASSERT_TRUE(state.pattern[3].readStep(0, active));
    TEST_ASSERT_TRUE(active);
    TEST_ASSERT_TRUE(state.pattern[3].readStep(7, active));
    TEST_ASSERT_TRUE(active);
    TEST_ASSERT_TRUE(state.pattern[3].readStep(1, active));
    TEST_ASSERT_FALSE(active);
    TEST_ASSERT_TRUE(state.pattern[3].readStep(35, active));
    TEST_ASSERT_TRUE(active);
    TEST_ASSERT_EQUAL_UINT8(3, state.pattern[3].getRatchet(0));
    TEST_ASSERT_EQUAL_UINT8(2, state.pattern[3].getRatchet(1));
}

void test_the_loader_masks_the_four_bits_above_the_last_step() {
    FakeEeprom ee;
    uint8_t content[persist::v3::CONTENT_BYTES];
    memset(content, 0, sizeof(content));
    content[4] = 0xFF;   // les bits 36 a 39 ne portent aucun step
    writeTemplateRecord(ee, 8, content, 36);

    flexseq::ModulatedPatternState state;
    TEST_ASSERT_TRUE(flexseq::loadTemplateIntoModulationBuffer(ee, state, 0, 8));
    TEST_ASSERT_EQUAL_UINT8(0x0F, state.pattern[0].stepByte(4));
}

void test_the_loader_refuses_a_length_out_of_range_without_touching_the_buffer() {
    FakeEeprom ee;
    uint8_t content[persist::v3::CONTENT_BYTES];
    memset(content, 0xFF, sizeof(content));
    writeTemplateRecord(ee, 10, content, 99);   // hors de [1, 36]

    flexseq::ModulatedPatternState state;
    state.length[1] = 20;
    state.pattern[1].setStepByte(0, 0x05);
    state.pattern[1].setRatchetByte(0, 0x30);

    TEST_ASSERT_FALSE(flexseq::loadTemplateIntoModulationBuffer(ee, state, 1, 10));

    TEST_ASSERT_EQUAL_UINT8(20, state.length[1]);
    TEST_ASSERT_EQUAL_UINT8(0x05, state.pattern[1].stepByte(0));
    TEST_ASSERT_EQUAL_UINT8(0x30, state.pattern[1].ratchetByte(0));
    for (uint8_t offset = 1; offset < persist::v3::STEP_BYTES; ++offset) {
        TEST_ASSERT_EQUAL_UINT8(0, state.pattern[1].stepByte(offset));
    }
}

void test_the_loader_refuses_a_length_of_zero_the_same_way() {
    FakeEeprom ee;
    uint8_t content[persist::v3::CONTENT_BYTES];
    memset(content, 0xFF, sizeof(content));
    writeTemplateRecord(ee, 11, content, 0);

    flexseq::ModulatedPatternState state;
    state.length[0] = 12;
    TEST_ASSERT_FALSE(flexseq::loadTemplateIntoModulationBuffer(ee, state, 0, 11));
    TEST_ASSERT_EQUAL_UINT8(12, state.length[0]);
    TEST_ASSERT_EQUAL_UINT8(0, state.pattern[0].stepByte(0));
}

void test_the_loader_accepts_the_two_bounds_of_the_stored_length() {
    FakeEeprom ee;
    uint8_t content[persist::v3::CONTENT_BYTES];
    memset(content, 0, sizeof(content));
    content[0] = 0x03;
    writeTemplateRecord(ee, 12, content, 1);
    writeTemplateRecord(ee, 13, content, 36);

    flexseq::ModulatedPatternState state;
    TEST_ASSERT_TRUE(flexseq::loadTemplateIntoModulationBuffer(ee, state, 0, 12));
    TEST_ASSERT_EQUAL_UINT8(1, state.length[0]);
    TEST_ASSERT_TRUE(flexseq::loadTemplateIntoModulationBuffer(ee, state, 1, 13));
    TEST_ASSERT_EQUAL_UINT8(36, state.length[1]);
}

void test_the_loader_touches_one_channel_only() {
    FakeEeprom ee;
    uint8_t content[persist::v3::CONTENT_BYTES];
    memset(content, 0xFF, sizeof(content));
    writeTemplateRecord(ee, 11, content, 24);

    flexseq::ModulatedPatternState state;
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        state.length[ch] = 5;
    }
    TEST_ASSERT_TRUE(flexseq::loadTemplateIntoModulationBuffer(ee, state, 2, 11));
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        if (ch == 2) {
            TEST_ASSERT_EQUAL_UINT8(24, state.length[ch]);
            continue;
        }
        TEST_ASSERT_EQUAL_UINT8(5, state.length[ch]);
        TEST_ASSERT_EQUAL_UINT8(0, state.pattern[ch].stepByte(0));
    }
}

void test_the_loader_writes_nothing_to_the_eeprom() {
    FakeEeprom ee;
    uint8_t content[persist::v3::CONTENT_BYTES];
    memset(content, 0, sizeof(content));
    writeTemplateRecord(ee, 12, content, 16);
    ee.writes = 0;

    flexseq::ModulatedPatternState state;
    TEST_ASSERT_TRUE(flexseq::loadTemplateIntoModulationBuffer(ee, state, 0, 12));
    TEST_ASSERT_EQUAL_UINT16(0, ee.writes);
}

void test_the_loader_leaves_the_instance_and_the_base_alone() {
    FakeEeprom ee;
    uint8_t content[persist::v3::CONTENT_BYTES];
    memset(content, 0xAA, sizeof(content));
    writeTemplateRecord(ee, 13, content, 30);

    SequencerEngine engine;
    engine.setBaseLength(4, 18);
    engine.setSelectedPattern(4, 5);
    engine.instanceForChannel(4)->writeStep(2, true);

    flexseq::ModulatedPatternState state;
    TEST_ASSERT_TRUE(flexseq::loadTemplateIntoModulationBuffer(ee, state, 4, 13));

    TEST_ASSERT_EQUAL_UINT8(18, engine.getBaseLength(4));
    TEST_ASSERT_EQUAL_INT8(5, engine.getSelectedPattern(4));
    bool active = false;
    TEST_ASSERT_TRUE(engine.instanceForChannel(4)->readStep(2, active));
    TEST_ASSERT_TRUE(active);
    TEST_ASSERT_EQUAL_UINT8(0x04, engine.instanceForChannel(4)->stepByte(0));
}

// ---------------------------------------------------------------------------
// E3.6.4.3 : le garde de disponibilite EEPROM.
// Storage::busy() est la seule verite materielle. Sous simavr EEPE est efface
// tout de suite, donc le chemin rouge n'y est pas atteignable : il se prouve
// ici, avec un FakeEeprom dont l'occupation est pilotable.
// ---------------------------------------------------------------------------

namespace {

void writeGuardRecord(FakeEeprom& ee, uint8_t index, uint8_t length) {
    uint8_t content[persist::v3::CONTENT_BYTES];
    memset(content, 0, sizeof(content));
    content[0] = 0x81;
    content[persist::v3::STEP_BYTES] = 0x23;
    writeTemplateRecord(ee, index, content, length);
}

}  // namespace

void test_a_busy_storage_loads_nothing_into_the_modulation_buffer() {
    FakeEeprom ee;
    writeGuardRecord(ee, 9, 12);

    flexseq::ModulatedPatternState state;
    state.length[3] = 7;
    state.pattern[3].setStepByte(0, 0x11);
    state.pattern[3].setRatchetByte(0, 0x40);

    ee.busyFlag = true;
    TEST_ASSERT_FALSE(
        flexseq::loadTemplateIntoModulationBufferIfStorageIsFree(ee, state, 3, 9));

    TEST_ASSERT_EQUAL_UINT8(7, state.length[3]);
    TEST_ASSERT_EQUAL_UINT8(0x11, state.pattern[3].stepByte(0));
    TEST_ASSERT_EQUAL_UINT8(0x40, state.pattern[3].ratchetByte(0));
    TEST_ASSERT_EQUAL_UINT16(0, ee.writes);
}

void test_a_free_storage_loads_the_template_into_the_modulation_buffer() {
    FakeEeprom ee;
    writeGuardRecord(ee, 9, 12);

    flexseq::ModulatedPatternState state;
    state.length[3] = 7;
    state.pattern[3].setStepByte(0, 0x11);

    ee.busyFlag = false;
    TEST_ASSERT_TRUE(
        flexseq::loadTemplateIntoModulationBufferIfStorageIsFree(ee, state, 3, 9));

    TEST_ASSERT_EQUAL_UINT8(12, state.length[3]);
    TEST_ASSERT_EQUAL_UINT8(0x81, state.pattern[3].stepByte(0));
    TEST_ASSERT_EQUAL_UINT8(3, state.pattern[3].getRatchet(0));
    TEST_ASSERT_EQUAL_UINT16(0, ee.writes);
}

void test_a_storage_that_becomes_free_loads_on_the_next_pass() {
    FakeEeprom ee;
    writeGuardRecord(ee, 9, 12);

    flexseq::ModulatedPatternState state;
    state.length[3] = 7;
    state.pattern[3].setStepByte(0, 0x11);

    ee.busyFlag = true;
    TEST_ASSERT_FALSE(
        flexseq::loadTemplateIntoModulationBufferIfStorageIsFree(ee, state, 3, 9));
    TEST_ASSERT_EQUAL_UINT8(7, state.length[3]);
    TEST_ASSERT_EQUAL_UINT8(0x11, state.pattern[3].stepByte(0));

    ee.busyFlag = false;
    TEST_ASSERT_TRUE(
        flexseq::loadTemplateIntoModulationBufferIfStorageIsFree(ee, state, 3, 9));
    TEST_ASSERT_EQUAL_UINT8(12, state.length[3]);
    TEST_ASSERT_EQUAL_UINT8(0x81, state.pattern[3].stepByte(0));
}

void test_a_free_storage_still_refuses_a_record_the_loader_refuses() {
    FakeEeprom ee;
    writeGuardRecord(ee, 10, 99);

    flexseq::ModulatedPatternState state;
    state.length[1] = 7;

    ee.busyFlag = false;
    TEST_ASSERT_FALSE(
        flexseq::loadTemplateIntoModulationBufferIfStorageIsFree(ee, state, 1, 10));
    TEST_ASSERT_EQUAL_UINT8(7, state.length[1]);
    TEST_ASSERT_EQUAL_UINT8(0, state.pattern[1].stepByte(0));
}


// ---------------------------------------------------------------------------
// E3.6.4.4 etape 4 : l'election et le tourniquet.
// Un canal par appel. Le curseur avance des l'election, jamais au succes.
// ---------------------------------------------------------------------------

namespace {

struct ServiceRig {
    FakeEeprom ee;
    SequencerEngine engine;
    flexseq::ModulatedPatternState state;

    ServiceRig() {
        for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
            engine.setChannelMode(ch, flexseq::MODE_SEQ);
        }
        uint8_t content[persist::v3::CONTENT_BYTES];
        for (uint8_t index = 0; index < persist::v3::TEMPLATE_COUNT; ++index) {
            memset(content, 0, sizeof(content));
            content[0] = static_cast<uint8_t>(index + 1);
            writeTemplateRecord(ee, index, content, 16);
        }
    }

    void route(uint8_t channel) {
        TEST_ASSERT_TRUE(engine.setCvDestination(channel, flexseq::CV_SOURCE_1,
                                                 flexseq::CV_DEST_PATTERN));
    }

    void unroute(uint8_t channel) {
        TEST_ASSERT_TRUE(engine.setCvDestination(channel, flexseq::CV_SOURCE_1,
                                                 flexseq::CV_DEST_NONE));
    }

    int8_t serve() {
        return flexseq::serviceOneModulationTemplateLoad(ee, engine, state);
    }
};

const uint8_t NONE = flexseq::ModulatedPatternState::NOT_MODULATED;

}  // namespace

void test_no_routing_elects_nobody_and_leaves_the_cursor() {
    ServiceRig r;
    TEST_ASSERT_EQUAL_INT8(-1, r.serve());
    TEST_ASSERT_EQUAL_UINT8(0, r.state.cursor);
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_EQUAL_UINT8(NONE, r.state.loaded[ch]);
    }
    TEST_ASSERT_EQUAL_UINT16(0, r.ee.writes);
}

void test_a_routed_channel_is_served_and_carries_its_template() {
    ServiceRig r;
    r.engine.setSelectedPattern(3, 7);
    r.route(3);

    TEST_ASSERT_EQUAL_INT8(3, r.serve());
    TEST_ASSERT_EQUAL_UINT8(7, r.state.loaded[3]);
    TEST_ASSERT_EQUAL_UINT8(16, r.state.length[3]);
    TEST_ASSERT_EQUAL_UINT8(8, r.state.pattern[3].stepByte(0));
    TEST_ASSERT_EQUAL_UINT8(4, r.state.cursor);
}

void test_a_channel_already_carrying_its_index_is_not_eligible() {
    ServiceRig r;
    r.engine.setSelectedPattern(3, 7);
    r.route(3);
    TEST_ASSERT_EQUAL_INT8(3, r.serve());

    TEST_ASSERT_EQUAL_INT8(-1, r.serve());
    TEST_ASSERT_EQUAL_UINT8(4, r.state.cursor);
}

void test_a_busy_storage_elects_nobody_and_leaves_the_cursor() {
    ServiceRig r;
    r.engine.setSelectedPattern(3, 7);
    r.route(3);
    r.ee.busyFlag = true;

    TEST_ASSERT_EQUAL_INT8(-1, r.serve());
    TEST_ASSERT_EQUAL_UINT8(NONE, r.state.loaded[3]);
    TEST_ASSERT_EQUAL_UINT8(0, r.state.cursor);
    TEST_ASSERT_EQUAL_UINT8(0, r.state.pattern[3].stepByte(0));
}

void test_removing_the_routing_releases_the_channel() {
    ServiceRig r;
    r.engine.setSelectedPattern(3, 7);
    r.route(3);
    TEST_ASSERT_EQUAL_INT8(3, r.serve());
    TEST_ASSERT_EQUAL_UINT8(7, r.state.loaded[3]);

    r.unroute(3);
    TEST_ASSERT_EQUAL_INT8(-1, r.serve());
    TEST_ASSERT_EQUAL_UINT8(NONE, r.state.loaded[3]);
}

void test_the_release_happens_even_while_the_storage_is_busy() {
    ServiceRig r;
    r.engine.setSelectedPattern(3, 7);
    r.route(3);
    TEST_ASSERT_EQUAL_INT8(3, r.serve());

    r.unroute(3);
    r.ee.busyFlag = true;
    TEST_ASSERT_EQUAL_INT8(-1, r.serve());
    TEST_ASSERT_EQUAL_UINT8(NONE, r.state.loaded[3]);
}

void test_a_refused_record_elects_the_channel_and_still_moves_the_cursor() {
    ServiceRig r;
    uint8_t content[persist::v3::CONTENT_BYTES];
    memset(content, 0xFF, sizeof(content));
    writeTemplateRecord(r.ee, 7, content, 99);   // hors de [1, 36]
    r.engine.setSelectedPattern(3, 7);
    r.route(3);

    TEST_ASSERT_EQUAL_INT8(3, r.serve());
    TEST_ASSERT_EQUAL_UINT8(NONE, r.state.loaded[3]);
    TEST_ASSERT_EQUAL_UINT8(0, r.state.pattern[3].stepByte(0));
    TEST_ASSERT_EQUAL_UINT8(4, r.state.cursor);
}

void test_one_channel_only_is_served_per_call() {
    ServiceRig r;
    r.engine.setSelectedPattern(1, 4);
    r.engine.setSelectedPattern(2, 5);
    r.route(1);
    r.route(2);

    TEST_ASSERT_EQUAL_INT8(1, r.serve());
    TEST_ASSERT_EQUAL_UINT8(4, r.state.loaded[1]);
    TEST_ASSERT_EQUAL_UINT8(NONE, r.state.loaded[2]);

    TEST_ASSERT_EQUAL_INT8(2, r.serve());
    TEST_ASSERT_EQUAL_UINT8(5, r.state.loaded[2]);
}

void test_six_eligible_channels_are_served_in_six_calls() {
    ServiceRig r;
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        r.engine.setSelectedPattern(ch, static_cast<uint8_t>(ch + 2));
        r.route(ch);
    }
    bool served[SequencerEngine::CHANNEL_COUNT] = {false, false, false, false, false, false};
    for (uint8_t pass = 0; pass < SequencerEngine::CHANNEL_COUNT; ++pass) {
        const int8_t channel = r.serve();
        TEST_ASSERT_TRUE(channel >= 0);
        TEST_ASSERT_FALSE(served[channel]);
        served[channel] = true;
    }
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        TEST_ASSERT_TRUE(served[ch]);
        TEST_ASSERT_EQUAL_UINT8(ch + 2, r.state.loaded[ch]);
    }
    TEST_ASSERT_EQUAL_INT8(-1, r.serve());
}

void test_the_sweep_starts_at_the_cursor_and_wraps() {
    ServiceRig r;
    r.engine.setSelectedPattern(1, 4);
    r.engine.setSelectedPattern(5, 9);
    r.route(1);
    r.route(5);
    r.state.cursor = 5;

    TEST_ASSERT_EQUAL_INT8(5, r.serve());
    TEST_ASSERT_EQUAL_UINT8(0, r.state.cursor);
    TEST_ASSERT_EQUAL_INT8(1, r.serve());
    TEST_ASSERT_EQUAL_UINT8(2, r.state.cursor);
}

void test_the_service_writes_no_byte_of_eeprom() {
    ServiceRig r;
    for (uint8_t ch = 0; ch < SequencerEngine::CHANNEL_COUNT; ++ch) {
        r.route(ch);
    }
    const uint16_t before = r.ee.writes;
    for (uint8_t pass = 0; pass < 8; ++pass) {
        r.serve();
    }
    TEST_ASSERT_EQUAL_UINT16(before, r.ee.writes);
}

void test_a_channel_outside_seq_is_released_even_when_routed() {
    ServiceRig r;
    r.engine.setChannelMode(3, flexseq::MODE_SEQ);
    r.engine.setSelectedPattern(3, 7);
    r.route(3);
    TEST_ASSERT_EQUAL_INT8(3, r.serve());
    TEST_ASSERT_EQUAL_UINT8(7, r.state.loaded[3]);

    TEST_ASSERT_TRUE(r.engine.setChannelMode(3, flexseq::MODE_CLOCK));
    TEST_ASSERT_EQUAL_INT8(-1, r.serve());
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(NONE, r.state.loaded[3],
        "hors SEQ le canal est relache : l'instance redevient jouee");
}

// ---------------------------------------------------------------------------
// STEP-8.1, P35 normative: PRD 10.3 points 6 and 7, ADR 0011.
// OLD carries a TRIPLET on step 1, NEW carries none. After the boundary
// that moves the index to NEW, the content read is NEW, so the cached
// ratchet must be NEW too: one trigger, a step of one unit.
void test_the_boundary_that_swaps_the_template_gives_the_content_and_the_ratchet_of_the_same_template() {
    ServiceRig r;
    uint8_t content[persist::v3::CONTENT_BYTES];

    memset(content, 0, sizeof(content));
    content[0] = 0x02;
    content[persist::v3::STEP_BYTES] = 0x70;
    writeTemplateRecord(r.ee, 5, content, 16);

    memset(content, 0, sizeof(content));
    content[0] = 0x02;
    writeTemplateRecord(r.ee, 0, content, 16);

    r.engine.setModulatedPatterns(&r.state);
    r.engine.setChannelMode(0, flexseq::MODE_SEQ);
    r.engine.setSelectedPattern(0, 5);
    r.route(0);
    TEST_ASSERT_EQUAL_INT8(0, r.serve());
    TEST_ASSERT_EQUAL_UINT8(5, r.state.loaded[0]);

    r.engine.start();
    r.engine.setCvInput(flexseq::CV_SOURCE_1, -165);
    r.engine.advance(96);
    TEST_ASSERT_EQUAL_INT8(1, r.engine.effectiveStep(0));
    TEST_ASSERT_EQUAL_INT8(0, r.engine.patternCvIndex(0));
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET,
                            r.engine.patternForChannel(0)->getRatchet(1));
    TEST_ASSERT_EQUAL_UINT8(3, r.engine.currentStepTriggers(0));
    TEST_ASSERT_EQUAL_UINT16(192, r.engine.currentStepTicks(0));

    TEST_ASSERT_EQUAL_INT8(0, r.serve());
    TEST_ASSERT_EQUAL_UINT8(0, r.state.loaded[0]);

    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_NONE,
                            r.engine.patternForChannel(0)->getRatchet(1));
    TEST_ASSERT_EQUAL_UINT8(1, r.engine.currentStepTriggers(0));
    TEST_ASSERT_EQUAL_UINT16(96, r.engine.currentStepTicks(0));
}

// STEP-8.1, the refused path is a characterization, green before B6 and
// after it: the loader validates the length before it writes one byte, so
// a refused record publishes nothing and the cache has nothing to follow.
void test_a_refused_template_at_the_boundary_leaves_the_buffer_and_the_cache_alone() {
    ServiceRig r;
    uint8_t content[persist::v3::CONTENT_BYTES];

    memset(content, 0, sizeof(content));
    content[0] = 0x02;
    content[persist::v3::STEP_BYTES] = 0x70;
    writeTemplateRecord(r.ee, 5, content, 16);

    memset(content, 0, sizeof(content));
    content[0] = 0x02;
    writeTemplateRecord(r.ee, 0, content, 0);

    r.engine.setModulatedPatterns(&r.state);
    r.engine.setChannelMode(0, flexseq::MODE_SEQ);
    r.engine.setSelectedPattern(0, 5);
    r.route(0);
    TEST_ASSERT_EQUAL_INT8(0, r.serve());
    TEST_ASSERT_EQUAL_UINT8(5, r.state.loaded[0]);

    r.engine.start();
    r.engine.setCvInput(flexseq::CV_SOURCE_1, -165);
    r.engine.advance(96);
    TEST_ASSERT_EQUAL_INT8(0, r.engine.patternCvIndex(0));
    TEST_ASSERT_EQUAL_UINT8(3, r.engine.currentStepTriggers(0));
    TEST_ASSERT_EQUAL_UINT16(192, r.engine.currentStepTicks(0));

    TEST_ASSERT_EQUAL_INT8(0, r.serve());
    TEST_ASSERT_EQUAL_UINT8(5, r.state.loaded[0]);
    TEST_ASSERT_EQUAL_UINT8(flexseq::RATCHET_TRIPLET,
                            r.engine.patternForChannel(0)->getRatchet(1));
    TEST_ASSERT_EQUAL_UINT8(16, r.state.length[0]);
    TEST_ASSERT_EQUAL_UINT8(3, r.engine.currentStepTriggers(0));
    TEST_ASSERT_EQUAL_UINT16(192, r.engine.currentStepTicks(0));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_the_loader_refuses_a_channel_out_of_range);
    RUN_TEST(test_the_loader_refuses_a_template_out_of_range);
    RUN_TEST(test_the_loader_brings_the_content_and_the_length);
    RUN_TEST(test_the_loader_masks_the_four_bits_above_the_last_step);
    RUN_TEST(test_the_loader_refuses_a_length_out_of_range_without_touching_the_buffer);
    RUN_TEST(test_the_loader_refuses_a_length_of_zero_the_same_way);
    RUN_TEST(test_the_loader_accepts_the_two_bounds_of_the_stored_length);
    RUN_TEST(test_the_loader_touches_one_channel_only);
    RUN_TEST(test_the_loader_writes_nothing_to_the_eeprom);
    RUN_TEST(test_the_loader_leaves_the_instance_and_the_base_alone);

    RUN_TEST(test_a_busy_storage_loads_nothing_into_the_modulation_buffer);
    RUN_TEST(test_a_free_storage_loads_the_template_into_the_modulation_buffer);
    RUN_TEST(test_a_storage_that_becomes_free_loads_on_the_next_pass);
    RUN_TEST(test_a_free_storage_still_refuses_a_record_the_loader_refuses);
    RUN_TEST(test_no_routing_elects_nobody_and_leaves_the_cursor);
    RUN_TEST(test_a_routed_channel_is_served_and_carries_its_template);
    RUN_TEST(test_a_channel_already_carrying_its_index_is_not_eligible);
    RUN_TEST(test_a_busy_storage_elects_nobody_and_leaves_the_cursor);
    RUN_TEST(test_removing_the_routing_releases_the_channel);
    RUN_TEST(test_the_release_happens_even_while_the_storage_is_busy);
    RUN_TEST(test_a_refused_record_elects_the_channel_and_still_moves_the_cursor);
    RUN_TEST(test_one_channel_only_is_served_per_call);
    RUN_TEST(test_six_eligible_channels_are_served_in_six_calls);
    RUN_TEST(test_the_sweep_starts_at_the_cursor_and_wraps);
    RUN_TEST(test_the_service_writes_no_byte_of_eeprom);
    RUN_TEST(test_a_channel_outside_seq_is_released_even_when_routed);

    RUN_TEST(test_the_boundary_that_swaps_the_template_gives_the_content_and_the_ratchet_of_the_same_template);
    RUN_TEST(test_a_refused_template_at_the_boundary_leaves_the_buffer_and_the_cache_alone);
    RUN_TEST(test_the_first_boot_seeds_the_templates_and_fills_the_instances_from_a1);
    RUN_TEST(test_a_nominal_boot_restores_the_instances_and_never_overwrites_them);
    RUN_TEST(test_a_valid_version_two_image_is_refused_without_migration);
    RUN_TEST(test_every_unknown_version_byte_falls_back_to_the_defaults);
    RUN_TEST(test_the_v3_defaults_reset_the_mode_the_offset_and_the_skip_chance);

    RUN_TEST(test_the_v3_scan_visits_and_writes_the_two_hundred_and_four_logical_bytes);
    RUN_TEST(test_the_v3_scan_touches_only_the_version_and_the_data_zone);
    RUN_TEST(test_the_v3_version_byte_is_the_last_address_written);
    RUN_TEST(test_the_v2_scan_still_writes_the_version_first);

    RUN_TEST(test_the_v3_logical_image_is_two_hundred_and_four_bytes);
    RUN_TEST(test_the_v3_version_holds_the_last_logical_index_at_the_first_address);
    RUN_TEST(test_the_v3_mapping_never_lands_in_the_template_zone);
    RUN_TEST(test_the_v3_mapping_covers_the_version_and_the_data_zone_exactly);
    RUN_TEST(test_the_v3_global_zone_reads_mod_and_range_as_zero);
    RUN_TEST(test_a_stored_mod_or_range_is_normalised_back_to_zero);
    RUN_TEST(test_the_v3_image_round_trips_the_six_instances);
    RUN_TEST(test_the_v3_reset_clears_the_six_instances_without_a_bank);

    RUN_TEST(test_the_v2_mapping_is_the_identity_the_scheduler_used_to_inline);
    RUN_TEST(test_the_v2_version_sits_at_the_first_logical_index);

    RUN_TEST(test_the_layout_is_the_one_the_prd_fixed);
    RUN_TEST(test_the_image_ends_below_the_original_memcode);

    RUN_TEST(test_the_v3_layout_is_the_one_the_prd_fixed);
    RUN_TEST(test_the_v3_format_version_is_three);
    RUN_TEST(test_the_v3_records_carry_thirty_six_steps);
    RUN_TEST(test_the_v3_global_zone_reserves_mod_and_range);
    RUN_TEST(test_the_v3_image_leaves_the_original_memcode_alone);

    RUN_TEST(test_the_pattern_keeps_its_twenty_three_bytes);
    RUN_TEST(test_a_raw_step_byte_refuses_an_index_past_the_pattern);
    RUN_TEST(test_a_raw_ratchet_byte_refuses_an_index_past_the_pattern);
    RUN_TEST(test_a_raw_step_byte_stores_what_it_is_given_without_canonicalising);

    RUN_TEST(test_the_v3_content_codec_round_trips_the_twenty_three_bytes);
    RUN_TEST(test_the_v3_codec_drops_the_four_bits_above_the_last_step_on_load);
    RUN_TEST(test_the_v3_codec_drops_the_four_bits_above_the_last_step_on_emit);
    RUN_TEST(test_the_v3_codec_leaves_the_other_step_bytes_whole);
    RUN_TEST(test_the_v3_codec_normalises_an_invalid_ratchet_nibble);
    RUN_TEST(test_an_invalid_v3_nibble_replaces_a_previous_ratchet_instead_of_keeping_it);
    RUN_TEST(test_the_v3_codec_ignores_an_offset_past_the_record);

    RUN_TEST(test_the_v3_template_record_round_trips_its_twenty_four_bytes);
    RUN_TEST(test_the_v3_template_record_accepts_every_length_in_range);
    RUN_TEST(test_the_v3_template_record_refuses_a_length_out_of_range);
    RUN_TEST(test_a_refused_length_leaves_the_loaded_content_intact);
    RUN_TEST(test_the_v3_template_record_clamps_the_length_it_emits);
    RUN_TEST(test_the_length_byte_touches_no_content_byte);
    RUN_TEST(test_the_v3_instance_record_carries_no_length);
    RUN_TEST(test_the_v3_length_bound_is_the_pattern_capacity_not_the_engine_cap);

    RUN_TEST(test_a_round_trip_restores_the_state_byte_for_byte);
    RUN_TEST(test_a_round_trip_restores_the_cv_destinations);
    RUN_TEST(test_a_round_trip_never_restores_a_hysteresis_zone);
    RUN_TEST(test_a_round_trip_keeps_a_base_length_of_thirty_six);
    RUN_TEST(test_an_all_zero_template_is_empty);
    RUN_TEST(test_one_active_step_makes_a_template_occupied);
    RUN_TEST(test_the_thirty_sixth_step_alone_makes_a_template_occupied);
    RUN_TEST(test_ratchets_alone_leave_a_template_empty);
    RUN_TEST(test_the_four_bits_above_the_last_step_do_not_occupy_a_template);
    RUN_TEST(test_an_empty_query_reads_five_bytes_and_writes_none);
    RUN_TEST(test_an_out_of_range_index_is_not_empty);
    RUN_TEST(test_a_template_request_arms_and_reports_itself);
    RUN_TEST(test_a_template_request_writes_one_byte_per_advance);
    RUN_TEST(test_a_template_request_ends_after_its_record);
    RUN_TEST(test_a_template_request_on_a_frozen_slot_arms_nothing);
    RUN_TEST(test_a_second_template_request_is_refused_while_one_is_in_flight);
    RUN_TEST(test_the_template_request_goes_before_the_image_scan);
    RUN_TEST(test_the_image_scan_resumes_after_the_template);
    RUN_TEST(test_no_advance_ever_writes_more_than_one_byte);
    RUN_TEST(test_save_template_writes_the_instance_content_into_the_record);
    RUN_TEST(test_save_template_writes_the_base_not_the_modulated_length);
    RUN_TEST(test_save_template_writes_the_channel_length_into_the_record);
    RUN_TEST(test_save_template_refuses_the_eight_frozen_slots_without_writing);
    RUN_TEST(test_save_template_accepts_the_eight_writable_slots);
    RUN_TEST(test_save_template_refuses_an_invalid_channel_or_index_without_writing);
    RUN_TEST(test_save_then_load_returns_the_same_pattern_and_length);
    RUN_TEST(test_load_template_copies_the_content_into_the_channel_instance);
    RUN_TEST(test_load_template_leaves_the_five_other_instances_untouched);
    RUN_TEST(test_load_template_keeps_a_length_of_thirty_six_in_the_base);
    RUN_TEST(test_load_template_needs_no_clamp_below_the_ceiling);
    RUN_TEST(test_load_template_refuses_an_invalid_length_without_losing_the_content);
    RUN_TEST(test_load_template_accepts_a_frozen_factory_slot);
    RUN_TEST(test_load_template_refuses_an_invalid_channel_or_index);
    RUN_TEST(test_load_template_writes_nothing_to_the_eeprom);
    RUN_TEST(test_the_patterns_survive_with_their_ratchets);

    RUN_TEST(test_a_wrong_version_byte_returns_the_defaults);
    RUN_TEST(test_a_blank_eeprom_returns_the_defaults);
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

    RUN_TEST(test_the_format_version_is_two);
    RUN_TEST(test_the_channel_record_carries_the_mode_the_offset_and_the_skip_chance);
    RUN_TEST(test_the_three_new_fields_sit_at_their_fixed_place_in_the_record);
    RUN_TEST(test_a_version_one_image_returns_the_defaults);
    RUN_TEST(test_a_bad_mode_byte_is_refused_while_the_next_record_still_loads);
    RUN_TEST(test_a_bad_skip_chance_byte_is_refused_while_the_next_record_still_loads);
    RUN_TEST(test_the_offset_never_exceeds_the_single_byte_the_format_gives_it);
    RUN_TEST(test_the_two_cv_target_bytes_are_reserved_and_read_as_zero);
    RUN_TEST(test_a_stored_cv_target_is_ignored_without_disturbing_the_record);

    return UNITY_END();
}
