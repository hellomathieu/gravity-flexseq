#!/usr/bin/env bash
#
# Build AVR + rapport memoire, avec verdict de budget.
#
# Sur ATmega328P le linker ne compte que la RAM STATIQUE (.data + .bss). La pile
# et le heap ne sont pas mesures : un seuil a 100 % serait inutile, le build
# echouerait avant. Le garde-fou est donc une RESERVE laissee libre pour la pile.
#
# GARDE-FOU DE DERIVE, en plus des plafonds. Un plafond ne se declenche qu'a
# 95 % de Flash : une fonctionnalite qui prend 3 ko passerait sans un mot. Le
# script compare donc chaque build a un RELEVE VERSIONNE (tools/memory-baseline)
# et refuse une croissance au-dela d'un seuil. Accepter la nouvelle empreinte est
# un acte deliberé :
#
#     ./tools/run-build-memory.sh --accept
#
# GARDE SUR LES AVERTISSEMENTS DE LA DEPENDANCE. Les compter sans les comparer
# laisse passer un avertissement NEUF en silence : c'est ainsi que le mauvais
# typage de `Clock::SetSource()` est reste invisible des mois, alors que le
# compilateur le signalait a chaque build complet. Le critere est celui de la
# caracterisation — conformite a une reference versionnee
# (`tools/dependency-warnings`), rouge DANS LES DEUX SENS : un avertissement neuf
# est un defaut a documenter, un avertissement disparu veut dire que la
# dependance a change. Acter :
#
#     ./tools/run-build-memory.sh --accept-warnings
#
# Le numero de ligne et la colonne sont retires de la comparaison, sinon un
# reformatage de la dependance ferait rougir. Les doublons sont CONSERVES : les
# deux conversions invalides de `clock.h` comptent pour deux, donc en perdre une
# fait rougir.
#
# ⚠️ Le garde ne tourne QUE sur un build qui compile quelque chose. Sur un build
# incremental sans recompilation le script le dit explicitement, pour la meme
# raison qu'il refuse d'annoncer « 0 avertissement » : ce serait un faux negatif.
#
# Une BAISSE n'est jamais un echec : elle est signalee, et `--accept` rafraichit
# le releve. Le fichier absent n'est pas un echec non plus — le script propose de
# le creer.
#
# LE PLAFOND FLASH EST PASSE DE 90 A 95 % le 2026-08-22, decide par le
# proprietaire sur le chiffre reel. Le firmware complet — deux ecrans, les huit
# gestes, le transport, la persistance — mesure 28050 o, soit 91,3 %. Ce n'est pas
# un plafond qu'on repousse pour se donner de l'air : la VRAIE limite est 30720 o,
# et 95 % laisse encore 1134 o d'avertissement avant elle. Ce qu'on accepte est
# une reserve plus petite, pas un risque de brique — l'editeur de liens refuserait
# bien avant.
#
# Deux campagnes de recherche d'economies ont precede cette decision et rendu
# 1378 o : -mcall-prologues (534), le jeu de glyphes reduit u8g2_font_5x7_tr au
# lieu de _tf (808), et un formateur de nombres partage (36). Mesures et ecartees :
# le constructeur du moteur (88 o), les leviers d'inlining (0 o). Ce qui reste est
# soit la dependance epinglee — uClock 974 o d'ISR, Wire 574, u8g2 ~1500, et
# 586 o d'allocateur qu'uClock impose en allouant 4 octets — soit les deux
# renderers dont l'interface a besoin.
#
# Seuils (surchargeables) :
#   RAM_RESERVE=256     octets qui doivent rester libres pour la pile
#   FLASH_BUDGET_PCT=98 part de Flash au-dela de laquelle on refuse
#   RAM_DRIFT=16        croissance de RAM acceptee sans acquittement
#   FLASH_DRIFT=512     croissance de Flash acceptee sans acquittement
#
# Le transcript de PlatformIO est MASQUE : il fait plusieurs centaines de lignes
# (U8g2 compile ~150 pilotes d'ecran) pour trois chiffres utiles. `VERBOSE=1` le
# reaffiche integralement.
#
# Sortie != 0 si un budget est depasse.
#
set -uo pipefail

cd "$(git rev-parse --show-toplevel)"

RAM_RESERVE="${RAM_RESERVE:-256}"
FLASH_BUDGET_PCT="${FLASH_BUDGET_PCT:-98}"
RAM_DRIFT="${RAM_DRIFT:-16}"
FLASH_DRIFT="${FLASH_DRIFT:-512}"
BASELINE="tools/memory-baseline"
ACCEPT=0
ACCEPT_WARN=0
for arg in "$@"; do
  case "$arg" in
    --accept) ACCEPT=1 ;;
    --accept-warnings) ACCEPT_WARN=1 ;;
    *) echo "argument inconnu : $arg (attendu : --accept ou --accept-warnings)" >&2; exit 2 ;;
  esac
done
VERBOSE="${VERBOSE:-0}"

if [ -t 1 ]; then
  C_OK=$'\033[32m'; C_ERR=$'\033[31m'; C_DIM=$'\033[2m'; C_B=$'\033[1m'; C_0=$'\033[0m'
  TTY=1
else
  C_OK=""; C_ERR=""; C_DIM=""; C_B=""; C_0=""; TTY=0
fi

# Localise pio : PATH d'abord, sinon l'installation PlatformIO par defaut.
# Meme resolution que tools/run-cpp-tests.sh. Sans elle, ce script echouait avec
# « pio: command not found » des que PlatformIO n'etait pas dans le PATH.
if command -v pio >/dev/null 2>&1; then
  PIO="pio"
elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then
  PIO="$HOME/.platformio/penv/bin/pio"
else
  echo "erreur : 'pio' introuvable (ni dans le PATH, ni dans ~/.platformio/penv/bin)." >&2
  echo "Installe PlatformIO Core : https://docs.platformio.org/en/latest/core/installation/" >&2
  exit 127
fi

# Outils binutils AVR, pour le detail par section et par symbole. Optionnels :
# leur absence degrade la sortie sans faire echouer le script.
find_avr_tool() {
  if command -v "avr-$1" >/dev/null 2>&1; then
    echo "avr-$1"
  elif [ -x "$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-$1" ]; then
    echo "$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-$1"
  fi
}
AVR_NM="$(find_avr_tool nm)"
AVR_SIZE="$(find_avr_tool size)"

ELF=".pio/build/nanoatmega328/firmware.elf"
LOG="$(mktemp -t flexseq-mem)"
trap 'rm -f "$LOG"' EXIT

# --- Build -------------------------------------------------------------------
# Une seule invocation. Le tableau des sections etait auparavant obtenu par un
# SECOND build en `-v` ; il vient desormais de `avr-size -A` sur le .elf, qui est
# exactement ce que ce build affichait. Meme donnee, meme fichier, un build de
# moins.
echo "${C_B}Build AVR${C_0} ${C_DIM}nanoatmega328${C_0}"

if [ "$VERBOSE" != "0" ]; then
  "$PIO" run -e nanoatmega328 2>&1 | tee "$LOG"
  build_status="${PIPESTATUS[0]}"
else
  : >"$LOG"
  "$PIO" run -e nanoatmega328 2>&1 | while IFS= read -r line; do
    printf '%s\n' "$line" >>"$LOG"
    if [ "$TTY" = "1" ]; then
      case "$line" in
        Compiling*)  n=$(( ${n:-0} + 1 )); printf '\r  %s▸%s compilation … %d unites   ' "$C_DIM" "$C_0" "$n" ;;
        Archiving*)  printf '\r  %s▸%s archivage …                    ' "$C_DIM" "$C_0" ;;
        Linking*)    printf '\r  %s▸%s edition de liens …             ' "$C_DIM" "$C_0" ;;
        Building*.hex) printf '\r  %s▸%s generation du .hex …          ' "$C_DIM" "$C_0" ;;
      esac
    fi
  done
  build_status="${PIPESTATUS[0]}"
  [ "$TTY" = "1" ] && printf '\r%*s\r' 45 ''
fi

if [ "$build_status" != "0" ]; then
  echo
  echo "  ${C_ERR}${C_B}❌ BUILD EN ECHEC${C_0} — pas de verdict memoire." >&2
  echo "  Erreurs :" >&2
  grep -E 'error:|Error [0-9]|\*\*\*' "$LOG" | head -15 >&2
  [ "$VERBOSE" = "0" ] && echo "  (VERBOSE=1 pour le transcript complet)" >&2
  exit "$build_status"
fi

units=$(grep -c '^Compiling ' "$LOG")
took=$(sed -nE 's/.*\[SUCCESS\] Took ([0-9.]+) seconds.*/\1/p' "$LOG" | tail -1)
printf '  %s✅ build OK%s  %s%s unites compilees, %s s%s\n' \
  "$C_OK" "$C_0" "$C_DIM" "$units" "${took:-?}" "$C_0"

# --- Avertissements ----------------------------------------------------------
# Ceux de la dependance figee sont connus et non corrigeables (libGravity est
# epinglee) : on les compte. Ceux de notre code sont les seuls actionnables :
# on les affiche.
WARN_REF="tools/dependency-warnings"
WARN_DRIFT=0
warn_all="$(sed -E 's/\x1B\[[0-9;]*[mK]//g' "$LOG" | grep -E '^[^ ].*: warning: ' | sort -u)"
warn_dep=$(printf '%s\n' "$warn_all" | grep -c '^\.pio/')
warn_own="$(printf '%s\n' "$warn_all" | grep -E '^(src|include)/')"
warn_own_n=$(printf '%s\n' "$warn_own" | grep -c .)

if [ "$units" = "0" ]; then
  # Rien n'a ete recompile : le build n'a produit AUCUN avertissement, ce qui ne
  # dit rien de l'etat du code. Annoncer « 0 » ici serait un faux negatif.
  printf '  %sbuild incrementiel — aucune unite recompilee, donc rien a dire des\n' "$C_DIM"
  printf '  avertissements (relancer apres `pio run -t clean` pour les revoir)%s\n' "$C_0"
else
  if [ "$warn_own_n" -gt 0 ]; then
    printf '  %s⚠  %d avertissement(s) dans NOTRE code :%s\n' "$C_ERR" "$warn_own_n" "$C_0"
    printf '%s\n' "$warn_own" | sed 's/^/     /'
  fi
  # Le compte n'est annonce que si la dependance a ete recompilee : sinon c'est
  # le meme faux negatif que d'annoncer « 0 » sur un build incremental.
  dep_units=$(grep -cE '^Compiling \.pio/build/[^/]+/lib' "$LOG")
  ref_units="$(sed -n 's/^# units=\([0-9]*\)$/\1/p' "$WARN_REF" 2>/dev/null)"
  if [ "$dep_units" = "${ref_units:-x}" ] || [ "$ACCEPT_WARN" = "1" ]; then
    printf '  %s%d avertissement(s) dans la dependance figee — connus, non corrigeables%s\n' \
    "$C_DIM" "$warn_dep" "$C_0"
  fi

  # Garde de derive sur les avertissements de la dependance. Les COMPTER sans les
  # comparer laisse passer un avertissement NEUF en silence : c'est ainsi que le
  # mauvais typage de Clock::SetSource() a vecu des mois. Le critere est celui de
  # la caracterisation : conformite a une reference versionnee, rouge DANS LES
  # DEUX SENS. Le numero de ligne et la colonne sont retires, sinon un simple
  # reformatage de la dependance ferait rougir.
  warn_norm="$(printf '%s\n' "$warn_all" | grep '^\.pio/' \
    | sed -E 's/^([^:]+):[0-9]+:[0-9]+: warning: /\1: /' | sort)"

  # Le garde n'a de sens que si la DEPENDANCE a ete recompilee. Un build qui ne
  # reconstruit qu'un de nos fichiers ne produit aucun avertissement de la
  # dependance : l'ensemble observe est vide, et le comparer annoncerait une
  # derive qui n'existe pas. Vecu le 2026-08-24, des le premier usage. Le critere
  # est donc le nombre d'unites de la dependance recompilees, garde dans la
  # reference ; s'il ne correspond pas, le garde dit qu'il n'a pas pu se
  # prononcer — jamais qu'il passe.
  if [ "$ACCEPT_WARN" = "1" ]; then
    { echo "# units=$dep_units"; printf '%s\n' "$warn_norm"; } > "$WARN_REF"
    printf '  %s✅%s reference d avertissements mise a jour (%d, sur %d unites)\n' \
      "$C_OK" "$C_0" "$warn_dep" "$dep_units"
  elif [ ! -f "$WARN_REF" ]; then
    printf '  %s⚠  aucune reference d avertissements (%s)%s\n' \
      "$C_DIM" "$WARN_REF" "$C_0"
    printf '     la creer : ./tools/run-build-memory.sh --accept-warnings\n'
  elif [ "$dep_units" != "${ref_units:-x}" ]; then
    printf '  %sgarde non evaluable : %s unite(s) de la dependance recompilee(s)\n' \
      "$C_DIM" "$dep_units"
    printf '  contre %s dans la reference — relancer apres `pio run -t clean`%s\n' \
      "${ref_units:-?}" "$C_0"
  elif ! diff -q <(sed '/^# /d' "$WARN_REF") <(printf '%s\n' "$warn_norm") >/dev/null 2>&1; then
    printf '  %s❌ DERIVE des avertissements de la dependance%s\n' "$C_ERR" "$C_0"
    diff <(sed '/^# /d' "$WARN_REF") <(printf '%s\n' "$warn_norm") | sed 's/^/     /'
    printf '     un avertissement NEUF est un defaut a documenter (docs/upstream-defects.md).\n'
    printf '     un avertissement DISPARU veut dire que la dependance a change.\n'
    printf '     acter : ./tools/run-build-memory.sh --accept-warnings\n'
    WARN_DRIFT=1
  else
    printf '  %sconformes a la reference versionnee%s\n' "$C_DIM" "$C_0"
  fi
fi

# --- Extraction des chiffres -------------------------------------------------
# Ligne visee : « RAM:  [====  ] 71.6% (used 1466 bytes from 2048 bytes) »
mem_pair() {
  sed -E 's/\x1B\[[0-9;]*[mK]//g' "$LOG" | grep -E "^$1:" | tail -1 \
    | sed -nE 's/.*used ([0-9]+) bytes from ([0-9]+) bytes.*/\1 \2/p'
}
read -r RAM_USED RAM_TOTAL <<<"$(mem_pair RAM)"
read -r FLASH_USED FLASH_TOTAL <<<"$(mem_pair Flash)"

if [ -z "${RAM_USED:-}" ] || [ -z "${FLASH_USED:-}" ]; then
  echo
  echo "  ⚠  Impossible de lire l'empreinte memoire dans la sortie du build." >&2
  echo "     Verdict non calcule ; le build lui-meme a reussi." >&2
  exit 0
fi

RAM_FREE=$(( RAM_TOTAL - RAM_USED ))
RAM_MAX=$(( RAM_TOTAL - RAM_RESERVE ))
FLASH_MAX=$(( FLASH_TOTAL * FLASH_BUDGET_PCT / 100 ))
# Arrondi au plus proche, pour afficher le meme chiffre que PlatformIO.
ram_pct_x10=$(( (RAM_USED * 1000 + RAM_TOTAL / 2) / RAM_TOTAL ))
flash_pct_x10=$(( (FLASH_USED * 1000 + FLASH_TOTAL / 2) / FLASH_TOTAL ))

fail=0
if [ "$RAM_USED" -le "$RAM_MAX" ]; then RAM_MARK="${C_OK}✅${C_0}"; else RAM_MARK="${C_ERR}❌${C_0}"; fail=1; fi
if [ "$FLASH_USED" -le "$FLASH_MAX" ]; then FLASH_MARK="${C_OK}✅${C_0}"; else FLASH_MARK="${C_ERR}❌${C_0}"; fail=1; fi

# --- Verdict -----------------------------------------------------------------
echo
echo "${C_B}=================== BUDGET MEMOIRE (ATmega328P) ===================${C_0}"
printf '  %s RAM statique  %5d / %d o  (%d.%d %%)  libre %d o  %s— reserve exigee %d o%s\n' \
  "$RAM_MARK" "$RAM_USED" "$RAM_TOTAL" $((ram_pct_x10/10)) $((ram_pct_x10%10)) "$RAM_FREE" \
  "$C_DIM" "$RAM_RESERVE" "$C_0"
printf '  %s Flash         %5d / %d o  (%d.%d %%)  %s— budget %d %% = %d o%s\n' \
  "$FLASH_MARK" "$FLASH_USED" "$FLASH_TOTAL" $((flash_pct_x10/10)) $((flash_pct_x10%10)) \
  "$C_DIM" "$FLASH_BUDGET_PCT" "$FLASH_MAX" "$C_0"
echo "${C_B}===================================================================${C_0}"

# --- Derive par rapport au releve versionne ------------------------------------
BASE_RAM=""; BASE_FLASH=""; BASE_DATE=""
if [ -f "$BASELINE" ]; then
  BASE_RAM="$(sed -n 's/^ram=//p' "$BASELINE" | head -1)"
  BASE_FLASH="$(sed -n 's/^flash=//p' "$BASELINE" | head -1)"
  BASE_DATE="$(sed -n 's/^date=//p' "$BASELINE" | head -1)"
fi

write_baseline() {
  {
    echo "# Empreinte de reference du firmware de production (env:nanoatmega328)."
    echo "# Mise a jour deliberee : ./tools/run-build-memory.sh --accept"
    echo "date=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "commit=$(git rev-parse --short HEAD 2>/dev/null || echo inconnu)"
    echo "ram=$RAM_USED"
    echo "flash=$FLASH_USED"
  } > "$BASELINE"
}

if [ "$ACCEPT" = "1" ]; then
  write_baseline
  printf '  %s✅%s releve mis a jour : RAM %d o, Flash %d o (%s)\n' \
    "$C_OK" "$C_0" "$RAM_USED" "$FLASH_USED" "$BASELINE"
elif [ -z "$BASE_RAM" ]; then
  printf '  %s—%s aucun releve de reference. Le creer : %s--accept%s\n' \
    "$C_DIM" "$C_0" "$C_B" "$C_0"
else
  d_ram=$(( RAM_USED - BASE_RAM ))
  d_flash=$(( FLASH_USED - BASE_FLASH ))
  drift=0
  [ "$d_ram" -gt "$RAM_DRIFT" ] && drift=1
  [ "$d_flash" -gt "$FLASH_DRIFT" ] && drift=1
  if [ "$drift" = "1" ]; then
    printf '  %s❌%s DERIVE  RAM %+d o, Flash %+d o  %spar rapport au %s%s\n' \
      "$C_ERR" "$C_0" "$d_ram" "$d_flash" "$C_DIM" "$BASE_DATE" "$C_0"
    echo "     Seuils : RAM +$RAM_DRIFT o, Flash +$FLASH_DRIFT o. Une croissance"
    echo "     au-dela ne doit pas passer inapercue. L'acquitter si elle est"
    echo "     voulue : ./tools/run-build-memory.sh --accept"
    fail=1
  elif [ "$d_ram" -lt "-$RAM_DRIFT" ] || [ "$d_flash" -lt "-$FLASH_DRIFT" ]; then
    printf '  %s✅%s derive   RAM %+d o, Flash %+d o  %s— baisse ; --accept pour rafraichir%s\n' \
      "$C_OK" "$C_0" "$d_ram" "$d_flash" "$C_DIM" "$C_0"
  else
    printf '  %s✅%s derive   RAM %+d o, Flash %+d o  %s— dans les seuils%s\n' \
      "$C_OK" "$C_0" "$d_ram" "$d_flash" "$C_DIM" "$C_0"
  fi
fi

if [ "$fail" = "0" ] && [ "$WARN_DRIFT" = "1" ]; then
  echo "  Les budgets sont respectes, mais les avertissements de la dependance"
  echo "  ont derive. Un avertissement neuf de la dependance est un defaut a"
  echo "  documenter, pas un detail : c'est ainsi que le mauvais typage de"
  echo "  Clock::SetSource() est reste invisible des mois."
  exit 1
fi

if [ "$fail" = "0" ]; then
  echo "  Les budgets sont respectes."
  exit 0
fi

# --- Diagnostic : d'ou vient le depassement ? --------------------------------
echo
echo "--- Origine du depassement -----------------------------------------"
if [ "$RAM_USED" -gt "$RAM_MAX" ]; then
  echo "RAM : il ne reste que $RAM_FREE o pour la pile et le heap (reserve exigee :"
  echo "      $RAM_RESERVE o). Sur AVR le linker n'en voit rien : un debordement de pile"
  echo "      se manifeste par une corruption silencieuse, pas par une erreur de build."
fi
if [ "$FLASH_USED" -gt "$FLASH_MAX" ]; then
  echo "Flash : $FLASH_USED o utilises, au-dela du budget de $FLASH_BUDGET_PCT %."
fi

if [ -n "$AVR_SIZE" ] && [ -f "$ELF" ]; then
  echo
  echo "Sections de $ELF :"
  "$AVR_SIZE" -A "$ELF" 2>/dev/null \
    | awk '/^\.(text|data|bss|rodata)/ {printf "  %-12s %8s o\n", $1, $2}'
fi

if [ -n "$AVR_NM" ] && [ -f "$ELF" ]; then
  echo
  echo "Plus gros consommateurs de RAM (.bss / .data) :"
  "$AVR_NM" --print-size --size-sort --radix=d "$ELF" 2>/dev/null \
    | awk 'toupper($3) ~ /^[BD]$/ {printf "  %6d o  %-3s %s\n", $2, $3, $4}' \
    | sort -k1,1nr | head -10
  echo
  echo "Plus gros consommateurs de Flash (.text / .rodata) :"
  "$AVR_NM" --print-size --size-sort --radix=d "$ELF" 2>/dev/null \
    | awk 'toupper($3) ~ /^[TR]$/ {printf "  %6d o  %-3s %s\n", $2, $3, $4}' \
    | sort -k1,1nr | head -10
  echo
  echo "Rappel : un symbole 'D' occupe a la fois de la RAM et de la Flash (son"
  echo "initialiseur). Une table 'constexpr' non 'PROGMEM' atterrit en .data."
else
  echo
  echo "(avr-nm introuvable : diagnostic par symbole indisponible)"
fi
echo "--------------------------------------------------------------------"
exit 1
