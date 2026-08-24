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
# L'ARTEFACT DE L'ADC EST CORRIGE, par une mesure a DEUX REGIMES en une seule
# execution. simavr planifie la fin d'une conversion apres `prescale` cycles au
# lieu de 13 x prescale : son ISR se declenche ~4x trop souvent, ce qui gonfle la
# duree d'un passage. A la moitie de la simulation, le harnais efface le bit ADIE
# d'ADCSRA — et comme c'est l'ISR qui relance les conversions, les couper les
# arrete toutes. Du MEME binaire on obtient donc la boucle avec, puis sans,
# activite d'ADC.
#
# La correction est un rapport de FRACTIONS de CPU, non une soustraction de
# maxima (leurs valeurs extremes viennent d'evenements differents) : la fraction
# volee en simulation se deduit des medianes, le cout par ISR s'en tire avec la
# cadence simulee — MESUREE au compteur de conversions du firmware — et la
# fraction materielle vient de la cadence arithmetique de 13 x 128 cycles. Le
# verdict porte sur l'estimation materielle.
# Sortie 0 si les deux passent, 1 sinon, 127 si un outil manque.
#
# LE PIC DU RAFRAICHISSEMENT COMPLET SE MESURE, IL NE SE DEDUIT PAS. Il ne
# concerne qu'une image sur seize (ADR 0001), et les images sont espacees de
# ~470 ms : a 8 s de simulation il n'en tombait souvent AUCUNE dans le regime
# sans ADC, et le maximum d'une image courante heritait alors de l'etiquette
# « rafraichissement complet ». Le harnais separe donc les deux populations par
# le nombre de bandes de l'image — 8 pour une image complete, moins sinon — et
# dit explicitement quand il n'a rien observe. DURATION vaut 32 s pour qu'il en
# observe, ce qui coute ~5 s de temps mur.
#
# Reglages : PASS_BUDGET_MS (defaut 12), DURATION (defaut 32 s de simulation).

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PASS_BUDGET_MS="${PASS_BUDGET_MS:-12}"
DURATION="${DURATION:-32}"
# ENVNAME choisit le firmware mesure, et le defaut est env:wokwi — pas la
# production — parce que l'objet de cette sonde est de BORNER la boucle quand elle
# REND. Depuis le 2026-08-22 la production demarre sur l'ecran principal, qui ne
# porte aucun element variant dans le temps : elle ne redessine donc presque
# jamais, et la mesurer ainsi ne rend que quelques echantillons. env:wokwi affiche
# EDIT PATTERN avec un playhead qui avance, avec le MEME renderer et le meme
# etalement : c'est l'ecran que la production atteint des que l'utilisateur entre
# dans EDIT, donc son pire cas.
#
# Ce que ce defaut ne porte PAS : env:wokwi n'echantillonne pas le CV, donc ses
# chiffres n'incluent pas la charge de l'ISR de l'ADC — mesuree a 5,6 % de CPU sur
# la production. ENVNAME=nanoatmega328 mesure la production telle qu'elle demarre.
ENVNAME="${ENVNAME:-wokwi}"

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
progress "build env:$ENVNAME"
if "$PIO" run -e "$ENVNAME" -d "$ROOT" > "$LOG" 2>&1; then
  printf '  %s✅%s firmware               %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" \
    "$(grep -E '^RAM:' "$LOG" | sed 's/.*(used /RAM /; s/ bytes from .*/ o/')" "$C_0"
else
  printf '\n'; tail -30 "$LOG"; die "build du firmware en echec"
fi

# --- 3. Simulation + verdict -------------------------------------------------
# L'adresse du compteur de conversions permet de MESURER la cadence d'ISR
# simulee, donc le facteur de correction, au lieu de la supposer.
AVR_NM="$(command -v avr-nm || echo "$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-nm")"
DONE=""
if [ -x "$AVR_NM" ]; then
  SYM="$("$AVR_NM" --radix=x "$ROOT/.pio/build/$ENVNAME/firmware.elf" \
         | grep -E "N_19completedE\$" | head -1 | cut -d' ' -f1)"
  [ -n "$SYM" ] && DONE="$(printf '0x%x' $(( 0x$SYM - 0x800000 )))"
fi
[ -n "$DONE" ] && printf '  %s\xe2\x9c\x85%s symbole                %scompleted %s%s\n' \
  "$C_OK" "$C_0" "$C_DIM" "$DONE" "$C_0"

progress "simulation ($DURATION s, deux regimes)"
set +e
"$BIN" "$ROOT/.pio/build/$ENVNAME/firmware.hex" "$DURATION" $DONE > "$LOG" 2>/dev/null
PROBE=$?
set -e
# Le tas de simavr est corrompu pendant un run (voir l'en-tete du harnais) : la
# sonde peut mourir dans l'allocateur APRES avoir tout mesure. On ne jette donc
# pas un rapport complet — on signale la sortie anormale et le verdict tranche
# sur les donnees, qui sont la ou elles ne sont pas.
if [ "$PROBE" -ne 0 ]; then
  printf '  %s⚠%s  la sonde s'"'"'est terminee anormalement (code %d) : tas de simavr.\n' \
    "$C_DIM" "$C_0" "$PROBE"
fi
printf '  %s✅%s simulation             %s%s s simulees%s\n' "$C_OK" "$C_0" "$C_DIM" "$DURATION" "$C_0"

PASS_BUDGET_MS="$PASS_BUDGET_MS" python3 - "$LOG" <<'PY'
import os, re, sys

txt = open(sys.argv[1], errors='replace').read()   # la sortie porte des octets d'UART
tty = sys.stdout.isatty()
OK, ERR, DIM, B, Z = ('\033[32m', '\033[31m', '\033[2m', '\033[1m', '\033[0m') if tty else ('',) * 5
mark = lambda good: f"{OK}\u2705{Z}" if good else f"{ERR}\u274c{Z}"
budget = float(os.environ["PASS_BUDGET_MS"])


def grab(pattern, cast=float):
    m = re.search(pattern, txt)
    return cast(m.group(1)) if m else None


bands = grab(r"(\d+) bandes de donnees", int)
frames = grab(r"en (\d+) images", int)
per_frame = re.search(r"bandes par image :(.*)", txt)
conform = grab(r"decoupage : (\d+) bandes sur \d+ font exactement 128", int)
band_med = grab(r"bande : transfert.*med\s+([\d.]+)")
med_a = grab(r"passage, ADC active.*med\s+([\d.]+)")
max_a = grab(r"passage, ADC active.*max\s+([\d.]+)")
med_b = grab(r"passage, ADC coupee.*med\s+([\d.]+)")
p90_b = grab(r"passage, ADC coupee.*p90\s+([\d.]+)")
max_b = grab(r"passage, ADC coupee.*max\s+([\d.]+)")
f_hw = grab(r"=> ([\d.]+) % sur materiel")
hw_max = grab(r"ESTIME SUR MATERIEL : ([\d.]+) ms au pire")
hw_p90 = grab(r"au pire, ([\d.]+) ms en p90")
hw_med = grab(r"ms en p90, ([\d.]+) ms en median")
hw_full = grab(r"RAFRAICHISSEMENT COMPLET : ([\d.]+) ms au pire")
full_frames = grab(r"RAFRAICHISSEMENT COMPLET : [\d.]+ ms au pire, sur (\d+) image", int)
full_ratio = grab(r"images hors ADC : \d+ courantes, \d+ completes  \(1 sur ([\d.]+)\)")
frame_med = grab(r"Image courante, estimee materiel\s+: ([\d.]+) ms")
frame_full = grab(r"Image de rafraichissement complet\s+: ([\d.]+) ms")

if None in (bands, conform, hw_max, hw_p90, max_b, med_a, med_b):
    # Deux causes tres differentes, et les confondre envoie chercher le defaut
    # au mauvais endroit. Le harnais n'emet sa ligne RESULTAT que s'il a des
    # echantillons : un binaire qui ne redessine pas produit une sortie
    # parfaitement lisible et parfaitement vide.
    if "=== LECTURE ===" in txt:
        print(f"  {mark(False)} aucun echantillon : ce binaire ne redessine pas assez")
        print(f"  {DIM}La sonde mesure le blocage PENDANT LE RENDU. Un firmware pose sur")
        print(f"  un ecran sans element variant dans le temps n'ouvre presque aucune")
        print(f"  image. Viser env:wokwi, ou la production avec")
        print(f"  PLATFORMIO_BUILD_FLAGS=-DFLEXSEQ_START_IN_EDIT=1{Z}")
    else:
        print(f"  {mark(False)} sortie de la sonde illisible")
        print("".join(c for c in txt if 32 <= ord(c) < 127 or c == "\n"))
    sys.exit(1)

# Le plancher etait `bands >= 8`, soit UNE image. Il ne protegeait que contre
# l'absence totale de mesure, jamais contre un echantillon trop maigre pour
# conclure : les 14 bandes de la production sur l'ecran principal le passaient,
# alors que c'est le cas qui avait fait basculer la sonde sur env:wokwi.
#
# Le plancher porte donc sur les IMAGES. Un rafraichissement complet arrive une
# fois sur seize, et le pire passage n'appartient qu'a celui-la : sous seize
# images, la sonde ne peut pas separer les deux populations et le dit.
MIN_FRAMES = 16
enough = frames >= MIN_FRAMES
sane = (conform == bands) and bands >= 8 and enough and "IMAGE TROP LONGUE" not in txt
corrected = f_hw is not None
fits = hw_p90 is not None and hw_p90 <= budget

print()
print(f"{B}============ BLOCAGE DE LA BOUCLE (esclave SSD1306 reel) ============{Z}")
if not enough:
    print(f"  {ERR}Echantillon trop maigre : {frames} images, il en faut {MIN_FRAMES}.{Z}")
    print(f"  {DIM}Un rafraichissement complet arrive 1 fois sur 16, et le pire passage")
    print(f"  n'appartient qu'a lui. Sous ce seuil la sonde ne mesure pas ce qu'elle")
    print(f"  annonce. Rallonger DURATION, ou viser un binaire qui redessine.{Z}")
print(f"  {mark(sane)} Mesure coherente   {bands} bandes de 128 o en {frames} images "
      f"{DIM}({per_frame[1].strip() if per_frame else '?'}){Z}")
if corrected:
    print(f"  {mark(True)} Artefact ADC       corrige : {f_hw:.1f} % de CPU sur materiel "
          f"{DIM}(mesure a deux regimes){Z}")
else:
    print(f"  {mark(False)} Artefact ADC       {ERR}NON corrige{Z} — chiffres surevalues")
print(f"  {mark(fits)} Passage courant    {hw_p90:6.2f} ms   "
      f"{DIM}— p90 estime materiel ; budget {budget:g} ms ; median {hw_med:.2f} ms{Z}")
if hw_full is not None:
    ratio = f"1 image sur {full_ratio:.0f}" if full_ratio else "ratio non mesure"
    print(f"  {DIM}   Pire passage    {hw_full:6.2f} ms   — rafraichissement complet, "
          f"{ratio}, mesure sur {full_frames}{Z}")
else:
    print(f"  {DIM}   Pire passage      non observe — aucune image complete dans le"
          f" regime sans ADC ; allonger DURATION{Z}")
    print(f"  {DIM}                    (maximum d'une image courante : {hw_max:.2f} ms){Z}")
print(f"{B}====================================================================={Z}")
print(f"  Transfert d'une bande      : {band_med/1000:.2f} ms")
print(f"  Passage simule, ADC active : {med_a/1000:.2f} ms med / {max_a/1000:.2f} ms max")
print(f"  Passage simule, ADC coupee : {med_b/1000:.2f} ms med / {max_b/1000:.2f} ms max")
if frame_med is not None:
    line = f"  Image entiere              : {frame_med:.1f} ms courante"
    if frame_full is not None:
        line += f" / {frame_full:.1f} ms complete"
    print(line + ", un passage par bande")
print()

if not sane:
    print(f"  {ERR}Le decoupage en bandes ne se verifie pas — mesure a ne pas croire.{Z}")
elif fits:
    print(f"  Le passage courant tient dans {budget:g} ms sur materiel.")
    if hw_full is not None:
        print(f"  Le pic de {hw_full:.2f} ms subsiste sur le rafraichissement complet,")
        print(f"  observe {full_frames} fois, {('1 image sur %.0f' % full_ratio) if full_ratio else 'ratio non mesure'} —")
        print("  filet volontaire : un oubli de la logique de bande sale s'y repare seul.")
    else:
        print("  Le pic du rafraichissement complet n'a PAS ete observe a cette duree :")
        print("  il n'est donc ni confirme ni infirme par cette execution.")
else:
    print(f"  {ERR}Le pire passage estime ({hw_max:.2f} ms) depasse le budget de "
          f"{budget:g} ms.{Z}")
    print("  Ce n'est PAS l'ADC, dont l'artefact est corrige. Regarder le cout par")
    print("  position dans l'image, que la sonde imprime : il dit quelle bande paie.")

sys.exit(0 if (sane and corrected and fits) else 1)

PY
