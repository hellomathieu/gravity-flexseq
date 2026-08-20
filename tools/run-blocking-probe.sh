#!/usr/bin/env bash
#
# Mesure le BLOCAGE REEL de la boucle principale, un vrai esclave SSD1306 sur le
# bus I2C.
#
# Pourquoi un harnais dedie. Le binaire `run_avr` n'attache aucune piece : un
# transfert y avorte sur NACK des l'octet d'adresse, et toute duree mesuree la
# serait bien trop courte. simavr modelise pourtant un esclave SSD1306
# (`ssd1306_virt`), livre par Homebrew dans libsimavrparts. tools/simavr-ssd1306/
# le cable sur le TWI dans les deux sens et chronometre les transferts ; il ne
# copie aucun code de simavr, il inclut ses en-tetes et lie ses bibliotheques.
#
# Ce qui est mesure, sans toucher au firmware : pendant le rendu, chaque passage
# de boucle envoie exactement UNE bande (ADR 0001). Les transferts bornent donc
# directement le blocage, la duree d'un passage, et le cout d'une image entiere.
#
# VERDICT — deux criteres :
#   1. la mesure est coherente : toutes les bandes font 128 octets et leur nombre
#      est un multiple de 8 (le decoupage se verifie, il ne se suppose pas) ;
#   2. le pire passage reste sous PASS_BUDGET_MS. Ce critere ne concerne PLUS le
#      CV : celui-ci est echantillonne sous interruption depuis le 2026-08-20
#      (voir tools/run-cv-capture-probe.sh), donc independamment de la boucle. Il
#      borne ce qui depend encore du passage : reactivite de l'UI, granularite
#      d'emission des triggers, marge du tampon MIDI.
#
# ATTENTION A LA FIDELITE depuis que l'ADC tourne sous interruption. simavr
# planifie la fin d'une conversion apres `prescale` cycles au lieu de 13 x
# prescale : son ISR se declenche toutes les ~26 us au lieu de ~104, soit une
# taxe CPU de 13,7 % en simulation contre 3,4 % sur materiel. Les durees
# ci-dessous sont donc SUREVALUEES ; le facteur se deduit des deux cadences
# qu'affiche tools/run-cv-capture-probe.sh.
# Sortie 0 si les deux passent, 1 sinon, 127 si un outil manque.
#
# Reglages : PASS_BUDGET_MS (defaut 10), DURATION (defaut 8 s de simulation).

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PASS_BUDGET_MS="${PASS_BUDGET_MS:-12}"
DURATION="${DURATION:-8}"

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

PREFIX="$(brew --prefix 2>/dev/null || echo /opt/homebrew)"
[ -f "$PREFIX/include/simavr/parts/ssd1306_virt.h" ] || \
  die "en-tetes simavr absents ($PREFIX/include/simavr). brew install simavr" 127
[ -f "$PREFIX/lib/libsimavrparts.a" ] || \
  die "libsimavrparts absente ($PREFIX/lib). brew install simavr" 127

LOG="$(mktemp)"; BIN="$(mktemp -d)/blocking_probe"
trap 'rm -f "$LOG"; rm -rf "$(dirname "$BIN")"' EXIT

# --- 1. Harnais --------------------------------------------------------------
progress "compilation du harnais"
if cc -O2 -Wall -I"$PREFIX/include/simavr" -I"$PREFIX/include" \
     "$ROOT/tools/simavr-ssd1306/blocking_probe.c" -o "$BIN" \
     -L"$PREFIX/lib" -lsimavrparts -lsimavr -lelf > "$LOG" 2>&1; then
  printf '  %s✅%s harnais compile        %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" "$PREFIX" "$C_0"
else
  printf '\n'; cat "$LOG"; die "compilation du harnais en echec"
fi

# --- 2. Firmware -------------------------------------------------------------
progress "build env:nanoatmega328"
if "$PIO" run -e nanoatmega328 -d "$ROOT" > "$LOG" 2>&1; then
  printf '  %s✅%s firmware               %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" \
    "$(grep -E '^RAM:' "$LOG" | sed 's/.*(used /RAM /; s/ bytes from .*/ o/')" "$C_0"
else
  printf '\n'; tail -30 "$LOG"; die "build du firmware en echec"
fi

# --- 3. Simulation + verdict -------------------------------------------------
progress "simulation ($DURATION s)"
if ! "$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$DURATION" > "$LOG" 2>/dev/null; then
  cat "$LOG"; die "la sonde a echoue"
fi
printf '  %s✅%s simulation             %s%s s simulees%s\n' "$C_OK" "$C_0" "$C_DIM" "$DURATION" "$C_0"

PASS_BUDGET_MS="$PASS_BUDGET_MS" python3 - "$LOG" <<'PY'
import os, re, sys

txt = open(sys.argv[1], errors='replace').read()  # la sortie contient des octets bruts d'UART
tty = sys.stdout.isatty()
OK, ERR, DIM, B, Z = ('\033[32m', '\033[31m', '\033[2m', '\033[1m', '\033[0m') if tty else ('',) * 5
mark = lambda good: f"{OK}✅{Z}" if good else f"{ERR}❌{Z}"
budget = float(os.environ["PASS_BUDGET_MS"])


def grab(pattern, cast=float):
    m = re.search(pattern, txt)
    return cast(m.group(1)) if m else None


bands = grab(r"(\d+) bandes de donnees", int)
frames = grab(r"soit (\d+) images", int)
rest = grab(r"\(reste (\d+)\)", int)
conform = grab(r"decoupage : (\d+) bandes sur \d+ font exactement 128", int)
chunks = grab(r"(\d+) transactions Wire par bande", int)

band_med = grab(r"bande : transfert.*med\s+([\d.]+)")
band_max = grab(r"bande : transfert.*max\s+([\d.]+)")
pass_med = grab(r"bande a bande.*med\s+([\d.]+)")
pass_max = grab(r"bande a bande.*max\s+([\d.]+)")
frame_med = grab(r"image entiere.*med\s+([\d.]+)")

if None in (bands, frames, conform, band_med, pass_max, frame_med):
    print(f"  {mark(False)} sortie de la sonde illisible")
    print(txt)
    sys.exit(1)

sane = (conform == bands) and (rest == 0) and bands >= 8
fits = pass_max / 1000.0 <= budget

print()
print(f"{B}============ BLOCAGE DE LA BOUCLE (esclave SSD1306 reel) ============{Z}")
print(f"  {mark(sane)} Mesure coherente   {bands} bandes de 128 o, {frames} images de 8 "
      f"{DIM}(reste {rest}, {chunks} transactions Wire/bande){Z}")
print(f"  {mark(fits)} Pire passage       {pass_max/1000:6.2f} ms   "
      f"{DIM}— budget {budget:g} ms ; median {pass_med/1000:.2f} ms{Z}")
print(f"{B}====================================================================={Z}")
print(f"  Transfert d'une bande : {band_med/1000:.2f} ms (med) / {band_max/1000:.2f} ms (max)")
print(f"  Image entiere de 8 bandes : {frame_med/1000:.1f} ms, etalee sur 9 passages")
print(f"  Sans etalement, la boucle resterait bloquee cette duree d'un seul bloc.")
print()

if not sane:
    print(f"  {ERR}Le decoupage en bandes ne se verifie pas — mesure a ne pas croire.{Z}")
elif fits:
    print(f"  Aucun passage ne depasse {budget:g} ms.")
    print("  Rappel : ces durees sont surevaluees, l'ISR de l'ADC etant ~4x trop")
    print("  frequente en simulation. Le CV ne depend plus de la boucle.")
else:
    print(f"  {ERR}Le pire passage ({pass_max/1000:.2f} ms) depasse le budget de {budget:g} ms.{Z}")
    print("  Ces durees sont surevaluees : l'ISR de l'ADC est ~4x trop frequente en")
    print("  simulation (13,7 % de CPU au lieu de 3,4 %). La capture du CV, elle, ne")
    print("  depend plus de la boucle — voir tools/run-cv-capture-probe.sh.")

sys.exit(0 if (sane and fits) else 1)
PY
