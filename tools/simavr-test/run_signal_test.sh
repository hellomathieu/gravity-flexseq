#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

AVR_GCC="$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-gcc"
AVR_SIZE="$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-size"
SIMAVR="$HOME/Downloads/simavr-source/simavr/run_avr"

ELF="gravity-ch1-test.elf"
VCD="gravity-ch1-test.vcd"

echo "==> BUILD"

"$AVR_GCC" \
  -mmcu=atmega328p \
  -DF_CPU=16000000UL \
  -Os \
  -g \
  -o "$ELF" \
  main.c

"$AVR_SIZE" "$ELF"

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
  "$ELF"

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

python3 -m unittest -v \
  test_vcd_parser.py \
  test_ch1_signal.py

echo
echo "TESTS       OK"
echo
echo "ALL         OK"
