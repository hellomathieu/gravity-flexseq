#include <flexseq/Persistence.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace flexseq;

namespace {

void usage() {
    std::fprintf(stderr,
        "usage: eeprom-image --mode clock|seq [--steps 0,3,4,9,15] [--tempo 120]\n"
        "                    [--ratchet 0:6,3:2] [--subdiv -8]\n"
        "writes the persistent image to stdout, one byte per byte, no header\n");
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
        } else {
            usage();
            return 2;
        }
    }
    if (mode == nullptr) {
        usage();
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

    for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
        engine.setSelectedPattern(channel, 0);
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
