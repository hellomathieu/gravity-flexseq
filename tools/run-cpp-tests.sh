#!/usr/bin/env bash
#
# Lance les tests C++ du domaine FlexSeq sur l'env host-native (sans hardware).
#
# Usage :
#   ./tools/run-cpp-tests.sh                  # tous les tests (env native)
#   ./tools/run-cpp-tests.sh test_pattern     # filtre un test (option -f de pio)
#   ./tools/run-cpp-tests.sh -l               # liste les tests sans les executer
#   VERBOSE=1 ./tools/run-cpp-tests.sh        # sortie detaillee (-vvv)
#
set -euo pipefail

# Racine du repo = dossier parent de tools/ (peu importe d'ou on lance le script).
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# Localise pio : PATH d'abord, sinon l'installation PlatformIO par defaut.
if command -v pio >/dev/null 2>&1; then
  PIO="pio"
elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then
  PIO="$HOME/.platformio/penv/bin/pio"
else
  echo "erreur : 'pio' introuvable (ni dans le PATH, ni dans ~/.platformio/penv/bin)." >&2
  echo "Installe PlatformIO Core : https://docs.platformio.org/en/latest/core/installation/" >&2
  exit 127
fi

ENV="native"
ARGS=("test" "-e" "$ENV")

[ "${VERBOSE:-0}" = "1" ] && ARGS+=("-vvv")

# Premier argument : soit -l/--list, soit un filtre de test.
if [ "${1:-}" = "-l" ] || [ "${1:-}" = "--list" ]; then
  ARGS+=("--list-tests")
elif [ -n "${1:-}" ]; then
  ARGS+=("-f" "$1")
fi

echo "==> $PIO ${ARGS[*]}"
exec "$PIO" "${ARGS[@]}"
