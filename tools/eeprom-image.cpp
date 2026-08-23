#include <flexseq/Persistence.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace flexseq;

namespace {

void usage() {
    std::fprintf(stderr,
        "usage: eeprom-image --mode clock|seq [--steps 0,3,4,9,15] [--tempo 120]\n"
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

}  // namespace

int main(int argc, char** argv) {
    const char* mode = nullptr;
    const char* stepList = nullptr;
    uint16_t tempo = UiController::DEFAULT_TEMPO;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode = argv[++i];
        } else if (std::strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
            stepList = argv[++i];
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
    UiController ui(engine, bank, transport);
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

    for (uint8_t channel = 0; channel < SequencerEngine::CHANNEL_COUNT; ++channel) {
        engine.setSelectedPattern(channel, 0);
        engine.setChannelMode(channel, channelMode);
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
