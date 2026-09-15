#include <flexseq/CvDestination.h>
#include <flexseq/Persistence.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace flexseq;

namespace {

void usage() {
    std::fprintf(stderr,
        "usage: eeprom-image --mode clock|seq [--steps 0,3,4,9,15] [--tempo 120]\n"
        "                    [--ratchet 0:6,3:2] [--subdiv -8] [--length 36]\n"
        "                    [--cv-target 1:2,2:2] [--clock-source 0]\n"
        "                    [--per-channel] [--format 2|4]\n"
        "                    [--bank <idx>:<steps>[:<step>/<code>,...]]...\n"
        "                    [--selected <idx>]\n"
        "writes the persistent image to stdout, one byte per byte, no header\n"
        "The bank starts EMPTY: the generator never seeds the factory content,\n"
        "which is the firmware's own job on an invalid image.\n"
        "--steps and --ratchet write the shared pattern 0, which every channel\n"
        "plays unless --selected or --per-channel says otherwise.\n"
        "--per-channel gives channel c the pattern c, each with its own\n"
        "content, and refuses --steps, --ratchet, --bank and --selected.\n"
        "--format picks the EEPROM layout. It defaults to 2, the historical\n"
        "format. Format 4 is the one the firmware reads.\n"
        "--length sets the base length of the six channels. The engine keeps\n"
        "its own bound, so an out-of-range value is refused here.\n"
        "--clock-source picks the persisted clock source, 0 to 5: 0 is INT, 1\n"
        "to 4 are the external PPQN 24, 4, 2 and 1, and 5 is MIDI.\n"
        "--cv-target routes a CV source to a destination on the six channels.\n"
        "The source is 1 or 2; the destination is the PRD 10.2 code, so 2 is\n"
        "LENGTH. Several pairs are separated by a comma.\n"
        "--bank writes one shared pattern of the bank with an explicit content:\n"
        "the active steps, then optional ratchets as step/code pairs. The option\n"
        "repeats; one index twice is refused. --bank needs --format 4.\n"
        "--selected sets the selected pattern of the six channels.\n");
}

struct PerChannelPattern {
    uint8_t steps[6];
    uint8_t stepCount;
    uint8_t ratchetStep;
    uint8_t ratchetCode;
};

const PerChannelPattern PER_CHANNEL[6] = {
    { { 0,  0,  0,  0,  0,  0 }, 1,  0, 2 },
    { { 1,  2,  0,  0,  0,  0 }, 2,  1, 3 },
    { { 3,  4,  5,  0,  0,  0 }, 3,  3, 4 },
    { { 6,  7,  8,  9,  0,  0 }, 4,  6, 6 },
    { { 10, 11, 12, 13, 14, 0 }, 5, 10, 7 },
    { { 0,  2,  5,  7, 10, 15 }, 6, 15, 2 },
};

bool writePerChannelPatterns(PatternBank& bank) {
    for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
        Pattern* p = bank.getPattern(channel);
        if (p == nullptr) {
            std::fprintf(stderr, "eeprom-image: pattern %u unreachable\n", channel);
            return false;
        }
        const PerChannelPattern& spec = PER_CHANNEL[channel];
        for (uint8_t i = 0; i < spec.stepCount; ++i) {
            p->writeStep(spec.steps[i], true);
        }
        if (!p->setRatchet(spec.ratchetStep, spec.ratchetCode)) {
            std::fprintf(stderr, "eeprom-image: ratchet refused on pattern %u\n", channel);
            return false;
        }
    }
    return true;
}

bool parseSteps(const char* list, uint8_t* steps, int& count) {
    count = 0;
    const char* p = list;
    while (*p != '\0' && count < Pattern::DEFAULT_TOTAL_STEPS) {
        char* end = nullptr;
        const long value = std::strtol(p, &end, 10);
        if (end == p || value < 0 || value >= Pattern::DEFAULT_TOTAL_STEPS) {
            return false;
        }
        steps[count++] = static_cast<uint8_t>(value);
        p = end;
        if (*p == ',') {
            ++p;
        }
    }
    return count > 0;
}

bool parseRatchets(const char* list, uint8_t* steps, uint8_t* codes, int& count) {
    count = 0;
    const char* p = list;
    while (*p != '\0' && count < Pattern::DEFAULT_TOTAL_STEPS) {
        char* end = nullptr;
        const long step = std::strtol(p, &end, 10);
        if (end == p || step < 0 || step >= Pattern::DEFAULT_TOTAL_STEPS
            || *end != ':') {
            return false;
        }
        p = end + 1;
        const long code = std::strtol(p, &end, 10);
        if (end == p || !isValidRatchet(static_cast<uint8_t>(code))) {
            return false;
        }
        steps[count] = static_cast<uint8_t>(step);
        codes[count] = static_cast<uint8_t>(code);
        ++count;
        p = end;
        if (*p == ',') {
            ++p;
        }
    }
    return count > 0;
}

bool parseBankPattern(const char* text, uint8_t& index, Pattern& pattern) {
    char* end = nullptr;
    const long idx = std::strtol(text, &end, 10);
    if (end == text || idx < 0 || idx >= persist::v4::BANK_COUNT || *end != ':') {
        return false;
    }
    index = static_cast<uint8_t>(idx);
    pattern.clear();
    const char* p = end + 1;
    int stepCount = 0;
    while (*p != '\0' && *p != ':') {
        const long step = std::strtol(p, &end, 10);
        if (end == p || step < 0 || step >= Pattern::DEFAULT_TOTAL_STEPS) {
            return false;
        }
        pattern.writeStep(static_cast<uint8_t>(step), true);
        ++stepCount;
        p = end;
        if (*p == ',') {
            ++p;
        }
    }
    if (stepCount == 0) {
        return false;
    }
    if (*p == '\0') {
        return true;
    }
    ++p;
    int ratchetCount = 0;
    while (*p != '\0') {
        const long step = std::strtol(p, &end, 10);
        if (end == p || step < 0 || step >= Pattern::DEFAULT_TOTAL_STEPS || *end != '/') {
            return false;
        }
        p = end + 1;
        const long code = std::strtol(p, &end, 10);
        if (end == p || !isValidRatchet(static_cast<uint8_t>(code))
            || !pattern.setRatchet(static_cast<uint8_t>(step), static_cast<uint8_t>(code))) {
            return false;
        }
        ++ratchetCount;
        p = end;
        if (*p == ',') {
            ++p;
        }
    }
    return ratchetCount > 0;
}

struct BankOverrides {
    bool set[persist::v4::BANK_COUNT];
    Pattern content[persist::v4::BANK_COUNT];
};

int emitVersionFour(SequencerEngine& engine, UiController& ui, Preferences& preferences) {
    PersistentImageV4 image(engine, ui, preferences);
    uint8_t physical[persist::v4::TOTAL_SIZE];
    bool covered[persist::v4::TOTAL_SIZE];
    for (uint16_t offset = 0; offset < persist::v4::TOTAL_SIZE; ++offset) {
        physical[offset] = 0;
        covered[offset] = false;
    }

    for (uint16_t index = 0; index < PersistentImageV4::SIZE; ++index) {
        const uint16_t address = image.addressAt(index);
        if (address < persist::BASE_ADDRESS
            || address >= persist::BASE_ADDRESS + persist::v4::TOTAL_SIZE) {
            std::fprintf(stderr, "eeprom-image: logical %u maps outside the format\n", index);
            return 2;
        }
        const uint16_t at = static_cast<uint16_t>(address - persist::BASE_ADDRESS);
        if (covered[at]) {
            std::fprintf(stderr, "eeprom-image: offset %u written twice\n", at);
            return 2;
        }
        physical[at] = image.byteAt(index);
        covered[at] = true;
    }

    for (uint16_t offset = 0; offset < persist::v4::TOTAL_SIZE; ++offset) {
        if (!covered[offset]) {
            std::fprintf(stderr, "eeprom-image: offset %u never written\n", offset);
            return 2;
        }
    }

    for (uint16_t offset = 0; offset < persist::v4::TOTAL_SIZE; ++offset) {
        std::putchar(physical[offset]);
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const char* mode = nullptr;
    const char* stepList = nullptr;
    const char* ratchetList = nullptr;
    const char* subdivText = nullptr;
    const char* lengthText = nullptr;
    const char* cvTargetText = nullptr;
    uint16_t tempo = UiController::DEFAULT_TEMPO;
    int clockSource = 0;
    bool perChannel = false;
    uint8_t format = persist::FORMAT_VERSION;
    BankOverrides overrides = {};
    int selected = -1;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode = argv[++i];
        } else if (std::strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
            stepList = argv[++i];
        } else if (std::strcmp(argv[i], "--ratchet") == 0 && i + 1 < argc) {
            ratchetList = argv[++i];
        } else if (std::strcmp(argv[i], "--subdiv") == 0 && i + 1 < argc) {
            subdivText = argv[++i];
        } else if (std::strcmp(argv[i], "--length") == 0 && i + 1 < argc) {
            lengthText = argv[++i];
        } else if (std::strcmp(argv[i], "--cv-target") == 0 && i + 1 < argc) {
            cvTargetText = argv[++i];
        } else if (std::strcmp(argv[i], "--tempo") == 0 && i + 1 < argc) {
            tempo = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--clock-source") == 0 && i + 1 < argc) {
            clockSource = std::atoi(argv[++i]);
            if (clockSource < 0
                || clockSource >= UiController::CLOCK_SOURCE_COUNT) {
                std::fprintf(stderr,
                             "eeprom-image: --clock-source accepts 0 to %d\n",
                             UiController::CLOCK_SOURCE_COUNT - 1);
                return 2;
            }
        } else if (std::strcmp(argv[i], "--per-channel") == 0) {
            perChannel = true;
        } else if (std::strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
            format = static_cast<uint8_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--bank") == 0 && i + 1 < argc) {
            uint8_t index = 0;
            Pattern content;
            if (!parseBankPattern(argv[++i], index, content)) {
                std::fprintf(stderr, "eeprom-image: bank pattern refused: %s\n", argv[i]);
                return 2;
            }
            if (overrides.set[index]) {
                std::fprintf(stderr, "eeprom-image: bank pattern %u given twice\n", index);
                return 2;
            }
            overrides.set[index] = true;
            overrides.content[index] = content;
        } else if (std::strcmp(argv[i], "--selected") == 0 && i + 1 < argc) {
            selected = std::atoi(argv[++i]);
            if (selected < 0 || selected >= SequencerEngine::PATTERN_COUNT) {
                std::fprintf(stderr, "eeprom-image: --selected accepts 0 to 15\n");
                return 2;
            }
        } else {
            usage();
            return 2;
        }
    }
    if (mode == nullptr) {
        usage();
        return 2;
    }
    if (format != persist::FORMAT_VERSION && format != persist::v4::FORMAT_VERSION) {
        std::fprintf(stderr, "eeprom-image: --format accepts 2 or 4\n");
        return 2;
    }
    if (perChannel && (stepList != nullptr || ratchetList != nullptr)) {
        std::fprintf(stderr,
                     "eeprom-image: --per-channel refuses --steps and --ratchet\n");
        return 2;
    }
    if (perChannel && selected >= 0) {
        std::fprintf(stderr, "eeprom-image: --per-channel refuses --selected\n");
        return 2;
    }
    bool anyBank = false;
    for (uint8_t index = 0; index < persist::v4::BANK_COUNT; ++index) {
        anyBank = anyBank || overrides.set[index];
    }
    if (anyBank && format != persist::v4::FORMAT_VERSION) {
        std::fprintf(stderr, "eeprom-image: --bank needs --format 4\n");
        return 2;
    }
    if (anyBank && perChannel) {
        std::fprintf(stderr, "eeprom-image: --per-channel refuses --bank\n");
        return 2;
    }

    ChannelMode channelMode;
    if (std::strcmp(mode, "clock") == 0) {
        channelMode = MODE_CLOCK;
    } else if (std::strcmp(mode, "seq") == 0) {
        channelMode = MODE_SEQ;
    } else {
        usage();
        return 2;
    }

    PatternBank bank;
    SequencerEngine engine;
    engine.setPatternBank(&bank);
    Transport transport(engine);
    UiController ui(engine, transport);
    Preferences preferences;
    PersistentImage image(bank, engine, ui, preferences);

    if (stepList != nullptr) {
        uint8_t steps[Pattern::DEFAULT_TOTAL_STEPS];
        int count = 0;
        if (!parseSteps(stepList, steps, count)) {
            std::fprintf(stderr, "eeprom-image: step list refused: %s\n", stepList);
            return 2;
        }
        Pattern* p = bank.getPattern(0);
        for (int i = 0; i < count; ++i) {
            p->writeStep(steps[i], true);
        }
    }

    if (ratchetList != nullptr) {
        uint8_t steps[Pattern::DEFAULT_TOTAL_STEPS];
        uint8_t codes[Pattern::DEFAULT_TOTAL_STEPS];
        int count = 0;
        if (!parseRatchets(ratchetList, steps, codes, count)) {
            std::fprintf(stderr, "eeprom-image: ratchet list refused: %s\n", ratchetList);
            return 2;
        }
        Pattern* p = bank.getPattern(0);
        for (int i = 0; i < count; ++i) {
            if (!p->setRatchet(steps[i], codes[i])) {
                std::fprintf(stderr, "eeprom-image: ratchet refused at step %u\n",
                             steps[i]);
                return 2;
            }
        }
    }

    for (uint8_t index = 0; index < persist::v4::BANK_COUNT; ++index) {
        if (overrides.set[index]) {
            *bank.getPattern(index) = overrides.content[index];
        }
    }

    if (perChannel && !writePerChannelPatterns(bank)) {
        return 2;
    }

    for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
        engine.setSelectedPattern(
            channel, perChannel ? channel : static_cast<uint8_t>(selected < 0 ? 0 : selected));
        engine.setChannelMode(channel, channelMode);
    }
    if (subdivText != nullptr) {
        const int16_t subdiv = static_cast<int16_t>(std::atoi(subdivText));
        for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
            if (!engine.setSubdiv(channel, subdiv)) {
                std::fprintf(stderr, "eeprom-image: subdiv refused: %s\n", subdivText);
                return 2;
            }
        }
    }
    if (cvTargetText != nullptr) {
        const char* p = cvTargetText;
        while (*p != '\0') {
            char* end = nullptr;
            const long source = std::strtol(p, &end, 10);
            if (end == p || source < 1 || source > CV_SOURCE_COUNT || *end != ':') {
                std::fprintf(stderr, "eeprom-image: cv target refused: %s\n", cvTargetText);
                return 2;
            }
            p = end + 1;
            const long destination = std::strtol(p, &end, 10);
            if (end == p || destination < 0 || destination >= CV_DESTINATION_COUNT) {
                std::fprintf(stderr, "eeprom-image: cv target refused: %s\n", cvTargetText);
                return 2;
            }
            for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
                if (!engine.setCvDestination(channel,
                                             static_cast<uint8_t>(source - 1),
                                             static_cast<CvDestination>(destination))) {
                    std::fprintf(stderr, "eeprom-image: cv target refused on channel %u\n",
                                 channel);
                    return 2;
                }
            }
            p = end;
            if (*p == ',') {
                ++p;
            }
        }
    }

    if (lengthText != nullptr) {
        const long wanted = std::strtol(lengthText, nullptr, 10);
        for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
            if (wanted < 0 || wanted > 255
                || !engine.setBaseLength(channel, static_cast<uint8_t>(wanted))) {
                std::fprintf(stderr, "eeprom-image: length refused: %s\n", lengthText);
                return 2;
            }
        }
    }
    if (!ui.setTempo(tempo)) {
        std::fprintf(stderr, "eeprom-image: tempo refused: %u\n", tempo);
        return 2;
    }
    if (!ui.setClockSource(static_cast<uint8_t>(clockSource))) {
        std::fprintf(stderr, "eeprom-image: clock source refused: %d\n",
                     clockSource);
        return 2;
    }

    if (format == persist::v4::FORMAT_VERSION) {
        return emitVersionFour(engine, ui, preferences);
    }

    for (uint16_t index = 0; index < PersistentImage::SIZE; ++index) {
        std::putchar(image.byteAt(index));
    }
    return 0;
}
