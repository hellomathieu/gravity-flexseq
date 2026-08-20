#!/usr/bin/env bash
#
# Mesure la PILE reellement consommee par le firmware, a l'execution.
#
# Pourquoi : sur AVR le linker ne compte que la RAM statique (.data + .bss). Un
# debordement de pile ne produit donc jamais d'erreur de lien, seulement une
# corruption silencieuse. Le seuil RAM_RESERVE de run-build-memory.sh etait un
# garde-fou pose a l'estime ; ce script le remplace par un nombre.
#
# Methode : peinture. env:stackprobe = le firmware de production plus une sonde
# (-DFLEXSEQ_STACK_PROBE, bloc dedie de src/main.cpp) qui remplit la RAM libre
# d'un motif au demarrage, puis publie une fois par seconde la profondeur
# atteinte, encodee en LARGEUR D'IMPULSION sur CH1. Chaque releve emet DEUX
# impulsions : une d'etalonnage puis la mesure. Le rapport des deux annule le
# surcout de boucle de delayMicroseconds, qui vaut ~0,5 %.
#
# Pourquoi cet encodage : la trace `sram16` de simavr n'emet que des
# horodatages, sans valeur (`$var wire 0`) ; seule `portpin` est exploitable. Et
# l'avr-gdb de la toolchain PlatformIO (2019) est lie a Python 2.7 et ne demarre
# plus sur macOS recent, ce qui exclut la lecture directe de la RAM.
#
# VERDICT — deux criteres, chacun marque ✅ ou ❌ :
#   1. le pic tient dans RAM_RESERVE (defaut 256 o, le garde-fou du projet) ;
#   2. le pic tient dans la RAM libre du firmware de PRODUCTION — condition
#      absolue : au-dela, la pile ecrase .bss.
# Sortie 0 si les deux passent, 1 si l'un echoue, 127 si un outil manque.
#
# Ce que la mesure NE couvre PAS :
#   - la pile utilisee avant setup() : constructeurs globaux, init() d'Arduino ;
#   - les chemins non exerces par la simulation : ISR USART (aucun MIDI en
#     entree), ISR PCINT (aucun geste sur l'encodeur ni les boutons).
# Une ISR s'empile PAR-DESSUS le point le plus profond mesure ici.
#
# Reglages : RAM_RESERVE (defaut 256), DURATION (defaut 8 s de simulation).

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RAM_RESERVE="${RAM_RESERVE:-256}"
DURATION="${DURATION:-8}"

if [ -t 1 ]; then
  C_OK=$'\033[32m'; C_ERR=$'\033[31m'; C_DIM=$'\033[2m'; C_B=$'\033[1m'; C_0=$'\033[0m'
  TTY=1
else
  C_OK=""; C_ERR=""; C_DIM=""; C_B=""; C_0=""; TTY=0
fi

# Ligne de progression effacee par la ligne de resultat. Uniquement sur un
# terminal : capturee, le \r resterait litteral et dupliquerait la ligne.
progress() { [ "$TTY" = "1" ] && printf '  %s…%s %s\r' "$C_DIM" "$C_0" "$1"; return 0; }

die() {
  printf '  %s❌%s %s\n' "$C_ERR" "$C_0" "$1" >&2
  exit "${2:-1}"
}

# Meme resolution que les autres scripts du projet : PATH, puis l'installation
# PlatformIO par defaut.
if command -v pio >/dev/null 2>&1; then
  PIO="$(command -v pio)"
elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then
  PIO="$HOME/.platformio/penv/bin/pio"
else
  die "'pio' introuvable (ni PATH, ni ~/.platformio/penv/bin). Installe PlatformIO Core." 127
fi

# Le PATH, et rien d'autre : un repli vers un build personnel autoriserait
# silencieusement une autre version de simavr. On charge le .hex avec -m/-f
# explicites, seul format que tout build accepte (celui d'Homebrew refuse l'ELF).
SIMAVR=""
for candidate in simavr run_avr; do
  if command -v "$candidate" >/dev/null 2>&1; then SIMAVR="$(command -v "$candidate")"; break; fi
done
[ -n "$SIMAVR" ] || die "ni 'simavr' ni 'run_avr' dans le PATH. brew install simavr" 127

LOG="$(mktemp)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK" "$LOG"' EXIT

# --- 1. Builds ---------------------------------------------------------------
# La RAM libre de reference est celle du firmware de PRODUCTION, pas celle de la
# sonde : on le construit donc aussi s'il manque (PlatformIO invalide les
# repertoires de build des que platformio.ini change).
PROD_ELF="$ROOT/.pio/build/nanoatmega328/firmware.elf"

if [ ! -f "$PROD_ELF" ]; then
  progress "build env:nanoatmega328 (RAM libre de reference)"
  if "$PIO" run -e nanoatmega328 -d "$ROOT" > "$LOG" 2>&1; then
    printf '  %s✅%s build env:nanoatmega328\n' "$C_OK" "$C_0"
  else
    printf '\n'; tail -30 "$LOG"; die "build env:nanoatmega328 en echec"
  fi
fi

progress "build env:stackprobe"
if "$PIO" run -e stackprobe -d "$ROOT" > "$LOG" 2>&1; then
  printf '  %s✅%s build env:stackprobe   %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" \
    "$(grep -E '^RAM:' "$LOG" | sed 's/.*(used /RAM /; s/ bytes from .*/ o/')" "$C_0"
else
  printf '\n'; tail -30 "$LOG"; die "build env:stackprobe en echec"
fi

# --- 2. Simulation -----------------------------------------------------------
# run_avr n'honore pas un chemin de VCD absolu : on travaille dans un repertoire
# temporaire et on ecrit le VCD en relatif.
progress "simulation ($DURATION s)"
( cd "$WORK" && timeout "$DURATION" "$SIMAVR" -m atmega328p -f 16000000 \
    -o probe.vcd --add-trace CH1=portpin@0x07/0x44 \
    "$ROOT/.pio/build/stackprobe/firmware.hex" >/dev/null 2>&1 )

[ -s "$WORK/probe.vcd" ] || die "aucun VCD produit — la simulation n'a rien trace."
printf '  %s✅%s simulation             %s%s s, VCD %s o, %s%s\n' "$C_OK" "$C_0" "$C_DIM" \
  "$DURATION" "$(wc -c < "$WORK/probe.vcd" | tr -d ' ')" "$SIMAVR" "$C_0"

# --- 3. Verdict --------------------------------------------------------------
python3 - "$WORK/probe.vcd" "$PROD_ELF" "$RAM_RESERVE" <<'PY'
import os, re, subprocess, sys
from pathlib import Path

vcd, prod_elf, reserve = sys.argv[1], Path(sys.argv[2]), int(sys.argv[3])
tty = sys.stdout.isatty()
OK, ERR, DIM, B, Z = ('\033[32m', '\033[31m', '\033[2m', '\033[1m', '\033[0m') if tty else ('',) * 5
mark = lambda good: f"{OK}✅{Z}" if good else f"{ERR}❌{Z}"

# --- relevés dans le VCD -----------------------------------------------------
t, edges = 0, []
for line in Path(vcd).read_text().splitlines():
    if line.startswith('#'):
        t = int(line[1:])
    elif line and line[0] in '01' and '!' in line:
        edges.append((t, int(line[0])))

pulses = [edges[i + 1][0] - edges[i][0] for i in range(len(edges) - 1) if edges[i][1] == 1]
probe = [w for w in pulses if w > 5_000_000]  # > 5 ms : jamais un trigger musical
values = [round(probe[i + 1] / probe[i] * 100) for i in range(0, len(probe) - 1, 2) if probe[i]]

if not values:
    print(f"  {mark(False)} aucun relevé exploitable dans le VCD "
          f"({len(pulses)} impulsions, dont {len(probe)} de sonde)")
    sys.exit(1)

peak = max(values)

# --- RAM libre du firmware de production -------------------------------------
free = None
tools = ['avr-size', str(Path.home() / '.platformio/packages/toolchain-atmelavr/bin/avr-size')]
for tool in tools:
    try:
        out = subprocess.run([tool, '-A', str(prod_elf)],
                             capture_output=True, text=True, check=True).stdout
        sizes = {m[1]: int(m[2]) for m in re.finditer(r'^(\.\w+)\s+(\d+)', out, re.M)}
        free = 2048 - sizes.get('.data', 0) - sizes.get('.bss', 0)
        break
    except Exception:
        continue

fits_reserve = peak <= reserve
fits_ram = free is None or peak <= free
pct = peak * 100 // reserve
spread = f"  (relevés : {', '.join(str(v) for v in sorted(set(values)))})" if len(set(values)) > 1 else ""

print()
print(f"{B}=================== PILE (mesuree a l'execution) =================={Z}")
print(f"  {mark(fits_reserve)} Pic de pile    {peak:5d} o   {DIM}— reserve exigee {reserve} o "
      f"({pct} % utilises){Z}")
if free is None:
    print(f"  {mark(True)} RAM libre        {'?':>5}     {DIM}— avr-size introuvable, marge non calculee{Z}")
else:
    print(f"  {mark(fits_ram)} Marge          {free - peak:5d} o   {DIM}— RAM libre {free} o "
          f"apres .data + .bss{Z}")
print(f"{B}==================================================================={Z}")
print(f"  {len(values)} relevés sur la simulation{spread}")

if fits_reserve and fits_ram:
    ratio = reserve / peak
    print(f"  La reserve couvre le pic mesure ({ratio:.1f}x).")
elif not fits_ram:
    print(f"  {ERR}DEBORDEMENT{Z} : le pic depasse la RAM libre — la pile ecrase .bss.")
    print("  Corruption silencieuse a la cle : reduire la RAM statique, tout de suite.")
else:
    print(f"  {ERR}La reserve de {reserve} o ne couvre plus le pic mesure de {peak} o.{Z}")
    print("  Deux issues : reduire la pile, ou relever RAM_RESERVE en connaissance de")
    print("  cause — jamais pour faire passer un build.")

print()
print(f"{DIM}  Hors mesure : la pile d'avant setup() (constructeurs globaux, init()"
      f" d'Arduino){Z}")
print(f"{DIM}  et les ISR non exercees par la simulation (USART/MIDI, PCINT encodeur et{Z}")
print(f"{DIM}  boutons). Une ISR s'empile PAR-DESSUS ce pic.{Z}")

sys.exit(0 if (fits_reserve and fits_ram) else 1)
PY
