#!/usr/bin/env bash
#
# Lance TOUTE la suite de tests FlexSeq : C++ domaine (env host-native) + modele
# TypeScript (sim/). Aucun hardware requis.
#
# Usage :
#   ./tools/run-all-tests.sh          # C++ puis TS, avec recap
#   ./tools/run-all-tests.sh --ts     # TS uniquement
#   ./tools/run-all-tests.sh --cpp    # C++ uniquement
#
# Sort en erreur (code != 0) si au moins une suite echoue.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

RUN_CPP=1
RUN_TS=1
case "${1:-}" in
  --ts)  RUN_CPP=0 ;;
  --cpp) RUN_TS=0 ;;
  "")    ;;
  *) echo "argument inconnu : $1 (attendu : --ts | --cpp | rien)" >&2; exit 2 ;;
esac

cpp_status="skip"
ts_status="skip"

if [ "$RUN_CPP" = "1" ]; then
  echo "=========================================="
  echo "  Tests C++ (env native, sans hardware)"
  echo "=========================================="
  if "$REPO_ROOT/tools/run-cpp-tests.sh"; then
    cpp_status="OK"
  else
    cpp_status="ECHEC"
  fi
  echo
fi

if [ "$RUN_TS" = "1" ]; then
  echo "=========================================="
  echo "  Tests TypeScript (sim/)"
  echo "=========================================="
  if [ ! -d "$REPO_ROOT/sim/node_modules" ]; then
    echo "sim/node_modules absent -> installation des dependances..."
    ( cd "$REPO_ROOT/sim" && npm install )
  fi
  if ( cd "$REPO_ROOT/sim" && npm test ); then
    ts_status="OK"
  else
    ts_status="ECHEC"
  fi
  echo
fi

echo "=================== RECAP ==================="
printf "  C++ (native) : %s\n" "$cpp_status"
printf "  TypeScript   : %s\n" "$ts_status"
echo "============================================"

# Code de sortie : erreur si l'une des suites lancees a echoue.
if [ "$cpp_status" = "ECHEC" ] || [ "$ts_status" = "ECHEC" ]; then
  exit 1
fi
exit 0
