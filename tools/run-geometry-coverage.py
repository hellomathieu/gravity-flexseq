#!/usr/bin/env python3
"""Couverture de la geometrie d'ecran — ADR 0012.

Les `static_assert` de mise en page gardent des RELATIONS : « ne pas se
chevaucher », « degager le filet ». Ce sont des inegalites, donc elles ont du
jeu. Une valeur peut bouger en gardant toutes les relations vraies.

`test/vectors/screen_geometry_vectors.tsv` garde les VALEURS. Les deux
mecanismes sont complementaires, et aucun ne remplace l'autre.

Cet outil ferme le trou entre eux : il extrait les identifiants que les
`static_assert` lisent, et il exige que le fichier de vecteurs nomme chacun.
Sans lui, un lot pourrait ajouter un `static_assert` sur une constante que rien
ne verrouille, et personne ne le verrait.

⚠️ Il ne porte AUCUNE table de correspondance. Le fichier de vecteurs porte
l'identifiant C++ exact dans sa colonne `cpp`, donc la confrontation est
mecanique : aucune copie a maintenir.

Sorties : 0 conforme · 1 une constante n'est pas gardee · 2 le controle est
inevaluable, ce qui compte pour un echec.
"""
import re
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parent.parent
ENTETES = [
    RACINE / "include" / "flexseq" / "PatternScreen.h",
    RACINE / "include" / "flexseq" / "MainScreen.h",
]
VECTEURS = RACINE / "test" / "vectors" / "screen_geometry_vectors.tsv"

V, R, J, D = "\033[32m", "\033[31m", "\033[33m", "\033[0m"


def invalide(cause):
    print(f"{R}⛔ INEVALUABLE{D} — {cause}")
    print("   Un controle qu'on ne peut pas evaluer compte pour un echec.")
    sys.exit(2)


def noms_du_fichier():
    if not VECTEURS.is_file():
        invalide(f"le fichier de vecteurs est absent : {VECTEURS}")
    lignes = [l for l in VECTEURS.read_text(encoding="utf-8").splitlines() if l]
    if len(lignes) < 2:
        invalide("le fichier de vecteurs ne porte aucun cas")
    noms = {}
    for numero, ligne in enumerate(lignes[1:], start=2):
        champs = ligne.split("\t")
        if len(champs) != 6:
            invalide(f"ligne {numero} : {len(champs)} champs au lieu de 6")
        nom_cpp = champs[4]
        if nom_cpp and nom_cpp != "-":
            noms[nom_cpp] = champs[1]
    if not noms:
        invalide("aucune ligne ne nomme d'identifiant C++")
    return noms


def identifiants_lus():
    """Les constantes que les gardes de MISE EN PAGE lisent.

    Une garde qui nomme un tampon — un identifiant en `_SCRATCH` — ne garde pas
    une mise en page : elle garde une CAPACITE. La difference n'est pas de forme
    mais de sujet, et elle compte : une relation de mise en page est une
    inegalite a jeu, tandis qu'une capacite est bornee contre une donnee reelle.
    `sizeof(LBL_SUBDIVISION) <= LABEL_SCRATCH` mord des que l'etiquette depasse
    le tampon, ce qu'une contre-epreuve a verifie a 10 contre 14. Ces gardes
    sortent donc du perimetre, et le rapport dit combien il en a ecarte plutot
    que de les taire.
    """
    lus = {}
    capacites = 0
    for entete in ENTETES:
        if not entete.is_file():
            invalide(f"en-tete absent : {entete}")
        source = entete.read_text(encoding="utf-8")
        trouves = re.findall(r"static_assert\s*\((.*?)(?:,\s*\"|\)\s*;)", source, re.S)
        if not trouves:
            invalide(f"aucun static_assert trouve dans {entete.name} :"
                     " l'extraction est cassee, ou les gardes ont disparu")
        for expression in trouves:
            if re.search(r"\b[A-Z][A-Z0-9_]*_SCRATCH\b", expression):
                capacites += 1
                continue
            expression = re.sub(r'"[^"]*"', "", expression)
            for ident in re.findall(r"\b[A-Z][A-Z0-9_]{2,}\b", expression):
                lus.setdefault(ident, set()).add(entete.name)
    if not lus:
        invalide("aucune garde de mise en page ne reste apres le tri :"
                 " l'extraction est cassee")
    return lus, capacites


def main():
    noms = noms_du_fichier()
    lus, capacites = identifiants_lus()
    manquants = sorted(i for i in lus if i not in noms)

    print("=" * 66)
    print("  COUVERTURE DE LA GEOMETRIE D'ECRAN (ADR 0012)")
    print("=" * 66)
    print(f"  identifiants lus par les static_assert : {len(lus)}")
    print(f"  gardes de CAPACITE ecartees (elles nomment un tampon) : {capacites}")
    print(f"  identifiants nommes par le fichier     : {len(noms)}")
    print(f"  couverts                               : {len(lus) - len(manquants)}")
    print("=" * 66)

    if manquants:
        print(f"{R}❌ NON CONFORME{D} — ces constantes sont lues par un"
              " static_assert et le fichier ne les nomme pas :")
        for ident in manquants:
            print(f"     {ident}   ({', '.join(sorted(lus[ident]))})")
        print()
        print("  Une relation est une inegalite : elle a du jeu. La valeur peut")
        print("  donc bouger en gardant la relation vraie, et rien ne le verrait.")
        print("  Ajoute chaque constante au fichier de vecteurs, ou dis")
        print("  explicitement pourquoi elle n'a pas a etre gardee.")
        return 1

    print(f"{V}✅ CONFORME{D} — chaque constante lue par un static_assert de mise")
    print("   en page est nommee par le fichier de vecteurs, donc sa valeur est")
    print("   gardee en plus de ses relations.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
