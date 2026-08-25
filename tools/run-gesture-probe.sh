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
  SELFTEST=1     ne lance PAS les gestes : rejoue trois mutations du
                 fractionnement et neuf cas negatifs du 4e temoin, et exige
                 que chacun soit detecte
  SUPPRESSED_SYMBOL=<regex>      vise un autre symbole que suppressedLong
  SUPPRESSED_ADDR_FORCE=<addr>   force l'adresse du compteur
  SUPPRESSED_BIAS=<n>            biaise la lecture d'apres du compteur
  SKIP_SHIFT=1                   n'injecte pas le geste SHIFT (classe 1)
  SKIP_EDIT=1                    n'entre pas dans EDIT (classe 2)
  EXPECT_RATCHET_APRES=<hh>      change l'attente du ratchet (classe 3)

Le harnais charge le binaire de production, attache l'ecran, lit la banque de
patterns en RAM et compte le trafic I2C (P2.0), puis injecte et verifie les
gestes du contrat :

  P2.1  rotation, sens mesure, un cran = un pas
  P2.2  appuis de 5, 60 et 900 ms
  P2.3  SHIFT plus rotation, maintien mesure
  P2.4  controle d'usine octet pour octet, triolet et bascule de step
  P2.5  LENGTH et SUBDIV lus sur les sorties, en ticks
  FRACT fractionnement d'un shiftRotate long en salves sous le seuil de 750 ms

Le contrat des gestes vit dans docs/gesture-injection.md.

Codes de sortie :

  0    tous les criteres verts
  1    au moins un critere rouge, ou une etape du harnais en echec
  3    controle de la banque en echec (classe 2) : aucun verdict sur le firmware
  4    salve de SHIFT refusee AVANT injection : le geste n'etait pas valide
  127  un outil manque
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
OK_COUNT=0
ok() {
  printf '  %s✅%s %-22s %s%s%s\n' "$C_OK" "$C_0" "$1" "$C_DIM" "$2" "$C_0"
  OK_COUNT=$((OK_COUNT + 1))
}
BAD_COUNT=0
INVAL_COUNT=0
bad() {
  printf '  %s❌%s %-22s %s%s%s\n' "$C_ERR" "$C_0" "$1" "$C_DIM" "$2" "$C_0"
  BAD_COUNT=$((BAD_COUNT + 1))
}
inval() {
  printf '  %s⛔%s %-22s %sINVALID — %s%s\n' "$C_ERR" "$C_0" "$1" "$C_DIM" "$2" "$C_0"
  INVAL_COUNT=$((INVAL_COUNT + 1))
}

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

CRAN_EXPECTED=6
TAB_SLOTS=8
SALVES_MIN_A=5
SALVES_MIN_B=2

WORK="$(mktemp -d)"
LOG="$WORK/log"
trap 'rm -rf "$WORK"' EXIT

printf '%s=== SONDE DE GESTES (P2.1 a P2.5, puis fractionnement) ===%s\n' "$C_B" "$C_0"

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

SUPPRESSED_PATTERN="${SUPPRESSED_SYMBOL:-14suppressedLongE$|^suppressedLong$}"
AVR_DATA_BASE=$(( 0x800000 ))

resolve_suppressed() {
  SUPPRESSED_ADDR="0"
  local rows count line nf addr size type
  rows="$("$NM" -S --defined-only "$ELF" | awk -v pat="$SUPPRESSED_PATTERN" '$NF ~ pat { print NF, $0 }')"
  if [ -z "$rows" ]; then count=0; else count="$(printf '%s\n' "$rows" | wc -l | tr -d ' ')"; fi

  if [ "$count" -eq 0 ]; then
    inval "symbole du 4e temoin" "aucun symbole ne correspond a $SUPPRESSED_PATTERN"
    return 1
  fi
  if [ "$count" -gt 1 ]; then
    inval "symbole du 4e temoin" "$count symboles correspondent : la selection serait arbitraire"
    printf '%s\n' "$rows" | sed 's/^/      /'
    return 1
  fi

  line="$rows"
  nf="$(printf '%s' "$line" | awk '{print $1}')"
  if [ "$nf" = "4" ]; then
    addr="$(printf '%s' "$line" | awk '{print $2}')"
    size="$(printf '%s' "$line" | awk '{print $3}')"
    type="$(printf '%s' "$line" | awk '{print $4}')"
  else
    addr="$(printf '%s' "$line" | awk '{print $2}')"
    size=""
    type="$(printf '%s' "$line" | awk '{print $3}')"
  fi

  case "$type" in
    b|B|d|D) ;;
    *) inval "symbole du 4e temoin" "type '$type' : ni .bss ni .data, l adresse n est pas une adresse de donnees"
       return 1 ;;
  esac

  if [ -z "$size" ]; then
    inval "symbole du 4e temoin" "st_size absent de la sortie de avr-nm -S : taille non verifiable"
    return 1
  fi
  if [ "$(( 0x$size ))" -ne 2 ]; then
    inval "symbole du 4e temoin" "taille $(( 0x$size )) octets au lieu de 2"
    return 1
  fi

  if [ "$(( 0x$addr ))" -lt "$AVR_DATA_BASE" ]; then
    inval "symbole du 4e temoin" "VMA 0x$addr sous 0x800000 : ce n est pas l espace de donnees"
    return 1
  fi
  SUPPRESSED_ADDR="$(printf '0x%x' $(( 0x$addr - AVR_DATA_BASE )))"
  ok "symbole du 4e temoin" "unique, type $type, 2 octets, VMA 0x$addr -> RAM $SUPPRESSED_ADDR"
  return 0
}

resolve_suppressed
if [ -n "${SUPPRESSED_ADDR_FORCE:-}" ]; then
  SUPPRESSED_ADDR="$SUPPRESSED_ADDR_FORCE"
  printf '  %s⚠%s  %-22s %sadresse forcee a %s pour exercer un chemin rouge%s\n' \
    "$C_ERR" "$C_0" "symbole du 4e temoin" "$C_DIM" "$SUPPRESSED_ADDR" "$C_0"
fi

if [ -n "${SELFTEST:-}" ]; then
  SELFTEST_BAD_AT_ENTRY="$BAD_COUNT"
  SELFTEST_INVAL_AT_ENTRY="$INVAL_COUNT"
  printf '\n%s--- CONTRE-EPREUVE DU FRACTIONNEMENT ---%s\n' "$C_B" "$C_0"
  SRC="$ROOT/tools/simavr-ssd1306/gesture_probe.cpp"
  SELF_FAILED=0
  selfbad() { printf '  %s❌%s %-22s %s%s%s\n' "$C_ERR" "$C_0" "$1" "$C_DIM" "$2" "$C_0"; SELF_FAILED=1; }
  build_mutant() {
    c++ -O2 -w -std=gnu++11 -I"$PREFIX/include/simavr" -I"$PREFIX/include" \
      -I"$ROOT/tools/simavr-ssd1306" -I"$ROOT/include" "$1" \
      "$ROOT/src/domain/FactoryPatterns.cpp" "$ROOT/src/domain/Pattern.cpp" \
      "$ROOT/src/domain/PatternBank.cpp" -o "$2" \
      -L"$PREFIX/lib" -lsimavrparts -lsimavr -lelf
  }

  progress "mutant 1 : aucun fractionnement"
  sed 's/const int burst = left < SHIFT_BURST_DETENTS ? left : SHIFT_BURST_DETENTS;/const int burst = left;/' \
    "$SRC" > "$WORK/m1.cpp"
  cmp -s "$SRC" "$WORK/m1.cpp" && die "mutant 1 : motif absent du code source" 2
  if build_mutant "$WORK/m1.cpp" "$WORK/m1" > "$LOG" 2>&1; then
    "$WORK/m1" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$BANK_ADDR" "$BOOT_MS" \
      "" 384 structure "$SUPPRESSED_ADDR" > "$WORK/m1.log" 2>&1
    RC=$?
    if [ "$RC" = "4" ]; then
      ok "mutant 1 detecte" "sans fractionnement, la salve est refusee AVANT injection (sortie 4)"
    else
      selfbad "mutant 1 non detecte" "sortie $RC au lieu de 4"
    fi
  else
    cat "$LOG"; selfbad "mutant 1 non detecte" "le mutant ne compile pas"
  fi

  progress "mutant 2 : plafond force a 20 crans"
  sed 's|#define SHIFT_BURST_DETENTS   ((SHIFT_HOLD_CEILING_MS - 1 - SHIFT_HOLD_FIXED_MS) / DETENT_MS)|#define SHIFT_BURST_DETENTS   20|' \
    "$SRC" > "$WORK/m2.cpp"
  cmp -s "$SRC" "$WORK/m2.cpp" && die "mutant 2 : motif absent du code source" 2
  if build_mutant "$WORK/m2.cpp" "$WORK/m2" > "$LOG" 2>&1; then
    selfbad "mutant 2 non detecte" "un plafond de 20 crans compile encore"
  else
    ok "mutant 2 detecte" "$(grep -c 'static assertion' "$LOG") assertions statiques refusent 20 crans"
  fi

  progress "mutant 3 : ni garde ni fractionnement"
  python3 - "$SRC" "$WORK/m3.cpp" <<'MUTANT3'
import sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src).read()
a = "        const int burst = left < SHIFT_BURST_DETENTS ? left : SHIFT_BURST_DETENTS;"
b = """    if (detents < 1 || detents > SHIFT_BURST_DETENTS
        || SHIFT_HOLD_MS(detents) >= SHIFT_HOLD_CEILING_MS) {"""
if a not in s or b not in s:
    sys.exit(2)
open(dst, "w").write(s.replace(a, "        const int burst = left;", 1).replace(b, "    if (0) {", 1))
MUTANT3
  [ $? = 0 ] || die "mutant 3 : motif absent du code source" 2
  if build_mutant "$WORK/m3.cpp" "$WORK/m3" > "$LOG" 2>&1; then
    "$WORK/m3" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$BANK_ADDR" "$BOOT_MS" \
      "" 384 structure "$SUPPRESSED_ADDR" > "$WORK/m3.log" 2>&1
    M3_HOLD="$(grep -E '^shift_salve ' "$WORK/m3.log" | awk '{print $5}' | sort -g | tail -1)"
    M3_SUPP="$(grep -E '^fract_suppressions ' "$WORK/m3.log" | awk '{print $2}')"
    M3_MASK="$(grep -E '^fract_masques ' "$WORK/m3.log" | awk '{print $2}')"
    M3_OVER=$(awk -v h="${M3_HOLD:-0}" 'BEGIN { print (h >= 750) ? 1 : 0 }')
    if [ "$M3_OVER" = "1" ] && [ "${M3_SUPP:-0}" -gt 0 ] 2>/dev/null; then
      ok "mutant 3 detecte" "maintien $M3_HOLD ms, $M3_SUPP appuis longs partis et absorbes (masques intacts=$M3_MASK)"
    else
      selfbad "mutant 3 non detecte" "maintien $M3_HOLD ms, suppressions $M3_SUPP"
    fi
  else
    cat "$LOG"; selfbad "mutant 3 non detecte" "le mutant ne compile pas"
  fi

  printf '\n%s--- CONTRE-EPREUVE DU 4e TEMOIN ---%s\n' "$C_B" "$C_0"

  expect_resolver_invalid() {
    local label="$1" pattern="$2"
    local keep_bad="$BAD_COUNT" keep_inval="$INVAL_COUNT"
    BAD_COUNT=0; INVAL_COUNT=0
    SUPPRESSED_PATTERN="$pattern"
    resolve_suppressed > "$WORK/resolve.log" 2>&1
    local rc=$?
    local got="$INVAL_COUNT" got_bad="$BAD_COUNT"
    BAD_COUNT="$keep_bad"; INVAL_COUNT="$keep_inval"
    if [ "$rc" != "0" ] && [ "$got" -ge 1 ] 2>/dev/null && [ "$got_bad" = "0" ]; then
      ok "$label" "$(sed -n '1s/^ *//p' "$WORK/resolve.log" | sed 's/.*INVALID — //')"
    else
      selfbad "$label" "accepte au lieu d etre INVALID (rc=$rc, invalid=$got, defauts=$got_bad)"
    fi
  }

  expect_addr_invalid() {
    local label="$1" addr="$2"
    "$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$BANK_ADDR" 1 \
      "" 384 symbolcheck "$addr" > "$WORK/addr.log" 2>&1
    local valide
    valide="$(grep -E '^suppressed_addr ' "$WORK/addr.log" | awk '{print $NF}')"
    if [ "${valide:-1}" = "0" ]; then
      ok "$label" "$(grep -E '^suppressed_addr ' "$WORK/addr.log" | sed 's/^suppressed_addr *//')"
    else
      selfbad "$label" "adresse $addr acceptee (valide=$valide)"
    fi
  }

  expect_run_invalid() {
    local label="$1" line="$2"
    shift 2
    if env -u SELFTEST "$@" "$0" > "$WORK/run.log" 2>&1; then
      selfbad "$label" "la sonde rend 0 alors que le temoin est invalide"
      return
    fi
    if grep -q "⛔" "$WORK/run.log" && grep -q "$line" "$WORK/run.log"; then
      ok "$label" "$(grep -m1 "$line" "$WORK/run.log" | sed 's/^ *//; s/.*INVALID — //')"
    else
      selfbad "$label" "rouge sans classement INVALID attendu ($line)"
    fi
  }

  progress "cas negatif : zero symbole"
  expect_resolver_invalid "zero symbole" 'aucunSymboleDeCeNom$'
  progress "cas negatif : plusieurs symboles"
  expect_resolver_invalid "plusieurs symboles" '14suppressedLongE$|10controllerE$'
  progress "cas negatif : taille differente de 2"
  expect_resolver_invalid "taille != 2" '21rotatedWhileShiftHeldE$'
  progress "cas negatif : mauvais type"
  expect_resolver_invalid "type hors donnees" '16onShiftLongPressEv$'
  SUPPRESSED_PATTERN="${SUPPRESSED_SYMBOL:-14suppressedLongE$|^suppressedLong$}"

  progress "cas negatif : adresse dans l espace d E/S"
  expect_addr_invalid "adresse sous ioend" 0x0050
  progress "cas negatif : objet a cheval sur ramend"
  expect_addr_invalid "objet hors SRAM" 0x08ff
  progress "cas negatif : lecture impossible"
  expect_addr_invalid "adresse nulle" 0

  progress "cas negatif : adresse fausse, chaine complete"
  expect_run_invalid "adresse fausse (bout en bout)" "temoin 2 : compteur" SUPPRESSED_ADDR_FORCE=0x0050
  progress "cas negatif : delta negatif, chaine complete"
  expect_run_invalid "delta negatif (bout en bout)" "negatif" SUPPRESSED_BIAS=-1

  printf '\n%s--- CONTRE-EPREUVE DU VERDICT GLOBAL ---%s\n' "$C_B" "$C_0"

  run_variant() {
    env -u SELFTEST "$@" "$0" > "$WORK/class.log" 2>&1
    CLASS_RC=$?
    CLASS_VERDICT="$(grep -oE 'VERDICT : (PASS|FAIL|INVALID)' "$WORK/class.log" | head -1 | awk '{print $3}')"
    CLASS_VERDICT="${CLASS_VERDICT:-AUCUN}"
    CLASS_INVAL="$(grep -c '⛔' "$WORK/class.log" | tr -d ' ')"
    CLASS_BAD="$(grep -c '❌' "$WORK/class.log" | tr -d ' ')"
    grep -q '⛔ VERDICT' "$WORK/class.log" && CLASS_INVAL=$((CLASS_INVAL - 1))
    grep -q '❌ VERDICT' "$WORK/class.log" && CLASS_BAD=$((CLASS_BAD - 1))
  }

  expect_verdict() {
    local label="$1" want_verdict="$2" want_rc="$3" want_bad="$4" want_inval="$5"
    shift 5
    run_variant "$@"
    local why=""
    [ "$CLASS_VERDICT" = "$want_verdict" ] || why="$why verdict=$CLASS_VERDICT(attendu $want_verdict)"
    [ "$CLASS_RC" = "$want_rc" ] || why="$why code=$CLASS_RC(attendu $want_rc)"
    case "$want_bad" in
      "+") [ "$CLASS_BAD" -ge 1 ] 2>/dev/null || why="$why defauts=$CLASS_BAD(attendu au moins 1)" ;;
      *)   [ "$CLASS_BAD" = "$want_bad" ] || why="$why defauts=$CLASS_BAD(attendu $want_bad)" ;;
    esac
    case "$want_inval" in
      "+") [ "$CLASS_INVAL" -ge 1 ] 2>/dev/null || why="$why invalid=$CLASS_INVAL(attendu au moins 1)" ;;
      "*") ;;
      *)   [ "$CLASS_INVAL" = "$want_inval" ] || why="$why invalid=$CLASS_INVAL(attendu $want_inval)" ;;
    esac
    if [ -z "$why" ]; then
      ok "$label" "verdict $CLASS_VERDICT, code $CLASS_RC, $CLASS_BAD defaut(s), $CLASS_INVAL non decidable(s)"
    else
      selfbad "$label" "$why"
    fi
  }

  progress "a : tout sain -> PASS"
  expect_verdict "a. tout sain -> PASS" PASS 0 0 0

  progress "b : classe 3 -> FAIL"
  expect_verdict "b. classe 3 -> FAIL" FAIL 1 1 "*" EXPECT_RATCHET_APRES=05
  WITNESS_INVAL="$(grep '⛔' "$WORK/class.log" \
    | grep -cE 'temoin I2C|maintien de SHIFT|entree dans EDIT|temoin patternBank|controle d usine|echantillons' | tr -d ' ')"
  if [ "$WITNESS_INVAL" = "0" ] && grep -q '❌ trois crans sous SHIFT' "$WORK/class.log" \
     && ! grep -q '⛔ trois crans sous SHIFT' "$WORK/class.log"; then
    ok "b. le defaut est a la racine" "aucun temoin necessaire INVALID : le FAIL est decidable"
  else
    selfbad "b. le defaut est a la racine" "temoins necessaires INVALID=$WITNESS_INVAL"
  fi

  progress "c : classe 2 -> INVALID"
  expect_verdict "c. classe 2 -> INVALID" INVALID 5 0 "+" SKIP_EDIT=1
  grep -q '⛔ entree dans EDIT' "$WORK/class.log" \
    && ok "c. la precondition est nommee" "entree dans EDIT non etablie" \
    || selfbad "c. la precondition est nommee" "le critere d entree dans EDIT n est pas INVALID"

  progress "d : classe 1 avec effet aval faux -> INVALID"
  expect_verdict "d. classe 1 -> INVALID" INVALID 5 0 "+" SKIP_SHIFT=1
  if grep -q '⛔ triolet pose' "$WORK/class.log" && ! grep -q '❌ triolet pose' "$WORK/class.log"; then
    ok "d. l aval ne devient pas FAIL" "le triolet lit 03 au lieu de 07, et reste non decidable"
  else
    selfbad "d. l aval ne devient pas FAIL" "le defaut aval a ete impute au firmware"
  fi

  if [ "$BAD_COUNT" = "$SELFTEST_BAD_AT_ENTRY" ] && [ "$INVAL_COUNT" = "$SELFTEST_INVAL_AT_ENTRY" ]; then
    ok "e. SELFTEST etanche" "BAD_COUNT et INVAL_COUNT inchanges ($BAD_COUNT / $INVAL_COUNT) : SELF_FAILED ne les alimente pas"
  else
    selfbad "e. SELFTEST etanche" "les compteurs globaux ont bouge : $SELFTEST_BAD_AT_ENTRY/$SELFTEST_INVAL_AT_ENTRY -> $BAD_COUNT/$INVAL_COUNT"
  fi

  printf '\n'
  if [ "$SELF_FAILED" = "0" ]; then
    printf '  %s✅ Les trois mutants sont detectes : le fractionnement est un critere, pas un commentaire.%s\n' "$C_OK" "$C_0"
    printf '  %s✅ Les neuf cas negatifs du 4e temoin rendent INVALID, jamais un defaut du firmware.%s\n' "$C_OK" "$C_0"
    printf '  %s✅ Les quatre chemins du verdict global : PASS/0, FAIL/1, INVALID/5 deux fois, tous verifies sur le code ET le mot.%s\n' "$C_OK" "$C_0"
    exit 0
  fi
  printf '  %s❌ Une mutation du fractionnement passe inapercue.%s\n' "$C_ERR" "$C_0"
  exit 1
fi

progress "demarrage simule ($BOOT_MS ms)"
if ! "$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$BANK_ADDR" "$BOOT_MS" \
     "" 384 structure "$SUPPRESSED_ADDR" > "$LOG" 2>&1; then
  RC=$?
  cat "$LOG"
  case "$RC" in
    3) die "INVALID (classe 2) : controle de la banque en echec, aucun verdict sur le firmware" 3 ;;
    4) die "INVALID (classe 2) : salve de SHIFT refusee avant injection, le geste n etait pas valide" 4 ;;
    *) die "le harnais s'est termine anormalement (code $RC)" ;;
  esac
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

printf '\n'
PRIME_TWI="$(grep -E '^amorce ' "$LOG" | awk '{print $NF}')"
if [ "${PRIME_TWI:-1}" = "0" ]; then
  ok "amorce" "premier cran avale, aucun trafic : anomalie auditee de l encodeur"
else
  ok "amorce" "premier cran deja pris en compte ($PRIME_TWI octets)"
fi

CRAN_LINES=0
CRAN_TWI_OK=1
CRAN_MOVE_OK=1
CRAN_DETAIL=""
while read -r NAME FROM TO TWI; do
  [ -n "$NAME" ] || continue
  CRAN_LINES=$((CRAN_LINES + 1))
  DELTA=$(( (TO - FROM + 8) % 8 ))
  [ "$DELTA" = "7" ] && DELTA=-1
  case "$NAME" in
    cran_A_*) WANT=1 ;;
    cran_B_*) WANT=-1 ;;
    *) WANT=0 ;;
  esac
  if [ "${TWI:-0}" -eq 0 ] 2>/dev/null; then
    CRAN_TWI_OK=0
    CRAN_DETAIL="$CRAN_DETAIL $NAME:sans-trafic"
  elif [ "$DELTA" != "$WANT" ]; then
    CRAN_MOVE_OK=0
    CRAN_DETAIL="$CRAN_DETAIL $NAME:$DELTA(attendu $WANT)"
  fi
done < <(grep -E '^cran_' "$LOG" | sed 's/,//; s/onglet //; s/->//; s/twi //')

if [ "$CRAN_LINES" -ne "$CRAN_EXPECTED" ]; then
  inval "six crans : echantillons" "$CRAN_LINES crans mesures au lieu de $CRAN_EXPECTED : rien a conclure"
  CRAN_TWI_OK=0
else
  ok "six crans : echantillons" "$CRAN_LINES crans mesures, le compte attendu du harnais"
fi
if [ "$CRAN_LINES" -eq "$CRAN_EXPECTED" ]; then
  if [ "$CRAN_TWI_OK" = "1" ]; then
    ok "six crans : temoin I2C" "chaque cran a produit du trafic : les six gestes ont atteint l interface"
  else
    inval "six crans : temoin I2C" "au moins un cran sans trafic —$CRAN_DETAIL"
  fi
  if [ "$CRAN_TWI_OK" != "1" ]; then
    inval "six crans : deplacement" "temoin de trafic invalide : le deplacement n est pas attribuable"
  elif [ "$CRAN_MOVE_OK" = "1" ]; then
    ok "six crans : deplacement" "chacun un pas, A-d-abord = +1, B-d-abord = -1"
  else
    bad "six crans : deplacement" "deplacement inattendu —$CRAN_DETAIL"
  fi
fi

printf '\n%s--- PRECONDITIONS DE L INSTRUMENT ---%s\n' "$C_B" "$C_0"
grep -E '^bank_inchangee|^controle_usine|^entree_edit' "$LOG" | sed 's/^/  /'
printf '\n'

BANK_OK="$(grep -E '^bank_inchangee ' "$LOG" | awk '{print $2}')"
if [ "$BANK_OK" = "1" ]; then
  ok "temoin patternBank" "banque inchangee par les rotations seules"
  W_BANK=1
else
  inval "temoin patternBank" "banque modifiee par les seules rotations : l instrument n est pas sain"
  W_BANK=0
fi

CTRL="$(grep -E '^controle_usine ' "$LOG" | awk '{print $2}')"
if [ "$CTRL" = "1" ]; then
  ok "controle d usine" "la banque lue en RAM est IDENTIQUE, octet pour octet, a celle que le domaine construit"
  W_FACTORY=1
else
  inval "controle d usine" "classe 2 : l instrument est faux, aucun verdict sur le firmware"
  W_FACTORY=0
fi

EDIT_TWI="$(grep -E '^entree_edit ' "$LOG" | awk '{print $5}')"
EDIT_SLOTS_BEFORE="$(grep -E '^entree_edit ' "$LOG" | awk '{print $7}')"
EDIT_SLOTS_AFTER="$(grep -E '^entree_edit ' "$LOG" | awk '{print $8}')"
EDIT_DISTINCT="$(grep -E '^entree_edit ' "$LOG" | awk '{print $10}')"
W_EDIT=0
if [ -z "$EDIT_TWI" ]; then
  inval "entree dans EDIT" "aucune mesure de navigation publiee"
elif [ "${EDIT_TWI:-0}" -eq 0 ] 2>/dev/null; then
  inval "entree dans EDIT" "aucun trafic : le geste de navigation n a pas ete injecte"
elif [ "${EDIT_SLOTS_BEFORE:-0}" != "$TAB_SLOTS" ]; then
  inval "entree dans EDIT" "avant le geste, ${EDIT_SLOTS_BEFORE:-?} creneaux encres au lieu de $TAB_SLOTS : l ecran de depart n est pas la barre d onglets"
elif [ "${EDIT_SLOTS_AFTER:-$TAB_SLOTS}" -ge "$TAB_SLOTS" ] 2>/dev/null; then
  inval "entree dans EDIT" "la barre d onglets est toujours dessinee (${EDIT_SLOTS_AFTER} creneaux) : EDIT n est pas etabli"
elif [ "$EDIT_DISTINCT" != "1" ]; then
  inval "entree dans EDIT" "l ecran n est distinct ni de la barre d onglets ni du niveau tab"
else
  ok "entree dans EDIT" "$EDIT_TWI octets, creneaux encres $EDIT_SLOTS_BEFORE -> $EDIT_SLOTS_AFTER, ecran distinct"
  W_EDIT=1
fi

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

if [ -z "$SIG0" ] || [ -z "$SIG5" ] || [ -z "$SIG60" ] || [ -z "$SIG900" ]; then
  inval "appuis : echantillons" "une signature d ecran manque : rien a comparer"
  W_PRESS=0
else
  ok "appuis : echantillons" "quatre signatures d ecran lues, dont la reference $SIG0"
  W_PRESS=1
fi

if [ "$W_PRESS" != "1" ]; then
  inval "appui de 5 ms" "signatures manquantes"
elif [ "$SIG5" != "$SIG0" ] && [ "${TWI5:-0}" -eq 0 ] 2>/dev/null; then
  inval "appui de 5 ms" "l ecran change sans un seul octet sur le bus : lecture contradictoire"
elif [ "${TWI5:-0}" -gt 0 ] 2>/dev/null; then
  bad "appui de 5 ms" "$TWI5 octets : le firmware a reagi a un appui plus court que l anti-rebond"
elif [ "$SIG5" != "$SIG0" ]; then
  bad "appui de 5 ms" "signature $SIG5 contre $SIG0 : l ecran a change sans appui pris en compte"
else
  ok "appui de 5 ms" "aucun effet, aucun trafic : l anti-rebond est respecte"
fi

if [ "$W_PRESS" != "1" ]; then
  inval "appui de 60 ms" "signatures manquantes"
elif [ "${TWI60:-0}" -eq 0 ] 2>/dev/null; then
  inval "appui de 60 ms" "aucun trafic : geste NON INJECTE"
elif [ "$SIG60" = "$SIG0" ]; then
  bad "appui de 60 ms" "$TWI60 octets mais l ecran est inchange : appui court non pris en compte"
else
  ok "appui de 60 ms" "l ecran change, $TWI60 octets : appui court pris en compte"
fi

if [ "$W_PRESS" != "1" ]; then
  inval "appui de 900 ms" "signatures manquantes"
elif [ "${TWI900:-0}" -eq 0 ] 2>/dev/null; then
  inval "appui de 900 ms" "aucun trafic : geste NON INJECTE"
elif [ "$SIG900" != "$SIG0" ]; then
  bad "appui de 900 ms" "signature $SIG900 contre $SIG0 attendu : le retour n est pas exact"
else
  ok "appui de 900 ms" "retour exact a l ecran de depart, $TWI900 octets : un seul niveau remonte"
fi

printf '\n%s--- SHIFT + ROTATION ---%s\n' "$C_B" "$C_0"
grep -E '^ratchet_|^shift_maintien_ms|^shift_twi|^masques_' "$LOG" | sed 's/^/  /'
printf '\n'

R_AVANT="$(grep -E '^ratchet_avant ' "$LOG" | awk '{print $2}')"
R_APRES="$(grep -E '^ratchet_apres ' "$LOG" | awk '{print $2}')"
HELD="$(grep -E '^shift_maintien_ms ' "$LOG" | awk '{print $2}')"
SHIFT_TWI="$(grep -E '^shift_twi ' "$LOG" | awk '{print $2}')"
MASQUES="$(grep -E '^masques_intacts ' "$LOG" | awk '{print $2}')"

if [ "${SHIFT_TWI:-0}" -gt 0 ] 2>/dev/null; then
  ok "temoin I2C" "$SHIFT_TWI octets : le geste a atteint l interface"
  W_SHIFT_TWI=1
else
  inval "temoin I2C" "aucun trafic : geste NON INJECTE"
  W_SHIFT_TWI=0
fi
UNDER=$(awk -v h="${HELD:-9999}" 'BEGIN { print (h < 750) ? 1 : 0 }')
if [ -n "$HELD" ] && [ "$UNDER" = "1" ]; then
  ok "maintien de SHIFT" "$HELD ms, sous le seuil de 750 ms"
  W_SHIFT_HOLD=1
else
  inval "maintien de SHIFT" "${HELD:-aucune mesure} ms : le geste injecte n etait pas valide"
  W_SHIFT_HOLD=0
fi

W_P23=$(( W_SHIFT_TWI * W_SHIFT_HOLD * W_BANK * W_FACTORY * W_EDIT ))
P23_OK=0
if [ "$W_P23" != "1" ]; then
  inval "trois crans sous SHIFT" "temoins amont invalides : l effet n est pas attribuable au firmware"
  inval "aucun parasite SHIFT" "temoins amont invalides : l effet n est pas attribuable au firmware"
else
  if [ "$R_AVANT" = "00" ] && [ "$R_APRES" = "${EXPECT_RATCHET_APRES:-04}" ]; then
    ok "trois crans sous SHIFT" "ratchet du step 0 : $R_AVANT -> $R_APRES, soit trois codes plus loin"
    P23_OK=1
  else
    bad "trois crans sous SHIFT" "ratchet $R_AVANT -> $R_APRES (attendu 00 -> ${EXPECT_RATCHET_APRES:-04})"
  fi
  if [ "$MASQUES" = "1" ]; then
    ok "aucun parasite SHIFT" "les steps du pattern sont intacts : aucun effacement"
  else
    bad "aucun parasite SHIFT" "le pattern a change : un SHIFT + appui long est parti"
  fi
fi

printf '\n%s--- VERIFICATION A ---%s\n' "$C_B" "$C_0"
grep -E '^triolet_|^banque_|^step1_' "$LOG" | sed 's/^/  /'
printf '\n'

TRI_POSE="$(grep -E '^triolet_pose ' "$LOG" | awk '{print $2}')"
TRI_POSE_TWI="$(grep -E '^triolet_pose ' "$LOG" | awk '{print $4}')"
TRI_OFF="$(grep -E '^triolet_retire ' "$LOG" | awk '{print $2}')"
TRI_OFF_TWI="$(grep -E '^triolet_retire ' "$LOG" | awk '{print $4}')"
REST="$(grep -E '^banque_restauree ' "$LOG" | awk '{print $2}')"
TOG="$(grep -E '^step1_bascule ' "$LOG" | awk '{print $2}')"
TOG_TWI="$(grep -E '^step1_bascule ' "$LOG" | awk '{print $4}')"
UNTOG="$(grep -E '^step1_rebascule ' "$LOG" | awk '{print $2}')"
UNTOG_TWI="$(grep -E '^step1_rebascule ' "$LOG" | awk '{print $4}')"
FINAL="$(grep -E '^banque_finale ' "$LOG" | awk '{print $2}')"

A_TWI_OK=1
A_TWI_DETAIL=""
for pair in "triolet_pose:$TRI_POSE_TWI" "triolet_retire:$TRI_OFF_TWI" \
            "step1_bascule:$TOG_TWI" "step1_rebascule:$UNTOG_TWI"; do
  n="${pair##*:}"
  if [ -z "$n" ] || [ "$n" -eq 0 ] 2>/dev/null; then
    A_TWI_OK=0
    A_TWI_DETAIL="$A_TWI_DETAIL ${pair%%:*}:${n:-absent}"
  fi
done
if [ "$A_TWI_OK" = "1" ]; then
  ok "temoin I2C" "$TRI_POSE_TWI, $TRI_OFF_TWI, $TOG_TWI, $UNTOG_TWI octets : les quatre gestes ont atteint l interface"
else
  inval "temoin I2C" "au moins un geste sans trafic —$A_TWI_DETAIL"
fi

W_A=$(( A_TWI_OK * W_BANK * W_FACTORY * W_EDIT * P23_OK ))
if [ "$W_A" != "1" ]; then
  inval "triolet pose" "temoins amont invalides (dont l etat laisse par P2.3) : l effet n est pas attribuable au firmware"
  inval "triolet retire" "temoins amont invalides : l effet n est pas attribuable au firmware"
  inval "step bascule" "temoins amont invalides : l effet n est pas attribuable au firmware"
  inval "step rebascule" "temoins amont invalides : l effet n est pas attribuable au firmware"
else
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
fi

printf '\n%s--- FRACTIONNEMENT DES SALVES SHIFT ---%s\n' "$C_B" "$C_0"
grep -E '^suppressed_addr |^fract_|^shift_salve |^shift_plafond |^shift_salves |^shift_maintien_max ' "$LOG" | sed 's/^/  /'
printf '\n'

CEIL_DETENTS="$(grep -E '^shift_plafond ' "$LOG" | awk '{print $2}')"
CEIL_MS="$(grep -E '^shift_plafond ' "$LOG" | awk '{print $3}')"
CEIL_SEUIL="$(grep -E '^shift_plafond ' "$LOG" | awk '{print $4}')"
ASKED="$(grep -E '^fract_demande ' "$LOG" | awk '{print $2}')"
BURSTS="$(grep -E '^fract_salves ' "$LOG" | awk '{print $2}')"
F_MASK="$(grep -E '^fract_masques ' "$LOG" | awk '{print $2}')"
F_BACK="$(grep -E '^fract_retour ' "$LOG" | awk '{print $2}')"
F_SUPP="$(grep -E '^fract_suppressions ' "$LOG" | awk '{print $2}')"
F_READ="$(grep -E '^fract_compteur ' "$LOG" | awk '{print $3}')"
F_BEFORE="$(grep -E '^fract_compteur ' "$LOG" | awk '{print $5}')"
F_AFTER="$(grep -E '^fract_compteur ' "$LOG" | awk '{print $7}')"
F_ENCSW="$(grep -E '^fract_enc_sw ' "$LOG" | awk '{print $2}')"
F_ADDR_OK="$(grep -E '^suppressed_addr ' "$LOG" | awk '{print $NF}')"
F_TWI="$(grep -E '^fract_twi ' "$LOG" | awk '{print $2}')"

OVER=0
MAXHOLD=0
NSALVES=0
OVERSIZED=0
while read -r CRANS HOLD; do
  [ -n "$CRANS" ] || continue
  NSALVES=$((NSALVES + 1))
  [ "${CRANS:-0}" -gt "${CEIL_DETENTS:-0}" ] 2>/dev/null && OVERSIZED=1
  OVER=$(awk -v h="$HOLD" -v o="$OVER" 'BEGIN { print (h >= 750) ? 1 : o }')
  MAXHOLD=$(awk -v h="$HOLD" -v m="$MAXHOLD" 'BEGIN { print (h > m) ? h : m }')
done <<EOF
$(grep -E '^shift_salve ' "$LOG" | awk '{print $2, $5}')
EOF

if [ "${NSALVES:-0}" -lt "$SALVES_MIN_A" ]; then
  inval "chaque salve mesuree" "$NSALVES salves mesurees, minimum $SALVES_MIN_A : rien a conclure"
  W_SALVES=0
elif [ "$OVER" = "0" ] && [ "$OVERSIZED" = "0" ]; then
  ok "chaque salve mesuree" "$NSALVES salves, maintien maximal $MAXHOLD ms, plafond $CEIL_DETENTS crans / $CEIL_MS ms, seuil $CEIL_SEUIL ms"
  W_SALVES=1
else
  inval "chaque salve mesuree" "maintien maximal $MAXHOLD ms, salve hors plafond=$OVERSIZED : injection invalide"
  W_SALVES=0
fi

EXPECTED_BURSTS=$(awk -v a="${ASKED:-0}" -v c="${CEIL_DETENTS:-1}" 'BEGIN { print int((a + c - 1) / c) }')
if [ "${BURSTS:-0}" = "$EXPECTED_BURSTS" ] && [ "${BURSTS:-0}" -gt 1 ] 2>/dev/null; then
  ok "fractionnement" "$ASKED crans decoupes en $BURSTS salves de $CEIL_DETENTS crans au plus"
  W_SPLIT=1
else
  inval "fractionnement" "$ASKED crans en ${BURSTS:-0} salves (attendu $EXPECTED_BURSTS) : le pilote a mal decoupe"
  W_SPLIT=0
fi
if [ "${F_TWI:-0}" -gt 0 ] 2>/dev/null; then
  ok "temoin I2C" "$F_TWI octets : les salves ont atteint l interface"
  W_FRACT_TWI=1
else
  inval "temoin I2C" "aucun trafic : geste NON INJECTE"
  W_FRACT_TWI=0
fi

if [ "${F_ENCSW:-x}" = "0" ]; then
  ok "fenetre non ambigue" "l interrupteur de l encodeur n a pas bouge : seul onShiftLongPress peut ecrire"
  W_ENCSW=1
else
  inval "fenetre non ambigue" "${F_ENCSW:-?} appuis sur l encodeur dans la fenetre : le compteur a deux ecrivains possibles"
  W_ENCSW=0
fi

if [ "${F_ADDR_OK:-0}" != "1" ] || [ "${F_READ:-0}" != "1" ]; then
  inval "temoin 2 : compteur" "compteur illisible (adresse valide=${F_ADDR_OK:-?}, lecture=${F_READ:-?}) : critere non evaluable"
elif [ "${F_SUPP:-x}" = "x" ]; then
  inval "temoin 2 : compteur" "aucun delta publie alors que la lecture est annoncee possible"
elif [ "$F_SUPP" -lt 0 ] 2>/dev/null; then
  inval "temoin 2 : compteur" "delta $F_SUPP negatif ($F_BEFORE -> $F_AFTER) : un compteur monotone ne decroit pas, l instrument est faux"
elif [ "$F_SUPP" = "0" ]; then
  ok "temoin 2 : compteur" "suppressedLong inchange a $F_BEFORE : aucun appui long n a ete ABSORBE"
else
  inval "temoin 2 : compteur" "$F_SUPP appuis longs absorbes ($F_BEFORE -> $F_AFTER) : le maintien injecte a franchi le seuil"
fi

W_FRACT=$(( W_FRACT_TWI * W_ENCSW * W_SALVES * W_SPLIT * W_BANK * W_FACTORY * W_EDIT ))
if [ "$W_FRACT" != "1" ]; then
  inval "temoin 1 : pattern" "temoins amont invalides : l effet n est pas attribuable au firmware"
  inval "retour a l usine" "temoins amont invalides : l effet n est pas attribuable au firmware"
else
  if [ "$F_MASK" = "1" ]; then
    ok "temoin 1 : pattern" "les steps sont intacts apres $BURSTS relachements : aucun appui long n est PASSE"
  else
    bad "temoin 1 : pattern" "le pattern a change : un SHIFT + appui long est parti pendant le fractionnement"
  fi
  if [ "$F_BACK" = "1" ]; then
    ok "retour a l usine" "les $ASKED crans inverses ramenent la banque entiere a celle d usine"
  else
    bad "retour a l usine" "la banque ne revient pas a celle d usine"
  fi
fi

LOG2="$WORK/log2"
progress "phase temporelle (P2.5)"
if ! "$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$BANK_ADDR" "$BOOT_MS" \
     "$WORK/image.bin" 384 temporal "$SUPPRESSED_ADDR" > "$LOG2" 2>&1; then
  RC=$?
  cat "$LOG2"
  case "$RC" in
    3) die "INVALID (classe 2) : controle de la banque en echec, aucun verdict sur le firmware" 3 ;;
    4) die "INVALID (classe 2) : salve de SHIFT refusee avant injection, le geste n etait pas valide" 4 ;;
    *) die "la phase temporelle s'est terminee anormalement (code $RC)" ;;
  esac
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
OVER2=0
MAX2=0
NSALVES2=0
while read -r CRANS HOLD; do
  [ -n "$CRANS" ] || continue
  NSALVES2=$((NSALVES2 + 1))
  [ "${CRANS:-0}" -gt "${CEIL_DETENTS:-0}" ] 2>/dev/null && OVER2=1
  OVER2=$(awk -v h="$HOLD" -v o="$OVER2" 'BEGIN { print (h >= 750) ? 1 : o }')
  MAX2=$(awk -v h="$HOLD" -v m="$MAX2" 'BEGIN { print (h > m) ? h : m }')
done <<EOF
$(grep -E '^shift_salve ' "$LOG2" | awk '{print $2, $5}')
EOF
if [ "$NSALVES2" -lt "$SALVES_MIN_B" ]; then
  inval "salves de la phase B" "$NSALVES2 salves mesurees, minimum $SALVES_MIN_B : rien a conclure"
  W_SALVES_B=0
elif [ "$OVER2" = "0" ]; then
  ok "salves de la phase B" "$NSALVES2 salves, maintien maximal $MAX2 ms, sous le seuil de 750 ms"
  W_SALVES_B=1
else
  inval "salves de la phase B" "maintien maximal $MAX2 ms ou salve hors plafond : injection invalide"
  W_SALVES_B=0
fi

if [ "$P_AV1" = "384" ] && [ "$G_AV1" = "24" ]; then
  ok "etat initial mesure" "OUT1 : periode 384 ticks (16 steps), step 24 ticks (x4)"
  W_B_INIT=1
else
  inval "etat initial mesure" "periode ${P_AV1:-absente}, step ${G_AV1:-absent} : precondition de la phase non etablie"
  W_B_INIT=0
fi
if [ "${TWI_L:-0}" -gt 0 ] 2>/dev/null && [ "${TWI_S:-0}" -gt 0 ] 2>/dev/null; then
  ok "temoin I2C" "$TWI_L puis $TWI_S octets : les deux gestes ont atteint l interface"
  W_B_TWI=1
else
  inval "temoin I2C" "un geste sans trafic : geste NON INJECTE"
  W_B_TWI=0
fi

W_B=$(( W_B_INIT * W_B_TWI * W_SALVES_B * W_FACTORY ))
if [ "$W_B" != "1" ]; then
  inval "LENGTH 16 -> 19" "temoins amont invalides : l effet n est pas attribuable au firmware"
  inval "non-contagion" "temoins amont invalides : l effet n est pas attribuable au firmware"
  inval "SUBDIV x4 -> x3" "temoins amont invalides : l effet n est pas attribuable au firmware"
  inval "report ADR 0004" "temoins amont invalides : l effet n est pas attribuable au firmware"
else
  if [ "$P_AP1" = "456" ]; then
    ok "LENGTH 16 -> 19" "periode OUT1 : 384 -> 456 ticks, soit 19 steps de 24"
  else
    bad "LENGTH 16 -> 19" "periode $P_AP1 au lieu de 456"
  fi
  CONTAGION=0
  CONTAGION_N=0
  for L in 2 3 4 5 6; do
    V="$(f2 p25_apres_OUT$L 2)"
    [ -n "$V" ] && CONTAGION_N=$((CONTAGION_N + 1))
    [ "$V" = "384" ] || CONTAGION=1
  done
  if [ "$CONTAGION_N" -ne 5 ]; then
    inval "non-contagion" "$CONTAGION_N sorties mesurees au lieu de 5 : rien a conclure"
  elif [ "$CONTAGION" = "0" ]; then
    ok "non-contagion" "OUT2 a OUT6 gardent leur periode de 384 ticks"
  else
    bad "non-contagion" "une autre sortie a change de periode"
  fi
  if [ "$SUB_AV" = "24" ] && [ "$SUB_AP" = "32" ]; then
    ok "SUBDIV x4 -> x3" "step : 24 -> 32 ticks"
  else
    bad "SUBDIV x4 -> x3" "step $SUB_AV -> $SUB_AP"
  fi
  if [ -z "$SUB_FIRST32" ] || [ -z "$SUB_FRONT" ]; then
    inval "report ADR 0004" "tick de frontiere ou de premier step manquant : rien a conclure"
  elif [ "$SUB_FIRST32" -ge "$SUB_FRONT" ] 2>/dev/null; then
    ok "report ADR 0004" "geste au tick $SUB_GESTE, frontiere $SUB_FRONT, premier step de 32 ticks a $SUB_FIRST32"
  else
    bad "report ADR 0004" "un step de 32 ticks des le tick $SUB_FIRST32, avant la frontiere $SUB_FRONT"
  fi
fi
printf '\n'
if [ "$BAD_COUNT" -gt 0 ]; then
  printf '  %s❌ VERDICT : FAIL — %d defaut(s) du firmware, %d critere(s) rendu(s) non decidable(s) en aval.%s\n' \
    "$C_ERR" "$BAD_COUNT" "$INVAL_COUNT" "$C_0"
  printf '  %s   Les INVALID aval decoulent du defaut deja detecte : ils ne le masquent pas.%s\n' "$C_DIM" "$C_0"
  exit 1
fi
if [ "$INVAL_COUNT" -gt 0 ]; then
  printf '  %s⛔ VERDICT : INVALID — 0 defaut du firmware, %d critere(s) non decidable(s).%s\n' \
    "$C_ERR" "$INVAL_COUNT" "$C_0"
  printf '  %s   Un prerequis ou un temoin necessaire manque : aucun verdict sur le firmware.%s\n' "$C_DIM" "$C_0"
  exit 5
fi
printf '  %s✅ P2.1 : un cran = un pas. Sens MESURE : A-d-abord = +1, B-d-abord = -1.%s\n' "$C_OK" "$C_0"
printf '  %s✅ P2.2 : 5 ms sans effet, 60 ms = appui court, 900 ms = appui long, retour exact.%s\n' "$C_OK" "$C_0"
printf '  %s✅ P2.3 : SHIFT tenu, trois crans, ratchet 00 -> 04, pattern intact, maintien sous 750 ms.%s\n' "$C_OK" "$C_0"
printf '  %s✅ P2.4 : controle d usine octet pour octet, triolet pose et retire, step bascule et rebascule.%s\n' "$C_OK" "$C_0"
printf '  %s✅ P2.5 : LENGTH et SUBDIV verifies sur les sorties, report au temps respecte, aucune contagion.%s\n' "$C_OK" "$C_0"
printf '  %s✅ FRACT : shiftRotate long decoupe en salves, chaque maintien mesure et sous 750 ms, pattern intact.%s\n' "$C_OK" "$C_0"
printf '  %s✅ VERDICT : PASS — %d criteres verts, 0 defaut, 0 non decidable.%s\n' "$C_OK" "$OK_COUNT" "$C_0"
exit 0
