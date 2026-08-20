#!/usr/bin/env bash
#
# Verifie qu'une impulsion CV de la largeur GARANTIE est vue par le firmware,
# dans les conditions reelles : rendu OLED actif, donc boucle principale chargee.
#
# Contexte. Le CV etait lu une fois par passage de boucle, et le pire passage
# dure 7,74 ms (ADR 0001) : une impulsion plus courte pouvait passer inapercue.
# FlexSeq echantillonne desormais le convertisseur SOUS INTERRUPTION
# (include/flexseq/CvSampler.h) pour garantir 1 ms. Une garantie non mesuree n'en
# est pas une.
#
# Methode. tools/simavr-ssd1306/cv_capture_probe.c attache l'esclave SSD1306 (sans
# lui la boucle est irrealistement rapide) et injecte des impulsions sur ADC7 via
# le mecanisme de simavr — millivolts sur ADC_IRQ_ADC7, en reponse a
# ADC_IRQ_OUT_TRIGGER. Le firmware n'est PAS instrumente : le harnais joue le
# consommateur du verrou, lisant et effacant le drapeau `pending` dans la RAM
# simulee, exactement ce que fera la boucle quand CV -> RESET existera. Les
# adresses viennent de avr-nm.
#
# FIDELITE — a savoir. simavr planifie la fin d'une conversion apres `prescale`
# cycles, la ou le materiel prend 13 x prescale : sa cadence est ~8x optimiste.
# La simulation valide donc la LOGIQUE (front, verrou, rearmement, sous charge) ;
# c'est l'arithmetique qui porte la garantie jusqu'au materiel — 13 x 128 cycles
# = 104 us par conversion, deux voies en alternance, soit une voie toutes les
# ~208 us. Le script affiche les deux et verifie que la seconde tient dans la
# largeur garantie.
#
# VERDICT : capture a 100 %, et cadence materielle sous la largeur garantie.
# Sortie 0 si les deux passent, 1 sinon, 127 si un outil manque.
#
# La PERIODE doit depasser la fenetre de grace du harnais (200 ms), sans quoi une
# impulsion encore en attente de son consommateur serait declaree perdue a
# l'injection suivante — d'ou 400 ms par defaut.
#
# Reglages : CV_PULSE_US (defaut 1000), PERIOD_US (defaut 400000), DURATION (12 s).

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CV_PULSE_US="${CV_PULSE_US:-1000}"
PERIOD_US="${PERIOD_US:-400000}"
DURATION="${DURATION:-12}"

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
[ -f "$PREFIX/lib/libsimavrparts.a" ] || die "simavr absent ($PREFIX). brew install simavr" 127

AVR_NM="$(command -v avr-nm || echo "$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-nm")"
[ -x "$AVR_NM" ] || die "avr-nm introuvable." 127

LOG="$(mktemp)"; BIN="$(mktemp -d)/cv_probe"
trap 'rm -f "$LOG"; rm -rf "$(dirname "$BIN")"' EXIT

progress "build env:nanoatmega328"
if "$PIO" run -e nanoatmega328 -d "$ROOT" > "$LOG" 2>&1; then
  printf '  %s✅%s firmware               %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" \
    "$(grep -E '^RAM:' "$LOG" | sed 's/.*(used /RAM /; s/ bytes from .*/ o/')" "$C_0"
else
  printf '\n'; tail -30 "$LOG"; die "build du firmware en echec"
fi

ELF="$ROOT/.pio/build/nanoatmega328/firmware.elf"
# Adresses dans l'espace de donnees : avr-nm les donne decalees de 0x800000.
addr_of() {
  local sym
  sym="$("$AVR_NM" --radix=x "$ELF" | grep -E "N_1$1E\$" | head -1 | cut -d' ' -f1)"
  [ -n "$sym" ] || return 1
  printf '0x%x' $(( 0x$sym - 0x800000 ))
}
PENDING="$(addr_of 7pending)" || die "symbole 'pending' introuvable dans le firmware"
COMPLETED="$(addr_of 9completed)" || die "symbole 'completed' introuvable"
printf '  %s✅%s symboles               %spending %s, completed %s%s\n' \
  "$C_OK" "$C_0" "$C_DIM" "$PENDING" "$COMPLETED" "$C_0"

progress "compilation du harnais"
if cc -O2 -Wall -I"$PREFIX/include/simavr" -I"$PREFIX/include" \
     "$ROOT/tools/simavr-ssd1306/cv_capture_probe.c" -o "$BIN" \
     -L"$PREFIX/lib" -lsimavrparts -lsimavr -lelf > "$LOG" 2>&1; then
  printf '  %s✅%s harnais compile        %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" "$PREFIX" "$C_0"
else
  printf '\n'; cat "$LOG"; die "compilation du harnais en echec"
fi

progress "simulation ($DURATION s, impulsions de $CV_PULSE_US us)"
ERRLOG="$(mktemp)"
if ! "$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$PENDING" \
       "$CV_PULSE_US" "$PERIOD_US" "$DURATION" "$COMPLETED" > "$LOG" 2>"$ERRLOG"; then
  # Le harnais rend 2 sur un argument incoherent : son message porte le
  # diagnostic, on ne le jette pas.
  if [ -s "$ERRLOG" ] && ! grep -q "injectees" "$LOG"; then
    printf '\n'; cat "$ERRLOG" >&2; rm -f "$ERRLOG"; die "le harnais a refuse de mesurer"
  fi
fi
rm -f "$ERRLOG"
printf '  %s✅%s simulation             %s%s s%s\n' "$C_OK" "$C_0" "$C_DIM" "$DURATION" "$C_0"

CV_PULSE_US="$CV_PULSE_US" python3 - "$LOG" <<'PY'
import os, re, sys

txt = open(sys.argv[1], errors='replace').read()
tty = sys.stdout.isatty()
OK, ERR, DIM, B, Z = ('\033[32m', '\033[31m', '\033[2m', '\033[1m', '\033[0m') if tty else ('',) * 5
mark = lambda good: f"{OK}✅{Z}" if good else f"{ERR}❌{Z}"
width = float(os.environ["CV_PULSE_US"])

m = re.search(r"injectees (\d+)\s+vues (\d+)\s+ratees (\d+)\s+taux ([\d.]+)", txt)
sim = re.search(r"une voie toutes les ([\d.]+) us \(SIMULEE\)", txt)
hw = re.search(r"toutes les (\d+) us, hors surcout", txt)
if not m or not hw:
    print(f"  {mark(False)} sortie du harnais illisible"); print(txt); sys.exit(1)

injected, seen, missed, rate = int(m[1]), int(m[2]), int(m[3]), float(m[4])
hw_us = float(hw[1])
captured = injected > 0 and missed == 0 and seen == injected
fast_enough = hw_us <= width

print()
print(f"{B}=========== CAPTURE D'UNE IMPULSION CV DE {width:.0f} us ==========={Z}")
print(f"  {mark(captured)} Capture            {seen}/{injected} vues, {missed} ratees "
      f"{DIM}({rate:.0f} %, rendu OLED actif){Z}")
print(f"  {mark(fast_enough)} Cadence materielle {hw_us:.0f} us par voie   "
      f"{DIM}— 13 x 128 cycles x 2 voies ; simulee : {sim[1] if sim else '?'} us{Z}")
print(f"{B}================================================================={Z}")
if captured and fast_enough:
    print(f"  Une impulsion de {width:.0f} us est vue a coup sur : elle dure plus qu'un")
    print(f"  intervalle d'echantillonnage, donc au moins un echantillon tombe dedans.")
else:
    if not captured:
        print(f"  {ERR}{missed} impulsion(s) perdue(s).{Z}")
    if not fast_enough:
        print(f"  {ERR}La cadence materielle ({hw_us:.0f} us) depasse la largeur garantie.{Z}")
sys.exit(0 if (captured and fast_enough) else 1)
PY
