#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

# Localise pio : PATH d'abord, sinon l'installation PlatformIO par defaut.
# Meme resolution que tools/run-cpp-tests.sh. Sans elle, ce script echouait avec
# « pio: command not found » des que PlatformIO n'etait pas dans le PATH.
if command -v pio >/dev/null 2>&1; then
  PIO="pio"
elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then
  PIO="$HOME/.platformio/penv/bin/pio"
else
  echo "erreur : 'pio' introuvable (ni dans le PATH, ni dans ~/.platformio/penv/bin)." >&2
  echo "Installe PlatformIO Core : https://docs.platformio.org/en/latest/core/installation/" >&2
  exit 127
fi

"$PIO" run -e nanoatmega328
printf '\n--- Detailed memory report ---\n'
"$PIO" run -e nanoatmega328 -v
