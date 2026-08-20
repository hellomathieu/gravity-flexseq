#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

AVR_GCC="$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-gcc"
AVR_SIZE="$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-size"
AVR_OBJCOPY="$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-objcopy"
ELF="gravity-ch1-test.elf"
HEX="gravity-ch1-test.hex"
VCD="gravity-ch1-test.vcd"

# Resolution de simavr : PATH d'abord, puis le clone local en REPLI. L'ancienne
# version codait ce clone en dur, ce qui faisait dependre le harnais d'un dossier
# hors du depot (invisible a un `git clone`, et dans ~/Downloads qu'on vide). On
# charge desormais le .hex avec -m/-f explicites : le simavr d'Homebrew refuse
# les ELF (`ELF format is not supported by this build`), le clone les accepte —
# le .hex marche avec les deux. Le clone reste un confort (support ELF, sources
# des pieces virtuelles), plus un prerequis.
find_simavr() {
  for candidate in \
      "$(command -v simavr 2>/dev/null || true)" \
      "$(command -v run_avr 2>/dev/null || true)" \
      "$HOME/Downloads/simavr-source/simavr/run_avr"; do
    if [ -n "$candidate" ] && [ -x "$candidate" ]; then echo "$candidate"; return 0; fi
  done
  return 1
}

if ! SIMAVR="$(find_simavr)"; then
  echo "erreur : ni 'simavr' ni 'run_avr' trouves (PATH, puis ~/Downloads/simavr-source)." >&2
  echo "Installe-le : brew install simavr" >&2
  exit 127
fi

echo "==> BUILD"

"$AVR_GCC" \
  -mmcu=atmega328p \
  -DF_CPU=16000000UL \
  -Os \
  -g \
  -o "$ELF" \
  main.c

"$AVR_SIZE" "$ELF"

# .hex en plus de l'ELF : c'est le format que les deux binaires simavr acceptent.
"$AVR_OBJCOPY" -O ihex -R .eeprom "$ELF" "$HEX"

echo "BUILD       OK"

echo
echo "==> SIMULATION"

rm -f "$VCD"

set +e

timeout 1 \
  "$SIMAVR" \
  -v \
  -m atmega328p \
  -f 16000000 \
  -o "$VCD" \
  --add-trace CH1=portpin@0x07/0x44 \
  "$HEX"

SIMAVR_EXIT=$?

set -e

# timeout terminates simavr intentionally.
# 124 is therefore the expected exit code here.
if [ "$SIMAVR_EXIT" -ne 124 ]; then
    echo "SIMULATION  FAIL (exit=$SIMAVR_EXIT)"
    exit 1
fi

if [ ! -s "$VCD" ]; then
    echo "SIMULATION  FAIL (VCD absent or empty)"
    exit 1
fi

echo "SIMULATION  OK"
echo "VCD         OK"

echo
echo "==> TESTS"

# test_square_wave.py assertionne LE VCD DE CE SCRIPT. Auparavant on lancait ici
# test_ch1_signal.py, qui lit celui de run_firmware_ch1_test.sh : la simulation
# de main.c n'etait verifiee par rien.
python3 -m unittest -v \
  test_vcd_parser.py \
  test_square_wave.py

echo
echo "TESTS       OK"
echo
echo "ALL         OK"
