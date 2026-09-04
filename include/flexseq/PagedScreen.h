#ifndef FLEXSEQ_PAGED_SCREEN_H
#define FLEXSEQ_PAGED_SCREEN_H

#include <stdint.h>

#include <flexseq/MainScreen.h>
#include <flexseq/Pattern.h>
#include <flexseq/PatternScreen.h>

namespace flexseq {

// PagedScreen — l'ecran EDIT PATTERN rendu UNE BANDE A LA FOIS (ADR 0001), en
// sautant celles dont le contenu n'a pas change.
//
// Le mode _1_ de libGravity n'alloue que 128 o de tampon pour un ecran de
// 1024 o : U8g2 rend donc l'image en 8 bandes horizontales successives. Les
// enchainer dans un seul appel bloque la boucle principale le temps de l'image
// entiere, pendant lequel les ticks s'accumulent et les onsets se tassent.
//
// BOUCLE DE PAGES MANUELLE, et non firstPage()/nextPage(). Le cycle d'une bande
// est `setBufferCurrTileRow` + `clearBuffer` + dessin + `sendBuffer`, ce que la
// boucle de U8g2 fait aussi — mais la faire nous-memes permet de NE PAS FAIRE ce
// cycle du tout pour une bande inchangee. Le SSD1306 est un ecran a memoire : une
// bande non envoyee continue d'afficher ce qu'elle affichait, a l'identique. Ne
// rien envoyer ne produit donc aucun scintillement.
//
// ⚠️ LE CYCLE EST INDIVISIBLE. Effacer le tampon sans le redessiner puis
// l'envoyer ferait DISPARAITRE la bande le temps d'une image — le seul defaut de
// cette optimisation qui serait visible. On saute les trois etapes ensemble, ou
// aucune.
//
// Ce qui est saute aujourd'hui : la bande du TITRE. Sa rastérisation coute
// ~8,8 ms par image (15 caracteres a ~0,59 ms, mesure par
// tools/run-blocking-probe.sh) alors que le titre ne change qu'au changement de
// pattern selectionne. La condition est GEOMETRIQUE et non un numero de bande :
// une bande entierement au-dessus du filet d'en-tete ne peut contenir que le
// titre. Si la mise en page changeait, la condition cesserait simplement de
// s'appliquer et l'on retomberait sur le comportement complet.
//
// Filet : une image sur FULL_REFRESH_EVERY est rendue INTEGRALEMENT, quoi qu'en
// dise la comparaison. Ce n'est pas une precaution contre le modele de l'ecran —
// il est certain — mais contre un defaut de notre propre logique : un oubli se
// repare alors tout seul en quelques images au lieu de rester affiche.
//
// Templatise sur le Display comme PatternScreen l'est sur le canvas : le firmware
// l'instancie sur `gravity.display`, un test sur un faux. Le Display est passe a
// chaque appel et non conserve — pas de reference stockee.
//
// Cout RAM : le modele gele (9 o) + la copie du Pattern (15 o) + l'etat de
// pagination et les deux empreintes de titre.
template <typename Display>
class PagedScreen {
public:
    static constexpr uint8_t ROWS = screen::HEIGHT / 8;   // 8 bandes
    static constexpr uint8_t FULL_REFRESH_EVERY = 16;     // filet

    enum Mode : uint8_t { MODE_PATTERN, MODE_MAIN };

    PagedScreen()
        : busy_(false), row_(0), tiles_(1), full_(true), sinceFull_(FULL_REFRESH_EVERY),
          titleHash_(0), drawnTitleHash_(0), titleEverDrawn_(false),
          mode_(MODE_PATTERN) {}

    // Vrai tant qu'une image reste a terminer.
    bool busy() const { return busy_; }

    // Gele ce qui sera affiche, puis rend la premiere bande utile.
    //
    // Le contenu du Pattern est COPIE par valeur : il est partage entre channels
    // et editable pendant la lecture (PRD 6.3), donc sans copie deux bandes de la
    // meme image pourraient montrer des contenus differents.
    Mode mode() const { return mode_; }

    // L'ecran principal : pas de saut de bande, la mise en page ne s'y prete pas
    // encore. Un changement d'ecran force une image COMPLETE, sinon les pixels de
    // l'ecran precedent survivraient dans une bande sautee.
    void begin(Display& display, const MainScreenModel& model) {
        const bool switched = (mode_ != MODE_MAIN);
        mode_ = MODE_MAIN;
        main_ = model;
        detail::measureMainScreen(display, main_);
        startFrame(display, switched);
    }

    void begin(Display& display, const PatternScreenModel& model) {
        const bool switched = (mode_ != MODE_PATTERN);
        mode_ = MODE_PATTERN;
        model_ = model;
        if (model.pattern != nullptr) {
            pattern_ = *model.pattern;
            model_.pattern = &pattern_;
        }
        // La largeur du titre une seule fois pour toute l'image : getStrWidth()
        // decode la police et coute ~1 ms par appel.
        if (model_.title != nullptr && model_.titleWidth == 0) {
            model_.titleWidth = static_cast<uint8_t>(display.getStrWidth(model_.title));
        }
        titleHash_ = hashOf(model_.title);
        startFrame(display, switched);
    }

    // Rend la bande suivante qui en a besoin. Renvoie false quand l'image est
    // complete (ou si aucune n'etait ouverte).
    //
    // drawPatternScreen() doit rester PURE : c'est ce qui rend l'etalement
    // possible, la meme fonction devant produire la meme image bande par bande.
    bool advance(Display& display) {
        if (!busy_) {
            return false;
        }
        row_ = static_cast<uint8_t>(row_ + tiles_);
        return renderFrom(display);
    }

private:
    void startFrame(Display& display, bool switched) {
        tiles_ = display.getBufferTileHeight();
        if (tiles_ == 0) {
            tiles_ = 1;
        }
        full_ = switched || (sinceFull_ >= FULL_REFRESH_EVERY);
        sinceFull_ = full_ ? 1 : static_cast<uint8_t>(sinceFull_ + 1);

        row_ = 0;
        busy_ = true;
        renderFrom(display);
    }

    // Avance jusqu'a la premiere bande a rendre, la rend, et dit s'il en restait.
    bool renderFrom(Display& display) {
        while (row_ < ROWS && skippable(row_)) {
            row_ = static_cast<uint8_t>(row_ + tiles_);
        }
        if (row_ >= ROWS) {
            busy_ = false;
            return false;
        }
        renderRow(display, row_);
        return true;
    }

    // Le cycle indivisible d'une bande.
    void renderRow(Display& display, uint8_t row) {
        const Band band = bandOf(row);
        display.setBufferCurrTileRow(row);
        display.clearBuffer();
        if (mode_ == MODE_MAIN) {
            drawMainScreen(display, main_, band);
            display.sendBuffer();
            return;
        }
        drawPatternScreen(display, model_, band);
        if (titleBand(band)) {
            drawnTitleHash_ = titleHash_;
            titleEverDrawn_ = true;
        }
        display.sendBuffer();
    }

    // Une bande entierement au-dessus du filet d'en-tete ne peut contenir que le
    // titre : tout le reste — filet, barres, steps, chiffres, curseur — est en
    // dessous. Condition GEOMETRIQUE, donc solidaire de la mise en page.
    static bool titleBand(const Band& band) {
        return band.y1 < screen::HEADER_LINE_Y;
    }

    bool skippable(uint8_t row) const {
        if (full_ || mode_ == MODE_MAIN) {
            return false;
        }
        const Band band = bandOf(row);
        if (!titleBand(band)) {
            return false;
        }
        return model_.title != nullptr && titleEverDrawn_
            && titleHash_ == drawnTitleHash_;
    }

    // La bande que l'affichage s'apprete a transferer, RAMENEE EN COORDONNEES
    // LOGIQUES.
    //
    // LA CONVERSION EST INDISPENSABLE. libGravity construit son objet en
    // `U8G2_R2` : U8g2 fait tourner de 180 degres AVANT de decouper a la bande
    // courante. Le renderer, lui, travaille en coordonnees logiques. Sans
    // l'inversion, chaque bande recevait exactement la MOITIE INVERSE de ce
    // qu'elle affiche, et l'ecran restait quasi blanc.
    Band bandOf(uint8_t row) const {
        const uint16_t d0 = static_cast<uint16_t>(row) * 8u;
        uint16_t d1 = d0 + static_cast<uint16_t>(tiles_) * 8u - 1u;
        if (d1 >= screen::HEIGHT) {
            d1 = screen::HEIGHT - 1;
        }
        return Band{static_cast<uint8_t>(screen::HEIGHT - 1 - d1),
                    static_cast<uint8_t>(screen::HEIGHT - 1 - d0)};
    }

    // Empreinte du titre. Somme multiplicative et non simple addition : « A2 » et
    // « B1 » ont la meme somme de caracteres, et ce sont precisement deux titres
    // voisins.
    static uint16_t hashOf(const char* s) {
        uint16_t h = 1;
        if (s != nullptr) {
            while (*s) {
                h = static_cast<uint16_t>(h * 31u + static_cast<uint8_t>(*s++));
            }
        }
        return h;
    }

    PatternScreenModel model_;
    Pattern pattern_;
    bool busy_;
    uint8_t row_;
    uint8_t tiles_;
    bool full_;
    uint8_t sinceFull_;
    uint16_t titleHash_;
    uint16_t drawnTitleHash_;
    bool titleEverDrawn_;
    MainScreenModel main_;
    Mode mode_;
};

}  // namespace flexseq

#endif // FLEXSEQ_PAGED_SCREEN_H
