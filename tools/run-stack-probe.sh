#!/usr/bin/env bash
#
# Mesure la PILE du firmware de production, a l'execution, sans l'instrumenter.
#
# Pourquoi. Sur AVR le linker ne compte que la RAM statique (.data + .bss). Un
# debordement de pile ne produit donc jamais d'erreur de lien, seulement une
# corruption silencieuse. Le seuil RAM_RESERVE de run-build-memory.sh etait un
# garde-fou pose a l'estime ; ce script le remplace par un nombre.
#
# Methode : peinture. tools/simavr-ssd1306/stack_probe.c ecrit un motif dans la
# RAM libre de la machine simulee AVANT le premier cycle, laisse tourner le
# firmware, puis relit la frontiere du motif intact depuis le HAUT (le bas de la
# RAM libre est le debut du tas : une allocation salirait le motif sans que la
# pile y soit descendue).
#
# LES DEUX ANGLES MORTS SONT FERMES depuis le 2026-08-20, et ils coutaient 43 o.
# La version precedente peignait depuis le firmware au debut de `setup()` et
# publiait le resultat en largeur d'impulsion, faute de pouvoir lire la memoire du
# simulateur. Elle exigeait une sonde dans main.cpp et un environnement dedie, et
# elle ne voyait ni la pile d'avant `setup()` (constructeurs globaux, init()
# d'Arduino : +24 o) ni les ISR d'entree, jamais exercees (+19 o). Un harnais C
# peint avant le premier cycle et injecte de quoi faire entrer les ISR : le
# binaire mesure est desormais celui qui sera flashe, sans un octet de sonde.
#
# VERDICT — trois criteres :
#   1. toutes les interruptions surveillees ont ete PARCOURUES. Sans cela la
#      mesure est incomplete en silence, ce qui etait exactement le defaut d'avant ;
#   2. le pic tient dans RAM_RESERVE, le garde-fou du projet ;
#   3. le pic tient dans la RAM libre — condition absolue : au-dela, la pile
#      ecrase .bss ;
#   4. l'ecriture EEPROM de la persistance a EU LIEU pendant la mesure : l'octet
#      de version relu vaut celui du format ACTIF, a l'adresse de ce format.
# Sortie 0 si les quatre passent, 1 sinon, 127 si un outil manque.
#
# LE FORMAT ACTIF EST LU SUR LE BINAIRE, JAMAIS DEVINE SUR LE FICHIER. La version
# precedente decoupait Persistence.h avant `namespace v3 {` et prenait les
# constantes qui restaient : elle a donc attendu la v2 apres l'activation de la
# v3, et rendu un rouge qui ne disait rien du firmware. L'image liee est
# desormais identifiee par ses symboles dans firmware.elf, et les constantes
# viennent du COMPILATEUR via tools/persistence-format.cpp, qui inclut l'en-tete
# normatif. Deux images liees, ou aucune, arretent le script sans verdict.
# La resolution vit dans tools/active-format.sh, partagee par les quatre sondes :
# quatre copies donneraient quatre facons de diverger. FLEXSEQ_FORMAT_FORCE=2 y
# est le levier de contre-epreuve.
#
# Le nombre d'octets non vierges est une MESURE, pas un critere : rien
# n'etablit aujourd'hui combien d'octets un semis d'usine laisse non vierges.
#
# Reglages : RAM_RESERVE (defaut 256), DURATION (defaut 8 s), QUIET=1 pour
# mesurer sans injection (utile pour attribuer l'ecart aux ISR).

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RAM_RESERVE="${RAM_RESERVE:-256}"
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
[ -f "$PREFIX/lib/libsimavrparts.a" ] || die "simavr absent ($PREFIX). brew install simavr" 127
AVR_NM="$(command -v avr-nm || echo "$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-nm")"
[ -x "$AVR_NM" ] || die "avr-nm introuvable." 127

LOG="$(mktemp)"; BIN="$(mktemp -d)/stack_probe"
trap 'rm -f "$LOG"; rm -rf "$(dirname "$BIN")"' EXIT

progress "build env:nanoatmega328"
if "$PIO" run -e nanoatmega328 -d "$ROOT" > "$LOG" 2>&1; then
  printf '  %s✅%s firmware               %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" \
    "$(grep -E '^RAM:' "$LOG" | sed 's/.*(used /RAM /; s/ bytes from .*/ o/')" "$C_0"
else
  printf '\n'; tail -30 "$LOG"; die "build en echec"
fi

ELF="$ROOT/.pio/build/nanoatmega328/firmware.elf"
END_HEX="$("$AVR_NM" --radix=x "$ELF" | grep -E " _end$" | head -1 | cut -d' ' -f1)"
[ -n "$END_HEX" ] || die "symbole '_end' introuvable dans le firmware"
END="$(printf '0x%x' $(( 0x$END_HEX - 0x800000 )))"
printf '  %s✅%s symbole                %s_end %s%s\n' "$C_OK" "$C_0" "$C_DIM" "$END" "$C_0"

# Temoin du chemin de chargement, utile seulement avec EE_IMAGE. loaded[] vit
# dans ModulatedPatternState, apres les six Pattern et les six longueurs.
LOADED_ADDR=""
if [ -n "${EE_IMAGE:-}" ]; then
  STATE_HEX="$("$AVR_NM" --radix=x --demangle "$ELF" \
    | grep -E "modulatedPatterns$" | head -1 | cut -d' ' -f1)"
  if [ -n "$STATE_HEX" ]; then
    LOADED_ADDR="$(printf '0x%x' $(( 0x$STATE_HEX - 0x800000 + 6 * 23 + 6 )))"
  fi
fi

. "$ROOT/tools/active-format.sh"
FMT_WORK="$(dirname "$BIN")"
flexseq_resolve_active_format "$ROOT" "$ELF" "$FMT_WORK" || exit $?
FORMAT_VERSION="$FLEXSEQ_FORMAT_VERSION"
IMAGE_SIZE="$FLEXSEQ_IMAGE_SIZE"
SCAN_SIZE="$FLEXSEQ_SCAN_SIZE"
VERSION_OFFSET="$FLEXSEQ_VERSION_OFFSET"
BASE_ADDRESS="$FLEXSEQ_BASE_ADDRESS"
flexseq_report_active_format "$C_OK" "$C_DIM" "$C_0"

progress "compilation du harnais"
if cc -O2 -Wall -DIMAGE_SIZE="$IMAGE_SIZE" -DVERSION_OFFSET="$VERSION_OFFSET" \
     -DBASE_ADDRESS="$BASE_ADDRESS" -I"$PREFIX/include/simavr" -I"$PREFIX/include" \
     "$ROOT/tools/simavr-ssd1306/stack_probe.c" -o "$BIN" \
     -L"$PREFIX/lib" -lsimavrparts -lsimavr -lelf > "$LOG" 2>&1; then
  printf '  %s✅%s harnais compile        %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" "$PREFIX" "$C_0"
else
  printf '\n'; cat "$LOG"; die "compilation du harnais en echec"
fi

progress "simulation ($DURATION s)"
LOADED_ADDR="$LOADED_ADDR" \
  "$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$END" "$DURATION" "${EE_IMAGE:-}" \
  > "$LOG" 2>/dev/null || die "la sonde a echoue"
printf '  %s✅%s simulation             %s%s s%s%s\n' "$C_OK" "$C_0" "$C_DIM" "$DURATION" \
  "${QUIET:+, sans injection}" "$C_0"

RAM_RESERVE="$RAM_RESERVE" QUIET="${QUIET:-}" FORMAT_VERSION="$FORMAT_VERSION" \
  EE_IMAGE="${EE_IMAGE:-}" LOADED="$LOADED_ADDR" \
  IMAGE_SIZE="$IMAGE_SIZE" SCAN_SIZE="$SCAN_SIZE" \
  VERSION_ADDRESS="$((BASE_ADDRESS + VERSION_OFFSET))" python3 - "$LOG" <<'PY'
import os, re, sys

txt = open(sys.argv[1], errors='replace').read()
tty = sys.stdout.isatty()
OK, ERR, DIM, B, Z = ('\033[32m', '\033[31m', '\033[2m', '\033[1m', '\033[0m') if tty else ('',) * 5
mark = lambda good: f"{OK}✅{Z}" if good else f"{ERR}❌{Z}"
reserve = int(os.environ["RAM_RESERVE"])
quiet = bool(os.environ.get("QUIET"))

m = re.search(r"pic (\d+) o sur (\d+) libres, marge (\d+)", txt)
version = re.search(r"octet de version a (\d+) : (\d+)", txt)
written = re.search(r"\((\d+) octets non vierges sur (\d+) lus\)", txt)
if not m:
    print(f"  {mark(False)} sortie du harnais illisible"); print(txt); sys.exit(1)
peak, free, margin = int(m[1]), int(m[2]), int(m[3])

isrs = re.findall(r"^  (\S+\s+\S+.*?)\s+(\d+)$", txt, re.M)
silent = [name.strip() for name, n in isrs if int(n) == 0]
all_entered = not silent

fits_reserve = peak <= reserve
fits_ram = peak <= free

print()
print(f"{B}=================== PILE (firmware de production) =================={Z}")
if quiet:
    print(f"  {mark(True)} Interruptions      injection desactivee (QUIET) "
          f"{DIM}— mesure volontairement partielle{Z}")
else:
    print(f"  {mark(all_entered)} Interruptions      "
          f"{len(isrs) - len(silent)}/{len(isrs)} vecteurs parcourus"
          + (f"  {ERR}muets : {', '.join(silent)}{Z}" if silent else
             f"  {DIM}(encodeur, uClock, millis, MIDI, ADC){Z}"))
print(f"  {mark(fits_reserve)} Pic de pile        {peak:4d} o   "
      f"{DIM}— reserve exigee {reserve} o ({peak * 100 // reserve} % utilises){Z}")
print(f"  {mark(fits_ram)} Marge              {margin:4d} o   "
      f"{DIM}— RAM libre {free} o apres .data + .bss{Z}")
expected_version = os.environ["FORMAT_VERSION"]
expected_address = os.environ["VERSION_ADDRESS"]
expected_size = os.environ["IMAGE_SIZE"]
scan_size = os.environ["SCAN_SIZE"]
read_back = written is not None and written[2] == expected_size
preloaded = bool(os.environ.get("EE_IMAGE"))
persisted = (version is not None and version[1] == expected_address
             and version[2] == expected_version and read_back)
loaded = re.search(r"loaded\[\]=([\d,]+)", txt)
if preloaded:
    if os.environ.get("LOADED") and loaded is None:
        print(f"  {mark(False)} Chargement          TEMOIN ABSENT — le releve de loaded[] n'a pas ete emis")
        sys.exit(1)
    if loaded is not None:
        values = [int(v) for v in loaded[1].split(',')]
        served = [v for v in values if v != 255]
        print(f"  {mark(bool(served))} Chargement          "
              f"loaded[] = {values}"
              f"{DIM}  — au moins un canal doit porter un template{Z}")
        if not served:
            print(f"  {ERR}   Aucun canal n'a charge : la chaine mesuree n'inclut PAS "
                  f"le chargement.{Z}")
            sys.exit(1)
    # Une image prechargee est acceptee par bootstrap(), qui n'ecrit alors rien.
    # L'octet de version relu serait le NOTRE : le critere passerait pour la
    # mauvaise raison. Il devient donc non evaluable, et la course ne mesure que
    # le chemin de chargement.
    print(f"  {DIM}·{Z} Persistance         "
          f"NON EVALUABLE — image prechargee, aucune ecriture du firmware attendue")
else:
    print(f"  {mark(persisted)} Persistance         "
          f"octet de version {version[2] if version else 'ABSENT'} "
          f"a {version[1] if version else '?'}, attendu {expected_version} a {expected_address}"
          f"{DIM}  (constate, pas suppose){Z}")
    print(f"  {DIM}   octets non vierges  {written[1] if written else '?'} sur "
          f"{written[2] if written else '?'} relus — mesure, sans attente ;"
          f" {scan_size} o sont balayes{Z}")
print(f"{B}==================================================================={Z}")

if not fits_ram:
    print(f"  {ERR}DEBORDEMENT : le pic depasse la RAM libre — la pile ecrase .bss.{Z}")
elif not fits_reserve:
    print(f"  {ERR}La reserve de {reserve} o ne couvre plus le pic de {peak} o.{Z}")
    print("  Reduire la pile, ou relever RAM_RESERVE en connaissance de cause —")
    print("  jamais pour faire passer un build.")
elif not all_entered and not quiet:
    print(f"  {ERR}Un vecteur muet rend la mesure incomplete{Z} : une ISR non parcourue")
    print("  s'empilerait par-dessus ce pic sans apparaitre ici.")
else:
    print(f"  La reserve couvre le pic mesure ({reserve / peak:.1f}x).")
    print(f"{DIM}  Couvert : la pile d'avant setup() (constructeurs globaux, init()"
          f" d'Arduino){Z}")
    print(f"{DIM}  et les six familles d'ISR, injection comprise, ainsi que l'ecriture{Z}")
    print(f"{DIM}  EEPROM de la persistance, constatee ci-dessus et non supposee. Restent{Z}")
    print(f"{DIM}  hors mesure les chemins que le firmware n'emprunte pas encore.{Z}")

sys.exit(0 if (fits_reserve and fits_ram and (persisted or preloaded)
               and (quiet or all_entered)) else 1)
PY
