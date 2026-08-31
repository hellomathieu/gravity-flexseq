#include <flexseq/CvSampler.h>

#include <avr/interrupt.h>
#include <avr/io.h>

#include <flexseq/CvGate.h>

// Voir include/flexseq/CvSampler.h pour le pourquoi et la cadence.
namespace {

// CV1_PIN = A7, CV2_PIN = A6 dans peripherials.h de libGravity.
constexpr uint8_t MUX_OF[flexseq::cv::COUNT] = {7, 6};

// Seuils du domaine : +1 V et +0,5 V, convention +/-512 de AnalogInput::Read().
constexpr int16_t ARM_MV = 102;
constexpr int16_t REARM_MV = 51;

// Etat possede EXCLUSIVEMENT par l'ISR : pas de partage, donc pas de volatile.
flexseq::CvGate gate[flexseq::cv::COUNT];
uint8_t converting = flexseq::cv::CV1;

// Etat partage avec la boucle principale.
volatile uint16_t latest[flexseq::cv::COUNT] = {0, 0};
volatile uint8_t pending = 0;  // un bit par voie : front vu, pas encore consomme
volatile uint32_t completed = 0;

// Calibration retenue par voie : `latestCalibrated()` en a besoin a chaque
// lecture, la ou `configure()` ne s'en servait que pour les deux seuils.
int16_t calLow[flexseq::cv::COUNT] = {0, 0};
int16_t calHigh[flexseq::cv::COUNT] = {0, 0};
int16_t calOffset[flexseq::cv::COUNT] = {0, 0};

inline void startConversion(uint8_t channel) {
    converting = channel;
    ADMUX = static_cast<uint8_t>((1 << REFS0) | MUX_OF[channel]);  // reference AVCC
    ADCSRA |= (1 << ADSC);
}

}  // namespace

namespace flexseq {
namespace cv {

void configure(uint8_t channel, int16_t calibrationLow, int16_t calibrationHigh,
               int16_t offset) {
    if (channel >= COUNT) {
        return;
    }
    calLow[channel] = calibrationLow;
    calHigh[channel] = calibrationHigh;
    calOffset[channel] = offset;
    gate[channel] = CvGate();
    gate[channel].configure(
        rawFromCalibrated(ARM_MV, calibrationLow, calibrationHigh, offset),
        rawFromCalibrated(REARM_MV, calibrationLow, calibrationHigh, offset));
}

void start() {
    pending = 0;
    completed = 0;

    // Prescaler 128 (125 kHz), interruption de fin de conversion, pas de
    // declenchement automatique : l'ISR relance elle-meme.
    ADCSRB = 0;
    ADCSRA = static_cast<uint8_t>((1 << ADEN) | (1 << ADIE) |
                                  (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0));
    startConversion(CV1);
}

bool takeEdge(uint8_t channel) {
    if (channel >= COUNT) {
        return false;
    }
    const uint8_t bit = static_cast<uint8_t>(1u << channel);
    uint8_t seen;
    const uint8_t sreg = SREG;
    cli();
    seen = static_cast<uint8_t>(pending & bit);
    pending = static_cast<uint8_t>(pending & ~bit);
    SREG = sreg;
    return seen != 0;
}

uint16_t latestRaw(uint8_t channel) {
    if (channel >= COUNT) {
        return 0;
    }
    uint16_t value;
    const uint8_t sreg = SREG;
    cli();
    value = latest[channel];  // 16 bits : lecture non atomique sans ce verrou
    SREG = sreg;
    return value;
}

int16_t latestCalibrated(uint8_t channel) {
    if (channel >= COUNT) {
        return 0;
    }
    return calibratedFromRaw(latestRaw(channel), calLow[channel], calHigh[channel],
                             calOffset[channel]);
}

uint32_t conversions() {
    uint32_t value;
    const uint8_t sreg = SREG;
    cli();
    value = completed;
    SREG = sreg;
    return value;
}

}  // namespace cv
}  // namespace flexseq

// Fin de conversion. Corps volontairement court : il tourne toutes les 104 us.
ISR(ADC_vect) {
    // ADCL AVANT ADCH : lire ADCL verrouille la paire jusqu'a la lecture d'ADCH.
    const uint8_t low = ADCL;
    const uint16_t raw = static_cast<uint16_t>(low | (static_cast<uint16_t>(ADCH) << 8));

    const uint8_t channel = converting;
    latest[channel] = raw;
    if (gate[channel].update(raw)) {
        pending = static_cast<uint8_t>(pending | (1u << channel));
    }
    ++completed;

    startConversion(channel == flexseq::cv::CV1 ? flexseq::cv::CV2 : flexseq::cv::CV1);
}
