#!/usr/bin/env bash

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DURATION="${DURATION:-60}"
TEMPO="${TEMPO:-120}"
STEPS="${STEPS:-0,3,4,9,15}"
RATCHETS="${RATCHETS:-}"
SUBDIV="${SUBDIV:-1}"
LENGTH="${LENGTH:-16}"
MODE="${MODE:-seq}"
SAVE="${SAVE:-}"
EDIT="${EDIT:-}"

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  cat <<'USAGE'
run-drift-probe.sh — derive cumulative du moteur de sequence, en simulation.

  DURATION=60      secondes SIMULEES
  TEMPO=120        BPM
  SUBDIV=1         valeur SUBDIV libGravity (1 = noire = 96 ticks)
  STEPS=0,3,4,9,15 steps actifs du pattern
  RATCHETS=        liste step:code, par exemple 0:6,3:2
  LENGTH=16        longueur jouee
  MODE=seq         seq ou clock
  EDIT=1           firmware pose sur l'ecran EDIT, transport demarre par lui-meme
  SAVE=1           conserve le CSV et le resume dans tools/timing-runs/

Sortie 0 si aucune derive cumulative n'est detectee et si la coherence tient,
1 sinon, 127 si un outil manque.
USAGE
  exit 0
fi

if [ -t 1 ]; then
  C_OK=$'\033[32m'; C_ERR=$'\033[31m'; C_DIM=$'\033[2m'; C_B=$'\033[1m'; C_0=$'\033[0m'; TTY=1
else
  C_OK=""; C_ERR=""; C_DIM=""; C_B=""; C_0=""; TTY=0
fi
progress() { [ "$TTY" = "1" ] && printf '  %s…%s %s\r' "$C_DIM" "$C_0" "$1"; return 0; }
die() { printf '  %s❌%s %s\n' "$C_ERR" "$C_0" "$1" >&2; exit "${2:-1}"; }
ok() { printf '  %s✅%s %-22s %s%s%s\n' "$C_OK" "$C_0" "$1" "$C_DIM" "$2" "$C_0"; }
bad() { printf '  %s❌%s %-22s %s%s%s\n' "$C_ERR" "$C_0" "$1" "$C_DIM" "$2" "$C_0"; }

if command -v pio >/dev/null 2>&1; then PIO="pio"
elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then PIO="$HOME/.platformio/penv/bin/pio"
else die "'pio' introuvable (ni PATH, ni ~/.platformio/penv/bin)." 127
fi
command -v npx >/dev/null 2>&1 || die "'npx' introuvable : l'analyse vit dans sim/." 127

PREFIX=""
for p in /opt/homebrew /usr/local; do
  [ -f "$p/lib/libsimavrparts.a" ] && PREFIX="$p" && break
done
[ -n "$PREFIX" ] || die "libsimavrparts absente. brew install simavr" 127

WORK="$(mktemp -d)"
LOG="$WORK/log"
trap 'rm -rf "$WORK"' EXIT

if [ "$SUBDIV" -gt 0 ] 2>/dev/null; then
  TICKS_PER_STEP=$((96 * SUBDIV))
else
  MULT=$((0 - SUBDIV))
  [ "$MULT" -gt 0 ] || die "SUBDIV invalide : $SUBDIV"
  [ $((96 % MULT)) -eq 0 ] || die "SUBDIV multiplicateur qui ne divise pas 96 : $SUBDIV"
  TICKS_PER_STEP=$((96 / MULT))
fi
TICK_US=$(awk -v t="$TEMPO" 'BEGIN { printf "%.6f", 60000000.0 / 96.0 / t }')

printf '%s=== SONDE DE DERIVE ===%s\n' "$C_B" "$C_0"

progress "compilation du harnais"
BIN="$WORK/drift_probe"
if cc -O2 -Wall -I"$PREFIX/include/simavr" -I"$PREFIX/include" \
     "$ROOT/tools/simavr-ssd1306/drift_probe.c" -o "$BIN" \
     -L"$PREFIX/lib" -lsimavrparts -lsimavr -lelf > "$LOG" 2>&1; then
  ok "harnais compile" "$PREFIX"
else
  printf '\n'; cat "$LOG"; die "compilation du harnais en echec"
fi

progress "build env:nanoatmega328"
PIO_EXTRA=""
[ "$TEMPO" != "120" ] && PIO_EXTRA="-DFLEXSEQ_DEFAULT_TEMPO=$TEMPO"
if [ -n "$EDIT" ]; then
  PIO_EXTRA="$PIO_EXTRA -DFLEXSEQ_START_IN_EDIT=1"
  export NO_PLAY=1
fi
if PLATFORMIO_BUILD_FLAGS="$PIO_EXTRA" "$PIO" run -e nanoatmega328 -d "$ROOT" > "$LOG" 2>&1; then
  ok "firmware" "$(grep -E '^RAM:' "$LOG" | sed 's/.*(used /RAM /; s/ bytes from .*/ o/')"
else
  printf '\n'; tail -30 "$LOG"; die "build du firmware en echec"
fi

progress "generateur d'image EEPROM"
GEN="$WORK/eeprom-image"
if c++ -std=gnu++11 -I"$ROOT/include" -o "$GEN" "$ROOT/tools/eeprom-image.cpp" \
     "$ROOT"/src/domain/*.cpp > "$LOG" 2>&1; then
  ok "generateur compile" "tools/eeprom-image.cpp"
else
  printf '\n'; cat "$LOG"; die "compilation du generateur en echec"
fi

GEN_ARGS="--mode $MODE --steps $STEPS --tempo $TEMPO --subdiv $SUBDIV"
[ -n "$RATCHETS" ] && GEN_ARGS="$GEN_ARGS --ratchet $RATCHETS"
if ! "$GEN" $GEN_ARGS > "$WORK/image.bin" 2>"$LOG"; then
  cat "$LOG"; die "generation de l'image EEPROM en echec"
fi
ok "image generee" "$(wc -c < "$WORK/image.bin" | tr -d ' ') octets, steps $STEPS, subdiv $SUBDIV"

progress "simulation ($DURATION s simulees)"
CSV="$WORK/onsets.csv"
START_S=$(date +%s)
set +e
"$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$DURATION" "$WORK/image.bin" \
  384 "$STEPS" "$RATCHETS" "$TICKS_PER_STEP" "$LENGTH" "$MODE" "$CSV" > "$LOG" 2>&1
PROBE=$?
set -e
WALL=$(( $(date +%s) - START_S ))
if [ "$PROBE" -ne 0 ]; then
  cat "$LOG"; die "la sonde s'est terminee anormalement (code $PROBE)"
fi
ok "simulation" "$DURATION s simulees en $WALL s reelles"
if [ -n "$EDIT" ]; then
  ok "ecran" "EDIT PATTERN, playhead anime, transport demarre par le firmware"
else
  ok "ecran" "principal, aucun element variant dans le temps"
fi

field() { grep -E "^$1 " "$LOG" | head -1 | awk '{print $2}'; }
TICKS="$(field ticks)"
MONO="$(field monotonic)"
MATCHED="$(field matched)"
DROPPED="$(field dropped)"
UNEXPECTED="$(field unexpected)"
EXPECTED_LINE="$(field expected_per_line)"
MIDI_START="$(field midi_start)"
MIDI_CLOCK="$(field midi_clock)"
SYNC="$(grep -E '^sync_gap_ticks ' "$LOG" | awk '{print $2}')"
PERIOD_US="$(grep -E '^isr_period_us ' "$LOG" | awk '{print $2}')"

printf '\n%s--- COHERENCE ---%s\n' "$C_B" "$C_0"
FAILED=0
[ "$MONO" = "1" ] && ok "horodatages" "strictement croissants" || { bad "horodatages" "non monotones"; FAILED=1; }
if [ "$SYNC" = "4..4" ]; then
  ok "ISR <-> tick" "MIDI Clock tous les 4 ticks, sans exception"
else
  bad "ISR <-> tick" "ecart sync24 observe : $SYNC (attendu 4..4)"; FAILED=1
fi
RESTART="$(field restart_after_edge)"
ANCHOR="$(field anchor_ms)"
if [ "$RESTART" = "0" ]; then
  ok "ancrage" "dernier MIDI Start a $ANCHOR ms, aucun redemarrage ensuite"
else
  bad "ancrage" "$RESTART redemarrage(s) apres le premier front"; FAILED=1
fi
ok "transport au boot" "$MIDI_START START / $(field midi_stop) STOP avant la mesure"
EXPECTED_TOTAL=$((EXPECTED_LINE * 6))
ok "onsets attendus" "$EXPECTED_LINE par ligne, $EXPECTED_TOTAL sur 6 lignes"
ok "onsets apparies" "$MATCHED"
if [ "$DROPPED" = "0" ]; then
  ok "onsets perdus" "0"
else
  bad "onsets perdus" "$DROPPED"; FAILED=1
fi
if [ "$UNEXPECTED" = "0" ]; then
  ok "onsets inattendus" "0"
else
  bad "onsets inattendus" "$UNEXPECTED"; FAILED=1
fi
ok "ticks" "$TICKS ticks, MIDI Clock $MIDI_CLOCK"
IDEAL_PPM=$(awk -v m="$PERIOD_US" -v i="$TICK_US" 'BEGIN { printf "%+.1f", (m - i) / i * 1e6 }')
ok "periode d'ISR (terme B)" "$PERIOD_US us contre $TICK_US ideal, soit $IDEAL_PPM ppm"

printf '\n'
set +e
( cd "$ROOT/sim" && npx vite-node src/analysis/driftReport.ts "$CSV" "$TICK_US" )
ANALYSIS=$?
set -e
[ "$ANALYSIS" -ne 0 ] && FAILED=1

if [ -n "$SAVE" ]; then
  DEST="$ROOT/tools/timing-runs"
  mkdir -p "$DEST"
  NAME="drift-${DURATION}s-${TEMPO}bpm-subdiv${SUBDIV}"
  [ -n "$EDIT" ] && NAME="$NAME-edit"
  [ -n "$RATCHETS" ] && NAME="$NAME-ratchet"
  cp "$CSV" "$DEST/$NAME.csv"
  cp "$LOG" "$DEST/$NAME.probe.txt"
  printf '  %s✅%s %-22s %s%s%s\n' "$C_OK" "$C_0" "conserve" "$C_DIM" "tools/timing-runs/$NAME.*" "$C_0"
fi

printf '\n'
if [ "$FAILED" -eq 0 ]; then
  printf '  %s✅ INSTRUMENTATION COHERENTE, aucune derive cumulative detectee.%s\n' "$C_OK" "$C_0"
else
  printf '  %s❌ Voir les lignes rouges ci-dessus.%s\n' "$C_ERR" "$C_0"
fi
exit "$FAILED"
