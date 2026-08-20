#ifndef FLEXSEQ_CV_GATE_H
#define FLEXSEQ_CV_GATE_H

#include <stdint.h>

namespace flexseq {

// CvGate — detection de FRONT sur une entree CV, en unites ADC BRUTES.
//
// Pourquoi pas `AnalogInput::IsRisingEdge()` : anomalie auditee de libGravity.
// `old_read_` y est declare `uint16_t` alors que `read_` est `int16_t`, donc une
// valeur precedente negative devient un grand positif : « elle etait haute » est
// vrai chaque fois qu'elle etait en realite negative, et un passage negatif ->
// positif ne produit AUCUN front. C'est le cas le plus naturel sur une entree
// bipolaire (PRD 10.5). Ni `read_` ni `old_read_` n'y sont initialises.
//
// Pourquoi des unites BRUTES : cette classe est mise a jour depuis l'ISR du
// convertisseur, ou l'on ne veut ni `map()` ni flottant. Les seuils sont
// precalcules une fois, depuis la calibration de libGravity (PRD 10.1), par
// l'adaptateur qui possede le convertisseur.
//
// Hypothese : la tension croit avec la valeur brute. Elle est fausse si
// `SetAttenuation()` est appele avec un pourcentage NEGATIF (libGravity inverse
// alors `read_`) ; FlexSeq ne l'appelle pas.
//
// Cette classe ne connait ni le materiel ni `volatile` : l'atomicite entre ISR
// et boucle principale appartient a l'adaptateur.
class CvGate {
public:
    CvGate() : arm_(0), rearm_(0), high_(false), latched_(false) {}

    // Seuils de Schmitt, en unites brutes. `arm` doit etre STRICTEMENT superieur
    // a `rearm` : l'ecart EST l'hysteresis, et c'est elle qui dispense de tout
    // anti-rebond (PRD 10.5 : armement au-dessus de +1 V, rearmement sous
    // +0,5 V).
    void configure(uint16_t arm, uint16_t rearm) {
        arm_ = arm;
        rearm_ = rearm;
    }

    // Un echantillon. Renvoie true au moment ou le seuil haut est franchi.
    //
    // FRONT uniquement, jamais niveau : une gate maintenue haute produit UN
    // evenement, pas un flux. L'etat precedent est initialise explicitement a
    // BAS, ce qui ecarte le faux front au demarrage.
    bool update(uint16_t raw) {
        if (!high_) {
            if (raw > arm_) {
                high_ = true;
                latched_ = true;
                return true;
            }
            return false;
        }
        if (raw < rearm_) {
            high_ = false;
        }
        return false;
    }

    // Le verrou : l'evenement est retenu des qu'il est vu, puis consomme. C'est
    // lui qui fait survivre une impulsion courte a un passage de boucle long —
    // l'impulsion doit etre VUE, pas vue au bon moment.
    bool takeEdge() {
        const bool edge = latched_;
        latched_ = false;
        return edge;
    }

    bool pending() const { return latched_; }

    // Vrai tant que l'entree n'est pas redescendue sous le seuil de rearmement.
    bool high() const { return high_; }

private:
    uint16_t arm_;
    uint16_t rearm_;
    bool high_;
    bool latched_;
};

// Convertit un seuil du domaine (+/-512, la convention de `AnalogInput::Read()`)
// en unites ADC brutes, avec la calibration de libGravity.
//
// `Process()` calcule `read_ = map(raw, 0, 1023, low, high) - offset`. On inverse
// donc : `raw = (value + offset - low) * 1023 / (high - low)`. En 32 bits, car
// `(value + offset - low) * 1023` deborde largement un entier 16 bits.
//
// Arrondi au PLUS PROCHE, et non tronque : `map()` tronquant a l'aller, l'inverse
// est de toute facon ambigu d'un pas — lequel vaut ~4,9 mV a l'entree du
// convertisseur, soit ~10 mV de CV. Immateriel devant un seuil de 1 V borde de
// 0,5 V d'hysteresis.
//
// Valeurs par defaut de libGravity (low = -566, high = 512, offset = 0) :
// +1 V (+102) -> 634 et +0,5 V (+51) -> 585.
inline uint16_t rawFromCalibrated(int16_t value, int16_t low, int16_t high, int16_t offset) {
    if (high <= low) {
        return 0;
    }
    const int32_t span = static_cast<int32_t>(high) - low;
    int32_t raw = (static_cast<int32_t>(value) + offset - low) * 1023 + span / 2;
    raw /= span;
    if (raw < 0) {
        return 0;
    }
    if (raw > 1023) {
        return 1023;
    }
    return static_cast<uint16_t>(raw);
}

}  // namespace flexseq

#endif // FLEXSEQ_CV_GATE_H
