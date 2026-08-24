#!/usr/bin/env bash
#
# Tests d'acceptation de l'ADAPTATEUR D'ENTREES (env:native_adapter).
#
# Troisieme categorie, distincte des deux autres et pour une raison structurelle :
#   - env:native            domaine pur, ni Arduino ni libGravity ;
#   - env:native_libgravity caracterisation de la dependance figee ;
#   - env:native_adapter    NOTRE code de liaison, compile contre les stubs
#                           Arduino de test/mocks/ et les en-tetes de libGravity.
#
# Ce qui se teste ici ne se teste nulle part ailleurs : la politique qui decide
# qu'un maintien ayant servi a tourner n'est PAS un appui long. Elle depend du
# temps et de l'etat des broches, donc le domaine ne peut pas la voir.
#
# Ces tests DOIVENT etre verts.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

if command -v pio >/dev/null 2>&1; then
  PIO="pio"
elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then
  PIO="$HOME/.platformio/penv/bin/pio"
else
  echo "erreur : 'pio' introuvable (ni dans le PATH, ni dans ~/.platformio/penv/bin)." >&2
  exit 127
fi

LIBDEPS="$REPO_ROOT/.pio/libdeps/nanoatmega328/libGravity/src"
if [ ! -d "$LIBDEPS" ]; then
  echo "libGravity absent de .pio/libdeps -> construction de l'environnement AVR..."
  "$PIO" run -e nanoatmega328 >/dev/null
fi

exec "$PIO" test -e native_adapter "$@"
