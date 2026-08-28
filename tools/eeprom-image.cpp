#include <flexseq/Persistence.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace flexseq;

namespace {

void usage() {
    std::fprintf(stderr,
        "usage: eeprom-image --mode clock|seq [--steps 0,3,4,9,15] [--tempo 120]\n"
        "                    [--ratchet 0:6,3:2] [--subdiv -8] [--per-channel]\n"
        "writes the persistent image to stdout, one byte per byte, no header\n"
        "--per-channel gives each channel its own template, and refuses\n"
        "--steps and --ratchet\n");
}

struct PerChannelTemplate {
    uint8_t steps[6];
    uint8_t stepCount;
    uint8_t ratchetStep;
    uint8_t ratchetCode;
};

const PerChannelTemplate PER_CHANNEL[6] = {
    { { 0,  0,  0,  0,  0,  0 }, 1,  0, 2 },
    { { 1,  2,  0,  0,  0,  0 }, 2,  1, 3 },
    { { 3,  4,  5,  0,  0,  0 }, 3,  3, 4 },
    { { 6,  7,  8,  9,  0,  0 }, 4,  6, 6 },
    { { 10, 11, 12, 13, 14, 0 }, 5, 10, 7 },
    { { 0,  2,  5,  7, 10, 15 }, 6, 15, 2 },
};

bool writePerChannelTemplates(PatternBank& bank) {
    for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
        Pattern* p = bank.getPattern(channel);
        if (p == nullptr) {
            std::fprintf(stderr, "eeprom-image: template %u unreachable\n", channel);
            return false;
        }
        const PerChannelTemplate& spec = PER_CHANNEL[channel];
        for (uint8_t i = 0; i < spec.stepCount; ++i) {
            p->writeStep(spec.steps[i], true);
        }
        if (!p->setRatchet(spec.ratchetStep, spec.ratchetCode)) {
            std::fprintf(stderr, "eeprom-image: ratchet refused on template %u\n",
                         channel);
            return false;
        }
    }
    return true;
}

bool parseSteps(const char* list, uint8_t* steps, int& count) {
    count = 0;
    const char* p = list;
    while (*p != '\0' && count < 24) {
        char* end = nullptr;
        const long value = std::strtol(p, &end, 10);
        if (end == p || value < 0 || value >= 24) {
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
    while (*p != '\0' && count < 24) {
        char* end = nullptr;
        const long step = std::strtol(p, &end, 10);
        if (end == p || step < 0 || step >= 24 || *end != ':') {
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

}  // namespace

int main(int argc, char** argv) {
    const char* mode = nullptr;
    const char* stepList = nullptr;
    const char* ratchetList = nullptr;
    const char* subdivText = nullptr;
    uint16_t tempo = UiController::DEFAULT_TEMPO;
    bool perChannel = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode = argv[++i];
        } else if (std::strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
            stepList = argv[++i];
        } else if (std::strcmp(argv[i], "--ratchet") == 0 && i + 1 < argc) {
            ratchetList = argv[++i];
        } else if (std::strcmp(argv[i], "--subdiv") == 0 && i + 1 < argc) {
            subdivText = argv[++i];
        } else if (std::strcmp(argv[i], "--tempo") == 0 && i + 1 < argc) {
            tempo = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--per-channel") == 0) {
            perChannel = true;
        } else {
            usage();
            return 2;
        }
    }
    if (mode == nullptr) {
        usage();
        return 2;
    }
    if (perChannel && (stepList != nullptr || ratchetList != nullptr)) {
        std::fprintf(stderr,
                     "eeprom-image: --per-channel refuses --steps and --ratchet\n");
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
    Transport transport(engine);
    UiController ui(engine, transport);
    Preferences preferences;
    PersistentImage image(bank, engine, ui, preferences);
    engine.setPatternBank(&bank);

    if (stepList != nullptr) {
        uint8_t steps[24];
        int count = 0;
        if (!parseSteps(stepList, steps, count)) {
            std::fprintf(stderr, "eeprom-image: step list refused: %s\n", stepList);
            return 2;
        }
        Pattern* p = bank.getPattern(0);
        if (p == nullptr) {
            std::fprintf(stderr, "eeprom-image: pattern 0 unreachable\n");
            return 1;
        }
        for (int i = 0; i < count; ++i) {
            p->writeStep(steps[i], true);
        }
    }

    if (ratchetList != nullptr) {
        uint8_t steps[24];
        uint8_t codes[24];
        int count = 0;
        if (!parseRatchets(ratchetList, steps, codes, count)) {
            std::fprintf(stderr, "eeprom-image: ratchet list refused: %s\n", ratchetList);
            return 2;
        }
        Pattern* p = bank.getPattern(0);
        if (p == nullptr) {
            std::fprintf(stderr, "eeprom-image: pattern 0 unreachable\n");
            return 1;
        }
        for (int i = 0; i < count; ++i) {
            if (!p->setRatchet(steps[i], codes[i])) {
                std::fprintf(stderr, "eeprom-image: ratchet refused at step %u\n",
                             steps[i]);
                return 2;
            }
        }
    }

    if (perChannel && !writePerChannelTemplates(bank)) {
        return 2;
    }

    for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
        engine.setSelectedPattern(channel, perChannel ? channel : 0);
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
    if (!ui.setTempo(tempo)) {
        std::fprintf(stderr, "eeprom-image: tempo refused: %u\n", tempo);
        return 2;
    }

    for (uint16_t index = 0; index < PersistentImage::SIZE; ++index) {
        std::putchar(image.byteAt(index));
    }
    return 0;
}
