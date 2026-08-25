#!/usr/bin/env bash

set -u

unset -f grep awk sed 2>/dev/null || true

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
DELAY_MS="${DELAY_MS:-0}"
DELAY_AT_MS="${DELAY_AT_MS:-5000}"

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
  DELAY_MS=0       retard artificiel ponctuel, en ms (cadence ADC portee au maximum)
  DELAY_AT_MS=5000 instant du retard, en ms de simulation
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
TIMELINE="$WORK/timeline.csv"
START_S=$(date +%s)
set +e
DELAY_MS="$DELAY_MS" DELAY_AT_MS="$DELAY_AT_MS" \
"$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$DURATION" "$WORK/image.bin" \
  384 "$STEPS" "$RATCHETS" "$TICKS_PER_STEP" "$LENGTH" "$MODE" "$CSV" "$TIMELINE" > "$LOG" 2>&1
PROBE=$?
set -e
WALL=$(( $(date +%s) - START_S ))
if [ "$PROBE" -ne 0 ]; then
  cat "$LOG"; die "la sonde s'est terminee anormalement (code $PROBE)"
fi
cp "$LOG" /tmp/drift-last.log 2>/dev/null || true
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
ok "appariement glouton" "perdus $DROPPED, inattendus $UNEXPECTED — INDICATIF, voir le comptage brut"
ok "ticks" "$TICKS ticks, MIDI Clock $MIDI_CLOCK"
IDEAL_PPM=$(awk -v m="$PERIOD_US" -v i="$TICK_US" 'BEGIN { printf "%+.1f", (m - i) / i * 1e6 }')
ok "periode d'ISR (terme B)" "$PERIOD_US us contre $TICK_US ideal, soit $IDEAL_PPM ppm"

WINDOW_ARGS=""
if [ "$DELAY_MS" != "0" ]; then
  printf '\n%s--- RETARD ARTIFICIEL ---%s\n' "$C_B" "$C_0"
  WFROM="$(grep -E '^delay_window_ticks ' "$LOG" | awk '{print $2}')"
  WTO="$(grep -E '^delay_window_ticks ' "$LOG" | awk '{print $4}')"
  TICKS_IN="$(field ticks_in_window)"
  BYTES="$(field delay_adc_isr)"
  USART="$(field adc_isr_total)"
  EXPECT_TICKS=$(awk -v d="$DELAY_MS" -v t="$TICK_US" 'BEGIN { printf "%.1f", d * 1000.0 / t }')
  ok "fenetre demandee" "$DELAY_MS ms a $DELAY_AT_MS ms"
  ok "fenetre observee" "ticks $WFROM a $WTO"
  ok "ISR ADC pendant" "$BYTES dans la fenetre, $USART sur toute la course"
  if [ "$TICKS_IN" -gt 0 ] 2>/dev/null; then
    ok "ISR servies pendant" "$TICKS_IN ticks dans la fenetre, ~$EXPECT_TICKS attendus"
  else
    bad "ISR servies pendant" "aucun tick dans la fenetre : le harnais a fige la machine"; FAILED=1
  fi
  grep -E '^width_(before|during|after)_ms ' "$LOG" | while read -r k rest; do
    ok "impulsion ${k#width_}" "$rest"
  done
  WD_BEFORE="$(grep -E '^width_before_ms ' "$LOG" | awk '{print $2}')"
  WD_DURING="$(grep -E '^width_during_ms ' "$LOG" | awk '{print $4}')"
  T_IN="$(field timing_in_max_us)"
  T_OUT="$(field timing_out_med_us)"
  PROOF1=$(awk -v i="$T_IN" -v o="$T_OUT" 'BEGIN { print (o > 0 && i > o * 3) ? 1 : 0 }')
  PROOF2=$(awk -v b="$WD_BEFORE" -v d="$WD_DURING" 'BEGIN { print (b > 0 && d > b * 1.5) ? 1 : 0 }')
  if [ "$PROOF1" = "1" ]; then
    ok "preuve 1, retard" "onset au plus tard a $T_IN us contre $T_OUT us hors fenetre"
  else
    bad "preuve 1, retard" "$T_IN us dans la fenetre contre $T_OUT us hors fenetre : pas de retard mesure"
  fi
  if [ "$PROOF2" = "1" ]; then
    ok "preuve 2, impulsion" "$WD_DURING ms pendant contre $WD_BEFORE ms mediane avant"
  else
    bad "preuve 2, impulsion" "$WD_DURING ms contre $WD_BEFORE ms : aucune impulsion ne chevauche la fenetre"
  fi
  if [ "$PROOF1" = "0" ] && [ "$PROOF2" = "0" ]; then
    bad "campagne" "aucune des deux preuves : NON VALIDE"; FAILED=1
  fi
  WINDOW_ARGS="$WFROM $WTO"
fi

printf '\n'
set +e
[ -z "$WINDOW_ARGS" ] && WINDOW_ARGS="0 0"
( cd "$ROOT/sim" && npx vite-node src/analysis/driftReport.ts "$CSV" "$TICK_US" $WINDOW_ARGS \
    "$TIMELINE" "${MAX_DELAY_TICKS:-}" )
ANALYSIS=$?
set -e
[ "$ANALYSIS" -ne 0 ] && FAILED=1

if [ -n "$SAVE" ]; then
  DEST="$ROOT/tools/timing-runs"
  mkdir -p "$DEST"
  NAME="drift-${DURATION}s-${TEMPO}bpm-subdiv${SUBDIV}"
  [ -n "$EDIT" ] && NAME="$NAME-edit"
  [ -n "$RATCHETS" ] && NAME="$NAME-ratchet"
  [ "$DELAY_MS" != "0" ] && NAME="$NAME-delay${DELAY_MS}ms"
  cp "$CSV" "$DEST/$NAME.csv"
  cp "$TIMELINE" "$DEST/$NAME.timeline.csv"
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
