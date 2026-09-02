#!/usr/bin/env bash
#
# Verifie la FONCTION MUSICALE du firmware de production : les six sorties
# emettent-elles le motif ecrit, quand, et pendant combien de temps ?
#
# POURQUOI CETTE SONDE EXISTE. Le chemin « contenu du pattern -> onset ->
# impulsion sur une broche » n'etait exerce par AUCUN binaire : `main.cpp` emet
# les triggers mais sa banque est vide et aucune UI ne permet encore d'y ecrire,
# `wokwi_main.cpp` porte du contenu mais n'instancie pas de TriggerSequencer. Les
# tests natifs validaient le domaine, run-screen-dump le rendu, run-blocking-probe
# le temps — et la fonction du module n'etait observee nulle part.
#
# Le firmware n'est pas instrumente : le contenu du pattern est ecrit dans la RAM
# simulee a l'adresse du symbole `patternBank`, lue par `avr-nm`. Le binaire
# mesure est celui qui sera flashe. Voir tools/simavr-ssd1306/trigger_probe.c.
#
# VERDICT — TROIS COURSES sur le meme firmware. Le
# mode et le contenu du pattern arrivent par une image EEPROM prechargee dans la
# machine simulee, fabriquee par tools/eeprom-image.cpp avec le code du domaine
# lui-meme : aucun decalage de structure privee, aucune constante du format
# recopiee dans le harnais.
#
#   COURSE CLOCK — le defaut d'usine (PRD 4.2). Le train est REGULIER, un ecart
#   par step a 5 % de la mediane. Le motif est dans la banque et n'est PAS joue :
#   ce critere verifie donc aussi que CLOCK ignore le contenu, comme l'original.
#
#   COURSE SEQ — le sequenceur joue le motif. Chaque ecart est converti en
#   nombre de steps, et la suite doit etre une ROTATION CYCLIQUE de celle du
#   motif : la phase de depart reste inconnue, ce qui est exactement
#   l'affirmation qui a un sens musical.
#
#   COURSE RATCHET — le meme motif avec un ratchet 6 sur le step 0. Les onsets
#   ne sont plus sur la grille des steps, donc ni l'ecart entre impulsions ni la
#   gigue par rapport a cette grille ne veulent dire quoi que ce soit : le
#   critere est le NOMBRE d'impulsions, et il ELIMINE la phase au lieu de la
#   supposer. Avec seq = k + m et ratchet = N*k + m, la difference vaut
#   (N-1)*k : elle doit etre un MULTIPLE EXACT de N-1, et le quotient est le
#   nombre de passages du step raccourci, deduit et jamais suppose. Un seul
#   sous-declenchement perdu casse la divisibilite.
#
#   Un premier critere comparait le RAPPORT des deux comptes a (active-1+N)/
#   active. Il etait FAUX : il supposait que le step raccourci passe autant de
#   fois que la moyenne des steps actifs. Mesure a 27 contre 12, soit x2.25 quand
#   il attendait x2.00, alors que 27-12 = 15 = 5x3 est exact.
#
#   Ce que cette course NE prouve PAS : que la dette d'onsets serve a quelque
#   chose. Un onset n'est perdu que si deux tombent dans le MEME passage, et le
#   creneau jouable le plus court vaut deux ticks — 4,17 ms au tempo maximum —
#   contre un passage de ~0,2 ms sur l'ecran principal. Aucune configuration
#   legale ne les fait se rencontrer ici. Ce qu'elle prouve, et qu'aucun binaire
#   n'avait jamais montre : les sous-declenchements atteignent les broches.
#
# Les deux courses partagent quatre criteres : les six sorties emettent, les six
# lignes battent ensemble a moins de 200 us, le tempo applique, et la gigue sous
# JITTER_BUDGET_PCT d'un step. Rapportees : la largeur d'impulsion reelle et
# PULSE de l'expandeur MIDI.
#
# `MUTATE=<step>` ajoute un step a l'IMAGE sans l'ajouter a l'ATTENTE. La course
# SEQ passe alors au rouge, la course CLOCK reste verte — et cette asymetrie est
# elle-meme la preuve que CLOCK ignore la banque.
#
# `DROP=<n>` ignore un front sur n : le train CLOCK cesse d'etre regulier et son
# critere passe au rouge. Il ne touche PAS la course RATCHET, qui saute l'analyse
# des ecarts ou DROP s'applique.
#
# `RATCHET_MUTATE=<step>:<code>` ajoute un ratchet a l'IMAGE et pas a l'ATTENTE.
# Avec un code dont N-1 ne divise pas celui du critere, la difference cesse
# d'etre un multiple exact : c'est le chemin rouge de la course RATCHET.
# Mesure : `RATCHET_MUTATE=3:3` donne 33-12 = 21 = 5x4+1.
#
# Reglages : DURATION (defaut 20 s de simulation), JITTER_BUDGET_PCT (defaut 2),
# RATCHET_STEP et RATCHET_CODE (defaut 0 et 6).
# Sortie 0 si les six courses passent, 1 sinon, 127 si un outil manque.

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DURATION="${DURATION:-20}"
JITTER_BUDGET_PCT="${JITTER_BUDGET_PCT:-2}"
# TEMPO change le tempo par defaut du firmware ET l'attente du harnais, de sorte
# que « la duree de step suit le tempo » soit verifiable et pas seulement crue.
TEMPO="${TEMPO:-120}"
STEP_TOLERANCE_PCT="${STEP_TOLERANCE_PCT:-1}"
# LENGTH change la longueur jouee dans l IMAGE ET l attente du harnais. La
# meme valeur part aux deux, sinon l ecart de bouclage serait faux. Defaut
# SequencerEngine::DEFAULT_LENGTH, donc le comportement nominal ne bouge pas.
LENGTH="${LENGTH:-16}"
# EXPECTED_LENGTH sert la contre-epreuve : donner au harnais une longueur
# differente de celle de l image doit rougir.
EXPECTED_LENGTH="${EXPECTED_LENGTH:-$LENGTH}"
# Injection CV : NOMINALE depuis la decision D8, 2026-08-31. Trois courses de
# plus, et elles exercent les DEUX sources :
#   cvzero     les deux entrees au zero mesure du module, routage 1:2
#   cv1length  CV1 a CV_NOMINAL_MV, routage 1:2
#   cv2length  CV2 a CV_NOMINAL_MV, routage 2:2
#
# Pourquoi les deux. La preuve CV1 seule ne dit rien de CV2 : le routage nomme un
# INDEX de source, et c'est l'index qui decide quelle voie est lue. Une course
# CV2 obtenue a la main se perdrait au prochain lot, alors que la propriete
# verrouillee est celle-ci :
#   CV1 -> source 1 -> LENGTH   vert
#   CV2 -> source 2 -> LENGTH   vert
#   mauvais routage             rouge
#   mauvais offset attendu      rouge
#
# EXPECTED_OFFSET est la zone attendue, un LITTERAL verifiable contre la table
# figee du lot LCV.3a : le harnais ne recopie pas le quantizer.
#
# Leviers de contre-epreuve. CV_TARGET force le routage des TROIS courses, donc
# `CV_TARGET=1:2` sur la course cv2length injecte sur CV2 en routant la source 1
# et doit rougir. CV1_MV et CV2_MV forcent la tension injectee.
CV_ZERO_MV="${CV_ZERO_MV:-2625}"
CV_NOMINAL_MV="${CV_NOMINAL_MV:-4150}"
EXPECTED_OFFSET="${EXPECTED_OFFSET:-10}"
CV1_MV="${CV1_MV:-$CV_NOMINAL_MV}"
CV2_MV="${CV2_MV:-$CV_NOMINAL_MV}"
CV_TARGET_FORCED="${CV_TARGET:-}"
CV_TARGET="${CV_TARGET:-1:2}"
# Course CVRESET : NOMINALE depuis F4.8, 2026-09-02. Un front injecte en cours
# de lecture re-origine la grille du canal route vers RESET. Trois criteres :
# la rotation cyclique AVANT le front, la latence du premier front apres lui,
# et la re-origine — la suite des ecarts apres le front est celle du motif
# DEPUIS LE STEP 0, dans l'ordre, phase connue. C'est la preuve du dernier
# maillon, ADC -> takeEdge -> masque de main.cpp -> moteur -> broche, que
# aucune suite de domaine ne peut donner : c'est la ou vit le mutant M7.
#
# Leviers : RESET_PULSE_MS (defaut 8137, choisi asynchrone de la grille),
# RESET_PULSE_MV (4000, au-dessus du seuil d'armement de +1 V),
# RESET_PULSE_SOURCE (1 ou 2 ; injecter sur la source NON routee doit rougir),
# RESET_LATENCY_MS (60 : un tick de 5,21 ms a 120 BPM en /1, plus la
# granularite du passage de boucle, avec de la marge — un critere operationnel,
# verifie par la mesure rapportee).
RESET_PULSE_MS="${RESET_PULSE_MS:-8137}"
RESET_PULSE_MV="${RESET_PULSE_MV:-4000}"
RESET_PULSE_SOURCE="${RESET_PULSE_SOURCE:-1}"
RESET_LATENCY_MS="${RESET_LATENCY_MS:-60}"
COURSES="${COURSES:-clock seq ratchet cvzero cv1length cv2length cvreset}"

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

PREFIX=""
for p in /opt/homebrew /usr/local; do
  [ -f "$p/lib/libsimavrparts.a" ] && PREFIX="$p" && break
done
[ -n "$PREFIX" ] || die "libsimavrparts absente. brew install simavr" 127

LOG="$(mktemp)"; BIN="$(mktemp -d)/trigger_probe"
trap 'rm -f "$LOG"; rm -rf "$(dirname "$BIN")"' EXIT

# --- 1. Harnais --------------------------------------------------------------
progress "compilation du harnais"
if cc -O2 -Wall -DBPM="$TEMPO" -I"$PREFIX/include/simavr" -I"$PREFIX/include" \
     "$ROOT/tools/simavr-ssd1306/trigger_probe.c" -o "$BIN" \
     -L"$PREFIX/lib" -lsimavrparts -lsimavr -lelf > "$LOG" 2>&1; then
  printf '  %s✅%s harnais compile        %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" "$PREFIX" "$C_0"
else
  printf '\n'; cat "$LOG"; die "compilation du harnais en echec"
fi

# --- 2. Firmware -------------------------------------------------------------
progress "build env:nanoatmega328"
PIO_EXTRA=""
if [ "$TEMPO" != "120" ]; then
  PIO_EXTRA="-DFLEXSEQ_DEFAULT_TEMPO=$TEMPO"
fi
if PLATFORMIO_BUILD_FLAGS="$PIO_EXTRA" "$PIO" run -e nanoatmega328 -d "$ROOT" > "$LOG" 2>&1; then
  printf '  %s✅%s firmware               %s%s%s\n' "$C_OK" "$C_0" "$C_DIM" \
    "$(grep -E '^RAM:' "$LOG" | sed 's/.*(used /RAM /; s/ bytes from .*/ o/')" "$C_0"
else
  printf '\n'; tail -30 "$LOG"; die "build du firmware en echec"
fi

. "$ROOT/tools/active-format.sh"
flexseq_resolve_active_format "$ROOT" "$ROOT/.pio/build/nanoatmega328/firmware.elf" \
  "$(dirname "$BIN")" || exit $?
flexseq_report_active_format "$C_OK" "$C_DIM" "$C_0"

# --- 3. Images EEPROM -------------------------------------------------------
# Fabriquees par le code du domaine, donc le format n'est decrit qu'une fois.
progress "generateur d'image EEPROM"
GEN="$(dirname "$BIN")/eeprom-image"
if c++ -std=gnu++11 -I"$ROOT/include" -o "$GEN" "$ROOT/tools/eeprom-image.cpp" \
     "$ROOT"/src/domain/*.cpp > "$LOG" 2>&1; then
  printf '  %s✅%s generateur compile     %stools/eeprom-image.cpp%s\n' \
    "$C_OK" "$C_0" "$C_DIM" "$C_0"
else
  printf '\n'; cat "$LOG"; die "compilation du generateur d'image en echec"
fi

STEPS="${STEPS:-0,3,4,9,15}"
IMAGE_STEPS="$STEPS"
if [ -n "${MUTATE:-}" ]; then
  IMAGE_STEPS="$STEPS,$MUTATE"
fi
# La course RATCHET rejoue le meme motif avec un ratchet 6 sur le step 0, donc
# six onsets la ou il y en avait un. Le critere est le NOMBRE d impulsions,
# compare a celui de la course SEQ : exact, auto-etalonne, sans arithmetique de
# cycle partiel.
RATCHET_STEP="${RATCHET_STEP:-0}"
RATCHET_CODE="${RATCHET_CODE:-6}"
for MODE in $COURSES; do
  GEN_ARGS="--mode seq"
  [ "$MODE" = "clock" ] && GEN_ARGS="--mode clock"
  case "$MODE" in
    cvzero)    GEN_ARGS="--mode seq --cv-target ${CV_TARGET_FORCED:-1:2}" ;;
    cv1length) GEN_ARGS="--mode seq --cv-target ${CV_TARGET_FORCED:-1:2}" ;;
    cv2length) GEN_ARGS="--mode seq --cv-target ${CV_TARGET_FORCED:-2:2}" ;;
    cvreset)   GEN_ARGS="--mode seq --cv-target ${CV_TARGET_FORCED:-1:3}" ;;
  esac
  # RATCHET_MUTATE=<step>:<code> ajoute un ratchet a l IMAGE et pas a l ATTENTE.
  # Avec un code dont N-1 ne divise pas celui du critere, la difference cesse
  # d etre un multiple exact et la course rougit. DROP ne peut pas jouer ce role :
  # il filtre les fronts dans l analyse des ecarts, que cette course saute.
  if [ "$MODE" = "ratchet" ]; then
    GEN_ARGS="--mode seq --ratchet $RATCHET_STEP:$RATCHET_CODE"
    [ -n "${RATCHET_MUTATE:-}" ] && GEN_ARGS="$GEN_ARGS,$RATCHET_MUTATE"
  fi
  if ! "$GEN" $GEN_ARGS --format "$FLEXSEQ_FORMAT_VERSION" --steps "$IMAGE_STEPS" --tempo "$TEMPO" \
       --length "$LENGTH" \
       > "$(dirname "$BIN")/ee-$MODE.bin" 2>"$LOG"; then
    cat "$LOG"; die "generation de l'image $MODE en echec"
  fi
done
IMAGE_BYTES="$(wc -c < "$(dirname "$BIN")/ee-${COURSES%% *}.bin" | tr -d ' ')"
printf '  %s✅%s images generees        %s%s octets, steps %s%s\n' \
  "$C_OK" "$C_0" "$C_DIM" "$IMAGE_BYTES" "$IMAGE_STEPS" "$C_0"
if [ -n "${MUTATE:-}" ]; then
  printf '  %s⚠%s  MUTATION               %sstep %s dans l'"'"'image, pas dans l'"'"'attente%s\n' \
    "$C_DIM" "$C_0" "$C_DIM" "$MUTATE" "$C_0"
fi

# --- 4. Simulations : une course par mode -----------------------------------
FAILED=0
SEQ_PULSES=0
for MODE in $COURSES; do
  progress "simulation $MODE ($DURATION s)"
  PROBE_MODE="$MODE"
  PROBE_LENGTH="$EXPECTED_LENGTH"
  PROBE_CV=""
  case "$MODE" in
    cvzero)
      PROBE_MODE="seq"; PROBE_CV="$CV_ZERO_MV $CV_ZERO_MV" ;;
    cv1length)
      PROBE_MODE="seq"
      PROBE_LENGTH=$(( EXPECTED_LENGTH + EXPECTED_OFFSET ))
      PROBE_CV="$CV1_MV $CV_ZERO_MV" ;;
    cv2length)
      PROBE_MODE="seq"
      PROBE_LENGTH=$(( EXPECTED_LENGTH + EXPECTED_OFFSET ))
      PROBE_CV="$CV_ZERO_MV $CV2_MV" ;;
    cvreset)
      PROBE_MODE="cvreset"; PROBE_CV="$CV_ZERO_MV $CV_ZERO_MV" ;;
  esac
  PROBE_ENV=""
  if [ "$MODE" = "cvreset" ]; then
    PROBE_ENV="RESET_PULSE_MV=$RESET_PULSE_MV RESET_PULSE_MS=$RESET_PULSE_MS RESET_PULSE_SOURCE=$RESET_PULSE_SOURCE"
  fi
  set +e
  env $PROBE_ENV "$BIN" "$ROOT/.pio/build/nanoatmega328/firmware.hex" "$DURATION" \
    "$(dirname "$BIN")/ee-$MODE.bin" 384 "$PROBE_MODE" "$STEPS" "$PROBE_LENGTH" $PROBE_CV > "$LOG" 2>&1
  PROBE=$?
  set -e
  if [ "$PROBE" -ne 0 ]; then
    printf '  %s⚠%s  la sonde s'"'"'est terminee anormalement (code %d)\n' \
      "$C_DIM" "$C_0" "$PROBE"
  fi
  printf '  %s✅%s simulation %-11s %s%s s simulees%s\n' \
    "$C_OK" "$C_0" "$MODE" "$C_DIM" "$DURATION" "$C_0"

  if [ "$MODE" = "seq" ]; then
    SEQ_PULSES="$(grep -o 'impulsions_ch1=[0-9]*' "$LOG" | tail -1 | cut -d= -f2)"
    SEQ_PULSES="${SEQ_PULSES:-0}"
  fi

  set +e
  JITTER_BUDGET_PCT="$JITTER_BUDGET_PCT" STEP_TOLERANCE_PCT="$STEP_TOLERANCE_PCT" \
    MODE="$PROBE_MODE" SEQ_PULSES="$SEQ_PULSES" STEPS="$STEPS" \
    RATCHET_CODE="$RATCHET_CODE" EXPECTED_LENGTH="$EXPECTED_LENGTH" \
    RESET_PULSE_MS="$RESET_PULSE_MS" RESET_LATENCY_MS="$RESET_LATENCY_MS" \
    python3 - "$LOG" <<'PY'
import os, re, sys

txt = open(sys.argv[1], errors='replace').read()
tty = sys.stdout.isatty()
OK, ERR, DIM, B, Z = ('\033[32m', '\033[31m', '\033[2m', '\033[1m', '\033[0m') if tty else ('',) * 5
mark = lambda good: f"{OK}✅{Z}" if good else f"{ERR}❌{Z}"
budget_pct = float(os.environ["JITTER_BUDGET_PCT"])
mode = os.environ["MODE"]
seq = mode == "seq"
ratchet = mode == "ratchet"
cvreset = mode == "cvreset"
step_tol = float(os.environ["STEP_TOLERANCE_PCT"])

m = re.search(r"RESULTAT (.*)", txt)
if not m:
    print(f"  {mark(False)} sortie de la sonde illisible")
    print("".join(c for c in txt if 32 <= ord(c) < 127 or c == "\n"))
    sys.exit(1)

kv = dict(p.split("=", 1) for p in m.group(1).split())
lines_on = int(kv["lignes_actives"]); lines_exp = int(kv["attendu"])
gaps_ok, gaps_total = (int(x) for x in kv["ecarts_ok"].split("/"))
width = float(kv["largeur_med"])
jit_med, jit_max = float(kv["gigue_med"]), float(kv["gigue_max"])
step_ms = float(kv["step_ms"])
same, coincident = kv["meme_compte"] == "1", kv["coincident"] == "1"
pulses, pulse_line = int(kv["impulsions_ch1"]), int(kv["pulse"])

# Le firmware demarre A L'ARRET. Aucune impulsion ne doit donc preceder l'appui
# sur PLAY : sans ce critere, la sonde prouverait que PLAY marche et pas que le
# module se tait avant.
first_ms = float(kv.get("premier_front_ms", "-1"))
play_ms = float(kv.get("play_ms", "0"))
silent_before_play = first_ms > play_ms

all_lines = lines_on == lines_exp
pattern_ok = gaps_total > 0 and gaps_ok == gaps_total

# Course RATCHET : le critere elimine la PHASE au lieu de la supposer. Un ratchet
# N sur un seul step actif donne, sur la meme duree et le meme tempo :
#     seq     = k + m        k occurrences du step raccourci, m des autres
#     ratchet = N*k + m
# donc  ratchet - seq = (N-1)*k. La difference doit etre un MULTIPLE EXACT de
# N-1, et le quotient est le nombre d occurrences — jamais suppose, toujours
# deduit. Un seul sous-declenchement perdu casse la divisibilite.
#
# Un premier critere comparait le RAPPORT a (active-1+N)/active. Il etait faux :
# il supposait que le step raccourci passe autant de fois que la moyenne des
# steps actifs. Mesure a 27 contre 12, soit x2.25 quand il attendait x2.00, alors
# que 27-12 = 15 = 5x3 est exact.
occurrences = 0
diff = 0
if ratchet:
    code = int(os.environ["RATCHET_CODE"])
    seq_pulses = int(os.environ["SEQ_PULSES"])
    diff = pulses - seq_pulses
    exact = code > 1 and diff > 0 and diff % (code - 1) == 0
    occurrences = diff // (code - 1) if code > 1 else 0
    pattern_ok = seq_pulses > 0 and exact and occurrences >= 2

# Course CVRESET : trois criteres sur les temps de front bruts. AVANT le front
# injecte, la suite des ecarts est une rotation cyclique de celle du motif —
# la course se controle elle-meme. Le premier front APRES lui arrive sous
# RESET_LATENCY_MS. Et la suite des ecarts apres lui est celle du motif DEPUIS
# LE STEP 0, dans l'ordre : la re-origine, phase connue, plus forte que la
# rotation.
rot_ok = latency_ok = anchored = False
first_delay = -1.0
pre_gaps = post_gaps = []
exp = []
if cvreset:
    reset_ms = float(os.environ["RESET_PULSE_MS"])
    latency_budget = float(os.environ["RESET_LATENCY_MS"])
    steps_list = [int(s) for s in os.environ["STEPS"].split(",")]
    length = int(os.environ["EXPECTED_LENGTH"])
    exp = [steps_list[i + 1] - steps_list[i] for i in range(len(steps_list) - 1)]
    exp.append(length - steps_list[-1] + steps_list[0])
    edges_m = re.search(r"^EDGES((?:\s+\d+\.\d+)+)$", txt, re.M)
    edges = [float(x) for x in edges_m.group(1).split()] if edges_m else []
    pre = [e for e in edges if e < reset_ms]
    post = [e for e in edges if e >= reset_ms]

    def to_steps(times):
        out = []
        for a, b in zip(times, times[1:]):
            gap = b - a
            r = round(gap / step_ms)
            out.append(r if r >= 1 and abs(gap - r * step_ms) <= 0.05 * step_ms else 0)
        return out

    pre_gaps = to_steps(pre)
    post_gaps = to_steps(post)
    n = len(exp)
    rot_ok = len(pre_gaps) >= 2 and any(
        all(g == exp[(i + ph) % n] for i, g in enumerate(pre_gaps))
        for ph in range(n))
    if post:
        first_delay = post[0] - reset_ms
    latency_ok = bool(post) and 0.0 <= first_delay <= latency_budget
    anchored = len(post_gaps) >= n and all(
        g == exp[i % n] for i, g in enumerate(post_gaps))
    pattern_ok = rot_ok and latency_ok and anchored
jit_pct = 100.0 * jit_max / step_ms if step_ms else 0.0
jit_ok = jit_pct <= budget_pct if not (ratchet or cvreset) else True
step_measured = float(kv.get("step_mesure", "0"))
bpm = int(kv.get("bpm", "0"))
step_err_pct = abs(step_measured - step_ms) / step_ms * 100.0 if step_ms else 100.0
step_ok = ((step_measured > 0.0 and step_err_pct <= step_tol)
           if not (ratchet or cvreset) else True)
duty = 100.0 * width / step_ms if step_ms else 0.0
duty_ok = duty < 50.0

print()
title = ("RATCHET — les sous-declenchements atteignent les broches" if ratchet
         else "CVRESET — un front re-origine la grille" if cvreset
         else "SEQ — le motif est joue" if seq else "CLOCK — le motif est ignore")
print(f"{B}====== FONCTION MUSICALE : {title} ======{Z}")
print(f"  {mark(all_lines)} Sorties actives    {lines_on}/{lines_exp}   "
      f"{DIM}— les 6 channels selectionnent le pattern 0 par defaut{Z}")
if ratchet:
    n1 = int(os.environ["RATCHET_CODE"]) - 1
    rest = diff % n1 if n1 else diff
    shown = f"{n1} x {occurrences}" + (f" + {rest}" if rest else "")
    print(f"  {mark(pattern_ok)} Sous-declenchements  {pulses} - {os.environ['SEQ_PULSES']} "
          f"= {diff} = {shown}   "
          f"{DIM}— multiple exact de N-1 ; le quotient est le nombre de passages "
          f"du step raccourci, deduit et non suppose{Z}")
elif cvreset:
    print(f"  {mark(rot_ok)} Rotation avant     ecarts {pre_gaps}   "
          f"{DIM}— rotation cyclique de {exp} avant le front{Z}")
    print(f"  {mark(latency_ok)} Latence du reset   premier front a "
          f"{first_delay:.1f} ms du front injecte   "
          f"{DIM}— budget {os.environ['RESET_LATENCY_MS']} ms{Z}")
    print(f"  {mark(anchored)} Re-origine         ecarts {post_gaps}   "
          f"{DIM}— la suite de {exp} DEPUIS le step 0, phase connue{Z}")
elif seq:
    print(f"  {mark(pattern_ok)} Motif joue         {gaps_ok}/{gaps_total} ecarts   "
          f"{DIM}— la suite est une rotation cyclique de celle du motif{Z}")
else:
    print(f"  {mark(pattern_ok)} Train regulier     {gaps_ok}/{gaps_total} ecarts   "
          f"{DIM}— un par step, a 5 % de la mediane ; le motif charge est ignore{Z}")
print(f"  {mark(silent_before_play)} Silence au demarrage  premier front a {first_ms:.0f} ms   "
      f"{DIM}— PLAY relache a {play_ms:.0f} ms ; le module demarre a l arret{Z}")
print(f"  {mark(same and coincident)} Six lignes en phase "
      f"{'meme compte, fronts < 200 us' if (same and coincident) else 'DESACCORD'}")
if not ratchet and not cvreset:
    print(f"  {mark(step_ok)} Tempo applique     {step_measured:.2f} ms par step   "
          f"{DIM}— attendu {step_ms:.2f} ms a {bpm} BPM ; ecart {step_err_pct:.2f} %"
          f" ; tolerance {step_tol:g} %{Z}")
    print(f"  {mark(jit_ok)} Gigue              {jit_max:.2f} ms   "
          f"{DIM}— {jit_pct:.2f} % d'un step de {step_ms:.0f} ms ; budget {budget_pct:g} %"
          f" ; mediane {jit_med:.2f} ms{Z}")
print(f"{B}================================================================={Z}")
print(f"  Impulsions observees       : {pulses} sur le channel 1")
print(f"  Largeur d'impulsion        : {width:.2f} ms  "
      f"({duty:.1f} % du step){'' if duty_ok else '   <-- PLUS DE LA MOITIE DU STEP'}")
print(f"  PULSE (expandeur MIDI)     : {pulse_line} impulsion(s)")
print()

# La largeur meritait d'etre expliquee plutot que seulement chiffree.
if width > 6.0:
    print(f"  {DIM}La largeur depasse les 5 ms de DEFAULT_TRIGGER_DURATION_MS parce que")
    print(f"  l'extinction a lieu dans outputs[ch].Process(), en FIN de loop() : une")
    print(f"  impulsion dure 5 ms ARRONDIS AU PASSAGE SUIVANT. Ce n'est pas un defaut")
    print(f"  de libGravity, c'est la granularite de notre boucle.{Z}")
if pulse_line == 0:
    print(f"  {DIM}PULSE reste muet : main.cpp ne pilote pas gravity.pulse. Observation,")
    print(f"  non defaut — l'expandeur MIDI n'est pas encore dans le chemin.{Z}")

ok = (all_lines and pattern_ok and same and coincident and jit_ok and duty_ok
      and step_ok and silent_before_play)
if ok:
    if cvreset:
        print(f"\n  Le front injecte re-origine la grille sur le step 0, "
              f"en {first_delay:.1f} ms.")
        print(f"  {DIM}Le maillon ADC -> takeEdge -> masque -> moteur -> broche est "
              f"exerce de bout en bout.{Z}")
    elif ratchet:
        print(f"\n  Un ratchet {os.environ['RATCHET_CODE']} sur un step actif ajoute "
              f"exactement {diff} impulsions en {occurrences} passages.")
        print(f"  {DIM}Le chemin ratchet -> sous-declenchement -> broche est exerce. Aucun{Z}")
        print(f"  {DIM}binaire ne l'avait jamais montre.{Z}")
    elif seq:
        print(f"\n  Les six sorties jouent le motif ecrit, en phase, au bon tempo.")
        print(f"  {DIM}Le chemin contenu du pattern -> sortie est exerce de bout en bout.{Z}")
    else:
        print(f"\n  Les six sorties emettent le train CLOCK, en phase, au bon tempo.")
        print(f"  {DIM}Le motif est dans la banque et n'est pas joue : c'est le{Z}")
        print(f"  {DIM}comportement de l'original.{Z}")
else:
    print(f"\n  {ERR}Au moins un critere echoue — ne pas flasher sur cette base.{Z}")
sys.exit(0 if ok else 1)
PY
  [ $? -ne 0 ] && FAILED=1
  set -e
done

if [ "$FAILED" -ne 0 ]; then
  printf '\n  %s❌%s Au moins une course echoue.\n' "$C_ERR" "$C_0"
  exit 1
fi
printf '\n  %s✅%s Les courses passent : CLOCK ignore, SEQ joue, RATCHET subdivise,\n' \
  "$C_OK" "$C_0"
printf '     %sCV1 et CV2 modulent LENGTH chacune par son propre index de source,%s\n' \
  "$C_DIM" "$C_0"
printf '     %set un front CV re-origine la grille du canal route vers RESET.%s\n' \
  "$C_DIM" "$C_0"
exit 0
