#!/usr/bin/env bash

set -u

PROJECT_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TEST_DIR="$(cd "$(dirname "$0")" && pwd)"

FIRMWARE="${PROJECT_ROOT}/.pio/build/simavr/firmware.hex"
VCD="${TEST_DIR}/gravity-flexseq-ch1.vcd"

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

if command -v pio >/dev/null 2>&1; then
  PIO="pio"
elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then
  PIO="$HOME/.platformio/penv/bin/pio"
else
  echo "erreur : 'pio' introuvable (ni PATH, ni ~/.platformio/penv/bin)." >&2
  exit 127
fi

echo "==> BUILD"

cd "$PROJECT_ROOT"

"$PIO" run -e simavr

if [ $? -ne 0 ]; then
    echo "BUILD       FAILED"
    exit 1
fi

echo "BUILD       OK"
echo

echo "==> SIMULATION"

rm -f "$VCD"

cd "$TEST_DIR"

timeout 0.5 \
"$SIMAVR" \
    -v \
    -m atmega328p \
    -f 16000000 \
    -o "$VCD" \
    --add-trace CH1=portpin@0x07/0x44 \
    "$FIRMWARE"

SIMAVR_EXIT=$?

# timeout returns 124 when it terminates simavr after the requested duration.
# This is expected: the firmware intentionally runs indefinitely.
if [ "$SIMAVR_EXIT" -ne 124 ]; then
    echo "SIMULATION FAILED (exit=$SIMAVR_EXIT)"
    exit 1
fi

if [ ! -s "$VCD" ]; then
    echo "VCD         FAILED"
    exit 1
fi

echo "SIMULATION  OK"
echo "VCD         OK"
echo

echo "==> TESTS"

python3 -m unittest -v \
    test_vcd_parser.py \
    test_ch1_signal.py

TEST_EXIT=$?

if [ "$TEST_EXIT" -ne 0 ]; then
    echo
    echo "TESTS       FAILED"
    exit "$TEST_EXIT"
fi

echo
echo "TESTS       OK"
echo
echo "ALL         OK"