#ifndef FLEXSEQ_PAGED_SCREEN_H
#define FLEXSEQ_PAGED_SCREEN_H

#include <stdint.h>

#include <flexseq/Pattern.h>
#include <flexseq/PatternScreen.h>

namespace flexseq {

// PagedScreen — l'ecran EDIT PATTERN rendu UNE BANDE A LA FOIS (ADR 0001).
//
// Le mode _1_ de libGravity n'alloue que 128 o de tampon pour un ecran de
// 1024 o : U8g2 rend donc l'image en 8 bandes horizontales successives. Les
// enchainer dans un seul appel bloque la boucle principale le temps de l'image
// entiere (~25 ms de bus a 400 kHz), pendant lesquels les ticks s'accumulent et
// les onsets se tassent au drainage suivant. Une bande par appel ramene le pire
// cas au huitieme.
//
// Templatise sur le Display comme PatternScreen l'est sur le canvas : le
// firmware l'instancie sur `gravity.display`, un test sur un faux. Le Display
// est passe A CHAQUE APPEL et non conserve — pas de reference stockee, donc pas
// de 2 octets de RAM pour rien.
//
// Interface attendue du Display : celle de U8g2 — firstPage(), nextPage(), plus
// les primitives de dessin que PatternScreen utilise.
//
// Cout RAM : le modele gele (8 o) + la copie du Pattern (15 o) + un drapeau.
template <typename Display>
class PagedScreen {
public:
    PagedScreen() : busy_(false) {}

    // Vrai tant qu'une image reste a terminer. On ne demarre jamais une image
    // par-dessus une autre : les 8 bandes appartiennent a la meme.
    bool busy() const { return busy_; }

    // Gele ce qui sera affiche, puis dessine la premiere bande.
    //
    // Le contenu du Pattern est COPIE par valeur. C'est necessaire, pas
    // prudentiel : le pattern est partage entre channels et editable pendant la
    // lecture (PRD 6.3), donc sans copie deux bandes de la meme image pourraient
    // montrer des contenus differents.
    //
    // firstPage() ne transfere RIEN — cet appel ne coute que du CPU.
    void begin(Display& display, const PatternScreenModel& model) {
        model_ = model;
        if (model.pattern != nullptr) {
            pattern_ = *model.pattern;
            model_.pattern = &pattern_;
        }
        display.firstPage();
        drawPatternScreen(display, model_);
        busy_ = true;
    }

    // Transfere la bande courante, puis dessine la suivante. Renvoie false quand
    // l'image est complete (ou si aucune n'etait ouverte).
    //
    // drawPatternScreen() doit rester PURE : c'est ce qui rend l'etalement
    // possible, la meme fonction devant produire la meme image huit fois.
    bool advance(Display& display) {
        if (!busy_) {
            return false;
        }
        if (display.nextPage() == 0) {
            busy_ = false;
            return false;
        }
        drawPatternScreen(display, model_);
        return true;
    }

private:
    PatternScreenModel model_;
    Pattern pattern_;
    bool busy_;
};

}  // namespace flexseq

#endif // FLEXSEQ_PAGED_SCREEN_H
