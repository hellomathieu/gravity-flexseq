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
// LES DEUX SENS SONT PRIS EN CHARGE. Le sens se deduit des seuils eux-memes :
// `arm > rearm` decrit une entree ou la tension croit avec la valeur brute, et
// `arm < rearm` l'entree inverse. Cela evite une hypothese tacite : libGravity
// nie `read_` si `SetAttenuation()` recoit un pourcentage NEGATIF, et il n'existe
// aucun accesseur pour le savoir. FlexSeq ne l'appelle pas aujourd'hui, mais si
// une atténuation inversee etait un jour exposee, il suffirait de passer les
// seuils correspondants — la porte n'a pas a changer.
//
// Cette classe ne connait ni le materiel ni `volatile` : l'atomicite entre ISR
// et boucle principale appartient a l'adaptateur.
class CvGate {
public:
    CvGate() : arm_(0), rearm_(0), high_(false), latched_(false), rising_(true),
               usable_(false) {}

    // Seuils de Schmitt, en unites brutes. L'ECART entre les deux EST
    // l'hysteresis, et c'est elle qui dispense de tout anti-rebond (PRD 10.5 :
    // armement au-dela de +1 V, rearmement en deca de +0,5 V).
    //
    // Leur ORDRE porte le sens de l'entree : `arm > rearm` pour une entree
    // croissante, `arm < rearm` pour une entree inversee. Deux seuils EGAUX
    // suppriment l'hysteresis, ce qui n'a pas de sens : la porte reste alors
    // inerte plutot que de claquer sur le bruit.
    void configure(uint16_t arm, uint16_t rearm) {
        arm_ = arm;
        rearm_ = rearm;
        rising_ = arm > rearm;
        usable_ = arm != rearm;
    }

    // Un echantillon. Renvoie true au moment ou le seuil haut est franchi.
    //
    // FRONT uniquement, jamais niveau : une gate maintenue haute produit UN
    // evenement, pas un flux. L'etat precedent est initialise explicitement a
    // BAS, ce qui ecarte le faux front au demarrage.
    bool update(uint16_t raw) {
        if (!usable_) {
            return false;
        }
        const bool crossed = rising_ ? (raw > arm_) : (raw < arm_);
        const bool released = rising_ ? (raw < rearm_) : (raw > rearm_);
        if (!high_) {
            if (crossed) {
                high_ = true;
                latched_ = true;
                return true;
            }
            return false;
        }
        if (released) {
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
    bool rising_;   // deduit de l'ordre des seuils
    bool usable_;   // faux si les seuils sont egaux : pas d'hysteresis
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

// L'ALLER : unites brutes -> convention +/-512 de `AnalogInput::Read()`. C'est
// exactement ce que fait `AnalogInput::Process()`, `map()` tronquant compris.
// Utile a qui veut AFFICHER une valeur de CV sans repasser par libGravity, dont
// le `Process()` n'est plus appele (voir CvSampler.h).
inline int16_t calibratedFromRaw(uint16_t raw, int16_t low, int16_t high, int16_t offset) {
    const int32_t mapped =
        static_cast<int32_t>(raw) * (static_cast<int32_t>(high) - low) / 1023 + low;
    int32_t value = mapped - offset;
    if (value < -512) {
        value = -512;
    }
    if (value > 512) {
        value = 512;
    }
    return static_cast<int16_t>(value);
}

}  // namespace flexseq

#endif // FLEXSEQ_CV_GATE_H
