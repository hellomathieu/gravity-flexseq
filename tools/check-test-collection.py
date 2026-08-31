#!/usr/bin/env python3
#
# Verifie que CHAQUE repertoire test/test_* est collecte par au moins un
# test_filter de platformio.ini, et que chaque entree de test_filter designe au
# moins un repertoire.
#
# Pourquoi. Decision D5 du proprietaire, 2026-08-31 : platformio.ini fait
# autorite sur l'inventaire des tests. Un repertoire absent de tout test_filter
# n'est donc pas une divergence d'inventaire, c'est un DEFAUT DE CONFIGURATION :
# rien ne le collecte, et la suite sort verte tout en etant incomplete. Le commit
# f0d6134 a produit exactement cela, et six tests sont restes silencieux quatre
# mois.
#
# Ce controle ne cree AUCUNE seconde liste : il compare le systeme de fichiers a
# platformio.ini. Rouge dans les deux sens, comme la suite de caracterisation.
#
# Usage :
#   ./tools/check-test-collection.py
#   EXTRA_DIR=test_faux ./tools/check-test-collection.py   # contre-epreuve rouge
#
# Sortie 0 si tout est collecte, 1 sur defaut, 2 si platformio.ini est illisible.

import fnmatch
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INI = os.path.join(ROOT, "platformio.ini")
TEST_DIR = os.path.join(ROOT, "test")


def read_filters():
    try:
        with open(INI, encoding="utf-8") as fh:
            text = fh.read()
    except OSError as exc:
        print("platformio.ini illisible : %s" % exc, file=sys.stderr)
        sys.exit(2)
    filters = {}
    for match in re.finditer(r"^\[env:([^\]]+)\](.*?)(?=^\[|\Z)", text, re.S | re.M):
        env, body = match.group(1), match.group(2)
        found = re.search(r"^test_filter\s*=\s*((?:\n[ \t]+\S+)+)", body, re.M)
        if not found:
            continue
        entries = [line.strip() for line in found.group(1).split("\n") if line.strip()]
        if entries:
            filters[env] = entries
    if not filters:
        print("aucun test_filter trouve dans platformio.ini", file=sys.stderr)
        sys.exit(2)
    return filters


def read_dirs():
    names = sorted(
        name
        for name in os.listdir(TEST_DIR)
        if name.startswith("test_") and os.path.isdir(os.path.join(TEST_DIR, name))
    )
    extra = os.environ.get("EXTRA_DIR", "").strip()
    if extra:
        names = sorted(set(names) | {extra})
    return names, extra


def main():
    filters = read_filters()
    dirs, extra = read_dirs()

    pairs = [(env, pat) for env, pats in sorted(filters.items()) for pat in pats]
    covered = {d: [env for env, pat in pairs if fnmatch.fnmatch(d, pat)] for d in dirs}
    matched = {
        (env, pat): [d for d in dirs if fnmatch.fnmatch(d, pat)] for env, pat in pairs
    }

    orphan_dirs = [d for d, envs in covered.items() if not envs]
    orphan_pats = [(env, pat) for (env, pat), hits in sorted(matched.items()) if not hits]

    print("=========== COLLECTE DES TESTS (decision D5) ===========")
    print("  repertoires test/test_*        %d" % len(dirs))
    print("  environnements avec un filtre  %d" % len(filters))
    for env in sorted(filters):
        hits = sorted({d for env2, pat in pairs if env2 == env for d in matched[(env, pat)]})
        print("    %-20s %2d entrees -> %2d modules" % (env, len(filters[env]), len(hits)))
    if extra:
        print("  EXTRA_DIR                      %s  (contre-epreuve)" % extra)

    ok = True
    if orphan_dirs:
        ok = False
        print("  ❌ repertoires collectes par AUCUN filtre : %d" % len(orphan_dirs))
        for d in orphan_dirs:
            print("       test/%s" % d)
        print("     Ces tests sont SILENCIEUX. Ajouter chacun au test_filter de son")
        print("     environnement dans platformio.ini, qui fait autorite (D5).")
    else:
        print("  ✅ tout repertoire est collecte    0 orphelin")

    if orphan_pats:
        ok = False
        print("  ❌ entrees de test_filter sans repertoire : %d" % len(orphan_pats))
        for env, pat in orphan_pats:
            print("       [env:%s] %s" % (env, pat))
        print("     Une entree perimee ne collecte rien et masque son propre retrait.")
    else:
        print("  ✅ toute entree designe un module  0 perimee")

    print("=======================================================")
    if not ok:
        print("  DEFAUT DE CONFIGURATION. La suite peut sortir verte en etant incomplete.")
        return 1
    print("  L'inventaire de platformio.ini couvre exactement les repertoires presents.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
