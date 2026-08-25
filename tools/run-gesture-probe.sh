#!/usr/bin/env bash

set -u

unset -f grep awk sed 2>/dev/null || true

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BOOT_MS="${BOOT_MS:-1200}"

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  cat <<'USAGE'
run-gesture-probe.sh — rejoue les gestes de l'interface sur le firmware de
production simule, et verifie leur effet.

  BOOT_MS=1200   duree de demarrage avant le premier geste

Phase P2.0 : le harnais charge le binaire de production, attache l'ecran, lit la
banque de patterns en RAM et compte le trafic I2C. Aucun geste n'est encore
injecte.

Le contrat des gestes vit dans docs/gesture-injection.md.

Sortie 0 si le harnais tourne, 1 sinon, 127 si un outil manque.
USAGE
  exit 0
fi

if [ -t 1 ]; then
  C_OK=$'\033[32m'; C_ERR=$'\033[31m'; C_DIM=$'\033[2m'; C_B=$'\033[1m'; C_0=$'\033[0m'; TTY=1
else
  C_OK=""; C_ERR=""; C_DIM=""; C_B=""; C_0=""; TTY=0
fi
progress() { [ "$TTY" = "1" ] && printf '  %s…%s %s\r' "$C_DIM" "$C_0" "$1"; return 0; }
die() { printf '  %s❌%s %s\n' "$C_ERR" "$C_0" "$1" >&2; exit "${2:-1}"; }
ok() { printf '  %s✅%s %-22s %s%s%s\n' "$C_OK" "$C_0" "$1" "$C_DIM" "$2" "$C_0"; }

if command -v pio >/dev/null 2>&1; then PIO="pio"
elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then PIO="$HOME/.platformio/penv/bin/pio"
else die "'pio' introuvable (ni PATH, ni ~/.platformio/penv/bin)." 127
fi

NM=""
for candidate in avr-nm "$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-nm"; do
  command -v "$candidate" >/dev/null 2>&1 && NM="$candidate" && break
  [ -x "$candidate" ] && NM="$candidate" && break
done
[ -n "$NM" ] || die "avr-nm introuvable" 127

PREFIX=""
for p in /opt/homebrew /usr/local; do
  [ -f "$p/lib/libsimavrparts.a" ] && PREFIX="$p" && break
done
[ -n "$PREFIX" ] || die "libsimavrparts absente. brew install simavr" 127

WORK="$(mktemp -d)"
LOG="$WORK/log"
trap 'rm -rf "$WORK"' EXIT

printf '%s=== SONDE DE GESTES (P2.1 a P2.5) ===%s\n' "$C_B" "$C_0"

progress "compilation du harnais"
BIN="$WORK/gesture_probe"
if c++ -O2 -Wall -std=gnu++11 -I"$PREFIX/include/simavr" -I"$PREFIX/include" \
     -I"$ROOT/include" \
     "$ROOT/tools/simavr-ssd1306/gesture_probe.cpp" \
     "$ROOT/src/domain/FactoryPatterns.cpp" "$ROOT/src/domain/Pattern.cpp" \
     "$ROOT/src/domain/PatternBank.cpp" -o "$BIN" \
     -L"$PREFIX/lib" -lsimavrparts -lsimavr -lelf > "$LOG" 2>&1; then
  ok "harnais compile" "$PREFIX"
else
  printf '\n'; cat "$LOG"; die "compilation du harnais en echec"
fi

progress "build env:nanoatmega328"
if "$PIO" run -e nanoatmega328 -d "$ROOT" > "$LOG" 2>&1; then
  ok "firmware de production" "$(grep -E '^RAM:' "$LOG" | sed 's/.*(used /RAM /; s/ bytes from .*/ o/')"
else
  printf '\n'; tail -30 "$LOG"; die "build du firmware en echec"
fi

progress "generateur d'image EEPROM"
GEN="$WORK/eeprom-image"
if c++ -std=gnu++11 -I"$ROOT/include" -o "$GEN" "$ROOT/tools/eeprom-image.cpp" \
     "$ROOT"/src/domain/*.cpp > "$LOG" 2>&1; then
  ok "generateur compile" "tools/eeprom-image.cpp"
else
  printf '\n'; cat "$LOG"; die "compilation du generateur en echec"
fi
if ! "$GEN" --mode seq --steps 0,3,4,9,15 --subdiv -4 --tempo 138 > "$WORK/image.bin" 2>"$LOG"; then
  cat "$LOG"; die "generation de l'image EEPROM en echec"
fi
ok "image EEPROM" "SEQ, steps 0,3,4,9,15, subdiv x4, 138 BPM"

ELF="$ROOT/.pio/build/nanoatmega328/firmware.elf"
BANK_ADDR="$("$NM" "$ELF" | grep -E ' [bB] .*patternBank' | head -1 | awk '{print "0x"$1}')"
[ -n "$BANK_ADDR" ] || die "symbole patternBank introuvable dans l'ELF"
ok "symbole patternBank" "$BANK_ADDR"

progress "demarrage simule ($BOOT_MS ms)"
if ! "$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$BANK_ADDR" "$BOOT_MS" \
     "" 384 structure > "$LOG" 2>&1; then
  cat "$LOG"; die "le harnais s'est termine anormalement"
fi
ok "demarrage" "$BOOT_MS ms simulees"

TWI="$(grep -E '^twi_bytes_boot ' "$LOG" | awk '{print $2}')"
if [ "${TWI:-0}" -gt 0 ] 2>/dev/null; then
  ok "temoin I2C" "$TWI octets pendant le demarrage"
else
  die "aucun trafic I2C au demarrage : le temoin de classe 1 ne fonctionne pas"
fi

printf '\n%s--- LECTURE DE LA BANQUE ---%s\n' "$C_B" "$C_0"
grep -E '^bank_low_masks|^bank_ratchets_a1' "$LOG"

printf '\n%s--- ROTATION ---%s\n' "$C_B" "$C_0"
grep -E '^tab_start|^amorce|^cran_' "$LOG" | sed 's/^/  /'

FAILED=0
bad() { printf '  %s❌%s %-22s %s%s%s\n' "$C_ERR" "$C_0" "$1" "$C_DIM" "$2" "$C_0"; FAILED=1; }

printf '\n'
PRIME_TWI="$(grep -E '^amorce ' "$LOG" | awk '{print $NF}')"
if [ "${PRIME_TWI:-1}" = "0" ]; then
  ok "amorce" "premier cran avale, aucun trafic : anomalie auditee de l encodeur"
else
  ok "amorce" "premier cran deja pris en compte ($PRIME_TWI octets)"
fi

STEPS_OK=1
while read -r NAME FROM TO TWI; do
  DELTA=$(( (TO - FROM + 8) % 8 ))
  [ "$DELTA" = "7" ] && DELTA=-1
  case "$NAME" in
    cran_A_*) WANT=1 ;;
    cran_B_*) WANT=-1 ;;
    *) WANT=0 ;;
  esac
  if [ "$DELTA" != "$WANT" ] || [ "${TWI:-0}" -eq 0 ]; then
    bad "$NAME" "deplacement $DELTA (attendu $WANT), trafic ${TWI:-0}"
    STEPS_OK=0
  fi
done < <(grep -E '^cran_' "$LOG" | sed 's/,//; s/onglet //; s/->//; s/twi //')

[ "$STEPS_OK" = "1" ] && ok "six crans" "chacun un pas, A-d-abord = +1, B-d-abord = -1, trafic a chaque fois"

printf '\n%s--- APPUIS ---%s\n' "$C_B" "$C_0"
grep -E '^signature_tabbar|^appui_' "$LOG" | sed 's/^/  /'
printf '\n'

SIG0="$(grep -E '^signature_tabbar ' "$LOG" | awk '{print $2}')"
SIG5="$(grep -E '^appui_5ms ' "$LOG" | awk '{print $3}' | tr -d ',')"
TWI5="$(grep -E '^appui_5ms ' "$LOG" | awk '{print $5}')"
SIG60="$(grep -E '^appui_60ms ' "$LOG" | awk '{print $3}' | tr -d ',')"
TWI60="$(grep -E '^appui_60ms ' "$LOG" | awk '{print $5}')"
SIG900="$(grep -E '^appui_900ms ' "$LOG" | awk '{print $3}' | tr -d ',')"
TWI900="$(grep -E '^appui_900ms ' "$LOG" | awk '{print $5}')"

if [ "$SIG5" = "$SIG0" ] && [ "${TWI5:-1}" = "0" ]; then
  ok "appui de 5 ms" "aucun effet, aucun trafic : l anti-rebond est respecte"
else
  bad "appui de 5 ms" "signature $SIG5 contre $SIG0, trafic $TWI5 : l anti-rebond n a pas mordu"
fi

if [ "$SIG60" != "$SIG0" ] && [ "${TWI60:-0}" -gt 0 ] 2>/dev/null; then
  ok "appui de 60 ms" "l ecran change, $TWI60 octets : appui court pris en compte"
else
  bad "appui de 60 ms" "signature $SIG60, trafic $TWI60 : appui court non vu"
fi

if [ "$SIG900" = "$SIG0" ] && [ "${TWI900:-0}" -gt 0 ] 2>/dev/null; then
  ok "appui de 900 ms" "retour exact a l ecran de depart, $TWI900 octets : un seul niveau remonte"
else
  bad "appui de 900 ms" "signature $SIG900 contre $SIG0 attendu, trafic $TWI900"
fi

printf '\n%s--- SHIFT + ROTATION ---%s\n' "$C_B" "$C_0"
grep -E '^entree_edit|^ratchet_|^shift_|^masques_' "$LOG" | sed 's/^/  /'
printf '\n'

R_AVANT="$(grep -E '^ratchet_avant ' "$LOG" | awk '{print $2}')"
R_APRES="$(grep -E '^ratchet_apres ' "$LOG" | awk '{print $2}')"
HELD="$(grep -E '^shift_maintien_ms ' "$LOG" | awk '{print $2}')"
SHIFT_TWI="$(grep -E '^shift_twi ' "$LOG" | awk '{print $2}')"
MASQUES="$(grep -E '^masques_intacts ' "$LOG" | awk '{print $2}')"

if [ "$R_AVANT" = "00" ] && [ "$R_APRES" = "04" ]; then
  ok "trois crans sous SHIFT" "ratchet du step 0 : $R_AVANT -> $R_APRES, soit trois codes plus loin"
else
  bad "trois crans sous SHIFT" "ratchet $R_AVANT -> $R_APRES (attendu 00 -> 04)"
fi
if [ "${SHIFT_TWI:-0}" -gt 0 ] 2>/dev/null; then
  ok "temoin I2C" "$SHIFT_TWI octets : le geste a atteint l interface"
else
  bad "temoin I2C" "aucun trafic : geste NON INJECTE, classe 1"
fi
UNDER=$(awk -v h="$HELD" 'BEGIN { print (h < 750) ? 1 : 0 }')
if [ "$UNDER" = "1" ]; then
  ok "maintien de SHIFT" "$HELD ms, sous le seuil de 750 ms"
else
  bad "maintien de SHIFT" "$HELD ms : au-dessus de 750 ms, un appui long a pu partir"
fi
if [ "$MASQUES" = "1" ]; then
  ok "aucun parasite SHIFT" "les steps du pattern sont intacts : aucun effacement"
else
  bad "aucun parasite SHIFT" "le pattern a change : un SHIFT + appui long est parti"
fi

printf '\n%s--- VERIFICATION A ---%s\n' "$C_B" "$C_0"
grep -E '^controle_usine|^triolet_|^banque_|^step1_' "$LOG" | sed 's/^/  /'
printf '\n'

CTRL="$(grep -E '^controle_usine ' "$LOG" | awk '{print $2}')"
TRI_POSE="$(grep -E '^triolet_pose ' "$LOG" | awk '{print $2}')"
TRI_OFF="$(grep -E '^triolet_retire ' "$LOG" | awk '{print $2}')"
REST="$(grep -E '^banque_restauree ' "$LOG" | awk '{print $2}')"
TOG="$(grep -E '^step1_bascule ' "$LOG" | awk '{print $2}')"
UNTOG="$(grep -E '^step1_rebascule ' "$LOG" | awk '{print $2}')"
FINAL="$(grep -E '^banque_finale ' "$LOG" | awk '{print $2}')"

if [ "$CTRL" = "1" ]; then
  ok "controle d usine" "la banque lue en RAM est IDENTIQUE, octet pour octet, a celle que le domaine construit"
else
  bad "controle d usine" "l instrument est faux : aucun verdict sur le firmware (classe 2)"
fi
if [ "$TRI_POSE" = "07" ]; then
  ok "triolet pose" "code 7 sur le step 0"
else
  bad "triolet pose" "code $TRI_POSE au lieu de 07"
fi
if [ "$TRI_OFF" = "00" ] && [ "$REST" = "1" ]; then
  ok "triolet retire" "code 00, et la banque redevient identique a celle d usine"
else
  bad "triolet retire" "code $TRI_OFF, banque restauree=$REST"
fi
if [ "$TOG" = "9113" ]; then
  ok "step bascule" "masque 9111 -> 9113 : le step 1 seul a change"
else
  bad "step bascule" "masque $TOG au lieu de 9113"
fi
if [ "$UNTOG" = "9111" ] && [ "$FINAL" = "1" ]; then
  ok "step rebascule" "masque 9111, et la banque entiere redevient celle d usine"
else
  bad "step rebascule" "masque $UNTOG, banque finale=$FINAL"
fi

LOG2="$WORK/log2"
progress "phase temporelle (P2.5)"
if ! "$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$BANK_ADDR" "$BOOT_MS" \
     "$WORK/image.bin" 384 temporal > "$LOG2" 2>&1; then
  cat "$LOG2"; die "la phase temporelle s'est terminee anormalement"
fi
printf '\n%s--- VERIFICATION B ---%s\n' "$C_B" "$C_0"
grep -E '^p25_' "$LOG2" | sed 's/^/  /'

f2() { grep -E "^$1 " "$LOG2" | head -1 | awk "{print \$$2}"; }
P_AV1="$(f2 p25_avant_OUT1 2)"; G_AV1="$(f2 p25_avant_OUT1 3)"
P_AP1="$(f2 p25_apres_OUT1 2)"
TWI_L="$(f2 p25_geste_length 3)"
SUB_GESTE="$(f2 p25_subdiv_geste 2)"; TWI_S="$(f2 p25_subdiv_geste 3)"
SUB_FRONT="$(f2 p25_subdiv_frontiere 2)"
SUB_APPLY="$(f2 p25_subdiv_applique 2)"
SUB_FIRST32="$(f2 p25_subdiv_premier32 2)"
SUB_AV="$(f2 p25_subdiv_avant 2)"; SUB_AP="$(f2 p25_subdiv_apres 2)"

printf '\n'
if [ "$P_AV1" = "384" ] && [ "$G_AV1" = "24" ]; then
  ok "etat initial mesure" "OUT1 : periode 384 ticks (16 steps), step 24 ticks (x4)"
else
  bad "etat initial mesure" "periode $P_AV1, step $G_AV1"
fi
if [ "${TWI_L:-0}" -gt 0 ] 2>/dev/null && [ "${TWI_S:-0}" -gt 0 ] 2>/dev/null; then
  ok "temoin I2C" "$TWI_L puis $TWI_S octets : les deux gestes ont atteint l interface"
else
  bad "temoin I2C" "un geste sans trafic : classe 1"
fi
if [ "$P_AP1" = "456" ]; then
  ok "LENGTH 16 -> 19" "periode OUT1 : 384 -> 456 ticks, soit 19 steps de 24"
else
  bad "LENGTH 16 -> 19" "periode $P_AP1 au lieu de 456"
fi
CONTAGION=0
for L in 2 3 4 5 6; do
  V="$(f2 p25_apres_OUT$L 2)"
  [ "$V" = "384" ] || CONTAGION=1
done
if [ "$CONTAGION" = "0" ]; then
  ok "non-contagion" "OUT2 a OUT6 gardent leur periode de 384 ticks"
else
  bad "non-contagion" "une autre sortie a change de periode"
fi
if [ "$SUB_AV" = "24" ] && [ "$SUB_AP" = "32" ]; then
  ok "SUBDIV x4 -> x3" "step : 24 -> 32 ticks"
else
  bad "SUBDIV x4 -> x3" "step $SUB_AV -> $SUB_AP"
fi
if [ "${SUB_FIRST32:-0}" -ge "${SUB_FRONT:-1}" ] 2>/dev/null; then
  ok "report ADR 0004" "geste au tick $SUB_GESTE, frontiere $SUB_FRONT, premier step de 32 ticks a $SUB_FIRST32"
else
  bad "report ADR 0004" "un step de 32 ticks des le tick $SUB_FIRST32, avant la frontiere $SUB_FRONT"
fi

BANK_OK="$(grep -E '^bank_inchangee ' "$LOG" | awk '{print $2}')"
if [ "$BANK_OK" = "1" ]; then
  ok "temoin patternBank" "banque inchangee par les rotations seules"
else
  bad "temoin patternBank" "banque modifiee par les seules rotations : INVALID"
fi

printf '\n'
if [ "$FAILED" -eq 0 ]; then
  printf '  %s✅ P2.1 : un cran = un pas. Sens MESURE : A-d-abord = +1, B-d-abord = -1.%s\n' "$C_OK" "$C_0"
  printf '  %s✅ P2.2 : 5 ms sans effet, 60 ms = appui court, 900 ms = appui long, retour exact.%s\n' "$C_OK" "$C_0"
  printf '  %s✅ P2.3 : SHIFT tenu, trois crans, ratchet 00 -> 04, pattern intact, maintien sous 750 ms.%s\n' "$C_OK" "$C_0"
  printf '  %s✅ P2.4 : controle d usine octet pour octet, triolet pose et retire, step bascule et rebascule.%s\n' "$C_OK" "$C_0"
  printf '  %s✅ P2.5 : LENGTH et SUBDIV verifies sur les sorties, report au temps respecte, aucune contagion.%s\n' "$C_OK" "$C_0"
else
  printf '  %s❌ INVALID : aucun verdict sur le firmware.%s\n' "$C_ERR" "$C_0"
  exit 1
fi
