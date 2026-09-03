#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr uint32_t OUTPUT_PPQN = 96;
constexpr uint32_t MICROSECONDS_PER_MINUTE = 60000000;
constexpr uint32_t MIN_BPM = 1;
constexpr uint32_t MAX_BPM = 400;
constexpr uint32_t PRODUCT_MIN_BPM = 20;
constexpr uint32_t PRODUCT_MAX_BPM = 200;
constexpr uint32_t INPUT_PPQN[4] = {24, 4, 2, 1};

constexpr uint32_t EXTERNAL_INTERVAL_FIRST = 2000;
constexpr uint32_t EXTERNAL_INTERVAL_LAST = 3000000;
constexpr uint32_t EXTERNAL_INTERVAL_STEP = 137;

constexpr uint32_t INTERNAL_TOLERANCE_US = 0;
constexpr uint32_t EXTERNAL_TOLERANCE_US = 1;
constexpr uint32_t DISPLAYED_TOLERANCE_BPM = 0;
constexpr uint32_t TIMER_TOLERANCE_US = 0;

constexpr uint32_t AVR_CLOCK_FREQ = 16000000;
constexpr uint32_t CYCLES_PER_US = AVR_CLOCK_FREQ / 1000000;
constexpr uint32_t PRESCALERS[5] = {1, 8, 64, 256, 1024};

const char* mutatedFamily = "";

bool familyIsMutated(const char* family) {
    return std::strcmp(mutatedFamily, family) == 0;
}

uint32_t floatBpmToMicroSeconds(float bpm) {
    return static_cast<uint32_t>(MICROSECONDS_PER_MINUTE / 96.0f / bpm);
}

float floatFrequencyToBpm(uint32_t interval, uint32_t inputPpqn) {
    return MICROSECONDS_PER_MINUTE / static_cast<float>(interval * inputPpqn);
}

float floatConstrainBpm(float bpm) {
    if (bpm < static_cast<float>(MIN_BPM)) {
        return static_cast<float>(MIN_BPM);
    }
    if (bpm > static_cast<float>(MAX_BPM)) {
        return static_cast<float>(MAX_BPM);
    }
    return bpm;
}

uint32_t integerBpmToMicroSeconds(uint32_t bpm) {
    if (familyIsMutated("I")) {
        return MICROSECONDS_PER_MINUTE / OUTPUT_PPQN / (bpm + 1);
    }
    return MICROSECONDS_PER_MINUTE / OUTPUT_PPQN / bpm;
}

uint32_t integerFrequencyToBpm(uint32_t interval, uint32_t inputPpqn) {
    const uint32_t product = interval * inputPpqn;
    if (product == 0) {
        return MAX_BPM;
    }
    return MICROSECONDS_PER_MINUTE / product;
}

uint32_t integerConstrainBpm(uint32_t bpm) {
    if (bpm < MIN_BPM) {
        return MIN_BPM;
    }
    if (bpm > MAX_BPM) {
        return MAX_BPM;
    }
    return bpm;
}

constexpr uint32_t PRODUCT_AT_MAX_BPM = MICROSECONDS_PER_MINUTE / MAX_BPM;
constexpr uint32_t PRODUCT_AT_MIN_BPM = MICROSECONDS_PER_MINUTE / MIN_BPM;

uint32_t integerExternalInterval(uint32_t interval, uint32_t inputPpqn) {
    const uint32_t product = interval * inputPpqn;
    if (product < PRODUCT_AT_MAX_BPM) {
        return integerBpmToMicroSeconds(MAX_BPM);
    }
    if (product > PRODUCT_AT_MIN_BPM) {
        return integerBpmToMicroSeconds(MIN_BPM);
    }
    if (familyIsMutated("E")) {
        return product / (OUTPUT_PPQN + 1);
    }
    return product / OUTPUT_PPQN;
}

uint32_t integerDisplayedBpm(uint32_t interval, uint32_t inputPpqn) {
    if (familyIsMutated("D")) {
        return integerConstrainBpm(integerFrequencyToBpm(interval, inputPpqn)) + 1;
    }
    return integerConstrainBpm(integerFrequencyToBpm(interval, inputPpqn));
}

struct TimerSetting {
    uint32_t prescaler;
    uint32_t ocr;
    bool refused;
};

TimerSetting floatTimerSetting(uint32_t us) {
    const float tickHertz = 1.0f / ((float)us / 1000000.0f);
    for (uint32_t i = 0; i < 5; ++i) {
        const uint32_t ocr =
            (uint32_t)(AVR_CLOCK_FREQ / (tickHertz * (float)PRESCALERS[i]));
        if (ocr < 65535) {
            return TimerSetting{PRESCALERS[i], ocr, false};
        }
    }
    return TimerSetting{0, 0, true};
}

TimerSetting integerTimerSetting(uint32_t us) {
    const uint32_t cycles = CYCLES_PER_US * us;
    for (uint32_t i = 0; i < 5; ++i) {
        const uint32_t ocr = cycles / PRESCALERS[i];
        if (ocr < 65535) {
            if (familyIsMutated("T")) {
                return TimerSetting{PRESCALERS[i], ocr + 1, false};
            }
            return TimerSetting{PRESCALERS[i], ocr, false};
        }
    }
    return TimerSetting{0, 0, true};
}

struct Verdict {
    const char* family;
    char subject[56];
    uint32_t cases;
    uint32_t mismatches;
    uint32_t worst;
    uint32_t tolerance;
    const char* unit;
};

void setSubject(Verdict& v, const char* text) {
    std::snprintf(v.subject, sizeof(v.subject), "%s", text);
}

void report(const Verdict& v) {
    const bool green = v.worst <= v.tolerance;
    std::printf("  %s %-2s %-46s %7u cas  %5u ecart(s)  pire %6u %s  tolerance %u\n",
                green ? "OK  " : "FAIL", v.family, v.subject,
                v.cases, v.mismatches, v.worst, v.unit, v.tolerance);
}

Verdict internalPath(uint32_t first, uint32_t last, const char* subject) {
    Verdict v = {"I", {0}, 0, 0, 0, INTERNAL_TOLERANCE_US, "us"};
    setSubject(v, subject);
    for (uint32_t bpm = first; bpm <= last; ++bpm) {
        const uint32_t byFloat = floatBpmToMicroSeconds(static_cast<float>(bpm));
        const uint32_t byInteger = integerBpmToMicroSeconds(bpm);
        const uint32_t gap = byFloat > byInteger ? byFloat - byInteger
                                                 : byInteger - byFloat;
        ++v.cases;
        if (gap != 0) {
            ++v.mismatches;
            if (gap > v.worst) {
                v.worst = gap;
            }
        }
    }
    return v;
}

Verdict externalPath(uint32_t inputPpqn) {
    Verdict v = {"E", {0}, 0, 0, 0, EXTERNAL_TOLERANCE_US, "us"};
    std::snprintf(v.subject, sizeof(v.subject),
                  "intervalle du timer, PPQN d entree %u", inputPpqn);
    for (uint32_t interval = EXTERNAL_INTERVAL_FIRST;
         interval <= EXTERNAL_INTERVAL_LAST;
         interval += EXTERNAL_INTERVAL_STEP) {
        const float bpm = floatConstrainBpm(floatFrequencyToBpm(interval, inputPpqn));
        const uint32_t byFloat = floatBpmToMicroSeconds(bpm);
        const uint32_t byInteger = integerExternalInterval(interval, inputPpqn);
        const uint32_t gap = byFloat > byInteger ? byFloat - byInteger
                                                 : byInteger - byFloat;
        ++v.cases;
        if (gap != 0) {
            ++v.mismatches;
            if (gap > v.worst) {
                v.worst = gap;
            }
        }
    }
    return v;
}

Verdict displayedTempo(uint32_t inputPpqn) {
    Verdict v = {"D", {0}, 0, 0, 0, DISPLAYED_TOLERANCE_BPM, "bpm"};
    std::snprintf(v.subject, sizeof(v.subject),
                  "tempo AFFICHE, PPQN d entree %u", inputPpqn);
    for (uint32_t interval = EXTERNAL_INTERVAL_FIRST;
         interval <= EXTERNAL_INTERVAL_LAST;
         interval += EXTERNAL_INTERVAL_STEP) {
        const float bpm = floatConstrainBpm(floatFrequencyToBpm(interval, inputPpqn));
        const uint32_t byFloat = static_cast<uint32_t>(bpm);
        const uint32_t byInteger = integerDisplayedBpm(interval, inputPpqn);
        const uint32_t gap = byFloat > byInteger ? byFloat - byInteger
                                                 : byInteger - byFloat;
        ++v.cases;
        if (gap != 0) {
            ++v.mismatches;
            if (gap > v.worst) {
                v.worst = gap;
            }
        }
    }
    return v;
}

uint32_t exactOcr(uint32_t us, uint32_t prescaler) {
    return (uint32_t)((uint64_t)AVR_CLOCK_FREQ * (uint64_t)us
                      / 1000000ULL / (uint64_t)prescaler);
}

Verdict timerSetting() {
    Verdict v = {"T", {0}, 0, 0, 0, TIMER_TOLERANCE_US, "pas d OCR"};
    setSubject(v, "OCR1A : l entier n est jamais MOINS exact que le flottant");
    uint32_t floatWorst = 0;
    uint32_t integerWorst = 0;
    uint32_t prescalerDiffers = 0;
    for (uint32_t bpm = MIN_BPM; bpm <= MAX_BPM; ++bpm) {
        const uint32_t us = MICROSECONDS_PER_MINUTE / OUTPUT_PPQN / bpm;
        const TimerSetting byFloat = floatTimerSetting(us);
        const TimerSetting byInteger = integerTimerSetting(us);
        ++v.cases;
        if (byFloat.refused != byInteger.refused
            || byFloat.prescaler != byInteger.prescaler) {
            ++prescalerDiffers;
            ++v.mismatches;
            v.worst = 0xFFFFFFFFu;
            continue;
        }
        const uint32_t exact = exactOcr(us, byInteger.prescaler);
        const uint32_t fErr = byFloat.ocr > exact ? byFloat.ocr - exact
                                                  : exact - byFloat.ocr;
        const uint32_t iErr = byInteger.ocr > exact ? byInteger.ocr - exact
                                                    : exact - byInteger.ocr;
        if (fErr > floatWorst) {
            floatWorst = fErr;
        }
        if (iErr > integerWorst) {
            integerWorst = iErr;
        }
        if (iErr > fErr) {
            ++v.mismatches;
            if (iErr - fErr > v.worst) {
                v.worst = iErr - fErr;
            }
        }
    }
    std::printf("     %-8s %-46s pire ecart a la valeur EXACTE : flottant %u, "
                "entier %u ; prediviseur different %u fois\n",
                "", "", floatWorst, integerWorst, prescalerDiffers);
    return v;
}

void usage() {
    std::fprintf(stderr,
        "usage: tempo-equivalence [--mutate I|E|D]\n"
        "compares the floating-point tempo arithmetic of uClock with an integer\n"
        "form, on three families:\n"
        "  I  the internal path, an integer BPM to a timer interval\n"
        "  E  the external path, a measured interval to a timer interval\n"
        "  D  the displayed tempo that Clock::Tempo() returns\n"
        "  T  the period OCR1A and the prescaler apply\n"
        "--mutate breaks the integer form of one family, so that its criterion\n"
        "is seen red. Exit 0 all families inside tolerance, 1 a family outside,\n"
        "2 the arguments were refused.\n");
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--mutate") == 0 && i + 1 < argc) {
            mutatedFamily = argv[++i];
            if (std::strcmp(mutatedFamily, "I") != 0
                && std::strcmp(mutatedFamily, "E") != 0
                && std::strcmp(mutatedFamily, "D") != 0
                && std::strcmp(mutatedFamily, "T") != 0) {
                std::fprintf(stderr,
                             "tempo-equivalence: --mutate accepts I, E, D or T\n");
                return 2;
            }
        } else {
            usage();
            return 2;
        }
    }

    std::printf("========== EQUIVALENCE DU CALCUL DE TEMPO ==========\n");
    if (mutatedFamily[0] != '\0') {
        std::printf("  levier : la forme entiere de la famille %s est cassee\n",
                    mutatedFamily);
    }

    Verdict verdicts[11];
    uint8_t count = 0;
    verdicts[count++] = internalPath(MIN_BPM, MAX_BPM,
                                     "bpm entier, plage de uClock 1 a 400");
    verdicts[count++] = internalPath(PRODUCT_MIN_BPM, PRODUCT_MAX_BPM,
                                     "bpm entier, plage produit 20 a 200");
    for (uint32_t i = 0; i < 4; ++i) {
        verdicts[count++] = externalPath(INPUT_PPQN[i]);
    }
    for (uint32_t i = 0; i < 4; ++i) {
        verdicts[count++] = displayedTempo(INPUT_PPQN[i]);
    }
    verdicts[count++] = timerSetting();

    uint8_t failed = 0;
    for (uint8_t i = 0; i < count; ++i) {
        report(verdicts[i]);
        if (verdicts[i].worst > verdicts[i].tolerance) {
            ++failed;
        }
    }
    std::printf("====================================================\n");
    if (failed != 0) {
        std::printf("  VERDICT : FAIL — %u famille(s) hors tolerance sur %u\n",
                    failed, count);
        return 1;
    }
    std::printf("  VERDICT : PASS — %u familles dans la tolerance\n", count);
    return 0;
}
