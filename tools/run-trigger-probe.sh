#!/usr/bin/env bash
#
# Verifie la FONCTION MUSICALE du firmware de production : les six sorties
# emettent-elles le motif ecrit, quand, et pendant combien de temps ?
#
# POURQUOI CETTE SONDE EXISTE. Le chemin « contenu du pattern -> onset ->
# impulsion sur une broche » n'etait exerce par AUCUN binaire : `main.cpp` emet
# les triggers mais sa banque est vide et aucune UI ne permet encore d'y ecrire,
# `wokwi_main.cpp` porte du contenu mais n'instancie pas de TriggerSequencer. Les
# tests natifs validaient le domaine, run-screen-dump le rendu, run-blocking-probe
# le temps — et la fonction du module n'etait observee nulle part.
#
# Le firmware n'est pas instrumente : le contenu du pattern est ecrit dans la RAM
# simulee a l'adresse du symbole `patternBank`, lue par `avr-nm`. Le binaire
# mesure est celui qui sera flashe. Voir tools/simavr-ssd1306/trigger_probe.c.
#
# VERDICT — quatre criteres, et deux mesures rapportees :
#   1. les SIX sorties emettent (elles selectionnent toutes le pattern 0 par
#      defaut, donc un silence sur l'une est un defaut de cablage logique) ;
#   2. le train est REGULIER — un ecart par step, a 5 % de la mediane. Depuis que
#      le defaut d'usine est CLOCK (PRD 4.2), c'est ce que le firmware emet, et
#      le motif injecte reste dans la banque sans etre joue : le critere verifie
#      donc aussi que CLOCK IGNORE le contenu du pattern, comme l'original ;
#   3. les six lignes battent ensemble, fronts a moins de 200 us ;
#   4. la gigue reste sous JITTER_BUDGET_PCT d'un step.
# Rapportees : la largeur d'impulsion reelle, et PULSE de l'expandeur MIDI.
#
# ⚠️ LE CHEMIN « CONTENU DU PATTERN -> SORTIE » N'EST PLUS EXERCE ICI, et c'est
# une suspension datee, pas un abandon. Le mode SEQ n'est atteignable par aucun
# moyen exterieur au firmware : la seule UI qui le reglera arrive au lot 11. Le
# critere revient au LOT 10, ou le format EEPROM v2 portera le mode : la sonde
# prechargera une EEPROM avec mode=SEQ et le contenu du pattern, ce qui passe par
# un format documente au lieu d'un decalage dans une structure privee.
#
# `MUTATE=<step>` ajoute un step a l'injection. Tant que le critere porte sur le
# train CLOCK, il ne le rougit plus — il rougira de nouveau au lot 10.
#
# `DROP=<n>` ignore un front sur n : le train n'est plus regulier et le critere 2
# passe au rouge. C'est ainsi que ce chemin a ete exerce.
#
# Reglages : DURATION (defaut 20 s de simulation), JITTER_BUDGET_PCT (defaut 2).
# Sortie 0 si les quatre passent, 1 sinon, 127 si un outil manque.

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DURATION="${DURATION:-20}"
JITTER_BUDGET_PCT="${JITTER_BUDGET_PCT:-2}"
# TEMPO change le tempo par defaut du firmware ET l'attente du harnais, de sorte
# que « la duree de step suit le tempo » soit verifiable et pas seulement crue.
TEMPO="${TEMPO:-120}"
STEP_TOLERANCE_PCT="${STEP_TOLERANCE_PCT:-1}"

if [ -t 1 ]; then
  C_OK=$'\033[32m'; C_ERR=$'\033[31m'; C_DIM=$'\033[2m'; C_B=$'\033[1m'; C_0=$'\033[0m'; TTY=1
else
  C_OK=""; C_ERR=""; C_DIM=""; C_B=""; C_0=""; TTY=0
fi
progress() { [ "$TTY" = "1" ] && printf '  %s…%s %s\r' "$C_DIM" "$C_0" "$1"; return 0; }
die() { printf '  %s❌%s %s\n' "$C_ERR" "$C_0" "$1" >&2; exit "${2:-1}"; }

if command -v pio >/dev/null 2>&1; then PIO="pio"
elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then PIO="$HOME/.platformio/penv/bin/pio"
else die "'pio' introuvable (ni PATH, ni ~/.platformio/penv/bin)." 127
fi

PREFIX=""
for p in /opt/homebrew /usr/local; do
  [ -f "$p/lib/libsimavrparts.a" ] && PREFIX="$p" && break
done
[ -n "$PREFIX" ] || die "libsimavrparts absente. brew install simavr" 127

LOG="$(mktemp)"; BIN="$(mktemp -d)/trigger_probe"
trap 'rm -f "$LOG"; rm -rf "$(dirname "$BIN")"' EXIT

# --- 1. Harnais --------------------------------------------------------------
progress "compilation du harnais"
if cc -O2 -Wall -DBPM="$TEMPO" -I"$PREFIX/include/simavr" -I"$PREFIX/include" \
     "$ROOT/tools/simavr-ssd1306/trigger_probe.c" -o "$BIN" \
     -L"$PREFIX/lib" -lsimavrparts -lsimavr -lelf > "$LOG" 2>&1; then
  printf '  %s✅%s harnais compile        %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" "$PREFIX" "$C_0"
else
  printf '\n'; cat "$LOG"; die "compilation du harnais en echec"
fi

# --- 2. Firmware -------------------------------------------------------------
progress "build env:nanoatmega328"
PIO_EXTRA=""
if [ "$TEMPO" != "120" ]; then
  PIO_EXTRA="-DFLEXSEQ_DEFAULT_TEMPO=$TEMPO"
fi
if PLATFORMIO_BUILD_FLAGS="$PIO_EXTRA" "$PIO" run -e nanoatmega328 -d "$ROOT" > "$LOG" 2>&1; then
  printf '  %s✅%s firmware               %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" \
    "$(grep -E '^RAM:' "$LOG" | sed 's/.*(used /RAM /; s/ bytes from .*/ o/')" "$C_0"
else
  printf '\n'; tail -30 "$LOG"; die "build du firmware en echec"
fi

# --- 3. Adresse de la banque -------------------------------------------------
# Sans elle la banque reste vide et aucun trigger ne peut sortir : le harnais
# refuse de mesurer plutot que de rapporter un silence comme un resultat.
AVR_NM="$(command -v avr-nm || echo "$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-nm")"
BANK=""
if [ -x "$AVR_NM" ]; then
  SYM="$("$AVR_NM" --radix=x "$ROOT/.pio/build/nanoatmega328/firmware.elf" \
         | grep -E 'N_111patternBankE$' | head -1 | cut -d' ' -f1)"
  [ -n "$SYM" ] && BANK="$(printf '0x%x' $(( 0x$SYM - 0x800000 )))"
fi
[ -n "$BANK" ] || die "symbole \`patternBank\` introuvable : avr-nm absent ou binaire inattendu."
printf '  %s✅%s symbole                %spatternBank %s%s\n' \
  "$C_OK" "$C_0" "$C_DIM" "$BANK" "$C_0"

# --- 4. Simulation -----------------------------------------------------------
progress "simulation ($DURATION s)"
set +e
"$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$DURATION" "$BANK" > "$LOG" 2>&1
PROBE=$?
set -e
if [ "$PROBE" -ne 0 ]; then
  printf '  %s⚠%s  la sonde s'"'"'est terminee anormalement (code %d)\n' \
    "$C_DIM" "$C_0" "$PROBE"
fi
printf '  %s✅%s simulation             %s%s s simulees%s\n' "$C_OK" "$C_0" "$C_DIM" "$DURATION" "$C_0"

JITTER_BUDGET_PCT="$JITTER_BUDGET_PCT" STEP_TOLERANCE_PCT="$STEP_TOLERANCE_PCT" python3 - "$LOG" <<'PY'
import os, re, sys

txt = open(sys.argv[1], errors='replace').read()
tty = sys.stdout.isatty()
OK, ERR, DIM, B, Z = ('\033[32m', '\033[31m', '\033[2m', '\033[1m', '\033[0m') if tty else ('',) * 5
mark = lambda good: f"{OK}✅{Z}" if good else f"{ERR}❌{Z}"
budget_pct = float(os.environ["JITTER_BUDGET_PCT"])
step_tol = float(os.environ["STEP_TOLERANCE_PCT"])

m = re.search(r"RESULTAT (.*)", txt)
if not m:
    print(f"  {mark(False)} sortie de la sonde illisible")
    print("".join(c for c in txt if 32 <= ord(c) < 127 or c == "\n"))
    sys.exit(1)

kv = dict(p.split("=", 1) for p in m.group(1).split())
lines_on = int(kv["lignes_actives"]); lines_exp = int(kv["attendu"])
gaps_ok, gaps_total = (int(x) for x in kv["ecarts_ok"].split("/"))
width = float(kv["largeur_med"])
jit_med, jit_max = float(kv["gigue_med"]), float(kv["gigue_max"])
step_ms = float(kv["step_ms"])
same, coincident = kv["meme_compte"] == "1", kv["coincident"] == "1"
pulses, pulse_line = int(kv["impulsions_ch1"]), int(kv["pulse"])

all_lines = lines_on == lines_exp
pattern_ok = gaps_total > 0 and gaps_ok == gaps_total
jit_pct = 100.0 * jit_max / step_ms if step_ms else 0.0
jit_ok = jit_pct <= budget_pct
step_measured = float(kv.get("step_mesure", "0"))
bpm = int(kv.get("bpm", "0"))
step_err_pct = abs(step_measured - step_ms) / step_ms * 100.0 if step_ms else 100.0
step_ok = step_measured > 0.0 and step_err_pct <= step_tol
duty = 100.0 * width / step_ms if step_ms else 0.0
duty_ok = duty < 50.0

print()
print(f"{B}=========== FONCTION MUSICALE (firmware de production) ==========={Z}")
print(f"  {mark(all_lines)} Sorties actives    {lines_on}/{lines_exp}   "
      f"{DIM}— les 6 channels selectionnent le pattern 0 par defaut{Z}")
print(f"  {mark(pattern_ok)} Train regulier     {gaps_ok}/{gaps_total} ecarts   "
      f"{DIM}— un par step, a 5 % de la mediane ; le motif injecte est ignore{Z}")
print(f"  {mark(same and coincident)} Six lignes en phase "
      f"{'meme compte, fronts < 200 us' if (same and coincident) else 'DESACCORD'}")
print(f"  {mark(step_ok)} Tempo applique     {step_measured:.2f} ms par step   "
      f"{DIM}— attendu {step_ms:.2f} ms a {bpm} BPM ; ecart {step_err_pct:.2f} %"
      f" ; tolerance {step_tol:g} %{Z}")
print(f"  {mark(jit_ok)} Gigue              {jit_max:.2f} ms   "
      f"{DIM}— {jit_pct:.2f} % d'un step de {step_ms:.0f} ms ; budget {budget_pct:g} %"
      f" ; mediane {jit_med:.2f} ms{Z}")
print(f"{B}================================================================={Z}")
print(f"  Impulsions observees       : {pulses} sur le channel 1")
print(f"  Largeur d'impulsion        : {width:.2f} ms  "
      f"({duty:.1f} % du step){'' if duty_ok else '   <-- PLUS DE LA MOITIE DU STEP'}")
print(f"  PULSE (expandeur MIDI)     : {pulse_line} impulsion(s)")
print()

# La largeur meritait d'etre expliquee plutot que seulement chiffree.
if width > 6.0:
    print(f"  {DIM}La largeur depasse les 5 ms de DEFAULT_TRIGGER_DURATION_MS parce que")
    print(f"  l'extinction a lieu dans outputs[ch].Process(), en FIN de loop() : une")
    print(f"  impulsion dure 5 ms ARRONDIS AU PASSAGE SUIVANT. Ce n'est pas un defaut")
    print(f"  de libGravity, c'est la granularite de notre boucle.{Z}")
if pulse_line == 0:
    print(f"  {DIM}PULSE reste muet : main.cpp ne pilote pas gravity.pulse. Observation,")
    print(f"  non defaut — l'expandeur MIDI n'est pas encore dans le chemin.{Z}")

ok = all_lines and pattern_ok and same and coincident and jit_ok and duty_ok and step_ok
if ok:
    print(f"\n  Les six sorties emettent le train CLOCK, en phase, au bon tempo.")
    print(f"  {DIM}Le chemin pattern -> sortie revient au lot 10 (EEPROM v2).{Z}")
else:
    print(f"\n  {ERR}Au moins un critere echoue — ne pas flasher sur cette base.{Z}")
sys.exit(0 if ok else 1)
PY
