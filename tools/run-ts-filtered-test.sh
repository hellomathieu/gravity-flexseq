#!/usr/bin/env bash
#
# Une course vitest RESTREINTE A UN TEST NOMME, avec refus du filtre vide.
#
# Elle existe pour la sonde de mutation. Un mutant qui abaisse l'hysteresis fait
# rougir QUATRE tests de test_length_cv, dont trois qui tiennent le litteral 8 et
# non la propriete. Au niveau de la suite entiere, un score N/N ne dit donc pas
# lequel a morde. Un filtre par nom exclut les trois autres de la course, et le
# rouge devient imputable au seul test de la propriete.
#
# LE REFUS DU FILTRE VIDE EST LA MOITIE UTILE DU SCRIPT. Mesure du 2026-08-31 :
# `vitest run <fichier> -t <motif introuvable>` sort **0** et annonce 11 tests
# ignores. Un test renomme transformerait donc le garde en vert permanent, et la
# sonde lirait le mutant comme non detecte, sans nommer la cause. C'est la meme
# regle que `--match` de run-mutation-probe.py, qui refuse un filtre ne
# selectionnant rien : une passe vide se lit comme une passe reussie.
#
# Sortie : 2 usage, 3 filtre vide, sinon le code de vitest.

set -uo pipefail

if [ "$#" -lt 2 ]; then
  echo "usage : $(basename "$0") <fichier de test relatif a sim/> <motif de nom>" >&2
  exit 2
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_FILE="$1"
NAME_FILTER="$2"
shift 2

REPORT="$(mktemp -t flexseq-vitest-XXXXXX.json)"
trap 'rm -f "$REPORT"' EXIT

cd "$REPO_ROOT/sim"
npx vitest run "$TEST_FILE" -t "$NAME_FILTER" \
  --reporter=json --outputFile="$REPORT" "$@"
VITEST_RC=$?

SELECTED="$(python3 - "$REPORT" <<'PY'
import json, sys
try:
    d = json.load(open(sys.argv[1]))
except Exception:
    print("unreadable")
    raise SystemExit(0)
print(int(d.get("numPassedTests", 0)) + int(d.get("numFailedTests", 0)))
PY
)"

if [ "$SELECTED" = "unreadable" ]; then
  echo "erreur : rapport vitest illisible ($REPORT) — verdict indecidable." >&2
  exit 3
fi

if [ "$SELECTED" -eq 0 ]; then
  echo "erreur : le filtre \"$NAME_FILTER\" ne selectionne AUCUN test de $TEST_FILE." >&2
  echo "         Un filtre vide sort vert et se lirait comme une reussite." >&2
  exit 3
fi

echo "$SELECTED test(s) selectionne(s) par \"$NAME_FILTER\" — code vitest $VITEST_RC."
exit "$VITEST_RC"
