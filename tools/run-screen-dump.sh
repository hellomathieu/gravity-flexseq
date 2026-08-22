#!/usr/bin/env bash
#
# Lit la MEMOIRE D'ECRAN que le panneau SSD1306 recoit reellement, et verifie la
# geometrie du rendu de bout en bout.
#
# Pourquoi. Le rendu OLED n'avait jamais ete constate autrement qu'a l'oeil dans
# Wokwi, et la prise en charge de la rotation par sa piece `board-ssd1306` n'avait
# jamais ete verifiee du tout (PRD 14). simavr modelise un vrai esclave SSD1306
# qui expose sa `vram` : on peut donc lire l'image affichee, sans Wokwi, sans
# jeton, et en tirer des assertions.
#
# Ce qui est verifie. Le firmware dessine dans un canvas LOGIQUE ; U8g2 applique
# `U8G2_R2` (180 degres) avant d'ecrire dans la memoire du panneau, donc un point
# logique (x, y) atterrit en (127-x, 63-y). Le harnais connait la geometrie par
# `flexseq/PatternScreen.h` — la meme source que le firmware, aucune constante
# recopiee — et verifie que l'encre est la ou cette transformation la place.
#
#   1. GEOMETRIE : les 24 steps sont a leur position attendue apres rotation ;
#   2. ROTATION   : le titre, en haut du canvas logique, apparait EN BAS du
#      panneau, et le PIED DE PAGE, en bas du canvas, apparait EN HAUT. Entre les
#      deux, la bande qui ne porte jamais rien doit rester vide. Les deux
#      extremites etant epinglees, une rotation inversee ne peut plus passer. C'est
#      ce qui justifie le montage physique de l'OLED a 180 degres, donc le
#      `"rotate": 180` de diagram.json.
#
# Ce controle a paye des sa premiere execution : il a montre un ecran QUASI BLANC.
# L'ecartement par bande d'ADR 0001 comparait une bande d'AFFICHAGE a des
# coordonnees LOGIQUES, et gardait donc la moitie inverse. Aucun test natif ne
# pouvait le voir : ils fournissaient la bande deja en coordonnees logiques.
#
# ASCII=1 imprime l'image en art ASCII, deux fois : telle que le panneau la porte,
# puis retournee — ce que voit l'oeil sur le module. PGM=<chemin> ecrit une image.
#
# Reglages : FIRMWARE (defaut env:wokwi, le contenu de demonstration),
# DURATION (3 s), LENGTH (deduite de l'environnement), SKIP_GEOMETRY=1, ENVNAME.

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENVNAME="${ENVNAME:-wokwi}"
DURATION="${DURATION:-3}"
# La LENGTH du contenu AFFICHE conditionne le controle : au-dela, un step n'est
# qu'un point d'un pixel. env:wokwi la fixe a 20 dans son contenu de
# demonstration ; les autres firmwares partent de SequencerEngine::DEFAULT_LENGTH.
# L'ecran rendu decoule de l'environnement : le demander a l'appelant serait un
# piege, les criteres n'ayant rien en commun entre les deux ecrans.
if [ -z "${SCREEN:-}" ]; then
  case "$ENVNAME" in
    mainscreen) SCREEN=main ;;
    *)          SCREEN=edit ;;
  esac
fi
export SCREEN

if [ -z "${LENGTH:-}" ]; then
  case "$ENVNAME" in
    wokwi) LENGTH=20 ;;
    *)     LENGTH=16 ;;
  esac
fi

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

LOG="$(mktemp)"; BIN="$(mktemp -d)/screen_dump"
trap 'rm -f "$LOG"; rm -rf "$(dirname "$BIN")"' EXIT

progress "build env:$ENVNAME"
if "$PIO" run -e "$ENVNAME" -d "$ROOT" > "$LOG" 2>&1; then
  printf '  %s✅%s firmware               %senv:%s, %s%s\n' "$C_OK" "$C_0" "$C_DIM" "$ENVNAME" \
    "$(grep -E '^RAM:' "$LOG" | sed 's/.*(used /RAM /; s/ bytes from .*/ o/')" "$C_0"
else
  printf '\n'; tail -30 "$LOG"; die "build en echec"
fi

progress "compilation du harnais"
if c++ -O2 -Wall -std=gnu++11 -I"$ROOT/include" -I"$PREFIX/include/simavr" -I"$PREFIX/include" \
     "$ROOT/tools/simavr-ssd1306/screen_dump.cpp" "$ROOT/src/domain/Pattern.cpp" -o "$BIN" \
     -L"$PREFIX/lib" -lsimavrparts -lsimavr -lelf > "$LOG" 2>&1; then
  printf '  %s✅%s harnais compile        %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" "$PREFIX" "$C_0"
else
  printf '\n'; cat "$LOG"; die "compilation du harnais en echec"
fi

progress "simulation ($DURATION s)"
set +e
"$BIN" "$ROOT/.pio/build/$ENVNAME/firmware.hex" "$DURATION" "$LENGTH" "${PGM:-}" > "$LOG" 2>/dev/null
PROBE=$?
set -e
printf '  %s✅%s simulation             %s%s s%s\n' "$C_OK" "$C_0" "$C_DIM" "$DURATION" "$C_0"

# L'art ASCII, s'il a ete demande, avant le verdict.
if [ -n "${ASCII:-}" ]; then
  sed -n '/^--- /,/^   +-*+$/p' "$LOG"
fi

# La surveillance continue conditionne le code de sortie : son rapport doit donc
# etre lisible, et non seulement present dans le journal.
if [ -n "${WATCH:-}" ]; then
  sed -n '/^=== SURVEILLANCE/,/^$/p' "$LOG"
fi

python3 - "$LOG" "$PROBE" <<'PY'
import re, sys

txt = open(sys.argv[1], errors='replace').read()
probe_status = int(sys.argv[2])
tty = sys.stdout.isatty()
OK, ERR, DIM, B, Z = ('\033[32m', '\033[31m', '\033[2m', '\033[1m', '\033[0m') if tty else ('',) * 5
mark = lambda good: f"{OK}✅{Z}" if good else f"{ERR}❌{Z}"

if "La memoire du panneau est VIDE" in txt:
    print()
    print(f"  {mark(False)} La memoire du panneau est VIDE : rien n'a ete affiche.")
    print("     Le bus peut porter du trafic et l'ecran rester blanc — c'est")
    print("     exactement le defaut que ce controle a trouve la premiere fois.")
    sys.exit(1)

main_ok = re.search(r"ecran principal (OK|KO)", txt)
if main_ok:
    tabs = re.search(r"(\d+) / (\d+) onglets a leur place", txt)
    bar = re.search(r"barre d'onglets \(panneau y (\d+)\.\.(\d+)\) : (\d+) pixels", txt)
    head = re.search(r"grande police \(panneau y (\d+)\.\.(\d+)\) : (\d+) pixels", txt)
    rule = re.search(r"filet \(panneau y (\d+)\) : (\d+) pixels", txt)
    ok = main_ok[1] == "OK"
    print()
    print(f"{B}========== ECRAN PRINCIPAL (memoire du panneau) =========={Z}")
    print(f"  {mark(tabs is not None and tabs[1] == tabs[2])} Barre d'onglets    "
          f"{tabs[1] if tabs else '?'}/{tabs[2] if tabs else '?'} onglets a leur place "
          f"{DIM}(apres U8G2_R2){Z}")
    print(f"  {mark(bar is not None and bar[3] != '0')} Rotation 180       "
          f"barre en HAUT du panneau ({bar[3] if bar else '?'} px en "
          f"y {bar[1] if bar else '?'}..{bar[2] if bar else '?'})")
    print(f"  {mark(head is not None and head[3] != '0')} Grande police      "
          f"en BAS du panneau ({head[3] if head else '?'} px en "
          f"y {head[1] if head else '?'}..{head[2] if head else '?'})")
    print(f"  {mark(rule is not None and rule[2] != '0')} Filet              "
          f"{DIM}panneau y {rule[1] if rule else '?'}, {rule[2] if rule else '?'} px{Z}")
    print(f"{B}=========================================================={Z}")
    sys.exit(0 if (ok and probe_status == 0) else 1)

steps = re.search(r"(\d+) / (\d+) steps a leur place", txt)
skipped = "ignoree (SKIP_GEOMETRY)" in txt
ink = re.search(r"encre totale (\d+) pixels", txt)
title = re.search(r"bande du titre \(panneau y (\d+)\.\.(\d+)\) : (\d+) pixels", txt)
footer = re.search(r"pied de page \(panneau y (\d+)\.\.(\d+)\)\s+: (\d+) pixels", txt)
gap = re.search(r"entre le pied et la grille \(y (\d+)\.\.(\d+)\) : (\d+) pixels", txt)
geom_ok = "geometrie OK" in txt
rot_ok = "rotation OK" in txt

print()
print(f"{B}============= RENDU OLED (memoire du panneau) ============={Z}")
if skipped:
    print(f"  {mark(True)} Geometrie          ignoree (SKIP_GEOMETRY)")
elif steps:
    print(f"  {mark(geom_ok)} Geometrie          {steps[1]}/{steps[2]} steps a leur place attendue "
          f"{DIM}(apres U8G2_R2){Z}")
print(f"  {mark(rot_ok)} Rotation 180       titre en bas ({title[3] if title else '?'} px en "
      f"y {title[1] if title else '?'}..{title[2] if title else '?'}), "
      f"pied en haut ({footer[3] if footer else '?'} px en "
      f"y {footer[1] if footer else '?'}..{footer[2] if footer else '?'})")
print(f"  {mark(gap is not None and gap[3] == '0')} Bande intermediaire "
      f"vide entre le pied et la grille "
      f"{DIM}(y {gap[1] if gap else '?'}..{gap[2] if gap else '?'}, "
      f"{gap[3] if gap else '?'} px){Z}")
print(f"{B}==========================================================={Z}")
if ink:
    print(f"  {ink[1]} pixels d'encre au total.")
if geom_ok and rot_ok:
    print("  Le firmware garde son U8G2_R2 : l'OLED monte tete en bas sur le")
    print("  module affiche donc a l'endroit. C'est ce que diagram.json modelise.")
sys.exit(0 if (geom_ok and rot_ok and probe_status == 0) else 1)
PY
