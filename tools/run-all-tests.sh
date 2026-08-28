#!/usr/bin/env bash
#
# Lance TOUTE la suite de tests FlexSeq : C++ domaine (env host-native), modele
# TypeScript (sim/), puis caracterisation de la dependance libGravity figee.
# Aucun hardware requis.
#
# Les deux categories restent DISTINCTES (regle CLAUDE.md) :
#   - acceptation FlexSeq  -> doit etre verte ;
#   - caracterisation libGravity -> reproduit des anomalies auditees, donc
#     partiellement rouge par construction. Ce qui est verifie la, c'est que
#     l'ensemble des echecs est EXACTEMENT celui audite (voir
#     tools/run-libgravity-tests.sh).
#
# Usage :
#   ./tools/run-all-tests.sh              # les trois suites, avec recap
#   ./tools/run-all-tests.sh --ts         # TypeScript uniquement
#   ./tools/run-all-tests.sh --cpp        # C++ acceptation uniquement
#   ./tools/run-all-tests.sh --libgravity # caracterisation uniquement
#
# Sort en erreur (code != 0) si au moins une suite echoue.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

RUN_CPP=1
RUN_TS=1
RUN_LIB=1
case "${1:-}" in
  --ts)         RUN_CPP=0; RUN_LIB=0 ;;
  --cpp)        RUN_TS=0;  RUN_LIB=0 ;;
  --libgravity) RUN_CPP=0; RUN_TS=0  ;;
  "")    ;;
  *) echo "argument inconnu : $1 (attendu : --ts | --cpp | --libgravity | rien)" >&2; exit 2 ;;
esac

cpp_status="skip"
ts_status="skip"
tc_status="skip"
lib_status="skip"
img_status="skip"
ad_status="skip"

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
  echo "=========================================="
  echo "  Typage TypeScript (tsc --noEmit)"
  echo "=========================================="
  # vitest ne type pas : voir test/README.
  if ( cd "$REPO_ROOT/sim" && npm run --silent typecheck ); then
    tc_status="OK"
    echo "  aucune erreur de typage"
  else
    tc_status="ECHEC"
  fi
  echo
fi

if [ "$RUN_CPP" = "1" ]; then
  echo "=========================================="
  echo "  Tests adaptateur d'entrees (env native_adapter)"
  echo "=========================================="
  if "$REPO_ROOT/tools/run-adapter-tests.sh"; then
    ad_status="OK"
  else
    ad_status="ECHEC"
  fi
  echo
fi

if [ "$RUN_CPP" = "1" ]; then
  echo "=========================================="
  echo "  Image EEPROM du generateur (octets emis)"
  echo "=========================================="
  if "$REPO_ROOT/tools/run-eeprom-image-check.sh"; then
    img_status="OK"
  else
    img_status="ECHEC"
  fi
  echo
fi

if [ "$RUN_LIB" = "1" ]; then
  echo "=========================================="
  echo "  Caracterisation libGravity"
  echo "=========================================="
  if "$REPO_ROOT/tools/run-libgravity-tests.sh"; then
    lib_status="OK"
  else
    lib_status="ECHEC"
  fi
  echo
fi

echo "=================== RECAP ==================="
printf "  C++ acceptation (native)   : %s\n" "$cpp_status"
printf "  Adaptateur d'entrees       : %s\n" "$ad_status"
printf "  Image EEPROM generee       : %s\n" "$img_status"
printf "  TypeScript (sim/)          : %s\n" "$ts_status"
printf "  Typage TypeScript          : %s\n" "$tc_status"
printf "  libGravity caracterisation : %s\n" "$lib_status"
echo "============================================"

# Code de sortie : erreur si l'une des suites lancees a echoue.
if [ "$cpp_status" = "ECHEC" ] || [ "$ad_status" = "ECHEC" ] \
   || [ "$ts_status" = "ECHEC" ] || [ "$tc_status" = "ECHEC" ] \
   || [ "$img_status" = "ECHEC" ] || [ "$lib_status" = "ECHEC" ]; then
  exit 1
fi
exit 0
