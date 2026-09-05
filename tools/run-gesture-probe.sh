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
  IMAGE_MUTATE=<offset>          altere un octet de l'image donnee a la machine,
                                 l'attendu gardant l'image d'origine
  RIG_STEPS=0,5,9                steps actifs du rig de recette
  RIG_SUBDIV=1                   subdiv du rig de recette
  EXPECT_R8_MASK / EXPECT_R9_NIBBLE / EXPECT_R12_NIBBLE
                                 deplacent l'attente d'une recette A
  R10_STEP=<n>                   vise un autre step que le 4 pour R10
  EXPECT_R1_PAS / EXPECT_R1_CHANNEL / EXPECT_R13_PERIODE
                                 deplacent l'attente d'une recette B
  SKIP_B_GESTE=1                 n'injecte pas les salves des recettes B ni de R2
  EXPECT_R2_PAS / EXPECT_R2_LENGTH / EXPECT_R2_CHANNEL
                                 deplacent l'attente de R2
  R2_ROTATIONS=<n>               nombre de rotations avant le cran de l'etape SUBDIV
  EXPECT_R11_NIBBLE_1 / EXPECT_R11_NIBBLE_TRIOLET / EXPECT_R11_CADENCE
  EXPECT_R11_CADENCE_PALIER      deplacent l'attente de R11
  R11_CRANS_SUBDIV=<n>           crans pour atteindre x24 depuis /1 (defaut : lu
                                 dans le domaine). La descente est ETAGEE : n-1
                                 crans jusqu'a un palier INTERIEUR a la liste,
                                 mesure, puis 1 cran jusqu'a x24. Sans le palier,
                                 le critere x24 passait pour tout n >= 8, l'index
                                 0 etant ecrete.
  SKIP_SHIFT=1                   n'injecte pas le geste SHIFT (classe 1)
  INSTANCE_BASE_FORCE=<adr>      force la base du tampon observe (0 = pointeur nul)
  INSTANCE_CHANNEL_FORCE=<n>     force le canal lu par les accesseurs gardes
  TEMPLATE_MUTATE=<offset>       change un octet de la zone EEPROM des templates entre
                                 les deux lectures du parcours instances
  R11_STEP_ROTATIONS=<n>         crans de deplacement du curseur dans R11 (defaut 5)
  SKIP_EDIT=1                    n'entre pas dans EDIT (classe 2)
  R13_CRANS_LENGTH=<1..12>       crans de la salve LENGTH de R13 (defaut 3). Le
                                 poser leve la limite empirique, pour balayer la
                                 plage entiere
  R13_CRANS_DESCENTE=<1..12>     crans de la salve INVERSE de R13, pour separer
                                 la valeur de depart du nombre de crans
  EDGE_SPACING_MS=<0.1..50>      delai entre les quatre fronts d'un cran
                                 (defaut 1). L'elargir CREUSE la perte de la
                                 ligne 44 au lieu de la supprimer : mesure du
                                 2026-09-05
  EXPECT_RATCHET_APRES=<hh>      change l'attente du ratchet (classe 3)

Le harnais charge le binaire de production, attache l'ecran, lit les instances de
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
  4    salve de SHIFT refusee AVANT injection : le geste n'etait pas valide
  5    oracle indecidable : aucun verdict sur le firmware. Couvre le symbole
       d'instrumentation absent ou ambigu, un pointeur d'instances illisible,
       un acces hors bornes, et le controle d'usine en echec (ancien code 3)
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

pointeur_sain() {
  local lisible echecs fautes
  lisible="$(grep -E '^instances_pointeur ' "$1" 2>/dev/null | awk '{print $5}')"
  echecs="$(grep -E '^instances_acces ' "$1" 2>/dev/null | awk '{print $5}')"
  fautes="$(grep -E '^instances_acces ' "$1" 2>/dev/null | awk '{print $7}')"
  [ "${lisible:-}" = "1" ] && [ "${echecs:-1}" = "0" ] && [ "${fautes:-1}" = "0" ]
}

garde_pointeur() {
  local journal="$1" etiquette="$2"
  pointeur_sain "$journal" && return 0
  inval "$etiquette" "pointeur d instances illisible ou acces hors bornes : aucun verdict sur ce parcours"
  return 1
}

verdict_invalid_exit() {
  printf '  %s⛔ VERDICT : INVALID — 0 defaut du firmware, %d critere(s) non decidable(s).%s\n' \
    "$C_ERR" "$INVAL_COUNT" "$C_0"
  printf '  %s   Un prerequis ou un temoin necessaire manque : aucun verdict sur le firmware.%s\n' \
    "$C_DIM" "$C_0"
  exit 5
}

dieinval() {
  local journal="$1" contexte="$2" lisible base
  lisible="$(grep -E '^instances_pointeur ' "$journal" 2>/dev/null | awk '{print $5}')"
  base="$(grep -E '^instances_pointeur ' "$journal" 2>/dev/null | awk '{print $3}')"
  if [ "${lisible:-1}" = "0" ]; then
    inval "temoin du pointeur" "$contexte — cause : pointeur d instances illisible (base ${base:-?})"
  else
    inval "controle d usine" "$contexte"
  fi
  verdict_invalid_exit
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
TIMING_STEPS="0,3,4,9,15"
TIMING_SUBDIV="-4"
TIMING_MASK="8219"
RIG_STEPS="${RIG_STEPS:-0,5,9}"
RIG_SUBDIV="${RIG_SUBDIV:-1}"
RIG_MASK_ATTENDU="0221"
RIG_SUBDIV_ATTENDU="1"
RIG_GAPS_MIN=6
RIG_KEPT_MIN=2
SALVES_MIN_A2=5
B_STEPS_ACTIFS_ATTENDU=3
B_PAS_BASE=96
B_PAS_CHANGE=48
B_DIST_MIN=6
B_RET_MIN=2
CURSEUR_BARRE=-1
MOD_ONGLET=4
MOD_PERIODE_BASE=1536
MOD_PERIODE_ROUTEE="${MOD_PERIODE_ROUTEE:-2496}"
MOD_CV1_MV="${MOD_CV1_MV:-4150}"
R13_ONGLET=3
R13_PERIODE_BASE=1536
R13_PERIODE_CHANGE_ATTENDUE=1824
R2_ONGLET=4
R2_PAS_BASE=96
R2_PERIODE_BASE=1536
R2_PAS_SUBDIV_ATTENDU=48
R2_LENGTH_BASE=16
R2_LENGTH_APRES_ATTENDUE=17
R11_ONGLET=4
R11_CADENCE_BASE=96
R11_CADENCE_PALIER_ATTENDUE=6
R11_CADENCE_X24_ATTENDUE=4
R11_NIBBLE_1_ATTENDU="02"
R11_NIBBLE_TRIOLET_ATTENDU="07"
R11_NIBBLE_ZERO="00"
R11_CANAL_ATTENDU=3
R11_OCTET_ATTENDU=7
R11_CODES_REFUSES="03 04 06"
TAB_COUNT_ECRAN=8
R8_MASK_ATTENDU="0229"
R9_OCTET_ATTENDU="60"
R12_OCTET_ATTENDU="70"
RIG_MASQUE_BASE="0221"
SALVES_MIN_A=5
SALVES_MIN_B=2

WORK="${KEEP_WORK:-$(mktemp -d)}"
mkdir -p "$WORK"
LOG="$WORK/log"
trap '[ -n "${KEEP_WORK:-}" ] || rm -rf "$WORK"' EXIT

printf '%s=== SONDE DE GESTES (P2.1 a P2.5, puis fractionnement) ===%s\n' "$C_B" "$C_0"

progress "compilation du harnais"
BIN="$WORK/gesture_probe"
if c++ -O2 -Wall -std=gnu++11 -I"$PREFIX/include/simavr" -I"$PREFIX/include" \
     -I"$ROOT/include" \
     "$ROOT/tools/simavr-ssd1306/gesture_probe.cpp" \
     "$ROOT"/src/domain/*.cpp -o "$BIN" \
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

. "$ROOT/tools/active-format.sh"
flexseq_resolve_active_format "$ROOT" "$ROOT/.pio/build/nanoatmega328/firmware.elf" \
  "$WORK" || exit $?
flexseq_report_active_format "$C_OK" "$C_DIM" "$C_0"

progress "generateur d'image EEPROM"
GEN="$WORK/eeprom-image"
if c++ -std=gnu++11 -I"$ROOT/include" -o "$GEN" "$ROOT/tools/eeprom-image.cpp" \
     "$ROOT"/src/domain/*.cpp > "$LOG" 2>&1; then
  ok "generateur compile" "tools/eeprom-image.cpp"
else
  printf '\n'; cat "$LOG"; die "compilation du generateur en echec"
fi
if ! "$GEN" --mode seq --steps "$TIMING_STEPS" --subdiv "$TIMING_SUBDIV" --tempo 138 \
     --format "$FLEXSEQ_FORMAT_VERSION" > "$WORK/image.bin" 2>"$LOG"; then
  cat "$LOG"; die "generation de l'image de mesure temporelle en echec"
fi
ok "image de mesure" "SEQ, steps $TIMING_STEPS, subdiv $TIMING_SUBDIV, 138 BPM — phases P2.1 a P2.5"
if ! "$GEN" --mode seq --steps "$RIG_STEPS" --subdiv "$RIG_SUBDIV" --tempo 138 \
     --format "$FLEXSEQ_FORMAT_VERSION" > "$WORK/rig.bin" 2>"$LOG"; then
  cat "$LOG"; die "generation du rig de recette en echec"
fi
ok "image de rig" "SEQ, steps $RIG_STEPS, subdiv $RIG_SUBDIV, 138 BPM — rig de la couche 1"
if ! "$GEN" --mode seq --per-channel --tempo 138 --format "$FLEXSEQ_FORMAT_VERSION" > "$WORK/perchannel.bin" 2>"$LOG"; then
  cat "$LOG"; die "generation de l'image differenciee en echec"
fi
ok "image differenciee" "SEQ, un template par canal, C0 a C5 vers les templates 0 a 5"

ELF="$ROOT/.pio/build/nanoatmega328/firmware.elf"
AVR_DATA_BASE=$(( 0x800000 ))

INSTANCE_BYTES="$("$BIN" --observed-instance-bytes 2>/dev/null | tr -d '[:space:]')"
case "$INSTANCE_BYTES" in
  ''|*[!0-9]*)
    inval "symbole des instances" "le harnais n annonce aucune taille d instances observable"
    exit 1 ;;
esac

INSTANCE_PTR_PATTERN="${INSTANCE_PTR_SYMBOL:-12instanceBaseE$|^instanceBase$}"
ptr_rows="$("$NM" -S --defined-only "$ELF" | awk -v pat="$INSTANCE_PTR_PATTERN" '$NF ~ pat { print NF, $0 }')"
if [ -z "$ptr_rows" ]; then ptr_count=0
else ptr_count="$(printf '%s\n' "$ptr_rows" | wc -l | tr -d ' ')"; fi

if [ "$ptr_count" -eq 0 ]; then
  inval "symbole des instances" "aucun symbole ne correspond a $INSTANCE_PTR_PATTERN"
  verdict_invalid_exit
fi
if [ "$ptr_count" -gt 1 ]; then
  inval "symbole des instances" "$ptr_count symboles correspondent : la selection serait arbitraire"
  printf '%s\n' "$ptr_rows" | sed 's/^/      /'
  verdict_invalid_exit
fi

ptr_nf="$(printf '%s' "$ptr_rows" | awk '{print $1}')"
if [ "$ptr_nf" = "4" ]; then
  ptr_vma="$(printf '%s' "$ptr_rows" | awk '{print $2}')"
  ptr_size="$(printf '%s' "$ptr_rows" | awk '{print $3}')"
  ptr_type="$(printf '%s' "$ptr_rows" | awk '{print $4}')"
else
  ptr_vma="$(printf '%s' "$ptr_rows" | awk '{print $2}')"
  ptr_size=""
  ptr_type="$(printf '%s' "$ptr_rows" | awk '{print $3}')"
fi

case "$ptr_type" in
  b|B|d|D) ;;
  *) inval "symbole des instances" "type '$ptr_type' : ni .bss ni .data, l adresse n est pas une adresse de donnees"
     verdict_invalid_exit ;;
esac
if [ -z "$ptr_size" ]; then
  inval "symbole des instances" "st_size absent de la sortie de avr-nm -S : taille non verifiable"
  verdict_invalid_exit
fi
if [ "$(( 0x$ptr_size ))" -ne 2 ]; then
  inval "symbole des instances" "taille $(( 0x$ptr_size )) octets : ce n est pas le pointeur d instrumentation, qui en fait 2"
  verdict_invalid_exit
fi
if [ "$(( 0x$ptr_vma ))" -lt "$AVR_DATA_BASE" ]; then
  inval "symbole des instances" "VMA 0x$ptr_vma sous 0x800000 : ce n est pas l espace de donnees"
  verdict_invalid_exit
fi
INSTANCE_PTR_ADDR="$(printf '0x%x' $(( 0x$ptr_vma - AVR_DATA_BASE )))"
ok "symbole des instances" "pointeur unique, type $ptr_type, 2 octets, VMA 0x$ptr_vma -> RAM $INSTANCE_PTR_ADDR ; $INSTANCE_BYTES octets observes a l adresse pointee"

SUPPRESSED_PATTERN="${SUPPRESSED_SYMBOL:-14suppressedLongE$|^suppressedLong$}"

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
  selfbad() { printf '  %s❌%s %-22s %s%s%s\n' "$C_ERR" "$C_0" "$1" "$C_DIM" "$2" "$C_0"; SELF_FAILED=$((SELF_FAILED + 1)); }
  build_mutant() {
    c++ -O2 -w -std=gnu++11 -I"$PREFIX/include/simavr" -I"$PREFIX/include" \
      -I"$ROOT/tools/simavr-ssd1306" -I"$ROOT/include" "$1" \
      "$ROOT"/src/domain/*.cpp -o "$2" \
      -L"$PREFIX/lib" -lsimavrparts -lsimavr -lelf
  }

  progress "mutant 1 : aucun fractionnement"
  python3 - "$SRC" "$WORK/m1.cpp" <<'MUTANT1'
import sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src).read()
a = """        count = burst::split(request.detents, verdict.effectiveLimit,
                             plan, burst::MAX_BURSTS);"""
if s.count(a) != 1:
    sys.exit(2)
b = """        plan[0] = request.detents;
        count = 1;"""
open(dst, "w").write(s.replace(a, b, 1))
MUTANT1
  [ $? = 0 ] || die "mutant 1 : motif absent du code source" 2
  cmp -s "$SRC" "$WORK/m1.cpp" && die "mutant 1 : mutation non appliquee" 2
  if build_mutant "$WORK/m1.cpp" "$WORK/m1" > "$LOG" 2>&1; then
    "$WORK/m1" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$INSTANCE_PTR_ADDR" "$BOOT_MS" \
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
a = """        count = burst::split(request.detents, verdict.effectiveLimit,
                             plan, burst::MAX_BURSTS);"""
b = """    if (detents < 1 || detents > SHIFT_BURST_DETENTS
        || SHIFT_HOLD_MS(detents) >= SHIFT_HOLD_CEILING_MS
        || holdPlanned >= (double)SHIFT_HOLD_CEILING_MS) {"""
if s.count(a) != 1 or s.count(b) != 1:
    sys.exit(2)
single = """        plan[0] = request.detents;
        count = 1;"""
open(dst, "w").write(s.replace(a, single, 1).replace(b, "    if (0) {", 1))
MUTANT3
  [ $? = 0 ] || die "mutant 3 : motif absent du code source" 2
  cmp -s "$SRC" "$WORK/m3.cpp" && die "mutant 3 : mutation non appliquee" 2
  if build_mutant "$WORK/m3.cpp" "$WORK/m3" > "$LOG" 2>&1; then
    "$WORK/m3" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$INSTANCE_PTR_ADDR" "$BOOT_MS" \
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
    "$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$INSTANCE_PTR_ADDR" 1 \
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
    | grep -cE 'temoin I2C|maintien de SHIFT|entree dans EDIT|temoin du pointeur|temoin des instances|controle d usine|echantillons' | tr -d ' ')"
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

  progress "P2.6.0 : image alteree cote machine"
  if env -u SELFTEST IMAGE_MUTATE=1 "$0" > "$WORK/img.log" 2>&1; then
    selfbad "P2.6.0 controle de l image" "la sonde rend 0 alors que l image injectee ne correspond pas a l attendu"
  else
    IMG_RC=$?
    if [ "$IMG_RC" = "5" ] && grep -q '⛔ instances : templates d usine' "$WORK/img.log"; then
      ok "P2.6.0 controle de l image" "un octet altere cote machine rend le controle du rig INVALID, sortie 5, aucun verdict firmware"
    else
      selfbad "P2.6.0 controle de l image" "sortie $IMG_RC au lieu de 5"
    fi
  fi

  progress "P2.6.1 : motif temporel injecte dans la phase rig"
  expect_verdict "P2.6.1 rig : mauvais steps" INVALID 5 0 "+" RIG_STEPS=0,3,4,9,15
  grep -q '⛔ image du rig' "$WORK/class.log" \
    && ok "P2.6.1 rig : identite" "le masque 8219 est refuse comme rig de recette" \
    || selfbad "P2.6.1 rig : identite" "un masque etranger est passe pour le rig"

  progress "P2.6.1 : subdiv x4 au lieu de /1"
  expect_verdict "P2.6.1 rig : mauvais subdiv" INVALID 5 0 "+" RIG_SUBDIV=-4
  if grep -qE '^ *rig_marche +7 crans, 4 pgcd' "$WORK/class.log" \
     && grep -q '⛔ rig : parcours de R11' "$WORK/class.log"; then
    ok "P2.6.1 rig : plafonnement" "a x4 le 7e cran lit deja 4 ticks — parcours plafonne, et non attribuable"
  else
    selfbad "P2.6.1 rig : plafonnement" "le plafonnement n est pas visible, ou le parcours a ete attribue"
  fi

  progress "P2.6.2 : attente de R8 deplacee"
  expect_verdict "P2.6.2 R8 -> FAIL" FAIL 1 1 "*" EXPECT_R8_MASK=0231
  progress "P2.6.2 : attente de R9 deplacee"
  expect_verdict "P2.6.2 R9 -> FAIL" FAIL 1 1 "*" EXPECT_R9_NIBBLE=05
  progress "P2.6.2 : attente de R12 deplacee"
  expect_verdict "P2.6.2 R12 -> FAIL" FAIL 1 1 "*" EXPECT_R12_NIBBLE=06

  progress "P2.6.2 : R10 vise un step ACTIF"
  expect_verdict "P2.6.2 R10 -> FAIL" FAIL 1 1 "+" R10_STEP=5
  if grep -q '❌ R10 : refus sur step inactif' "$WORK/class.log" \
     && grep -q '⛔ R12 : triolet sur step 9' "$WORK/class.log"; then
    ok "P2.6.2 R10 n est pas un faux vert" "un ratchet ecrit sur un step actif rend le critere rouge, et R12 devient non decidable"
  else
    selfbad "P2.6.2 R10 n est pas un faux vert" "le critere « rien ne change » n a pas rougi, ou R12 a herite d un defaut"
  fi

  progress "P2.6.2 : precondition EDIT absente"
  expect_verdict "P2.6.2 EDIT absent -> INVALID" INVALID 5 0 "+" SKIP_EDIT=1
  if [ "$(grep -cE '❌ (R8|R9|R10|R12)' "$WORK/class.log")" = "0" ] \
     && [ "$(grep -cE '⛔ (R8|R9|R10|R12)' "$WORK/class.log")" -ge 7 ]; then
    ok "P2.6.2 recettes A non attribuables" "les 7 criteres de recette sont INVALID, aucun defaut firmware"
  else
    selfbad "P2.6.2 recettes A non attribuables" "une recette A a produit un defaut firmware sans precondition"
  fi

  progress "P2.6.3 : attente du pas de R1 deplacee"
  expect_verdict "P2.6.3 R1 pas -> FAIL" FAIL 1 1 "*" EXPECT_R1_PAS=60
  progress "P2.6.3 : R1 attend l effet sur un autre channel"
  expect_verdict "P2.6.3 R1 channel -> FAIL" FAIL 1 2 "*" EXPECT_R1_CHANNEL=2
  if grep -q '❌ R1 : non-contagion' "$WORK/class.log"; then
    ok "P2.6.3 non-contagion sait rougir" "attendre l effet sur un autre channel fait rougir la sortie visee ET la non-contagion"
  else
    selfbad "P2.6.3 non-contagion sait rougir" "le critere de non-contagion n a pas rougi"
  fi
  progress "P2.6.3 : geste B non injecte"
  expect_verdict "P2.6.3 geste B absent -> INVALID" INVALID 5 0 "+" SKIP_B_GESTE=1
  progress "P2.6.3 : attente de periode de R13 deplacee"
  expect_verdict "P2.6.3 R13 -> FAIL" FAIL 1 1 "*" EXPECT_R13_PERIODE=1920

  progress "P2.6.4 : attente du pas de R2 deplacee"
  expect_verdict "P2.6.4 R2 pas -> FAIL" FAIL 1 1 "*" EXPECT_R2_PAS=60
  progress "P2.6.4 : R2 pretend que LENGTH change"
  expect_verdict "P2.6.4 R2 length -> FAIL" FAIL 1 1 "*" EXPECT_R2_LENGTH=17
  progress "P2.6.4 : R2 attend l effet sur un autre channel"
  expect_verdict "P2.6.4 R2 channel -> FAIL" FAIL 1 3 "*" EXPECT_R2_CHANNEL=2
  if grep -q '❌ R2 : non-contagion' "$WORK/class.log"; then
    ok "P2.6.4 R2 non-contagion sait rougir" "le critere nomme la sortie qui a change"
  else
    selfbad "P2.6.4 R2 non-contagion sait rougir" "le critere de non-contagion n a pas rougi"
  fi
  progress "P2.6.4 : une seule rotation avant le cran de SUBDIV"
  expect_verdict "P2.6.4 R2 une rotation -> FAIL" FAIL 1 2 "*" R2_ROTATIONS=1
  if grep -q '❌ R2 : LENGTH inchangee sous SUBDIV' "$WORK/class.log"; then
    ok "P2.6.4 absence d effet sait rougir" "avec une seule rotation le cran touche LENGTH, qui tombe a 15 : le critere n est pas un faux vert"
  else
    selfbad "P2.6.4 absence d effet sait rougir" "le critere « LENGTH inchangee » est reste vert alors que LENGTH a change"
  fi
  progress "P2.6.4 : gestes de R2 non injectes"
  expect_verdict "P2.6.4 R2 geste absent -> INVALID" INVALID 5 0 "+" SKIP_B_GESTE=1

  progress "P2.6.5 : attente du premier nibble deplacee"
  expect_verdict "P2.6.5 R11 nibble1 -> FAIL" FAIL 1 1 "*" EXPECT_R11_NIBBLE_1=03
  progress "P2.6.5 : attente du triolet deplacee"
  expect_verdict "P2.6.5 R11 triolet -> FAIL" FAIL 1 1 "*" EXPECT_R11_NIBBLE_TRIOLET=06
  progress "P2.6.5 : attente de cadence deplacee"
  expect_verdict "P2.6.5 R11 cadence -> INVALID" INVALID 5 0 "+" EXPECT_R11_CADENCE=6
  progress "P2.6.5 : gestes de R11 non injectes"
  expect_verdict "P2.6.5 R11 geste absent -> INVALID" INVALID 5 0 "+" SKIP_B_GESTE=1
  progress "P2.6.5 : rester a /1 au lieu de x24"
  expect_verdict "P2.6.5 R11 sans x24 -> FAIL" FAIL 1 3 "+" R11_CRANS_SUBDIV=0
  if grep -q '❌ R11 : les codes refuses n apparaissent jamais' "$WORK/class.log" \
     && grep -q '⛔ R11 : cadence x24' "$WORK/class.log"; then
    ok "P2.6.5 le critere des codes refuses sait rougir" "a /1 la marche donne 02 03 04 : le critere nomme les codes ecrits, et la cadence reste non decidable"
  else
    selfbad "P2.6.5 le critere des codes refuses sait rougir" "le critere est reste vert alors qu un code refuse a x24 a ete ecrit"
  fi

  progress "d : classe 1 avec effet aval faux -> INVALID"
  expect_verdict "d. classe 1 -> INVALID" INVALID 5 0 "+" SKIP_SHIFT=1
  if grep -q '⛔ triolet pose' "$WORK/class.log" && ! grep -q '❌ triolet pose' "$WORK/class.log"; then
    ok "d. l aval ne devient pas FAIL" "le triolet lit 03 au lieu de 07, et reste non decidable"
  else
    selfbad "d. l aval ne devient pas FAIL" "le defaut aval a ete impute au firmware"
  fi

  progress "f : pointeur d instances nul -> INVALID"
  expect_verdict "f. pointeur nul -> INVALID" INVALID 5 0 "+" INSTANCE_BASE_FORCE=0
  if grep -q '⛔ temoin du pointeur' "$WORK/class.log"; then
    ok "f. la cause est nommee" "le temoin du pointeur rend INVALID, aucun defaut impute au firmware"
  else
    selfbad "f. la cause est nommee" "aucun ⛔ sur le temoin du pointeur : la cause n est pas attribuee"
  fi

  progress "g : pointeur hors RAM -> INVALID"
  expect_verdict "g. pointeur hors RAM -> INVALID" INVALID 5 0 "+" INSTANCE_BASE_FORCE=0x1FFF
  if grep -q '⛔ temoin du pointeur' "$WORK/class.log"; then
    ok "g. la cause est nommee" "base hors RAM : INVALID, aucun defaut impute au firmware"
  else
    selfbad "g. la cause est nommee" "aucun ⛔ sur le temoin du pointeur"
  fi

  progress "h : canal invalide -> INVALID"
  expect_verdict "h. canal invalide -> INVALID" INVALID 5 0 "+" INSTANCE_CHANNEL_FORCE=6
  if grep -q '⛔ temoin du pointeur' "$WORK/class.log"; then
    ok "h. la cause est nommee" "acces hors bornes comptes : INVALID, aucun defaut impute au firmware"
  else
    selfbad "h. la cause est nommee" "aucun ⛔ sur le temoin du pointeur"
  fi

  printf '\n%s--- CONTRE-EPREUVE DE L ORACLE PAR CANAL ---%s\n' "$C_B" "$C_0"

  progress "j : l oracle lit l instance 0 partout -> FAIL"
  expect_verdict "j. oracle sur l instance 0 -> FAIL" FAIL 1 "+" "*" INSTANCE_CHANNEL_FORCE=0
  if grep -qE '❌ instances : six (masques|comptes)' "$WORK/class.log"; then
    ok "j. les criteres par canal ont des dents" "masques ou comptes rouges : lire l instance 0 ne passe plus"
  else
    selfbad "j. les criteres par canal ont des dents" "aucun critere par canal n a rougi"
  fi

  progress "k : le geste R11 vise un autre step actif -> FAIL"
  expect_verdict "k. rang R11 deplace -> FAIL" FAIL 1 "+" "*" R11_STEP_ROTATIONS=9
  if grep -q 'octet 9' "$WORK/class.log"; then
    ok "k. l octet dans l instance est verifie" "la sonde lit l octet 9 et le refuse : l attente de rang a des dents"
  else
    selfbad "k. l octet dans l instance est verifie" "l octet lu n a pas ete rapporte"
  fi

  progress "l : un octet du template change -> FAIL"
  expect_verdict "l. template modifie -> FAIL" FAIL 1 "+" "*" TEMPLATE_MUTATE=0
  if grep -q '❌ instances : templates EEPROM stables' "$WORK/class.log"; then
    ok "l. la non-modification du template a des dents" "un seul octet change dans la zone EEPROM des templates suffit a rougir"
  else
    selfbad "l. la non-modification du template a des dents" "le critere de template n a pas rougi"
  fi

  progress "i : symbole d instrumentation absent -> INVALID"
  expect_verdict "i. symbole absent -> INVALID" INVALID 5 0 "+" INSTANCE_PTR_SYMBOL=aucun_symbole_de_ce_nom
  if grep -q '⛔ symbole des instances' "$WORK/class.log"; then
    ok "i. la cause est nommee" "symbole absent : INVALID et code 5, jamais le code d un defaut firmware"
  else
    selfbad "i. la cause est nommee" "aucun ⛔ sur le symbole des instances"
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
  printf '  %s❌ SELFTEST : %d cas en echec. Chacun est marque ❌ ci-dessus.%s\n' "$C_ERR" "$SELF_FAILED" "$C_0"
  exit 1
fi

progress "demarrage simule ($BOOT_MS ms)"
"$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$INSTANCE_PTR_ADDR" "$BOOT_MS" \
     "" 384 structure "$SUPPRESSED_ADDR" > "$LOG" 2>&1
RC=$?
if [ "$RC" != "0" ]; then
  cat "$LOG"
  case "$RC" in
    3) dieinval "$LOG" "controle des instances en echec, aucun verdict sur le firmware" ;;
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
grep -E '^instances_low_masks|^instances_ratchets_c0' "$LOG"

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
grep -E '^instances_pointeur|^instances_acces|^instances_inchangees|^controle_source|^inst_attendus|^controle_usine|^entree_edit' "$LOG" | sed 's/^/  /'
printf '\n'

PTR_BASE="$(grep -E '^instances_pointeur ' "$LOG" | awk '{print $3}')"
PTR_LISIBLE="$(grep -E '^instances_pointeur ' "$LOG" | awk '{print $5}')"
PTR_FORCE="$(grep -E '^instances_pointeur ' "$LOG" | awk '{print $7}')"
ACC_LECTURES="$(grep -E '^instances_acces ' "$LOG" | awk '{print $3}')"
ACC_ECHECS="$(grep -E '^instances_acces ' "$LOG" | awk '{print $5}')"
ACC_FAUTES="$(grep -E '^instances_acces ' "$LOG" | awk '{print $7}')"
if [ -z "$PTR_LISIBLE" ] || [ -z "$ACC_ECHECS" ] || [ -z "$ACC_FAUTES" ]; then
  inval "temoin du pointeur" "la sonde ne publie pas l etat du pointeur d instances : decidabilite non evaluable"
  W_PTR=0
elif [ "$PTR_LISIBLE" != "1" ]; then
  inval "temoin du pointeur" "base $PTR_BASE illisible (nulle ou hors RAM) : les 138 octets observes ne viennent pas des instances"
  W_PTR=0
elif [ "${ACC_ECHECS}" != "0" ]; then
  inval "temoin du pointeur" "$ACC_ECHECS lecture(s) sur $ACC_LECTURES ont echoue : le tampon observe est partiellement nul"
  W_PTR=0
elif [ "${ACC_FAUTES}" != "0" ]; then
  inval "temoin du pointeur" "$ACC_FAUTES acces hors bornes (canal force $PTR_FORCE) : l oracle ne sait pas quel canal il lit"
  W_PTR=0
else
  ok "temoin du pointeur" "base $PTR_BASE lisible, $ACC_LECTURES lectures, 0 echec, 0 acces hors bornes"
  W_PTR=1
fi

INSTANCES_LINE="$(grep -E '^instances_inchangees ' "$LOG")"
INSTANCES_OK="$(printf '%s' "$INSTANCES_LINE" | awk '{print $2}')"
if [ -z "$INSTANCES_LINE" ]; then
  inval "temoin des instances" "cle instances_inchangees absente du journal : la sonde et le harnais ne parlent plus le meme contrat"
  W_INSTANCES=0
elif [ "$INSTANCES_OK" = "1" ]; then
  ok "temoin des instances" "instances inchangees par les rotations seules"
  W_INSTANCES=1
else
  inval "temoin des instances" "instances modifiees par les seules rotations : l instrument n est pas sain"
  W_INSTANCES=0
fi

CTRL="$(grep -E '^controle_usine ' "$LOG" | awk '{print $2}')"
CTRL_SRC="$(grep -E '^controle_source ' "$LOG" | awk '{print $2}')"
if [ "$CTRL_SRC" != "usine" ]; then
  inval "controle d usine" "attendu construit depuis '${CTRL_SRC:-rien}' alors qu aucune image n est prechargee"
  W_FACTORY=0
elif [ "$CTRL" = "1" ]; then
  ok "controle d usine" "attendu = $CTRL_SRC ; les instances lues en RAM sont IDENTIQUES, octet pour octet, a celles que le domaine construit"
  W_FACTORY=1
else
  inval "controle d usine" "classe 2 : l instrument est faux, aucun verdict sur le firmware"
  W_FACTORY=0
fi

EDIT_TWI="$(grep -E '^entree_edit ' "$LOG" | awk '{print $5}')"
EDIT_SLOTS_BEFORE="$(grep -E '^entree_edit ' "$LOG" | awk '{print $7}')"
EDIT_SLOTS_AFTER="$(grep -E '^entree_edit ' "$LOG" | awk '{print $8}')"
EDIT_DISTINCT="$(grep -E '^entree_edit ' "$LOG" | awk '{print $10}')"
EDIT_CURSEUR="$(grep -E '^entree_edit ' "$LOG" | awk '{print $12}')"
W_EDIT=0
if [ -z "$EDIT_TWI" ]; then
  inval "entree dans EDIT" "aucune mesure de navigation publiee"
elif [ "${EDIT_TWI:-0}" -eq 0 ] 2>/dev/null; then
  inval "entree dans EDIT" "aucun trafic : le geste de navigation n a pas ete injecte"
elif [ "${EDIT_SLOTS_BEFORE:-0}" != "$TAB_SLOTS" ] \
     || [ "${EDIT_CURSEUR:-x}" != "$CURSEUR_BARRE" ]; then
  inval "entree dans EDIT" "avant le geste, ${EDIT_SLOTS_BEFORE:-?} creneaux et curseur ${EDIT_CURSEUR:-rien} : l ecran de depart n est pas la barre d onglets"
elif [ "${EDIT_SLOTS_AFTER:-$TAB_SLOTS}" -ge "$TAB_SLOTS" ] 2>/dev/null; then
  inval "entree dans EDIT" "la barre d onglets est toujours dessinee (${EDIT_SLOTS_AFTER} creneaux) : EDIT n est pas etabli"
elif [ "$EDIT_DISTINCT" != "1" ]; then
  inval "entree dans EDIT" "l ecran n est distinct ni de la barre d onglets ni du niveau tab"
else
  ok "entree dans EDIT" "$EDIT_TWI octets, depart sur la BARRE (aucune ligne surlignee), creneaux $EDIT_SLOTS_BEFORE -> $EDIT_SLOTS_AFTER, ecran distinct"
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

W_P23=$(( W_SHIFT_TWI * W_SHIFT_HOLD * W_PTR * W_INSTANCES * W_FACTORY * W_EDIT ))
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
grep -E '^triolet_|^instances_restaurees|^instances_finales|^step1_' "$LOG" | sed 's/^/  /'
printf '\n'

TRI_POSE="$(grep -E '^triolet_pose ' "$LOG" | awk '{print $2}')"
TRI_POSE_TWI="$(grep -E '^triolet_pose ' "$LOG" | awk '{print $4}')"
TRI_OFF="$(grep -E '^triolet_retire ' "$LOG" | awk '{print $2}')"
TRI_OFF_TWI="$(grep -E '^triolet_retire ' "$LOG" | awk '{print $4}')"
REST="$(grep -E '^instances_restaurees ' "$LOG" | awk '{print $2}')"
TOG="$(grep -E '^step1_bascule ' "$LOG" | awk '{print $2}')"
TOG_TWI="$(grep -E '^step1_bascule ' "$LOG" | awk '{print $4}')"
UNTOG="$(grep -E '^step1_rebascule ' "$LOG" | awk '{print $2}')"
UNTOG_TWI="$(grep -E '^step1_rebascule ' "$LOG" | awk '{print $4}')"
FINAL="$(grep -E '^instances_finales ' "$LOG" | awk '{print $2}')"

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

W_A=$(( A_TWI_OK * W_PTR * W_INSTANCES * W_FACTORY * W_EDIT * P23_OK ))
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
    ok "triolet retire" "code 00, et les instances redeviennent identiques a celles d usine"
  else
    bad "triolet retire" "code $TRI_OFF, instances restaurees=$REST"
  fi
  if [ "$TOG" = "9113" ]; then
    ok "step bascule" "masque 9111 -> 9113 : le step 1 seul a change"
  else
    bad "step bascule" "masque $TOG au lieu de 9113"
  fi
  if [ "$UNTOG" = "9111" ] && [ "$FINAL" = "1" ]; then
    ok "step rebascule" "masque 9111, et les six instances redeviennent celles d usine"
  else
    bad "step rebascule" "masque $UNTOG, instances finales=$FINAL"
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

W_FRACT=$(( W_FRACT_TWI * W_ENCSW * W_SALVES * W_SPLIT * W_PTR * W_INSTANCES * W_FACTORY * W_EDIT ))
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
    ok "retour a l usine" "les $ASKED crans inverses ramenent les six instances a celles d usine"
  else
    bad "retour a l usine" "les instances ne reviennent pas a celles d usine"
  fi
fi

LOG2="$WORK/log2"
progress "phase temporelle (P2.5)"
"$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$INSTANCE_PTR_ADDR" "$BOOT_MS" \
     "$WORK/image.bin" 384 temporal "$SUPPRESSED_ADDR" > "$LOG2" 2>&1
RC=$?
if [ "$RC" != "0" ]; then
  cat "$LOG2"
  case "$RC" in
    3) dieinval "$LOG2" "controle des instances en echec, aucun verdict sur le firmware" ;;
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

CTRL_B="$(grep -E '^controle_usine ' "$LOG2" | awk '{print $2}')"
CTRL_B_SRC="$(grep -E '^controle_source ' "$LOG2" | awk '{print $2}')"
IMG_B_MASK="$(grep -E '^image_lue ' "$LOG2" | awk '{print $3}')"
IMG_B_SUBDIV="$(grep -E '^image_lue ' "$LOG2" | awk '{print $5}')"
if [ "$IMG_B_MASK" != "$TIMING_MASK" ] || [ "$IMG_B_SUBDIV" != "$TIMING_SUBDIV" ]; then
  inval "image de la phase B" "masque ${IMG_B_MASK:-rien} subdiv ${IMG_B_SUBDIV:-rien} : ce n est pas l image de mesure temporelle ($TIMING_MASK / $TIMING_SUBDIV)"
  W_B_IMAGE=0
elif [ "$CTRL_B_SRC" = "image" ] && [ "$CTRL_B" = "1" ]; then
  ok "image de la phase B" "masque $IMG_B_MASK subdiv $IMG_B_SUBDIV : image de MESURE TEMPORELLE, instances identiques a l attendu"
  W_B_IMAGE=1
else
  inval "image de la phase B" "attendu = ${CTRL_B_SRC:-rien}, controle = ${CTRL_B:-rien} : l etat injecte n est pas verifie"
  W_B_IMAGE=0
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

garde_pointeur "$LOG2" "B : temoin du pointeur" || W_B_IMAGE=0
W_B=$(( W_B_INIT * W_B_TWI * W_SALVES_B * W_FACTORY * W_B_IMAGE ))
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
LOG3="$WORK/log3"
progress "rig de la couche 1 (P2.6.1)"
"$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$INSTANCE_PTR_ADDR" "$BOOT_MS" \
     "$WORK/rig.bin" 384 rig "$SUPPRESSED_ADDR" > "$LOG3" 2>&1
RC=$?
if [ "$RC" != "0" ]; then
  cat "$LOG3"
  case "$RC" in
    3) dieinval "$LOG3" "controle des instances en echec sur le rig, aucun verdict sur le firmware" ;;
    4) die "INVALID (classe 2) : salve de SHIFT refusee avant injection, le geste n etait pas valide" 4 ;;
    *) die "la phase rig s'est terminee anormalement (code $RC)" ;;
  esac
fi

printf '\n%s--- RIG DE LA COUCHE 1 (P2.6.1) ---%s\n' "$C_B" "$C_0"
grep -E '^controle_source|^inst_attendus|^image_lue|^controle_usine|^rig_' "$LOG3" | sed 's/^/  /'
printf '\n'

RIG_SRC="$(grep -E '^controle_source ' "$LOG3" | awk '{print $2}')"
RIG_CTRL="$(grep -E '^controle_usine ' "$LOG3" | awk '{print $2}')"
RIG_MASK_LU="$(grep -E '^image_lue ' "$LOG3" | awk '{print $3}')"
RIG_SUBDIV_LU="$(grep -E '^image_lue ' "$LOG3" | awk '{print $5}')"
RIG_LEN_LU="$(grep -E '^image_lue ' "$LOG3" | awk '{print $7}')"
RIG_MODE_LU="$(grep -E '^image_lue ' "$LOG3" | awk '{print $9}')"

if [ "$RIG_MASK_LU" != "$RIG_MASK_ATTENDU" ] || [ "$RIG_SUBDIV_LU" != "$RIG_SUBDIV_ATTENDU" ]; then
  inval "image du rig" "masque ${RIG_MASK_LU:-rien} subdiv ${RIG_SUBDIV_LU:-rien} : ce n est pas le rig de la couche 1 ($RIG_MASK_ATTENDU / $RIG_SUBDIV_ATTENDU)"
  W_RIG=0
elif [ "$RIG_SRC" = "image" ] && [ "$RIG_CTRL" = "1" ]; then
  ok "image du rig" "masque $RIG_MASK_LU (steps $RIG_STEPS), subdiv $RIG_SUBDIV_LU, length $RIG_LEN_LU, mode $RIG_MODE_LU : RIG DE RECETTE"
  W_RIG=1
else
  inval "image du rig" "attendu = ${RIG_SRC:-rien}, controle = ${RIG_CTRL:-rien}"
  W_RIG=0
fi

rig_field() { grep -E "^rig_marche .* $1 crans," "$LOG3" | head -1 | awk "{print \$$2}"; }
PAS0="$(grep -E '^rig_pas_initial ' "$LOG3" | awk '{print $2}')"
GAPS0="$(grep -E '^rig_pas_initial ' "$LOG3" | awk '{print $6}')"
KEPT0="$(grep -E '^rig_pas_initial ' "$LOG3" | awk '{print $4}')"

if [ "$W_RIG" != "1" ]; then
  inval "rig : SUBDIV /1" "image du rig invalide : le pas mesure n est pas attribuable"
elif [ "${GAPS0:-0}" -lt "$RIG_GAPS_MIN" ] 2>/dev/null || [ "${KEPT0:-0}" -lt "$RIG_KEPT_MIN" ] 2>/dev/null; then
  inval "rig : SUBDIV /1" "${GAPS0:-0} distances et ${KEPT0:-0} retenues, minimum $RIG_GAPS_MIN / $RIG_KEPT_MIN : rien a conclure"
  W_RIG=0
elif [ "$PAS0" = "96" ]; then
  ok "rig : SUBDIV /1" "pas de $PAS0 ticks mesure sur les broches, $GAPS0 distances, $KEPT0 retenues"
else
  bad "rig : SUBDIV /1" "pas de ${PAS0:-rien} ticks au lieu de 96"
fi

RIG_WALK_OK=1
RIG_WALK_DETAIL=""
for pair in "7:6" "8:4" "9:4"; do
  crans="${pair%%:*}"; attendu="${pair##*:}"
  ligne="$(grep -E "^rig_marche +$crans crans," "$LOG3" | head -1)"
  mesure="$(printf '%s' "$ligne" | awk '{print $4}')"
  twi="$(printf '%s' "$ligne" | awk '{print $NF}')"
  RIG_WALK_DETAIL="$RIG_WALK_DETAIL ${crans}crans=${mesure:-rien}"
  [ "$mesure" = "$attendu" ] || RIG_WALK_OK=0
  [ "${twi:-0}" -gt 0 ] 2>/dev/null || RIG_WALK_OK=0
done
if [ "$W_RIG" != "1" ]; then
  inval "rig : parcours de R11" "image du rig invalide : le parcours n est pas attribuable"
elif [ "$RIG_WALK_OK" = "1" ]; then
  ok "rig : parcours de R11" "96 ->$RIG_WALK_DETAIL ticks : index 0 atteint au 8e cran, le 9e ne bouge plus"
else
  bad "rig : parcours de R11" "parcours$RIG_WALK_DETAIL (attendu 7crans=6 8crans=4 9crans=4)"
fi

LOG4="$WORK/log4"
progress "recettes de verification A (P2.6.2)"
"$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$INSTANCE_PTR_ADDR" "$BOOT_MS" \
     "$WORK/rig.bin" 384 recettesA "$SUPPRESSED_ADDR" > "$LOG4" 2>&1
RC=$?
if [ "$RC" != "0" ]; then
  cat "$LOG4"
  case "$RC" in
    3) dieinval "$LOG4" "controle des instances en echec sur les recettes A, aucun verdict sur le firmware" ;;
    4) die "INVALID (classe 2) : salve de SHIFT refusee avant injection, le geste n etait pas valide" 4 ;;
    *) die "la phase des recettes A s'est terminee anormalement (code $RC)" ;;
  esac
fi

printf '\n%s--- RECETTES DE VERIFICATION A : R8 R9 R10 R12 (P2.6.2) ---%s\n' "$C_B" "$C_0"
grep -E '^controle_source|^inst_attendus|^image_lue|^controle_usine|^rA_' "$LOG4" | sed 's/^/  /'
printf '\n'

a2() { grep -E "^$1 " "$LOG4" | head -1 | awk "{print \$$2}"; }

A2_SRC="$(a2 controle_source 2)"
A2_CTRL="$(a2 controle_usine 2)"
A2_MASK_IMG="$(a2 image_lue 3)"
A2_SUBDIV_IMG="$(a2 image_lue 5)"
if [ "$A2_MASK_IMG" != "$RIG_MASK_ATTENDU" ] || [ "$A2_SUBDIV_IMG" != "$RIG_SUBDIV_ATTENDU" ]; then
  inval "A : image du rig" "masque ${A2_MASK_IMG:-rien} subdiv ${A2_SUBDIV_IMG:-rien} : ce n est pas le rig de la couche 1"
  W_A2_IMG=0
elif [ "$A2_SRC" = "image" ] && [ "$A2_CTRL" = "1" ]; then
  ok "A : image du rig" "masque $A2_MASK_IMG subdiv $A2_SUBDIV_IMG, instances identiques a l attendu"
  W_A2_IMG=1
else
  inval "A : image du rig" "attendu = ${A2_SRC:-rien}, controle = ${A2_CTRL:-rien}"
  W_A2_IMG=0
fi

A2_EDIT_TWI="$(a2 rA_edit 3)"
A2_EDIT_AV="$(a2 rA_edit 5)"
A2_EDIT_AP="$(a2 rA_edit 6)"
if [ -z "$A2_EDIT_TWI" ]; then
  inval "A : entree dans EDIT" "aucune mesure de navigation publiee"
  W_A2_EDIT=0
elif [ "${A2_EDIT_TWI:-0}" -eq 0 ] 2>/dev/null; then
  inval "A : entree dans EDIT" "aucun trafic : le geste de navigation n a pas ete injecte"
  W_A2_EDIT=0
elif [ "${A2_EDIT_AV:-0}" != "$TAB_SLOTS" ] || [ "${A2_EDIT_AP:-$TAB_SLOTS}" -ge "$TAB_SLOTS" ] 2>/dev/null; then
  inval "A : entree dans EDIT" "creneaux encres ${A2_EDIT_AV:-?} -> ${A2_EDIT_AP:-?} : EDIT n est pas etabli"
  W_A2_EDIT=0
else
  ok "A : entree dans EDIT" "$A2_EDIT_TWI octets, creneaux encres $A2_EDIT_AV -> $A2_EDIT_AP"
  W_A2_EDIT=1
fi

A2_BASE_MASK="$(a2 rA_base 3)"
A2_BASE_O6="$(a2 rA_base 5)"
A2_BASE_O8="$(a2 rA_base 7)"
A2_BASE_ECARTS="$(a2 rA_base 9)"
if [ -z "$A2_BASE_MASK" ]; then
  inval "A : etat initial" "aucune lecture d instances publiee"
  W_A2_BASE=0
elif [ "$A2_BASE_MASK" = "$RIG_MASQUE_BASE" ] && [ "$A2_BASE_O6" = "00" ] \
     && [ "$A2_BASE_O8" = "00" ] && [ "$A2_BASE_ECARTS" = "0" ]; then
  ok "A : etat initial" "masque $A2_BASE_MASK, ratchets a 00, 0 ecart avec l attendu du rig"
  W_A2_BASE=1
else
  inval "A : etat initial" "masque ${A2_BASE_MASK:-rien}, octets ${A2_BASE_O6:-?}/${A2_BASE_O8:-?}, ${A2_BASE_ECARTS:-?} ecarts"
  W_A2_BASE=0
fi

OVER_A2=0; MAX_A2=0; NSALVES_A2=0
while read -r CRANS HOLD; do
  [ -n "$CRANS" ] || continue
  NSALVES_A2=$((NSALVES_A2 + 1))
  [ "${CRANS:-0}" -gt "${CEIL_DETENTS:-0}" ] 2>/dev/null && OVER_A2=1
  OVER_A2=$(awk -v h="$HOLD" -v o="$OVER_A2" 'BEGIN { print (h >= 750) ? 1 : o }')
  MAX_A2=$(awk -v h="$HOLD" -v m="$MAX_A2" 'BEGIN { print (h > m) ? h : m }')
done <<EOF
$(grep -E '^shift_salve ' "$LOG4" | awk '{print $2, $5}')
EOF
if [ "$NSALVES_A2" -lt "$SALVES_MIN_A2" ]; then
  inval "A : salves mesurees" "$NSALVES_A2 salves, minimum $SALVES_MIN_A2 : rien a conclure"
  W_A2_SALVES=0
elif [ "$OVER_A2" = "0" ]; then
  ok "A : salves mesurees" "$NSALVES_A2 salves, maintien maximal $MAX_A2 ms, sous le seuil de 750 ms"
  W_A2_SALVES=1
else
  inval "A : salves mesurees" "maintien maximal $MAX_A2 ms ou salve hors plafond : injection invalide"
  W_A2_SALVES=0
fi

garde_pointeur "$LOG4" "A2 : temoin du pointeur" || W_A2_IMG=0
W_A2=$(( W_A2_IMG * W_A2_EDIT * W_A2_BASE * W_A2_SALVES * W_FACTORY ))
R8_OK=0
R9_OK=0

juge_a2() {
  local label="$1" cle="$2" champ="$3" attendu="$4" ecarts="$5" premier="$6" twi_champ="$7"
  local vu ec pr tw
  vu="$(a2 "$cle" "$champ")"; ec="$(a2 "$cle" "$ecarts")"
  tw="$(a2 "$cle" "$twi_champ")"
  pr="-"
  [ -n "$9" ] && pr="$(a2 "$cle" "$premier")"
  if [ -z "$vu" ] || [ -z "$ec" ]; then
    inval "$label" "aucun echantillon publie pour $cle"
    return 1
  fi
  if [ "${tw:-0}" -eq 0 ] 2>/dev/null; then
    inval "$label" "aucun trafic : geste NON INJECTE"
    return 1
  fi
  if [ "$vu" = "$attendu" ] && [ "$ec" = "$8" ] && { [ -z "$9" ] || [ "$pr" = "$9" ]; }; then
    ok "$label" "valeur $vu, $ec ecart(s) avec l attendu du rig, $tw octets"
    return 0
  fi
  bad "$label" "valeur $vu (attendu $attendu), $ec ecart(s) (attendu $8), premier octet ${pr:-?}"
  return 1
}

if [ "$W_A2" != "1" ]; then
  inval "R8 : bascule du step 3" "temoins amont invalides : l effet n est pas attribuable au firmware"
  inval "R8 : retour" "temoins amont invalides"
  inval "R9 : ratchet sur step 5 actif" "temoins amont invalides"
  inval "R9 : retour" "temoins amont invalides"
  inval "R10 : refus sur step inactif" "temoins amont invalides"
  inval "R12 : triolet sur step 9" "temoins amont invalides"
  inval "R12 : retour" "temoins amont invalides"
else
  juge_a2 "R8 : bascule du step 3" rA_r8_pose 3 "${EXPECT_R8_MASK:-$R8_MASK_ATTENDU}" 5 7 9 1 0 && R8_OK=1
  juge_a2 "R8 : retour" rA_r8_retour 3 "$RIG_MASQUE_BASE" 5 0 7 0 "" || R8_OK=0

  if [ "$R8_OK" != "1" ]; then
    inval "R9 : ratchet sur step 5 actif" "R8 n a pas etabli la correspondance rotations -> step"
    inval "R9 : retour" "R8 n a pas etabli la correspondance rotations -> step"
  else
    juge_a2 "R9 : ratchet sur step 5 actif" rA_r9_pose 3 "${EXPECT_R9_NIBBLE:-$R9_OCTET_ATTENDU}" 5 7 9 1 7 && R9_OK=1
    juge_a2 "R9 : retour" rA_r9_retour 3 "00" 5 0 7 0 "" || R9_OK=0
  fi

  if [ "$R8_OK" != "1" ] || [ "$R9_OK" != "1" ]; then
    inval "R10 : refus sur step inactif" "R8 et R9 n ont pas etabli la correspondance rotations -> step : le refus n est pas attribuable"
    inval "R12 : triolet sur step 9" "R8 et R9 n ont pas etabli la correspondance rotations -> step"
    inval "R12 : retour" "R8 et R9 n ont pas etabli la correspondance"
  else
    R10_CIBLE="$(a2 rA_r10 3)"; R10_ROT="$(a2 rA_r10 5)"
    R10_O6="$(a2 rA_r10 7)"; R10_EC="$(a2 rA_r10 9)"; R10_TWI="$(a2 rA_r10 11)"
    R10_OK=0
    if [ -z "$R10_EC" ]; then
      inval "R10 : refus sur step inactif" "aucun echantillon publie"
    elif [ "${R10_TWI:-0}" -eq 0 ] 2>/dev/null; then
      inval "R10 : refus sur step inactif" "aucun trafic : un refus et un geste non injecte seraient indistinguables"
    elif [ "$R10_EC" = "0" ] && [ "$R10_O6" = "00" ]; then
      ok "R10 : refus sur step inactif" "step $R10_CIBLE atteint en $R10_ROT rotations, $R10_TWI octets, instances INCHANGEES"
      R10_OK=1
    else
      bad "R10 : refus sur step inactif" "step $R10_CIBLE : octet6 $R10_O6, $R10_EC ecart(s) — un ratchet a ete ecrit"
    fi
    if [ "$R10_OK" != "1" ]; then
      inval "R12 : triolet sur step 9" "R10 a laisse les instances hors de l etat du rig : l effet n est pas attribuable"
      inval "R12 : retour" "R10 a laisse les instances hors de l etat du rig"
    else
      juge_a2 "R12 : triolet sur step 9" rA_r12_pose 3 "${EXPECT_R12_NIBBLE:-$R12_OCTET_ATTENDU}" 5 7 9 1 9
      juge_a2 "R12 : retour" rA_r12_retour 3 "00" 5 0 7 0 ""
    fi
  fi
fi

LOG5="$WORK/log5"
progress "recettes de verification B : R1 et R13 (P2.6.3)"
"$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$INSTANCE_PTR_ADDR" "$BOOT_MS" \
     "$WORK/rig.bin" 384 recettesB "$SUPPRESSED_ADDR" > "$LOG5" 2>&1
RC=$?
if [ "$RC" != "0" ]; then
  cat "$LOG5"
  case "$RC" in
    3) dieinval "$LOG5" "controle des instances en echec sur les recettes B, aucun verdict sur le firmware" ;;
    4) die "INVALID (classe 2) : salve de SHIFT refusee avant injection, le geste n etait pas valide" 4 ;;
    *) die "la phase des recettes B s'est terminee anormalement (code $RC)" ;;
  esac
fi

printf '\n%s--- RECETTES DE VERIFICATION B : R1 et R13 (P2.6.3) ---%s\n' "$C_B" "$C_0"
grep -E '^controle_source|^inst_attendus|^image_lue|^controle_usine|^rB_' "$LOG5" | sed 's/^/  /'
printf '\n'

B_SRC="$(grep -E '^controle_source ' "$LOG5" | awk '{print $2}')"
B_CTRL="$(grep -E '^controle_usine ' "$LOG5" | awk '{print $2}')"
B_MASK="$(grep -E '^image_lue ' "$LOG5" | awk '{print $3}')"
B_SUB="$(grep -E '^image_lue ' "$LOG5" | awk '{print $5}')"
B_ACTIFS="$(grep -E '^rB_steps_actifs ' "$LOG5" | awk '{print $2}')"

if [ "$B_MASK" != "$RIG_MASK_ATTENDU" ] || [ "$B_SUB" != "$RIG_SUBDIV_ATTENDU" ]; then
  inval "B : image du rig" "masque ${B_MASK:-rien} subdiv ${B_SUB:-rien} : ce n est pas le rig de la couche 1"
  W_B3_IMG=0
elif [ "$B_SRC" = "image" ] && [ "$B_CTRL" = "1" ] && [ "$B_ACTIFS" = "$B_STEPS_ACTIFS_ATTENDU" ]; then
  ok "B : image du rig" "masque $B_MASK subdiv $B_SUB, $B_ACTIFS steps actifs derives de l image, instances identiques a l attendu"
  W_B3_IMG=1
else
  inval "B : image du rig" "controle ${B_CTRL:-rien}, steps actifs ${B_ACTIFS:-rien} (attendu $B_STEPS_ACTIFS_ATTENDU)"
  W_B3_IMG=0
fi

R1_NAV_OK=1; R1_CIBLE_OK=1; R1_CONTAG_OK=1; R1_REST_OK=1; R1_RETOUR_OK=1; R1_ECH_OK=1
R1_NAV_D=""; R1_CIBLE_D=""; R1_CONTAG_D=""; R1_REST_D=""; R1_RETOUR_D=""; R1_ECH_D=""
R1_LIGNES=0
for k in 1 2 3 4 5 6; do
  nav="$(grep -E "^rB_r1_nav +k $k " "$LOG5" | head -1)"
  chg="$(grep -E "^rB_r1_change +k $k " "$LOG5" | head -1)"
  res="$(grep -E "^rB_r1_restaure +k $k " "$LOG5" | head -1)"
  ret="$(grep -E "^rB_r1_retour +k $k " "$LOG5" | head -1)"
  if [ -z "$nav" ] || [ -z "$chg" ] || [ -z "$res" ] || [ -z "$ret" ]; then
    R1_ECH_OK=0; R1_ECH_D="$R1_ECH_D k$k:ligne-absente"; continue
  fi
  R1_LIGNES=$((R1_LIGNES + 1))
  barre="$(printf '%s' "$nav" | awk '{print $5}')"
  dedans="$(printf '%s' "$nav" | awk '{print $7}')"
  navtwi="$(printf '%s' "$nav" | awk '{print $9}')"
  { [ "$barre" = "$k" ] && [ "$dedans" = "$k" ] && [ "${navtwi:-0}" -gt 0 ] 2>/dev/null; } \
    || { R1_NAV_OK=0; R1_NAV_D="$R1_NAV_D k$k:barre=$barre,dedans=$dedans,twi=$navtwi"; }

  chgtwi="$(printf '%s' "$chg" | awk '{print $5}')"
  [ "${chgtwi:-0}" -gt 0 ] 2>/dev/null || { R1_ECH_OK=0; R1_ECH_D="$R1_ECH_D k$k:geste-sans-trafic"; }
  dist="$(printf '%s' "$chg" | awk '{print $14}')"
  retn="$(printf '%s' "$chg" | awk '{print $16}')"
  { [ "${dist:-0}" -ge "$B_DIST_MIN" ] && [ "${retn:-0}" -ge "$B_RET_MIN" ]; } 2>/dev/null \
    || { R1_ECH_OK=0; R1_ECH_D="$R1_ECH_D k$k:distances=$dist,retenues=$retn"; }

  cible="${EXPECT_R1_CHANNEL:-$k}"
  for o in 1 2 3 4 5 6; do
    v="$(printf '%s' "$chg" | awk -v c=$((7 + o - 1)) '{print $c}')"
    if [ "$o" = "$cible" ]; then
      [ "$v" = "${EXPECT_R1_PAS:-$B_PAS_CHANGE}" ] \
        || { R1_CIBLE_OK=0; R1_CIBLE_D="$R1_CIBLE_D k$k:OUT$o=$v"; }
    else
      [ "$v" = "$B_PAS_BASE" ] \
        || { R1_CONTAG_OK=0; R1_CONTAG_D="$R1_CONTAG_D k$k:OUT$o=$v"; }
    fi
  done

  for o in 1 2 3 4 5 6; do
    v="$(printf '%s' "$res" | awk -v c=$((7 + o - 1)) '{print $c}')"
    [ "$v" = "$B_PAS_BASE" ] || { R1_REST_OK=0; R1_REST_D="$R1_REST_D k$k:OUT$o=$v"; }
  done

  rong="$(printf '%s' "$ret" | awk '{print $5}')"
  rcre="$(printf '%s' "$ret" | awk '{print $7}')"
  { [ "$rong" = "$k" ] && [ "$rcre" = "$TAB_COUNT_ECRAN" ]; } \
    || { R1_RETOUR_OK=0; R1_RETOUR_D="$R1_RETOUR_D k$k:onglet=$rong,creneaux=$rcre"; }
done

if [ "$R1_LIGNES" -ne 6 ] || [ "$R1_ECH_OK" != "1" ]; then
  inval "R1 : echantillons" "$R1_LIGNES onglets mesures sur 6 —$R1_ECH_D"
  W_R1_ECH=0
else
  ok "R1 : echantillons" "6 onglets, chaque geste avec trafic, au moins $B_DIST_MIN distances et $B_RET_MIN retenues par sortie"
  W_R1_ECH=1
fi
garde_pointeur "$LOG5" "B3 : temoin du pointeur" || W_B3_IMG=0
if [ "$W_R1_ECH" != "1" ] || [ "$W_B3_IMG" != "1" ]; then
  inval "R1 : navigation" "echantillons ou image invalides"
  W_R1_NAV=0
elif [ "$R1_NAV_OK" = "1" ]; then
  ok "R1 : navigation" "les six onglets atteints et confirmes a l ecran, sur la barre puis dedans"
  W_R1_NAV=1
else
  bad "R1 : navigation" "onglet non confirme —$R1_NAV_D"
  W_R1_NAV=0
fi

R1_OK=0
if [ "$W_R1_NAV" != "1" ]; then
  inval "R1 : la sortie visee change" "navigation non etablie"
  inval "R1 : non-contagion" "navigation non etablie"
  inval "R1 : restauration" "navigation non etablie"
  inval "R1 : retour a la barre" "navigation non etablie"
else
  CIBLE_OK=0
  if [ "$R1_CIBLE_OK" = "1" ]; then
    ok "R1 : la sortie visee change" "OUT(k+1) passe de $B_PAS_BASE a ${EXPECT_R1_PAS:-$B_PAS_CHANGE} ticks, pour k = 1 a 6"
    CIBLE_OK=1
  else
    bad "R1 : la sortie visee change" "pas attendu ${EXPECT_R1_PAS:-$B_PAS_CHANGE} —$R1_CIBLE_D"
  fi
  CONTAG_OK=0
  if [ "$R1_CONTAG_OK" = "1" ]; then
    ok "R1 : non-contagion" "les cinq autres sorties restent a $B_PAS_BASE ticks, pour les six onglets"
    CONTAG_OK=1
  else
    bad "R1 : non-contagion" "une autre sortie a change —$R1_CONTAG_D"
  fi
  REST_OK=0
  if [ "$R1_REST_OK" = "1" ]; then
    ok "R1 : restauration" "les six sorties reviennent a $B_PAS_BASE ticks apres le cran inverse"
    REST_OK=1
  else
    bad "R1 : restauration" "restauration incomplete —$R1_REST_D"
  fi
  RET_OK=0
  if [ "$R1_RETOUR_OK" = "1" ]; then
    ok "R1 : retour a la barre" "appui long : onglet conserve et $TAB_COUNT_ECRAN creneaux encres, pour les six"
    RET_OK=1
  else
    bad "R1 : retour a la barre" "retour incorrect —$R1_RETOUR_D"
  fi
  [ "$CIBLE_OK" = "1" ] && [ "$CONTAG_OK" = "1" ] && [ "$REST_OK" = "1" ] && [ "$RET_OK" = "1" ] && R1_OK=1
fi

R13_NAV="$(grep -E '^rB_r13_nav ' "$LOG5" | head -1)"
R13_CHG="$(grep -E '^rB_r13_change ' "$LOG5" | head -1)"
R13_RES="$(grep -E '^rB_r13_restaure ' "$LOG5" | head -1)"
if [ -z "$R13_NAV" ] || [ -z "$R13_CHG" ] || [ -z "$R13_RES" ]; then
  inval "R13 : composition" "une ligne de mesure manque : rien a conclure"
elif [ "$R1_OK" != "1" ]; then
  inval "R13 : composition" "R1 n a pas etabli la carte onglet -> sortie : l effet n est pas attribuable"
else
  R13_ONSETS="$(printf '%s' "$R13_CHG" | awk '{print $17}')"
  R13_CUR="$(printf '%s' "$R13_CHG" | awk '{print $19}')"
  R13_TWI="$(printf '%s' "$R13_CHG" | awk '{print $7}')"
  R13_TWIR="$(printf '%s' "$R13_CHG" | awk '{print $8}')"
  R13_ONG="$(printf '%s' "$R13_CHG" | awk '{print $3}')"
  R13_CRE="$(printf '%s' "$R13_CHG" | awk '{print $5}')"
  ONSETS_MIN=$(( 2 * B_STEPS_ACTIFS_ATTENDU ))
  if [ "${R13_TWI:-0}" -eq 0 ] 2>/dev/null || [ "${R13_TWIR:-0}" -eq 0 ] 2>/dev/null; then
    inval "R13 : composition" "geste sans trafic : NON INJECTE"
  elif [ "${R13_ONSETS:-0}" -lt "$ONSETS_MIN" ] 2>/dev/null; then
    inval "R13 : composition" "${R13_ONSETS:-0} onsets, minimum $ONSETS_MIN soit deux cycles : rien a conclure"
  elif [ "$R13_ONG" != "$R13_ONGLET" ] || [ "$R13_CRE" != "$TAB_COUNT_ECRAN" ] \
       || [ "${R13_CUR:-x}" != "$CURSEUR_BARRE" ]; then
    inval "R13 : composition" "apres le retour : onglet $R13_ONG, $R13_CRE creneaux, curseur ${R13_CUR:-rien} — l etat d interface n est pas etabli"
  else
    R13_PER_OK=1; R13_PER_D=""
    for o in 1 2 3 4 5 6; do
      v="$(printf '%s' "$R13_CHG" | awk -v c=$((10 + o - 1)) '{print $c}')"
      if [ "$o" = "$R13_ONGLET" ]; then
        [ "$v" = "${EXPECT_R13_PERIODE:-$R13_PERIODE_CHANGE_ATTENDUE}" ] \
          || { R13_PER_OK=0; R13_PER_D="$R13_PER_D OUT$o=$v"; }
      else
        [ "$v" = "$R13_PERIODE_BASE" ] || { R13_PER_OK=0; R13_PER_D="$R13_PER_D OUT$o=$v"; }
      fi
    done
    if [ "$R13_PER_OK" = "1" ]; then
      ok "R13 : composition" "onglet $R13_ONG retrouve, $R13_CRE creneaux, OUT$R13_ONGLET a ${EXPECT_R13_PERIODE:-$R13_PERIODE_CHANGE_ATTENDUE} ticks, les cinq autres a $R13_PERIODE_BASE"
    else
      bad "R13 : composition" "periodes —$R13_PER_D (attendu OUT$R13_ONGLET=${EXPECT_R13_PERIODE:-$R13_PERIODE_CHANGE_ATTENDUE}, autres $R13_PERIODE_BASE)"
    fi
    R13_REST_OK=1; R13_REST_D=""
    for o in 1 2 3 4 5 6; do
      v="$(printf '%s' "$R13_RES" | awk -v c=$((10 + o - 1)) '{print $c}')"
      [ "$v" = "$R13_PERIODE_BASE" ] || { R13_REST_OK=0; R13_REST_D="$R13_REST_D OUT$o=$v"; }
    done
    if [ "$R13_REST_OK" = "1" ]; then
      ok "R13 : restauration" "les six sorties reviennent a $R13_PERIODE_BASE ticks"
    else
      bad "R13 : restauration" "restauration incomplete —$R13_REST_D"
    fi
  fi
fi

LOG6="$WORK/log6"
progress "recette R2 (P2.6.4)"
"$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$INSTANCE_PTR_ADDR" "$BOOT_MS" \
     "$WORK/rig.bin" 384 recetteR2 "$SUPPRESSED_ADDR" > "$LOG6" 2>&1
RC=$?
if [ "$RC" != "0" ]; then
  cat "$LOG6"
  case "$RC" in
    3) dieinval "$LOG6" "controle des instances en echec sur R2, aucun verdict sur le firmware" ;;
    4) die "INVALID (classe 2) : salve de SHIFT refusee avant injection, le geste n etait pas valide" 4 ;;
    *) die "la phase R2 s'est terminee anormalement (code $RC)" ;;
  esac
fi

printf '\n%s--- RECETTE R2 : LA DISTANCE ET LE CHAMP (P2.6.4) ---%s\n' "$C_B" "$C_0"
grep -E '^controle_source|^inst_attendus|^image_lue|^controle_usine|^rC_' "$LOG6" | sed 's/^/  /'
printf '\n'

c2()  { grep -E "^rC_$1 " "$LOG6" | head -1 | awk "{print \$$2}"; }
c2o() { grep -E "^rC_$1_OUT$2 " "$LOG6" | head -1 | awk "{print \$$3}"; }

R2_SRC="$(grep -E '^controle_source ' "$LOG6" | awk '{print $2}')"
R2_CTRL="$(grep -E '^controle_usine ' "$LOG6" | awk '{print $2}')"
R2_MASK="$(grep -E '^image_lue ' "$LOG6" | awk '{print $3}')"
R2_SUB="$(grep -E '^image_lue ' "$LOG6" | awk '{print $5}')"
R2_ACTIFS="$(grep -E '^rC_steps_actifs ' "$LOG6" | awk '{print $2}')"
if [ "$R2_MASK" != "$RIG_MASK_ATTENDU" ] || [ "$R2_SUB" != "$RIG_SUBDIV_ATTENDU" ] \
   || [ "$R2_ACTIFS" != "$B_STEPS_ACTIFS_ATTENDU" ]; then
  inval "R2 : image du rig" "masque ${R2_MASK:-rien} subdiv ${R2_SUB:-rien} steps ${R2_ACTIFS:-rien} : ce n est pas le rig"
  W_R2_IMG=0
elif [ "$R2_SRC" = "image" ] && [ "$R2_CTRL" = "1" ]; then
  ok "R2 : image du rig" "masque $R2_MASK subdiv $R2_SUB, $R2_ACTIFS steps actifs, instances identiques a l attendu"
  W_R2_IMG=1
else
  inval "R2 : image du rig" "controle ${R2_CTRL:-rien}"
  W_R2_IMG=0
fi

R2_ECH_OK=1; R2_ECH_D=""
R2_ONSETS_MIN=$(( 2 * B_STEPS_ACTIFS_ATTENDU ))
for st in base length lenrest subdiv subrest; do
  [ -n "$(c2 "$st" 3)" ] || { R2_ECH_OK=0; R2_ECH_D="$R2_ECH_D $st:entete-absente"; continue; }
  case "$st" in
    base) ;;
    *) [ "$(c2 "$st" 5)" -gt 0 ] 2>/dev/null \
         || { R2_ECH_OK=0; R2_ECH_D="$R2_ECH_D $st:geste-sans-trafic"; } ;;
  esac
  for o in 1 2 3 4 5 6; do
    d="$(c2o "$st" $o 7)"; r="$(c2o "$st" $o 9)"; on="$(c2o "$st" $o 11)"; pa="$(c2o "$st" $o 3)"
    { [ "${d:-0}" -ge "$B_DIST_MIN" ] && [ "${r:-0}" -ge "$B_RET_MIN" ] \
      && [ "${on:-0}" -ge "$R2_ONSETS_MIN" ] && [ "${pa:-0}" -gt 0 ]; } 2>/dev/null \
      || { R2_ECH_OK=0; R2_ECH_D="$R2_ECH_D $st/OUT$o:d=$d,r=$r,onsets=$on,pas=$pa"; }
  done
done
if [ "$R2_ECH_OK" = "1" ]; then
  ok "R2 : echantillons" "5 etapes x 6 sorties : au moins $B_DIST_MIN distances, $B_RET_MIN retenues, $R2_ONSETS_MIN onsets, et un pas non nul"
  W_R2_ECH=1
else
  inval "R2 : echantillons" "plancher non atteint —$R2_ECH_D"
  W_R2_ECH=0
fi

R2_BASE_OK=1; R2_BASE_D=""
for o in 1 2 3 4 5 6; do
  [ "$(c2o base $o 3)" = "$R2_PAS_BASE" ] && [ "$(c2o base $o 5)" = "$R2_PERIODE_BASE" ] \
    || { R2_BASE_OK=0; R2_BASE_D="$R2_BASE_D OUT$o=$(c2o base $o 3)/$(c2o base $o 5)"; }
done
if [ "$R2_BASE_OK" = "1" ] && [ "$(c2 base 7)" = "0" ]; then
  ok "R2 : etat initial" "les six sorties a $R2_PAS_BASE / $R2_PERIODE_BASE ticks, instances a 0 ecart"
  W_R2_BASE=1
else
  inval "R2 : etat initial" "etat de depart non etabli —$R2_BASE_D ecarts=$(c2 base 7)"
  W_R2_BASE=0
fi

garde_pointeur "$LOG6" "R2 : temoin du pointeur" || W_R2_IMG=0
W_R2=$(( W_R2_IMG * W_R2_ECH * W_R2_BASE * R1_OK ))
if [ "$W_R2" != "1" ]; then
  inval "R2 : une rotation touche LENGTH" "temoins amont invalides (dont R1) : l effet n est pas attribuable"
  inval "R2 : deux rotations touchent SUBDIV" "temoins amont invalides"
  inval "R2 : LENGTH inchangee sous SUBDIV" "temoins amont invalides"
  inval "R2 : non-contagion" "temoins amont invalides"
  inval "R2 : pattern intact" "temoins amont invalides"
  inval "R2 : restaurations" "temoins amont invalides"
  inval "R2 : retour a la barre" "temoins amont invalides"
else
  CIB="${EXPECT_R2_CHANNEL:-$R2_ONGLET}"
  LPAS="$(c2o length $CIB 3)"; LPER="$(c2o length $CIB 5)"
  LLEN=$(( LPER / LPAS ))
  if [ "$LPAS" = "$R2_PAS_BASE" ] && [ "$LLEN" = "$R2_LENGTH_APRES_ATTENDUE" ]; then
    ok "R2 : une rotation touche LENGTH" "OUT$CIB : pas inchange a $LPAS, periode $LPER, donc LENGTH $R2_LENGTH_BASE -> $LLEN"
  else
    bad "R2 : une rotation touche LENGTH" "OUT$CIB : pas $LPAS, periode $LPER, LENGTH deduite $LLEN (attendu pas $R2_PAS_BASE et LENGTH $R2_LENGTH_APRES_ATTENDUE)"
  fi

  SPAS="$(c2o subdiv $CIB 3)"; SPER="$(c2o subdiv $CIB 5)"
  SLEN=$(( SPER / SPAS ))
  if [ "$SPAS" = "${EXPECT_R2_PAS:-$R2_PAS_SUBDIV_ATTENDU}" ]; then
    ok "R2 : deux rotations touchent SUBDIV" "OUT$CIB : pas $R2_PAS_BASE -> $SPAS ticks"
  else
    bad "R2 : deux rotations touchent SUBDIV" "OUT$CIB : pas $SPAS (attendu ${EXPECT_R2_PAS:-$R2_PAS_SUBDIV_ATTENDU})"
  fi
  if [ "$SLEN" = "${EXPECT_R2_LENGTH:-$R2_LENGTH_BASE}" ]; then
    ok "R2 : LENGTH inchangee sous SUBDIV" "OUT$CIB : periode $SPER / pas $SPAS = LENGTH $SLEN, inchangee"
  else
    bad "R2 : LENGTH inchangee sous SUBDIV" "OUT$CIB : LENGTH deduite $SLEN (attendu ${EXPECT_R2_LENGTH:-$R2_LENGTH_BASE}) — le geste a touche un autre champ"
  fi

  R2_CON_OK=1; R2_CON_D=""
  for st in base length lenrest subdiv subrest; do
    for o in 1 2 3 4 5 6; do
      [ "$o" = "$CIB" ] && continue
      [ "$(c2o "$st" $o 3)" = "$R2_PAS_BASE" ] && [ "$(c2o "$st" $o 5)" = "$R2_PERIODE_BASE" ] \
        || { R2_CON_OK=0; R2_CON_D="$R2_CON_D $st/OUT$o=$(c2o "$st" $o 3)/$(c2o "$st" $o 5)"; }
    done
  done
  [ "$R2_CON_OK" = "1" ] \
    && ok "R2 : non-contagion" "les cinq autres sorties restent a $R2_PAS_BASE / $R2_PERIODE_BASE aux cinq etapes" \
    || bad "R2 : non-contagion" "une autre sortie a change —$R2_CON_D"

  R2_PAT_OK=1; R2_PAT_D=""
  for st in base length lenrest subdiv subrest; do
    [ "$(c2 "$st" 7)" = "0" ] || { R2_PAT_OK=0; R2_PAT_D="$R2_PAT_D $st=$(c2 "$st" 7)"; }
  done
  [ "$R2_PAT_OK" = "1" ] \
    && ok "R2 : pattern intact" "0 ecart avec l attendu du rig aux cinq etapes : aucun step ni ratchet touche" \
    || bad "R2 : pattern intact" "les instances ont change —$R2_PAT_D ecart(s)"

  R2_RES_OK=1; R2_RES_D=""
  for st in lenrest subrest; do
    for o in 1 2 3 4 5 6; do
      [ "$(c2o "$st" $o 3)" = "$R2_PAS_BASE" ] && [ "$(c2o "$st" $o 5)" = "$R2_PERIODE_BASE" ] \
        || { R2_RES_OK=0; R2_RES_D="$R2_RES_D $st/OUT$o=$(c2o "$st" $o 3)/$(c2o "$st" $o 5)"; }
    done
  done
  [ "$R2_RES_OK" = "1" ] \
    && ok "R2 : restaurations" "apres chaque cran inverse, les six sorties reviennent a $R2_PAS_BASE / $R2_PERIODE_BASE" \
    || bad "R2 : restaurations" "restauration incomplete —$R2_RES_D"

  RB_ONG="$(grep -E '^rC_retour_barre ' "$LOG6" | awk '{print $3}')"
  RB_CRE="$(grep -E '^rC_retour_barre ' "$LOG6" | awk '{print $5}')"
  RB_TWI="$(grep -E '^rC_retour_barre ' "$LOG6" | awk '{print $7}')"
  RB_CUR="$(grep -E '^rC_retour_barre ' "$LOG6" | awk '{print $9}')"
  if [ -z "$RB_ONG" ] || [ -z "$RB_CUR" ]; then
    inval "R2 : retour a la barre" "aucune mesure de retour publiee"
  elif [ "${RB_TWI:-0}" -eq 0 ] 2>/dev/null; then
    inval "R2 : retour a la barre" "aucun trafic : appui long NON INJECTE"
  elif [ "$RB_ONG" = "$R2_ONGLET" ] && [ "$RB_CRE" = "$TAB_COUNT_ECRAN" ] \
       && [ "$RB_CUR" = "$CURSEUR_BARRE" ]; then
    ok "R2 : retour a la barre" "onglet $RB_ONG conserve, $RB_CRE creneaux, aucune ligne surlignee : la BARRE, et pas l interieur d un onglet"
  else
    bad "R2 : retour a la barre" "onglet $RB_ONG, $RB_CRE creneaux, curseur $RB_CUR (barre = $CURSEUR_BARRE)"
  fi
fi

LOG8b="$WORK/log8b"
progress "recette MOD (P2.6.6)"
CV1_MV="$MOD_CV1_MV" "$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" \
     "$INSTANCE_PTR_ADDR" "$BOOT_MS" "$WORK/rig.bin" 384 recetteMOD \
     "$SUPPRESSED_ADDR" > "$LOG8b" 2>&1
RC=$?
if [ "$RC" != "0" ]; then
  cat "$LOG8b"
  die "la phase MOD s'est terminee anormalement (code $RC)"
fi

printf '\n%s--- RECETTE MOD : LE ROUTAGE POSE PAR UN GESTE (P2.6.6) ---%s\n' "$C_B" "$C_0"
grep -E '^rM_' "$LOG8b" | sed 's/^/  /'
printf '\n'

MOD_INJ="$(grep -E '^rM_injection ' "$LOG8b" | awk '{print $3}')"
MOD_AV="$(grep -E '^rM_avant ' "$LOG8b" | awk '{print $3}')"
MOD_AP="$(grep -E '^rM_apres ' "$LOG8b" | awk '{print $3}')"
MOD_TWI="$(grep -E '^rM_geste ' "$LOG8b" | awk '{print $3}')"
MOD_W=1
if [ "${MOD_INJ:-0}" != "1" ]; then
  inval "MOD : injection du CV" "aucune injection : le routage ne pourrait rien changer"
  MOD_W=0
elif [ "$MOD_AV" != "$MOD_PERIODE_BASE" ]; then
  inval "MOD : etat de depart" "periode $MOD_AV au lieu de $MOD_PERIODE_BASE : le canal ne joue pas sa longueur de base"
  MOD_W=0
elif [ "${MOD_TWI:-0}" -eq 0 ] 2>/dev/null; then
  inval "MOD : geste injecte" "aucun trafic : l interface n a jamais vu le geste"
  MOD_W=0
else
  ok "MOD : etat de depart" "periode $MOD_AV ticks sans routage, et le CV est injecte : il n agit pas encore"
fi

if [ "$MOD_W" != "1" ]; then
  inval "MOD : le geste route le CV" "temoins amont invalides"
elif [ "$MOD_AP" = "$MOD_PERIODE_ROUTEE" ]; then
  ok "MOD : le geste route le CV" "periode $MOD_AV -> $MOD_AP ticks : deux crans sur MOD ont mis CV1 sur LENGTH, et les broches le montrent"
else
  bad "MOD : le geste route le CV" "periode $MOD_AP au lieu de $MOD_PERIODE_ROUTEE apres le geste"
fi

LOG7="$WORK/log7"
progress "recette R11 (P2.6.5)"
"$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$INSTANCE_PTR_ADDR" "$BOOT_MS" \
     "$WORK/rig.bin" 384 recetteR11 "$SUPPRESSED_ADDR" > "$LOG7" 2>&1
RC=$?
if [ "$RC" != "0" ]; then
  cat "$LOG7"
  case "$RC" in
    3) dieinval "$LOG7" "controle des instances en echec sur R11, aucun verdict sur le firmware" ;;
    4) die "INVALID (classe 2) : salve de SHIFT refusee avant injection, le geste n etait pas valide" 4 ;;
    *) die "la phase R11 s'est terminee anormalement (code $RC)" ;;
  esac
fi

printf '\n%s--- RECETTE R11 : LES CODES REFUSES A x24 (P2.6.5) ---%s\n' "$C_B" "$C_0"
grep -E '^controle_source|^inst_attendus|^image_lue|^controle_usine|^rD_' "$LOG7" | sed 's/^/  /'
printf '\n'

d2() { grep -E "^rD_$1 " "$LOG7" | head -1 | awk "{print \$$2}"; }

D_SRC="$(grep -E '^controle_source ' "$LOG7" | awk '{print $2}')"
D_CTRL="$(grep -E '^controle_usine ' "$LOG7" | awk '{print $2}')"
D_MASK="$(grep -E '^image_lue ' "$LOG7" | awk '{print $3}')"
D_SUB="$(grep -E '^image_lue ' "$LOG7" | awk '{print $5}')"
if [ "$D_MASK" != "$RIG_MASK_ATTENDU" ] || [ "$D_SUB" != "$RIG_SUBDIV_ATTENDU" ]; then
  inval "R11 : image du rig" "masque ${D_MASK:-rien} subdiv ${D_SUB:-rien} : ce n est pas le rig"
  W_R11_IMG=0
elif [ "$D_SRC" = "image" ] && [ "$D_CTRL" = "1" ]; then
  ok "R11 : image du rig" "masque $D_MASK subdiv $D_SUB, instances identiques a l attendu"
  W_R11_IMG=1
else
  inval "R11 : image du rig" "controle ${D_CTRL:-rien}"
  W_R11_IMG=0
fi

W_R11_ECH_DESC=1; W_R11_ECH_AVAL=1; D_ECH_D=""
for f in base palier x24; do
  dd="$(d2 "$f" 5)"; rr="$(d2 "$f" 7)"
  { [ "${dd:-0}" -ge "$B_DIST_MIN" ] && [ "${rr:-0}" -ge "$B_RET_MIN" ]; } 2>/dev/null \
    || { W_R11_ECH_DESC=0; D_ECH_D="$D_ECH_D $f:distances=$dd,retenues=$rr"; }
done
for g in nav_subdiv x24; do
  case "$g" in
    nav_subdiv) tw="$(d2 "$g" 5)" ;;
    *)          tw="$(d2 "$g" 11)" ;;
  esac
  [ "${tw:-0}" -gt 0 ] 2>/dev/null || { W_R11_ECH_DESC=0; D_ECH_D="$D_ECH_D $g:sans-trafic"; }
done
dd="$(d2 cadence_fin 5)"; rr="$(d2 cadence_fin 7)"
{ [ "${dd:-0}" -ge "$B_DIST_MIN" ] && [ "${rr:-0}" -ge "$B_RET_MIN" ]; } 2>/dev/null \
  || { W_R11_ECH_AVAL=0; D_ECH_D="$D_ECH_D cadence_fin:distances=$dd,retenues=$rr"; }
for g in cran1 cran2 cran3 retour; do
  case "$g" in
    retour) tw="$(d2 "$g" 9)" ;;
    *)      tw="$(d2 "$g" 11)" ;;
  esac
  [ "${tw:-0}" -gt 0 ] 2>/dev/null || { W_R11_ECH_AVAL=0; D_ECH_D="$D_ECH_D $g:sans-trafic"; }
done
NAVED_TWI="$(grep -E '^rD_nav_edit ' "$LOG7" | awk '{print $6}')"
[ "${NAVED_TWI:-0}" -gt 0 ] 2>/dev/null || { W_R11_ECH_AVAL=0; D_ECH_D="$D_ECH_D nav_edit:sans-trafic"; }
if [ "$W_R11_ECH_DESC" = "1" ] && [ "$W_R11_ECH_AVAL" = "1" ]; then
  ok "R11 : echantillons" "planchers atteints sur les quatre fenetres, trafic present sur les sept gestes"
else
  inval "R11 : echantillons" "plancher ou trafic manquant —$D_ECH_D (descente $W_R11_ECH_DESC, aval $W_R11_ECH_AVAL)"
fi

if [ "$(d2 base 3)" = "$R11_CADENCE_BASE" ] && [ "$(d2 base 9)" = "0" ]; then
  ok "R11 : etat initial" "cadence $(d2 base 3) ticks, instances a 0 ecart"
  W_R11_BASE=1
else
  inval "R11 : etat initial" "cadence $(d2 base 3), ecarts $(d2 base 9) : etat de depart non etabli"
  W_R11_BASE=0
fi

NAVE_AV="$(grep -E '^rD_nav_edit ' "$LOG7" | awk '{print $3}')"
NAVE_AP="$(grep -E '^rD_nav_edit ' "$LOG7" | awk '{print $4}')"
if [ "${NAVE_AV:-0}" = "$TAB_SLOTS" ] && [ "${NAVE_AP:-$TAB_SLOTS}" -lt "$TAB_SLOTS" ] 2>/dev/null; then
  ok "R11 : entree dans EDIT" "creneaux encres $NAVE_AV -> $NAVE_AP, $NAVED_TWI octets"
  W_R11_EDIT=1
else
  inval "R11 : entree dans EDIT" "creneaux ${NAVE_AV:-?} -> ${NAVE_AP:-?} : EDIT n est pas etabli"
  W_R11_EDIT=0
fi

garde_pointeur "$LOG7" "R11 : temoin du pointeur" || W_R11_IMG=0
W_R11_DESC=$(( W_R11_IMG * W_R11_ECH_DESC * W_R11_BASE ))
W_R11=$(( W_R11_DESC * W_R11_ECH_AVAL * W_R11_EDIT ))

D_PAL="$(d2 palier 3)"
D_PAL_ATTENDU="${EXPECT_R11_CADENCE_PALIER:-$R11_CADENCE_PALIER_ATTENDUE}"
if [ "$W_R11_DESC" != "1" ]; then
  inval "R11 : palier interieur" "temoins amont invalides"
  W_R11_PAL=0
elif [ "$D_PAL" = "$D_PAL_ATTENDU" ]; then
  ok "R11 : palier interieur" "$D_PAL ticks par step apres $(d2 nav_subdiv 9) crans : un cran de plus ou de moins donnerait une autre cadence"
  W_R11_PAL=1
else
  bad "R11 : palier interieur" "cadence $D_PAL au lieu de $D_PAL_ATTENDU apres $(d2 nav_subdiv 9) crans : la descente n a pas compte ses crans"
  W_R11_PAL=0
fi

D_CAD="$(d2 x24 3)"
if [ "$W_R11_DESC" != "1" ]; then
  inval "R11 : cadence x24" "temoins amont invalides"
  W_R11_CAD=0
elif [ "$W_R11_PAL" != "1" ]; then
  inval "R11 : cadence x24" "le palier interieur n est pas etabli : le point de depart du dernier cran est inconnu"
  W_R11_CAD=0
elif [ "$D_CAD" = "${EXPECT_R11_CADENCE:-$R11_CADENCE_X24_ATTENDUE}" ]; then
  ok "R11 : cadence x24" "$D_CAD ticks par step, un cran depuis le palier, mesures sur OUT4 AVANT toute edition de ratchet"
  W_R11_CAD=1
else
  inval "R11 : cadence x24" "cadence $D_CAD au lieu de ${EXPECT_R11_CADENCE:-$R11_CADENCE_X24_ATTENDUE} : la premisse « incompatible a x24 » n est pas etablie"
  W_R11_CAD=0
fi

if [ "$W_R11" != "1" ]; then
  inval "R11 : premier cran" "temoins amont invalides"
  inval "R11 : le triolet est retenu" "temoins amont invalides"
  inval "R11 : plafond du choix" "temoins amont invalides"
  inval "R11 : les codes refuses n apparaissent jamais" "temoins amont invalides"
  inval "R11 : retour a 00" "temoins amont invalides"
  inval "R11 : cadence restauree" "temoins amont invalides"
else
  N1="$(d2 cran1 3)"; O1="$(d2 cran1 5)"; E1="$(d2 cran1 7)"; P1="$(d2 cran1 9)"
  C1="$(d2 cran1 13)"; B1="$(d2 cran1 15)"
  if [ "$N1" = "${EXPECT_R11_NIBBLE_1:-$R11_NIBBLE_1_ATTENDU}" ] && [ "$E1" = "1" ] \
     && [ "$C1" = "$R11_CANAL_ATTENDU" ] && [ "$B1" = "$R11_OCTET_ATTENDU" ]; then
    ok "R11 : premier cran" "nibble $N1 (octet 0x$O1), 1 seul ecart, canal $C1 octet $B1 (offset $P1)"
  else
    bad "R11 : premier cran" "nibble $N1 (attendu ${EXPECT_R11_NIBBLE_1:-$R11_NIBBLE_1_ATTENDU}), $E1 ecart(s), canal ${C1:-?} octet ${B1:-?} (offset $P1)"
  fi

  N2="$(d2 cran2 3)"; O2="$(d2 cran2 5)"; E2="$(d2 cran2 7)"; P2="$(d2 cran2 9)"
  C2="$(d2 cran2 13)"; B2="$(d2 cran2 15)"
  if [ "$N2" = "${EXPECT_R11_NIBBLE_TRIOLET:-$R11_NIBBLE_TRIOLET_ATTENDU}" ] && [ "$E2" = "1" ] \
     && [ "$C2" = "$R11_CANAL_ATTENDU" ] && [ "$B2" = "$R11_OCTET_ATTENDU" ]; then
    ok "R11 : le triolet est retenu" "nibble $N2 (octet 0x$O2) : R3, R4 et R6 sautes en un seul cran, canal $C2 octet $B2"
  else
    bad "R11 : le triolet est retenu" "nibble $N2 (attendu ${EXPECT_R11_NIBBLE_TRIOLET:-$R11_NIBBLE_TRIOLET_ATTENDU}), $E2 ecart(s), canal ${C2:-?} octet ${B2:-?} (offset $P2)"
  fi

  N3="$(d2 cran3 3)"; E3="$(d2 cran3 7)"; T3="$(d2 cran3 11)"
  if [ "$N3" = "$N2" ] && [ "$E3" = "$E2" ] && [ "${T3:-0}" -gt 0 ] 2>/dev/null; then
    ok "R11 : plafond du choix" "3e cran : nibble inchange a $N3, $T3 octets de trafic — le geste a bien atteint le controleur"
  else
    bad "R11 : plafond du choix" "nibble $N3 (attendu $N2), $E3 ecart(s), trafic $T3"
  fi

  D_REF_OK=1; D_REF_D=""
  for et in cran1 cran2 cran3 retour; do
    n="$(d2 "$et" 3)"
    for c in $R11_CODES_REFUSES; do
      [ "$n" = "$c" ] && { D_REF_OK=0; D_REF_D="$D_REF_D $et=$n"; }
    done
  done
  if [ "$D_REF_OK" = "1" ]; then
    ok "R11 : les codes refuses n apparaissent jamais" "aucune etape ne montre 03, 04 ni 06 dans le nibble du step 5"
  else
    bad "R11 : les codes refuses n apparaissent jamais" "un code refuse a x24 a ete ecrit —$D_REF_D"
  fi

  NR="$(d2 retour 3)"; ER="$(d2 retour 7)"
  if [ "$NR" = "$R11_NIBBLE_ZERO" ] && [ "$ER" = "0" ]; then
    ok "R11 : retour a 00" "nibble $NR, les six instances identiques a l attendu du rig"
  else
    bad "R11 : retour a 00" "nibble $NR, $ER ecart(s)"
  fi

  CF="$(d2 cadence_fin 3)"; EF="$(d2 cadence_fin 9)"
  if [ "$CF" = "$R11_CADENCE_BASE" ] && [ "$EF" = "0" ]; then
    ok "R11 : cadence restauree" "retour a $CF ticks par step, instances a 0 ecart"
  else
    bad "R11 : cadence restauree" "cadence $CF (attendu $R11_CADENCE_BASE), $EF ecart(s)"
  fi
fi

INST_MASQUES_ATTENDUS="0001 0006 0038 03c0 7c00 84a5"
# Depuis 5.c.3 chaque instance --format 3 --per-channel porte un step de plus,
# le marqueur 18 + c. Les comptes sont ecrits en litteral, jamais derives.
INST_ACTIFS_ATTENDUS="2 3 4 5 6 7"
INST_SELECTION_ATTENDUE="0 1 2 3 4 5"
INST_CANAL_ATTENDU=3
INST_OCTET_ATTENDU=0

LOG8="$WORK/log8"
progress "parcours instances (separation template/instance)"
"$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$INSTANCE_PTR_ADDR" "$BOOT_MS" \
     "$WORK/perchannel.bin" 384 instances "$SUPPRESSED_ADDR" > "$LOG8" 2>&1
RC=$?
if [ "$RC" != "0" ]; then
  cat "$LOG8"
  case "$RC" in
    3) dieinval "$LOG8" "controle des instances en echec sur le parcours instances, aucun verdict sur le firmware" ;;
    *) die "le parcours instances s'est termine anormalement (code $RC)" ;;
  esac
fi

printf '\n%s--- PARCOURS INSTANCES : SEPARATION TEMPLATE / INSTANCE ---%s\n' "$C_B" "$C_0"
grep -E '^controle_source|^inst_attendus|^controle_usine|^inst_' "$LOG8" | sed 's/^/  /'
printf '\n'

I_SRC="$(grep -E '^controle_source ' "$LOG8" | awk '{print $2}')"
I_CTRL="$(grep -E '^controle_usine ' "$LOG8" | awk '{print $2}')"
I_MASQUES="$(grep -E '^inst_masques ' "$LOG8" | sed 's/^inst_masques *//' | tr -s ' ')"
I_ACTIFS="$(grep -E '^inst_actifs ' "$LOG8" | sed 's/^inst_actifs *//' | tr -s ' ')"
I_SELECTION="$(grep -E '^inst_selection ' "$LOG8" | sed 's/^inst_selection *//' | tr -s ' ')"
I_ECARTS="$(grep -E '^inst_edit ' "$LOG8" | awk '{print $5}')"
I_CANAL="$(grep -E '^inst_edit ' "$LOG8" | awk '{print $9}')"
I_OCTET="$(grep -E '^inst_edit ' "$LOG8" | awk '{print $11}')"
I_TWI="$(grep -E '^inst_edit ' "$LOG8" | awk '{print $13}')"
I_EE_USINE="$(grep -E '^inst_tpl_usine ' "$LOG8" | awk '{print $3}')"
I_EE_USINE_OU="$(grep -E '^inst_tpl_usine ' "$LOG8" | awk '{print $5}')"
I_EE_LISIBLE="$(grep -E '^inst_tpl_usine ' "$LOG8" | awk '{print $7}')"
I_EE_DERIVE="$(grep -E '^inst_tpl_eeprom ' "$LOG8" | awk '{print $3}')"
I_EE_DERIVE_OU="$(grep -E '^inst_tpl_eeprom ' "$LOG8" | awk '{print $5}')"
I_EE_LU="$(grep -E '^inst_tpl_eeprom ' "$LOG8" | awk '{print $7}')"
I_ONGLET="$(grep -E '^inst_nav_edit ' "$LOG8" | awk '{print $3}')"
I_ONGLET_VISE="$(grep -E '^inst_nav_edit ' "$LOG8" | awk '{print $5}')"
I_BARRE="$(grep -E '^inst_nav_edit ' "$LOG8" | awk '{print $7}')"
I_EDIT_CRENEAUX="$(grep -E '^inst_nav_edit ' "$LOG8" | awk '{print $8}')"

garde_pointeur "$LOG8" "instances : temoin du pointeur" && W_INST_PTR=1 || W_INST_PTR=0
if [ "$I_SRC" = "image" ] && [ "$I_CTRL" = "1" ]; then
  ok "instances : image differenciee" "image lue, six instances conformes a leur template octet pour octet"
  W_INST_IMG=1
else
  inval "instances : image differenciee" "source ${I_SRC:-rien}, controle d usine ${I_CTRL:-rien} : l instrument n est pas sain"
  W_INST_IMG=0
fi
W_INST=$(( W_INST_PTR * W_INST_IMG ))

if [ "$W_INST" != "1" ]; then
  inval "instances : six selections" "temoins amont invalides"
  inval "instances : six masques distincts" "temoins amont invalides"
  inval "instances : six comptes distincts" "temoins amont invalides"
  inval "instances : edition du canal 3" "temoins amont invalides"
else
  if [ "$I_SELECTION" = "$INST_SELECTION_ATTENDUE" ]; then
    ok "instances : six selections" "selectedPattern $I_SELECTION : chaque canal vise son propre template"
  else
    bad "instances : six selections" "selectedPattern '$I_SELECTION' au lieu de '$INST_SELECTION_ATTENDUE'"
  fi

  if [ "$I_MASQUES" = "$INST_MASQUES_ATTENDUS" ]; then
    ok "instances : six masques distincts" "$I_MASQUES : aucune instance n est confondable avec une autre"
  else
    bad "instances : six masques distincts" "masques '$I_MASQUES' au lieu de '$INST_MASQUES_ATTENDUS'"
  fi

  if [ "$I_ACTIFS" = "$INST_ACTIFS_ATTENDUS" ]; then
    ok "instances : six comptes distincts" "$I_ACTIFS steps actifs : activeStepsInInstance mesure bien le canal demande"
  else
    bad "instances : six comptes distincts" "comptes '$I_ACTIFS' au lieu de '$INST_ACTIFS_ATTENDUS'"
  fi

  if [ "$I_ONGLET" = "$I_ONGLET_VISE" ] && [ "${I_BARRE:-0}" = "$TAB_SLOTS" ] \
     && [ "${I_EDIT_CRENEAUX:-$TAB_SLOTS}" -lt "$TAB_SLOTS" ] 2>/dev/null; then
    ok "instances : navigation atteinte" "onglet $I_ONGLET vise $I_ONGLET_VISE, creneaux $I_BARRE -> $I_EDIT_CRENEAUX : la barre est bien quittee pour EDIT"
  else
    inval "instances : navigation atteinte" "onglet ${I_ONGLET:-?} au lieu de ${I_ONGLET_VISE:-?}, creneaux ${I_BARRE:-?} -> ${I_EDIT_CRENEAUX:-?} : le canal edite n est pas attribuable"
  fi

  if [ "${I_TWI:-0}" -eq 0 ] 2>/dev/null; then
    inval "instances : edition du canal 3" "aucun trafic : le geste d edition n a pas ete injecte"
  elif [ "$I_ECARTS" = "1" ] && [ "$I_CANAL" = "$INST_CANAL_ATTENDU" ] \
       && [ "$I_OCTET" = "$INST_OCTET_ATTENDU" ]; then
    ok "instances : edition du canal 3" "1 seul octet change, canal $I_CANAL octet $I_OCTET : les cinq autres instances sont intactes"
  else
    bad "instances : edition du canal 3" "$I_ECARTS ecart(s), canal ${I_CANAL:-?} octet ${I_OCTET:-?} (attendu 1 ecart, canal $INST_CANAL_ATTENDU octet $INST_OCTET_ATTENDU)"
  fi
fi

if [ -z "${I_EE_LISIBLE:-}" ] || [ "${I_EE_LISIBLE:-0}" != "1" ]; then
  inval "instances : templates d usine" "la zone EEPROM des templates n est pas lisible : critere non decidable"
elif [ "$I_EE_USINE" = "0" ]; then
  ok "instances : templates d usine" "les 384 octets EEPROM portent A1..A8 puis huit records vides, longueur 16"
else
  inval "instances : templates d usine" "$I_EE_USINE octet(s) hors attendu dans l image FOURNIE, premier a l offset ${I_EE_USINE_OU:-?} : le rig est faux, aucun verdict sur le firmware"
fi

if [ "${I_EE_LU:-0}" != "1" ]; then
  inval "instances : templates EEPROM stables" "l EEPROM simulee n a pas pu etre relue par AVR_IOCTL_EEPROM_GET"
elif [ "$W_INST" != "1" ]; then
  inval "instances : templates EEPROM stables" "temoins amont invalides"
elif [ "${I_EE_USINE:-1}" != "0" ]; then
  inval "instances : templates EEPROM stables" "le controle du rig est invalide : une derive n est pas attribuable au firmware"
elif [ "$I_EE_DERIVE" = "0" ]; then
  ok "instances : templates EEPROM stables" "les 384 octets RELUS de l EEPROM simulee sont identiques a l image fournie"
else
  bad "instances : templates EEPROM stables" "$I_EE_DERIVE octet(s) EEPROM modifie(s), premier a l offset ${I_EE_DERIVE_OU:-?} : l edition a touche un template"
fi

printf '\n'

LOG9="$WORK/log9"
progress "parcours bootstrap (semis du firmware au premier demarrage)"
"$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$INSTANCE_PTR_ADDR" "$BOOT_MS" \
     "$WORK/perchannel.bin" 384 bootstrap "$SUPPRESSED_ADDR" > "$LOG9" 2>&1
RC=$?
if [ "$RC" != "0" ]; then
  cat "$LOG9"
  die "le parcours bootstrap s'est termine anormalement (code $RC)"
fi

printf '\n%s--- PARCOURS BOOTSTRAP : LE FIRMWARE SEME SES TEMPLATES ---%s\n' "$C_B" "$C_0"
grep -E '^boot_|^attendu_bootstrap|^controle_source|^inst_attendus' "$LOG9" | sed 's/^/  /'
printf '\n'

B_REP_LU="$(grep -E '^boot_repare ' "$LOG9" | awk '{print $3}')"
B_REP_VAL="$(grep -E '^boot_repare ' "$LOG9" | awk '{print $7}')"
B_REP_ATT="$(grep -E '^boot_repare ' "$LOG9" | awk '{print $9}')"
B_SEM_LU="$(grep -E '^boot_semis ' "$LOG9" | awk '{print $3}')"
B_SEM_ECARTS="$(grep -E '^boot_semis ' "$LOG9" | awk '{print $5}')"
B_SEM_PREM="$(grep -E '^boot_semis ' "$LOG9" | awk '{print $7}')"
B_INST="$(grep -E '^boot_instances ' "$LOG9" | sed 's/^boot_instances *//' | sed 's/ attendu.*//' | tr -s ' ')"
B_INST_ATT="$(grep -E '^boot_instances ' "$LOG9" | awk '{print $NF}')"
B_VER_VUE="$(grep -E '^boot_version ' "$LOG9" | awk '{print $3}')"
B_VER_VAL="$(grep -E '^boot_version ' "$LOG9" | awk '{print $5}')"
B_VER_ATT="$(grep -E '^boot_version ' "$LOG9" | awk '{print $7}')"
B_ATTENTE="$(grep -E '^boot_version ' "$LOG9" | awk '{print $9}')"
B_PLAFOND="$(grep -E '^boot_version ' "$LOG9" | awk '{print $11}')"

if [ "${B_REP_LU:-0}" != "1" ]; then
  inval "bootstrap : octet repare" "l EEPROM simulee n a pas pu etre relue apres le demarrage"
elif [ "$B_REP_VAL" = "$B_REP_ATT" ]; then
  ok "bootstrap : octet repare" "l octet de template corrompu vaut de nouveau $B_REP_ATT : SEUL le semis du firmware a pu le reparer"
else
  bad "bootstrap : octet repare" "l octet corrompu vaut $B_REP_VAL au lieu de $B_REP_ATT : le semis n a pas eu lieu"
fi

if [ "${B_SEM_LU:-0}" != "1" ]; then
  inval "bootstrap : semis complet" "EEPROM simulee non relue"
elif [ "$B_SEM_ECARTS" = "0" ]; then
  ok "bootstrap : semis complet" "les 384 octets de templates portent A1..A8 puis huit records vides"
else
  bad "bootstrap : semis complet" "$B_SEM_ECARTS octet(s) hors attendu, premier a l offset ${B_SEM_PREM:-?}"
fi

B_INST_ATTENDU="$(printf '%s %s %s %s %s %s' "$B_INST_ATT" "$B_INST_ATT" "$B_INST_ATT" "$B_INST_ATT" "$B_INST_ATT" "$B_INST_ATT")"
garde_pointeur "$LOG9" "bootstrap : temoin du pointeur" && W_BOOT_PTR=1 || W_BOOT_PTR=0
if [ "$W_BOOT_PTR" != "1" ]; then
  inval "bootstrap : six instances sur A1" "temoin du pointeur invalide : les masques lus ne sont pas attribuables au firmware"
elif [ "$(echo $B_INST)" = "$(echo $B_INST_ATTENDU)" ]; then
  ok "bootstrap : six instances sur A1" "les six masques valent $B_INST_ATT, et non ceux de l image differenciee"
else
  bad "bootstrap : six instances sur A1" "masques $B_INST au lieu de six fois $B_INST_ATT"
fi

if [ "${B_VER_VUE:-0}" != "1" ]; then
  bad "bootstrap : version ecrite" "la version relue vaut ${B_VER_VAL:-?} au lieu de ${B_VER_ATT:-3} apres ${B_ATTENTE:-?} ms"
elif [ "$B_VER_VAL" = "$B_VER_ATT" ]; then
  ok "bootstrap : version ecrite" "l octet de version vaut $B_VER_ATT, releve par AVR_IOCTL_EEPROM_GET"
else
  bad "bootstrap : version ecrite" "version $B_VER_VAL au lieu de $B_VER_ATT"
fi

if [ "${B_VER_VUE:-0}" = "1" ] && [ "${B_ATTENTE:-0}" -lt "${B_PLAFOND:-0}" ] 2>/dev/null; then
  ok "bootstrap : plafond non atteint" "balayage termine apres ${B_ATTENTE} ms, garde-fou a ${B_PLAFOND} ms"
else
  bad "bootstrap : plafond non atteint" "le garde-fou de ${B_PLAFOND:-?} ms a ete atteint : le balayage n a pas abouti"
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
