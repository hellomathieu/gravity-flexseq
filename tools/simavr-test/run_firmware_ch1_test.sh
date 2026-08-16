#!/usr/bin/env bash

set -u

PROJECT_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TEST_DIR="$(cd "$(dirname "$0")" && pwd)"

FIRMWARE="${PROJECT_ROOT}/.pio/build/simavr/firmware.elf"
VCD="${TEST_DIR}/gravity-flexseq-ch1.vcd"
SIMAVR="${HOME}/Downloads/simavr-source/simavr/run_avr"

echo "==> BUILD"

cd "$PROJECT_ROOT"

pio run -e simavr

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