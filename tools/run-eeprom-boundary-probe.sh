#!/usr/bin/env bash
#
# Verifie que le firmware n'ecrit PAS hors de sa fenetre EEPROM.
#
# Pourquoi. include/flexseq/Persistence.h borne le FORMAT par deux
# `static_assert` : l'image commence au-dessus des reglages du firmware
# d'origine, et finit sous son `memCode` de l'adresse 1023. Ces bornes sont
# celles de la disposition, pas celles des ecritures. Rien n'observait les
# ecritures elles-memes.
#
# La fenetre n'est PAS ecrite ici : elle vient du format actif, resolu sur le
# binaire par tools/active-format.sh. Le harnais ne connait donc aucune adresse
# du format.
#
# TROIS RESULTATS, ET ILS NE SE CONFONDENT PAS :
#   PASS     les deux zones protegees sont intactes, et la fenetre a ete ecrite ;
#   FAIL     une zone protegee a change — defaut du firmware ;
#   INVALID  la fenetre n'a pas ete ecrite, ou l'octet de version n'est pas
#            celui du format actif. Les zones sont alors intactes pour une
#            raison sans rapport avec l'isolation, et un vert serait vide.
#            Sortie 5, jamais 1 : ce n'est pas un defaut du firmware.
#
# CE QUE CETTE MESURE N'ETABLIT PAS. Elle porte sur les chemins que le firmware
# emprunte pendant la course. Ce n'est pas une preuve d'absence de tout chemin
# futur ; cette question appartient a l'audit statique des points d'ecriture.
#
# Reglages : DURATION (defaut 10 s simulees), BOUNDARY_MUTATE=<adresse> pour la
# contre-epreuve, qui doit rendre FAIL.

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DURATION="${DURATION:-10}"

if [ -t 1 ]; then
  C_OK=$'\033[32m'; C_ERR=$'\033[31m'; C_WARN=$'\033[33m'; C_DIM=$'\033[2m'; C_B=$'\033[1m'; C_0=$'\033[0m'; TTY=1
else
  C_OK=""; C_ERR=""; C_WARN=""; C_DIM=""; C_B=""; C_0=""; TTY=0
fi
progress() { [ "$TTY" = "1" ] && printf '  %s…%s %s\r' "$C_DIM" "$C_0" "$1"; return 0; }
die() { printf '  %s❌%s %s\n' "$C_ERR" "$C_0" "$1" >&2; exit "${2:-1}"; }

if command -v pio >/dev/null 2>&1; then PIO="pio"
elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then PIO="$HOME/.platformio/penv/bin/pio"
else die "'pio' introuvable (ni PATH, ni ~/.platformio/penv/bin)." 127
fi

PREFIX="$(brew --prefix 2>/dev/null || echo /opt/homebrew)"
[ -f "$PREFIX/lib/libsimavrparts.a" ] || die "simavr absent ($PREFIX). brew install simavr" 127

WORK="$(mktemp -d)"; LOG="$WORK/log"
trap 'rm -rf "$WORK"' EXIT

printf '%s=== FRONTIERE EEPROM ===%s\n' "$C_B" "$C_0"

progress "build env:nanoatmega328"
if "$PIO" run -e nanoatmega328 -d "$ROOT" > "$LOG" 2>&1; then
  printf '  %s✅%s firmware               %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" \
    "$(grep -E '^RAM:' "$LOG" | sed 's/.*(used /RAM /; s/ bytes from .*/ o/')" "$C_0"
else
  printf '\n'; tail -30 "$LOG"; die "build du firmware en echec"
fi

. "$ROOT/tools/active-format.sh"
flexseq_resolve_active_format "$ROOT" "$ROOT/.pio/build/nanoatmega328/firmware.elf" "$WORK" || exit $?
flexseq_report_active_format "$C_OK" "$C_DIM" "$C_0"

progress "compilation du harnais"
BIN="$WORK/eeprom_boundary_probe"
if cc -O2 -Wall -I"$PREFIX/include/simavr" -I"$PREFIX/include" \
     "$ROOT/tools/simavr-ssd1306/eeprom_boundary_probe.c" -o "$BIN" \
     -L"$PREFIX/lib" -lsimavrparts -lsimavr -lelf > "$LOG" 2>&1; then
  printf '  %s✅%s harnais compile        %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" "$PREFIX" "$C_0"
else
  printf '\n'; cat "$LOG"; die "compilation du harnais en echec"
fi

progress "simulation ($DURATION s)"
if ! "$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" \
     "$FLEXSEQ_BASE_ADDRESS" "$FLEXSEQ_IMAGE_SIZE" "$DURATION" > "$LOG" 2>"$WORK/err"; then
  printf '\n'; cat "$WORK/err"; die "la sonde a echoue"
fi
printf '  %s✅%s simulation             %s%s s simulees%s\n' "$C_OK" "$C_0" "$C_DIM" \
  "$DURATION" "$C_0"

FORMAT_VERSION="$FLEXSEQ_FORMAT_VERSION" BASE_ADDRESS="$FLEXSEQ_BASE_ADDRESS" \
  IMAGE_SIZE="$FLEXSEQ_IMAGE_SIZE" MUTATE="${BOUNDARY_MUTATE:-}" python3 - "$LOG" <<'PY'
import os, re, sys

txt = open(sys.argv[1], errors='replace').read()
tty = sys.stdout.isatty()
OK, ERR, WARN, DIM, B, Z = (('\033[32m', '\033[31m', '\033[33m', '\033[2m', '\033[1m', '\033[0m')
                            if tty else ('',) * 6)
mark = lambda good: f"{OK}✅{Z}" if good else f"{ERR}❌{Z}"

window = re.search(r"fenetre_ecrite (\d+) sur (\d+) octets, version (\d+) a (\d+)", txt)
low = re.search(r"zone_basse ecarts (\d+) premier (-?\d+) sur (\d+) octets", txt)
high = re.search(r"zone_haute ecarts (\d+) premier (-?\d+) sur (\d+) octets", txt)
memcode = re.search(r"memcode_1023 (\d+) attendu (\d+)", txt)
if not (window and low and high and memcode):
    print(f"  {mark(False)} sortie du harnais illisible"); print(txt); sys.exit(5)

written, size, version, first = (int(window[1]), int(window[2]), window[3], int(window[4]))
expected_version = os.environ["FORMAT_VERSION"]
mutate = os.environ.get("MUTATE", "")

active = written > 0 and version == expected_version
low_ok = int(low[1]) == 0
high_ok = int(high[1]) == 0
memcode_ok = memcode[1] == memcode[2]

print()
print(f"{B}================= FRONTIERE EEPROM ================={Z}")
if mutate:
    print(f"  {WARN}⚠{Z}  MUTATION           adresse {mutate} — contre-epreuve, le rouge est attendu")
print(f"  {mark(active)} Fenetre ecrite     {written}/{size} octets, version {version} a {first}"
      f"{DIM}  — condition de VALIDITE, pas un succes{Z}")
print(f"  {mark(low_ok)} Zone basse         {low[1]} ecart(s) sur {low[3]} octets"
      + (f"  {ERR}premier a {low[2]}{Z}" if not low_ok else
         f"  {DIM}0..{first - 1}, reglages du firmware d'origine{Z}"))
print(f"  {mark(high_ok)} Zone haute         {high[1]} ecart(s) sur {high[3]} octets"
      + (f"  {ERR}premier a {high[2]}{Z}" if not high_ok else
         f"  {DIM}{first + size}..1023, dont le memCode{Z}"))
print(f"  {mark(memcode_ok)} memCode 1023       lu {memcode[1]}, attendu {memcode[2]}")
print(f"{B}==================================================={Z}")

if not active:
    print(f"  {WARN}INVALID{Z} : la fenetre n'a pas ete ecrite, ou sa version n'est pas"
          f" {expected_version}.")
    print("  Les zones protegees seraient intactes pour une raison sans rapport")
    print("  avec l'isolation. Ne pas lire ce resultat comme un succes.")
    sys.exit(5)
if low_ok and high_ok and memcode_ok:
    print(f"{DIM}  Aucune ecriture n'est sortie de la fenetre pendant la course.{Z}")
    print(f"{DIM}  Portee : les chemins empruntes pendant cette course, et eux seuls.{Z}")
    sys.exit(0)
print(f"  {ERR}Une zone protegee a change : une ecriture est sortie de la fenetre.{Z}")
sys.exit(1)
PY
