#ifndef FLEXSEQ_CV_SAMPLER_H
#define FLEXSEQ_CV_SAMPLER_H

#include <stdint.h>

namespace flexseq {

// CvSampler — echantillonnage des 2 entrees CV SOUS INTERRUPTION.
//
// Pourquoi. Le CV n'etait lu qu'une fois par passage de la boucle principale
// (`Gravity::Process()` appelle `cv1.Process()` / `cv2.Process()`, qui font un
// `analogRead` bloquant). Le pire passage mesure 7,74 ms rendu OLED actif
// (ADR 0001), donc une impulsion plus courte pouvait passer inapercue. Garantir
// 1 ms exige d'echantillonner independamment de la boucle.
//
// Consequence assumee : FlexSeq prend la PROPRIETE du convertisseur. libGravity
// ne doit plus appeler `analogRead` — sans quoi une conversion bloquante de la
// boucle principale entrerait en collision avec celles de l'ISR. `main.cpp`
// n'appelle donc plus `gravity.Process()` mais ses morceaux : boutons et
// encodeur. Les sorties etaient deja pilotees explicitement (l'index non
// initialise de `Gravity::Process()`), de sorte que FlexSeq ne depend plus du
// tout de cette fonction.
//
// Cadence. Prescaler 128 -> horloge ADC a 125 kHz, soit 13 cycles = 104 us par
// conversion, les deux voies en alternance : chacune est donc lue toutes les
// ~208 us. Une impulsion de 1 ms recoit ainsi 4 a 5 echantillons. Le prescaler
// reste a 128 et non plus rapide : les memes echantillons serviront a la
// quantification des destinations CV (PRD 10.4), qui a besoin des 10 bits.
//
// Les conversions sont RELANCEES depuis l'ISR plutot que laissees en roue libre :
// changer ADMUX en roue libre entre en course avec le demarrage de la conversion
// suivante, et l'echantillon pourrait etre attribue a la mauvaise voie.
//
// Aucun temporisateur n'est utilise : uClock possede Timer1.
namespace cv {

enum : uint8_t {
    CV1 = 0,  // A7 / ADC7
    CV2 = 1,
    COUNT = 2,
};

// Seuils de Schmitt d'une voie, calcules une fois en unites brutes depuis SA
// calibration libGravity (PRD 10.5 : +1 V et +0,5 V). Chaque entree a la sienne :
// le « Calibration CV » du PRD 4 est un reglage par entree.
void configure(uint8_t channel, int16_t calibrationLow, int16_t calibrationHigh,
               int16_t offset);

// Configure le convertisseur et lance la premiere conversion.
void start();

// Consomme le verrou de front d'une voie. Vrai si une impulsion a ete VUE depuis
// le dernier appel, quelle qu'ait ete sa duree.
bool takeEdge(uint8_t channel);

// Derniere valeur brute connue (0..1023), pour les destinations qui s'appliquent
// a la frontiere de step (PRD 10.3).
uint16_t latestRaw(uint8_t channel);

// Derniere valeur connue dans la convention +/-512 de `AnalogInput::Read()`.
// PRD 10.1 : le domaine ne voit JAMAIS le brut du convertisseur, donc la
// calibration reste ici. L'arithmetique est celle de `calibratedFromRaw()`,
// deja eprouvee : elle n'est pas recopiee.
int16_t latestCalibrated(uint8_t channel);

// Nombre de conversions achevees, pour verifier que l'echantillonnage tourne.
uint32_t conversions();

}  // namespace cv
}  // namespace flexseq

#endif // FLEXSEQ_CV_SAMPLER_H
