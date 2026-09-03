#!/usr/bin/env bash
# Compares the floating-point tempo arithmetic of uClock with an integer form.
#
# It serves conditions 3, 4 and 10 of the amendment of ADR 0008. Condition 4
# asks that this measurement live here, so that it replays.
#
# Three families, and every expected value is a LITERAL reproduction of the
# formula of the pinned dependency:
#   I  the internal path, an integer BPM to a timer interval   tolerance 0 us
#   E  the external path, a measured interval to an interval   tolerance 1 us
#   D  the displayed tempo that Clock::Tempo() returns         tolerance 0 bpm
#
# Levers: MUTATE=I|E|D breaks the integer form of one family, so its criterion
# is seen red.
#
# Exit codes: 0 every family inside tolerance, 1 a family outside, 2 the harness
# could not run.
#
# LIMIT, and it is named: this runs on the HOST. A float is IEEE 32 bits there
# as on the AVR, but the rounding of the AVR division is not verified here.
# Condition 5 of the amendment covers that gap, on the pins.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

MUTATE="${MUTATE:-}"

if [ -t 1 ]; then
  C_OK=$'\033[32m'; C_ERR=$'\033[31m'; C_DIM=$'\033[2m'; C_0=$'\033[0m'
else
  C_OK=""; C_ERR=""; C_DIM=""; C_0=""
fi

if ! command -v c++ >/dev/null 2>&1; then
  printf '  %s❌%s aucun compilateur c++ sur le PATH.\n' "$C_ERR" "$C_0" >&2
  exit 2
fi

if ! c++ -std=gnu++11 -O2 -Wall -Werror \
     -o "$WORK/tempo-equivalence" "$ROOT/tools/tempo-equivalence.cpp" \
     > "$WORK/build.log" 2>&1; then
  printf '  %s❌%s le programme ne compile pas :\n' "$C_ERR" "$C_0" >&2
  cat "$WORK/build.log" >&2
  exit 2
fi
printf '  %s✅%s programme compile     %s%s%s\n' \
  "$C_OK" "$C_0" "$C_DIM" "tools/tempo-equivalence.cpp" "$C_0"

ARGS=()
if [ -n "$MUTATE" ]; then
  ARGS+=(--mutate "$MUTATE")
fi

set +e
"$WORK/tempo-equivalence" "${ARGS[@]+"${ARGS[@]}"}"
RC=$?
set -e
exit "$RC"
