#!/usr/bin/env bash
#
# Build AVR + rapport memoire, avec verdict de budget.
#
# Sur ATmega328P le linker ne compte que la RAM STATIQUE (.data + .bss). La pile
# et le heap ne sont pas mesures : un seuil a 100 % serait inutile, le build
# echouerait avant. Le garde-fou est donc une RESERVE laissee libre pour la pile.
#
# Seuils (surchargeables) :
#   RAM_RESERVE=512     octets qui doivent rester libres pour la pile + heap
#   FLASH_BUDGET_PCT=90 part de Flash au-dela de laquelle on refuse
#
# Sortie != 0 si un budget est depasse.
#
set -uo pipefail

cd "$(git rev-parse --show-toplevel)"

RAM_RESERVE="${RAM_RESERVE:-512}"
FLASH_BUDGET_PCT="${FLASH_BUDGET_PCT:-90}"

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

# avr-nm sert au diagnostic par symbole. Optionnel : son absence degrade la
# sortie sans faire echouer le script.
if command -v avr-nm >/dev/null 2>&1; then
  AVR_NM="avr-nm"
elif [ -x "$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-nm" ]; then
  AVR_NM="$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-nm"
else
  AVR_NM=""
fi

ELF=".pio/build/nanoatmega328/firmware.elf"
LOG="$(mktemp -t flexseq-mem)"
trap 'rm -f "$LOG"' EXIT

"$PIO" run -e nanoatmega328 2>&1 | tee "$LOG"
build_status="${PIPESTATUS[0]}"
if [ "$build_status" != "0" ]; then
  echo
  echo "❌ BUILD EN ECHEC — pas de verdict memoire." >&2
  exit "$build_status"
fi

printf '\n--- Detailed memory report ---\n'
"$PIO" run -e nanoatmega328 -v 2>&1 | tee -a "$LOG"

# --- Extraction des chiffres -------------------------------------------------
# Lignes visees : « RAM:  [====  ] 71.6% (used 1466 bytes from 2048 bytes) »
# On prend la DERNIERE occurrence (deux builds ont ete lances) et on retire les
# eventuels codes ANSI.
mem_line() {
  sed -E 's/\x1B\[[0-9;]*[mK]//g' "$LOG" | grep -E "^$1:" | tail -1
}
mem_pair() {
  mem_line "$1" | sed -nE 's/.*used ([0-9]+) bytes from ([0-9]+) bytes.*/\1 \2/p'
}

read -r RAM_USED RAM_TOTAL <<<"$(mem_pair RAM)"
read -r FLASH_USED FLASH_TOTAL <<<"$(mem_pair Flash)"

if [ -z "${RAM_USED:-}" ] || [ -z "${FLASH_USED:-}" ]; then
  echo
  echo "⚠️  Impossible de lire l'empreinte memoire dans la sortie du build." >&2
  echo "    Verdict non calcule ; le build lui-meme a reussi." >&2
  exit 0
fi

RAM_FREE=$(( RAM_TOTAL - RAM_USED ))
RAM_MAX=$(( RAM_TOTAL - RAM_RESERVE ))
FLASH_MAX=$(( FLASH_TOTAL * FLASH_BUDGET_PCT / 100 ))
ram_pct_x10=$(( RAM_USED * 1000 / RAM_TOTAL ))
flash_pct_x10=$(( FLASH_USED * 1000 / FLASH_TOTAL ))

fail=0
if [ "$RAM_USED" -le "$RAM_MAX" ]; then RAM_MARK="✅"; else RAM_MARK="❌"; fail=1; fi
if [ "$FLASH_USED" -le "$FLASH_MAX" ]; then FLASH_MARK="✅"; else FLASH_MARK="❌"; fail=1; fi

# --- Verdict -----------------------------------------------------------------
echo
echo "=================== BUDGET MEMOIRE (ATmega328P) ==================="
printf '  %s RAM statique  %5d / %d o  (%d.%d %%)  libre %d o  — reserve exigee %d o\n' \
  "$RAM_MARK" "$RAM_USED" "$RAM_TOTAL" $((ram_pct_x10/10)) $((ram_pct_x10%10)) "$RAM_FREE" "$RAM_RESERVE"
printf '  %s Flash         %5d / %d o  (%d.%d %%)  — budget %d %% = %d o\n' \
  "$FLASH_MARK" "$FLASH_USED" "$FLASH_TOTAL" $((flash_pct_x10/10)) $((flash_pct_x10%10)) \
  "$FLASH_BUDGET_PCT" "$FLASH_MAX"
echo "==================================================================="

if [ "$fail" = "0" ]; then
  echo "  Les deux budgets sont respectes."
  exit 0
fi

# --- Diagnostic : d'ou vient le depassement ? --------------------------------
echo
echo "--- Origine du depassement -----------------------------------------"
if [ "$RAM_MARK" = "❌" ]; then
  echo "RAM : il ne reste que $RAM_FREE o pour la pile et le heap (reserve exigee :"
  echo "      $RAM_RESERVE o). Sur AVR le linker n'en voit rien : un debordement de pile"
  echo "      se manifeste par une corruption silencieuse, pas par une erreur de build."
fi
if [ "$FLASH_MARK" = "❌" ]; then
  echo "Flash : $FLASH_USED o utilises, au-dela du budget de $FLASH_BUDGET_PCT %."
fi

echo
echo "Sections de $ELF :"
sed -E 's/\x1B\[[0-9;]*[mK]//g' "$LOG" \
  | awk '/^\.(text|data|bss|rodata)/ {printf "  %-12s %8s o\n", $1, $2}' | sort -u

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
