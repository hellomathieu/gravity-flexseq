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
# Courses PATOLD, PATNEW, CVPATTERN : la fixture P35 du lot STEP, decidee le
# 2026-09-03. Deux templates ecrits par --template : OLD (index 8) porte seize
# steps actifs et un TRIPLET sur chacun, NEW (index 15) les memes seize steps et
# aucun ratchet. CV1 est route vers PATTERN, les six canaux selectionnent OLD.
#   patold     CV1 au zero mesure          -> index 8, tous les ecarts font 2/3 step
#   patnew     --selected 15, pas de CV    -> tous les ecarts font 1 step
#   cvpattern  CV1 monte a CV_NOMINAL_MV a PAT_PULSE_MS et y reste : zone +10,
#              index 8 + 10 ecrete a 15 par patternIndexFor(), donc OLD -> NEW
# Le critere de cvpattern est le NOMBRE D'ONSETS du step ou l'index change :
#   1  le ratchet vient du template dont le contenu est lu (P35 tenue)
#   3  le ratchet TRIPLET en cache vient de OLD, le contenu de NEW (P35 violee)
# La frontiere est anchree par D79 : le premier front est l'onset arme du
# step 0, et sous OLD chaque step emet trois onsets, donc les frontieres sont
# les fronts d'indice multiple de trois. Le step qui precede la bascule doit
# encore jouer OLD, sinon le verdict est INVALID : la bascule a eu lieu avant.
# Leviers : PAT_PULSE_MS (defaut 8137), PAT_MIN_LATENCY_MS (defaut 30 : sous ce
# delai la zone peut ne pas etre relue a la frontiere, verdict INVALID).
# Ces trois courses sont NOMINALES depuis STEP-8.7, 2026-09-03 : P35 est
# implementee par B6 (ADR 0011), le service qui charge le tampon rafraichit le
# cache du canal dans le bloc qui publie le chargement. Avant B6, cvpattern
# lisait 3 onsets sur la production ; avec B6 elle lit 1, et le mutant M-B6
# (run-mutation-probe.py, suite probe-cvpattern) la rougit a 3. La suite P35
# seule : COURSES="patold patnew cvpattern" ./tools/run-trigger-probe.sh
PAT_OLD=8
PAT_NEW=15
PAT_STEPS="0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15"
PAT_TRIPLETS="0/7,1/7,2/7,3/7,4/7,5/7,6/7,7/7,8/7,9/7,10/7,11/7,12/7,13/7,14/7,15/7"
PAT_PULSE_MS="${PAT_PULSE_MS:-8137}"
PAT_MIN_LATENCY_MS="${PAT_MIN_LATENCY_MS:-30}"
# Course CVSTEP (STEP-12, 2026-09-03) : la consommation PHYSIQUE de STEP sur les
# six sorties. Six INSTANCES distinctes (--instance du generateur, ce que le canal
# joue sans routage PATTERN), longueur 16, aucun ratchet, CV1 -> STEP tenu a
# CV_NOMINAL_MV : zone +10 des la premiere frontiere. L'oracle est a PHASE CONNUE :
# l'onset arme de PLAY (D79) fixe n = 0, lu au step 0 a zone NULLE ; la frontiere
# n >= 1 porte un onset ssi (n mod 16) est dans l'HORAIRE LITTERAL de la sortie.
# Les horaires sont derives UNE FOIS a la main (WORKPLAN 12.1) : le harnais ne
# recopie ni readStepFor(), ni le quantizer, ni le decalage. STEP_EXPECTED_OFFSET
# choisit la table : 10 (nominal) ou 0 (CP1). CVSTEP_SWAP=a:b echange les tables
# de deux sorties (CP4). Aucun critere ne porte sur le nombre global de fronts.
CVSTEP_INSTANCES="--instance 0:0,3,4,9,15 --instance 1:1,2,7,12 --instance 2:0,5,6,11,13 --instance 3:2,8,9,10 --instance 4:4,7,14 --instance 5:1,6,12,13,15"
CVSTEP_TABLE_0="0,3,4,9,15;1,2,7,12;0,5,6,11,13;2,8,9,10;4,7,14;1,6,12,13,15"
CVSTEP_TABLE_10="5,6,9,10,15;2,7,8,13;1,3,6,11,12;0,8,14,15;4,10,13;2,3,5,7,12"
CVSTEP_STEP0="1;0;1;0;0;0"
STEP_EXPECTED_OFFSET="${STEP_EXPECTED_OFFSET:-10}"
CVSTEP_SWAP="${CVSTEP_SWAP:-}"
CVSTEP_TOL_MS="${CVSTEP_TOL_MS:-25}"
# Course EXTCLOCK (lot XCLK, 2026-09-03) : l'horloge EXTERNE, que RIEN n'exercait
# — ni une sonde, ni le materiel (dette du lot 5). Le harnais injecte un creneau
# sur PD2 (EXT_PIN vaut 2) et l'image porte --clock-source, donc le firmware
# demarre le transport a la PREMIERE impulsion : PLAY est inerte hors horloge
# interne (src/hal/TransportAdapter.cpp, fidele a Gravity.ino:321-322).
#
# ⚠️ ELLE N'EST PAS NOMINALE A L'ETAPE XCLK.2b : ses criteres arrivent en 2c, et
# une course sans critere qui compterait pour verte serait pire qu'aucune course.
# Lancer : COURSES=extclock ./tools/run-trigger-probe.sh
#
# ⚠️ LA FENETRE DE STABILISATION EST DERIVEE, PAS SUPPOSEE. uClock lisse
# l'intervalle externe par une PLL, PLL_X = 220, donc le residu vaut
# (220/256)^n apres n impulsions : 20 impulsions pour 5 %, 31 pour 1 %, 46 pour
# 0,1 %. EXT_DISCARD porte ce nombre, et sa valeur par defaut est 31.
EXT_PPQN="${EXT_PPQN:-24}"
EXT_DISCARD="${EXT_DISCARD:-31}"
# ⚠️ LA FENETRE DE MESURE SE COMPTE EN PAS, PAS EN IMPULSIONS. Une impulsion
# d'entree vaut 96 / ppqn ticks de sortie, donc un pas de /1 vaut ppqn
# impulsions : a PPQN 24, quarante impulsions ne font que 1,7 pas, et aucun
# critere de train ne tient sur 1,7 pas. EXT_DISCARD reste en impulsions, lui,
# parce que la PLL de uClock converge PAR IMPULSION.
EXT_MEASURE_STEPS="${EXT_MEASURE_STEPS:-20}"
EXT_START_MS="${EXT_START_MS:-300}"
EXT_PULSE_US="${EXT_PULSE_US:-1000}"
# EXT_TRACE_MS pose le TEMOIN DU TIMER : la sonde lit OCR1A et le prediviseur
# de TCCR1B, donc la periode que le timer applique VRAIMENT, et n imprime que
# les changements. 0 le desactive.
EXT_TRACE_MS="${EXT_TRACE_MS:-0}"
# CONTRE-EPREUVES de la course. EXT_EXPECT_PERIOD_US decouple l ATTENDU de
# l INJECTE : sans ce levier, une seule variable nourrit les deux cotes et la
# course confirmerait son hypothese. EXT_PIN_FORCE injecte sur une autre broche,
# donc le firmware ne recoit aucune horloge et la mesure devient non evaluable.
EXT_EXPECT_PERIOD_US="${EXT_EXPECT_PERIOD_US:-}"
EXT_PIN_FORCE="${EXT_PIN_FORCE:-}"
# La borne de C3 est DERIVEE de PHASE_FACTOR, 16 >> 8, donc 6,25 %. La surcharge
# existe pour la contre-epreuve, comme JITTER_BUDGET_PCT sur les autres courses.
EXT_C3_BOUND_PCT="${EXT_C3_BOUND_PCT:-}"
# cvstep est NOMINALE depuis STEP-12.4, 2026-09-03 : onze courses.
# extclock est NOMINALE depuis XCLK.2c, 2026-09-03, a PPQN 24 SEULEMENT : elle
# demande 15 s a cette cadence, contre 31, 50 et 87 s a PPQN 4, 2 et 1. Ces trois
# cadences restent a la demande — une porte trop couteuse finit par ne plus etre
# lancee, et elle ne protege alors plus rien.
COURSES="${COURSES:-clock seq ratchet cvzero cv1length cv2length cvreset patold patnew cvpattern cvstep extclock}"

# La periode et le code de source se DERIVENT du PPQN d'entree et du tempo.
# Rien n'est recopie : 60 000 000 / (tempo x ppqn) est la definition du PPQN.
case "$EXT_PPQN" in
  24) EXT_SOURCE_CODE=1 ;;
  4)  EXT_SOURCE_CODE=2 ;;
  2)  EXT_SOURCE_CODE=3 ;;
  1)  EXT_SOURCE_CODE=4 ;;
  *)  EXT_SOURCE_CODE="" ;;
esac
EXT_PERIOD_US="${EXT_PERIOD_US:-$(( 60000000 / (TEMPO * EXT_PPQN) ))}"

# ⚠️ LE GEL DU DEFAUT 12 doit etre ABSORBE avant toute mesure, et il est DERIVE,
# pas suppose. docs/upstream-defects.md entree 12 : au passage en horloge
# externe, uClock ecrete le tempo a MIN_BPM = 1, donc un tick de sortie de
# 60000000 / 96 / 1 = 625000 us, et la recuperation attend mod_clock_ref ticks,
# soit 96 / input_ppqn. Le proprietaire a decide le 2026-09-03 de CONSIGNER ce
# defaut sans toucher au fork : la course doit donc vivre avec.
EXT_MIN_BPM_TICK_US=$(( 60000000 / 96 / 1 ))
# ⚠️ Surchargeable, et c'est la CONTRE-EPREUVE de C5 : EXT_FREEZE_MS=0 ouvre la
# fenetre pendant le gel, donc le temoin doit y voir le tick de 625 ms et rougir.
EXT_FREEZE_MS="${EXT_FREEZE_MS:-$(( (96 / EXT_PPQN) * EXT_MIN_BPM_TICK_US / 1000 ))}"

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

# --- 2bis. Garde de la course EXTCLOCK --------------------------------------
# Elle refuse AVANT de simuler : une course trop courte rendrait une fenetre de
# mesure vide, et un verdict sur une fenetre vide se lit comme un succes.
case " $COURSES " in
  *" extclock "*)
    [ -n "$EXT_SOURCE_CODE" ] || die "EXT_PPQN accepte 24, 4, 2 ou 1 — recu '$EXT_PPQN'." 2
    EXT_STEP_MS=$(( EXT_PERIOD_US * EXT_PPQN / 1000 ))
    EXT_NEEDED_MS=$(( EXT_START_MS + EXT_FREEZE_MS \
                      + EXT_DISCARD * EXT_PERIOD_US / 1000 \
                      + EXT_MEASURE_STEPS * EXT_STEP_MS ))
    EXT_NEEDED_S=$(( (EXT_NEEDED_MS + 1999) / 1000 ))
    if [ "$DURATION" -lt "$EXT_NEEDED_S" ]; then
      die "extclock a PPQN $EXT_PPQN demande DURATION >= $EXT_NEEDED_S s : gel de $EXT_FREEZE_MS ms (defaut 12), puis $EXT_DISCARD impulsions jetees et $EXT_MEASURE_STEPS pas mesures de $EXT_STEP_MS ms, depart $EXT_START_MS ms. DURATION vaut $DURATION." 2
    fi
    printf '  %s✅%s garde extclock         %sPPQN %s, source %s, periode %s us, gel %s ms, DURATION %s s >= %s s%s\n' \
      "$C_OK" "$C_0" "$C_DIM" "$EXT_PPQN" "$EXT_SOURCE_CODE" "$EXT_PERIOD_US" \
      "$EXT_FREEZE_MS" "$DURATION" "$EXT_NEEDED_S" "$C_0"
    ;;
esac

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
    patold|cvpattern)
      GEN_ARGS="--mode seq --cv-target ${CV_TARGET_FORCED:-1:1} --selected $PAT_OLD" ;;
    patnew)
      GEN_ARGS="--mode seq --cv-target ${CV_TARGET_FORCED:-1:1} --selected $PAT_NEW" ;;
    cvstep)
      GEN_ARGS="--mode seq --cv-target ${CV_TARGET_FORCED:-1:4} $CVSTEP_INSTANCES" ;;
    extclock)
      GEN_ARGS="--mode seq --clock-source ${EXT_SOURCE_CODE_FORCED:-$EXT_SOURCE_CODE}" ;;
  esac
  case "$MODE" in
    patold|patnew|cvpattern)
      GEN_ARGS="$GEN_ARGS --template $PAT_OLD:$PAT_STEPS:$PAT_TRIPLETS --template $PAT_NEW:$PAT_STEPS" ;;
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
    patold|patnew|cvpattern)
      PROBE_MODE="cvpattern"; PROBE_CV="$CV_ZERO_MV $CV_ZERO_MV" ;;
    cvstep)
      PROBE_MODE="cvstep"; PROBE_CV="$CV_NOMINAL_MV $CV_ZERO_MV" ;;
  esac
  COURSE_NAME="$MODE"
  PROBE_ENV=""
  if [ "$MODE" = "cvreset" ]; then
    PROBE_ENV="RESET_PULSE_MV=$RESET_PULSE_MV RESET_PULSE_MS=$RESET_PULSE_MS RESET_PULSE_SOURCE=$RESET_PULSE_SOURCE"
  fi
  if [ "$MODE" = "extclock" ]; then
    PROBE_MODE="extclock"
    # Le temoin est OBLIGATOIRE pour cette course : C5 le lit. On borne le
    # nombre d'echantillons pour rester sous la capacite du harnais.
    [ "$EXT_TRACE_MS" = "0" ] && EXT_TRACE_MS=$(( DURATION * 1000 / 400 + 1 ))
    PROBE_ENV="EXT_PERIOD_US=$EXT_PERIOD_US EXT_PULSE_US=$EXT_PULSE_US EXT_START_MS=$EXT_START_MS EXT_TRACE_MS=$EXT_TRACE_MS"
    [ -n "$EXT_PIN_FORCE" ] && PROBE_ENV="$PROBE_ENV EXT_PIN=$EXT_PIN_FORCE"
  fi
  if [ "$MODE" = "cvpattern" ]; then
    # Le CV monte et RESTE haut : la largeur depasse la duree de la course.
    PROBE_ENV="RESET_PULSE_MV=$CV_NOMINAL_MV RESET_PULSE_MS=$PAT_PULSE_MS RESET_PULSE_WIDTH_MS=$(( DURATION * 1000 )) RESET_PULSE_SOURCE=1"
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
    PAT_COURSE="$COURSE_NAME" PAT_PULSE_MS="$PAT_PULSE_MS" PAT_MIN_LATENCY_MS="$PAT_MIN_LATENCY_MS" \
    CVSTEP_TABLE_0="$CVSTEP_TABLE_0" CVSTEP_TABLE_10="$CVSTEP_TABLE_10" CVSTEP_STEP0="$CVSTEP_STEP0" \
    STEP_EXPECTED_OFFSET="$STEP_EXPECTED_OFFSET" CVSTEP_SWAP="$CVSTEP_SWAP" CVSTEP_TOL_MS="$CVSTEP_TOL_MS" \
    EXT_PPQN="$EXT_PPQN" EXT_PERIOD_US="$EXT_PERIOD_US" EXT_DISCARD="$EXT_DISCARD" \
    EXT_MEASURE_STEPS="$EXT_MEASURE_STEPS" TEMPO="$TEMPO" \
    EXT_EXPECT_PERIOD_US="$EXT_EXPECT_PERIOD_US" EXT_C3_BOUND_PCT="$EXT_C3_BOUND_PCT" \
    EXT_FREEZE_MS="$EXT_FREEZE_MS" EXT_START_MS="$EXT_START_MS" \
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
cvpattern = mode == "cvpattern"
cvstep = mode == "cvstep"
pat_course = os.environ["PAT_COURSE"]
raw_edges = cvreset or cvpattern or cvstep
step_tol = float(os.environ["STEP_TOLERANCE_PCT"])

m = re.search(r"RESULTAT (.*)", txt)
if not m:
    print(f"  {mark(False)} sortie de la sonde illisible")
    print("".join(c for c in txt if 32 <= ord(c) < 127 or c == "\n"))
    sys.exit(1)

kv = dict(p.split("=", 1) for p in m.group(1).split())

# --- Course EXTCLOCK, lot XCLK.2c : CINQ criteres --------------------------
# Tous les attendus sont DERIVES. Le harnais ne recopie ni le quantizer de uClock,
# ni sa PLL : il en derive des bornes.
#
#   un pas de /1 vaut 96 ticks de sortie
#   une impulsion d'entree vaut 96 / ppqn ticks de sortie
#   donc  tick attendu = periode x ppqn / 96   et   pas attendu = periode x ppqn
#
# La fenetre s'ouvre APRES le gel du defaut 12 et APRES la convergence de la PLL,
# dont le residu vaut (220/256)^n apres n impulsions.
if pat_course == "extclock":
    injected_us = int(os.environ["EXT_PERIOD_US"])
    period_us = int(os.environ.get("EXT_EXPECT_PERIOD_US") or injected_us)
    ppqn = int(os.environ["EXT_PPQN"])
    discard = int(os.environ["EXT_DISCARD"])
    measure_steps = int(os.environ["EXT_MEASURE_STEPS"])
    freeze_ms = int(os.environ["EXT_FREEZE_MS"])
    start_ms = int(os.environ["EXT_START_MS"])
    length = int(os.environ["EXPECTED_LENGTH"])
    steps = [int(x) for x in os.environ["STEPS"].split(",")]

    tick_us = period_us * ppqn / 96.0
    step_us = float(period_us * ppqn)
    open_ms = start_ms + freeze_ms + discard * period_us / 1000.0
    close_ms = open_ms + measure_steps * step_us / 1000.0
    pll_residual = (220.0 / 256.0) ** discard
    c1_budget_pct = 100.0 * (pll_residual + 1.0 / 96.0)   # PLL + un tick sur 96
    c3_bound_pct = 100.0 / 16.0                            # PHASE_FACTOR 16 >> 8
    if os.environ.get("EXT_C3_BOUND_PCT"):
        c3_bound_pct = float(os.environ["EXT_C3_BOUND_PCT"])
        print(f"    {DIM}levier : borne C3 forcee a {c3_bound_pct:.2f} %{Z}")

    print(f"    PPQN d entree            : {ppqn}, periode injectee {injected_us} us")
    if period_us != injected_us:
        print(f"    {DIM}levier : l attendu est calcule sur {period_us} us,"
              f" pas sur l injecte{Z}")
    print(f"    pas attendu (derive)     : {step_us / 1000.0:.2f} ms"
          f"   tick {tick_us:.2f} us")
    print(f"    fenetre de mesure        : {open_ms:.0f} a {close_ms:.0f} ms"
          f"   (gel {freeze_ms} ms + {discard} impulsions)")
    print(f"    budget C1 derive         : {c1_budget_pct:.2f} %"
          f"   (residu PLL {100.0 * pll_residual:.2f} % + un tick)")

    em = re.search(r"^EDGES (.*)$", txt, re.M)
    edges = [float(x) for x in em.group(1).split()] if em else []
    win = [e for e in edges if open_ms <= e <= close_ms]

    # --- C4, la garde de la mesure : INVALID, jamais FAIL --------------------
    if len(win) < 3:
        print(f"  {mark(False)} C4 fenetre exploitable "
              f"{len(win)} front(s) dans la fenetre, il en faut 3")
        print(f"  {DIM}VERDICT INVALID : la course n a pas produit de fenetre"
              f" mesurable.{Z}")
        print(f"  {DIM}Ce n est PAS un defaut du firmware : c est une mesure"
              f" non evaluable.{Z}")
        sys.exit(5)
    print(f"  {mark(True)} C4 fenetre exploitable "
          f"{len(win)} fronts entre {win[0]:.0f} et {win[-1]:.0f} ms")

    # --- C5, le gel est TERMINE dans la fenetre ------------------------------
    tm = re.search(r"^TRACE (.*)$", txt, re.M)
    samples = []
    if tm:
        for pair in tm.group(1).split():
            at, per = pair.split(":")
            samples.append((float(at), float(per)))
    inside = [(a, p) for a, p in samples if open_ms <= a <= close_ms]
    if not inside:
        print(f"  {mark(False)} C5 gel termine "
              f"aucun echantillon du temoin dans la fenetre")
        print(f"  {DIM}VERDICT INVALID : sans temoin, C1 pourrait moyenner des"
              f" pas de 60 s.{Z}")
        sys.exit(5)
    worst = max(inside, key=lambda s: abs(s[1] - tick_us))
    off_pct = 100.0 * abs(worst[1] - tick_us) / tick_us
    c5 = off_pct <= c3_bound_pct
    print(f"  {mark(c5)} C5 gel termine       "
          f"pire tick {worst[1]:.0f} us a {worst[0]:.0f} ms, "
          f"{off_pct:.2f} % de l attendu (borne {c3_bound_pct:.2f} %)")

    # --- les ecarts, convertis en NOMBRE DE PAS ------------------------------
    gaps = [win[i + 1] - win[i] for i in range(len(win) - 1)]
    counts = [int(round(g * 1000.0 / step_us)) for g in gaps]
    total_steps = sum(counts)
    measured_step_us = (win[-1] - win[0]) * 1000.0 / total_steps if total_steps else 0.0

    # --- C1, la MOYENNE sur la fenetre --------------------------------------
    c1_off = 100.0 * abs(measured_step_us - step_us) / step_us
    c1 = c1_off <= c1_budget_pct
    print(f"  {mark(c1)} C1 pas moyen         "
          f"{measured_step_us / 1000.0:.2f} ms contre {step_us / 1000.0:.2f} "
          f"attendu, ecart {c1_off:.2f} % (budget {c1_budget_pct:.2f} %)")

    # --- C2, rotation cyclique du motif, sans phase --------------------------
    act = sorted(x for x in steps if x < length)
    expected = [(act[(i + 1) % len(act)] - act[i]) % length or length
                for i in range(len(act))] if len(act) > 1 else []
    # ⚠️ La fenetre peut contenir PLUS d ecarts qu un cycle : la suite attendue se
    # repete autant de fois qu il faut avant d etre tronquee. Sans cela un cycle
    # et demi rougirait un firmware correct.
    def cycle_from(i):
        need = -(-len(counts) // len(expected)) + 1
        rot = expected[i:] + expected[:i]
        return (rot * need)[:len(counts)]
    c2 = any(counts == cycle_from(i) for i in range(len(expected))) if expected else False
    print(f"  {mark(c2)} C2 rotation cyclique "
          f"{counts} contre {expected} a une rotation pres")

    # --- C3, l oscillation instantanee --------------------------------------
    devs = [100.0 * abs(g * 1000.0 - c * step_us) / (c * step_us)
            for g, c in zip(gaps, counts) if c > 0]
    c3_worst = max(devs) if devs else 0.0
    c3 = c3_worst <= c3_bound_pct
    print(f"  {mark(c3)} C3 oscillation       "
          f"pire ecart {c3_worst:.2f} % (borne derivee {c3_bound_pct:.2f} %)")

    sys.exit(0 if (c1 and c2 and c3 and c5) else 1)

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
# Courses PATOLD / PATNEW / CVPATTERN : la fixture P35. Chaque ecart est
# classe : 'T' pour 2/3 de step, le TRIPLET de OLD ; 'S' pour un step, le NEW
# sans ratchet ; '?' sinon. patold exige des 'T' seulement, patnew des 'S'
# seulement. cvpattern cherche la frontiere de bascule Tb et compte ses onsets.
pat_old_ok = pat_new_ok = pat_switch_ok = False
pat_invalid = ""
pat_count = -1
pat_delay = -1.0
pat_classes = ""
if cvpattern:
    edges_m = re.search(r"^EDGES((?:\s+\d+\.\d+)+)$", txt, re.M)
    edges = [float(x) for x in edges_m.group(1).split()] if edges_m else []
    tol = 0.05 * step_ms

    def classify(gap):
        if abs(gap - step_ms * 2.0 / 3.0) <= tol:
            return "T"
        if abs(gap - step_ms) <= tol:
            return "S"
        return "?"

    gaps = [classify(b - a) for a, b in zip(edges, edges[1:])]
    pat_classes = "".join(gaps)
    if pat_course == "patold":
        pat_old_ok = len(gaps) >= 6 and all(g == "T" for g in gaps)
        pattern_ok = pat_old_ok
    elif pat_course == "patnew":
        pat_new_ok = len(gaps) >= 6 and all(g == "S" for g in gaps)
        pattern_ok = pat_new_ok
    else:
        pulse_ms = float(os.environ["PAT_PULSE_MS"])
        min_latency = float(os.environ["PAT_MIN_LATENCY_MS"])
        pre = [i for i, e in enumerate(edges) if e < pulse_ms]
        pat_old_ok = len(pre) >= 6 and all(gaps[i] == "T" for i in pre[:-1])
        boundaries = [i for i in range(0, len(edges), 3) if edges[i] >= pulse_ms]
        if not pat_old_ok:
            pat_invalid = "OLD ne joue pas des triolets avant l'impulsion"
        elif not boundaries:
            pat_invalid = "aucune frontiere apres l'impulsion"
        else:
            ib = boundaries[0]
            pat_delay = edges[ib] - pulse_ms
            before = gaps[ib - 3:ib] if ib >= 3 else []
            if pat_delay < min_latency:
                pat_invalid = "frontiere a %.1f ms de l'impulsion, sous %.0f ms" % (
                    pat_delay, min_latency)
            elif before != ["T", "T", "T"]:
                pat_invalid = "le step avant la bascule ne joue plus OLD : %s" % "".join(before)
            else:
                after = gaps[ib:ib + 4]
                if after[:1] == ["S"]:
                    pat_count = 1
                    tail = gaps[ib:]
                elif after == ["T", "T", "T", "S"]:
                    pat_count = 3
                    tail = gaps[ib + 3:]
                elif after == ["T", "T", "T", "T"]:
                    pat_invalid = "aucune bascule observee : OLD continue apres l'impulsion"
                    tail = []
                else:
                    pat_invalid = "onsets du step de bascule illisibles : %s" % "".join(after)
                    tail = []
                if pat_count > 0:
                    pat_new_ok = len(tail) >= 6 and all(g == "S" for g in tail)
                    if not pat_new_ok:
                        pat_invalid = "NEW n'est pas adopte apres la bascule : %s" % "".join(tail[:12])
        pat_switch_ok = pat_count == 1 and pat_new_ok and not pat_invalid
        pattern_ok = pat_switch_ok

# Course CVSTEP : six horaires litteraux, un critere par sortie, puis six flux
# distincts. n = 0 est l'onset arme (D79), lu au step 0 a zone nulle ; n >= 1 suit
# la table choisie par STEP_EXPECTED_OFFSET. Aucun front hors horaire n'est tolere.
cvstep_rows = []
cvstep_ok = False
cvstep_distinct = False
cvstep_anchor = -1.0
if cvstep:
    tables = {"0": os.environ["CVSTEP_TABLE_0"], "10": os.environ["CVSTEP_TABLE_10"]}
    key = os.environ["STEP_EXPECTED_OFFSET"]
    if key not in tables:
        print(f"  {mark(False)} STEP_EXPECTED_OFFSET={key} : aucune table litterale pour cette valeur")
        sys.exit(2)
    table = [set(int(x) for x in part.split(",")) for part in tables[key].split(";")]
    step0 = [part == "1" for part in os.environ["CVSTEP_STEP0"].split(";")]
    swap = os.environ.get("CVSTEP_SWAP", "")
    if swap:
        a, b = (int(x) - 1 for x in swap.split(":"))
        table[a], table[b] = table[b], table[a]
        step0[a], step0[b] = step0[b], step0[a]
    tol = float(os.environ["CVSTEP_TOL_MS"])
    edges_by_out = []
    for i in range(1, 7):
        m_i = re.search(r"^EDGES%d((?:\s+\d+\.\d+)*)$" % i, txt, re.M)
        edges_by_out.append([float(x) for x in m_i.group(1).split()] if m_i else [])
    early = [e for es in edges_by_out for e in es if play_ms < e <= play_ms + 60.0]
    if early:
        cvstep_anchor = min(early)
        t_end = max((es[-1] for es in edges_by_out if es), default=cvstep_anchor)
        n_max = int((t_end - cvstep_anchor) / step_ms) - 1
        observed_sets = []
        all_ok = True
        for i, es in enumerate(edges_by_out):
            observed, extra = set(), 0
            for e in es:
                n = int(round((e - cvstep_anchor) / step_ms))
                if n < 0 or n > n_max or abs(e - cvstep_anchor - n * step_ms) > tol:
                    if n <= n_max:
                        extra += 1
                    continue
                observed.add(n)
            expected = set(n for n in range(1, n_max + 1) if (n % 16) in table[i])
            if step0[i]:
                expected.add(0)
            ok_i = observed == expected and extra == 0
            all_ok = all_ok and ok_i
            observed_sets.append(frozenset(observed))
            cvstep_rows.append((i + 1, ok_i, len(observed & expected), len(expected),
                                len(observed - expected) + extra, sorted(expected - observed)[:4]))
        cvstep_distinct = len(set(observed_sets)) == 6
        cvstep_ok = all_ok and cvstep_distinct and n_max >= 32
    pattern_ok = cvstep_ok

jit_pct = 100.0 * jit_max / step_ms if step_ms else 0.0
jit_ok = jit_pct <= budget_pct if not (ratchet or raw_edges) else True
step_measured = float(kv.get("step_mesure", "0"))
bpm = int(kv.get("bpm", "0"))
step_err_pct = abs(step_measured - step_ms) / step_ms * 100.0 if step_ms else 100.0
step_ok = ((step_measured > 0.0 and step_err_pct <= step_tol)
           if not (ratchet or raw_edges) else True)
duty = 100.0 * width / step_ms if step_ms else 0.0
duty_ok = duty < 50.0

print()
title = ("RATCHET — les sous-declenchements atteignent les broches" if ratchet
         else "CVRESET — un front re-origine la grille" if cvreset
         else "PATOLD — la fixture OLD joue ses triolets" if pat_course == "patold"
         else "PATNEW — la fixture NEW joue un onset par step" if pat_course == "patnew"
         else "CVPATTERN — P35 au step ou l'index PATTERN change" if cvpattern
         else "CVSTEP — STEP consomme sur les six sorties" if cvstep
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
elif pat_course == "patold":
    print(f"  {mark(pat_old_ok)} Triolets partout   {len(pat_classes)} ecarts {pat_classes[:24]}   "
          f"{DIM}— T = 2/3 step, le TRIPLET de OLD sur chaque step{Z}")
elif pat_course == "patnew":
    print(f"  {mark(pat_new_ok)} Un onset par step  {len(pat_classes)} ecarts {pat_classes[:24]}   "
          f"{DIM}— S = 1 step, NEW sans ratchet{Z}")
elif cvstep:
    if cvstep_anchor < 0:
        print(f"  {mark(False)} Ancrage D79        aucun front dans les 60 ms apres PLAY : "
              f"l'horaire n'a pas d'origine")
    for out, ok_i, hit, exp, wrong, missing in cvstep_rows:
        print(f"  {mark(ok_i)} OUT{out} horaire      {hit}/{exp} onsets a l'heure, {wrong} hors horaire"
              + (f"   {DIM}manquants n={missing}{Z}" if missing else "")
              + (f"   {DIM}— table +{os.environ['STEP_EXPECTED_OFFSET']}, n=0 = onset arme{Z}" if out == 1 else ""))
    print(f"  {mark(cvstep_distinct)} Six flux distincts "
          f"{DIM}— une duplication ou une permutation de sortie se voit ici{Z}")
elif cvpattern:
    print(f"  {mark(pat_old_ok)} OLD avant          "
          f"{DIM}— triolets jusqu'a l'impulsion a {os.environ['PAT_PULSE_MS']} ms{Z}")
    print(f"  {mark(pat_count == 1)} Onsets a la bascule  "
          f"{pat_count if pat_count > 0 else '?'}   "
          f"{DIM}— frontiere a {pat_delay:.1f} ms de l'impulsion ; 1 = P35 tenue, "
          f"3 = ratchet de OLD sur le contenu de NEW{Z}")
    print(f"  {mark(pat_new_ok)} NEW apres          "
          f"{DIM}— un onset par step des la frontiere suivante{Z}")
    if pat_invalid:
        print(f"  {ERR}INVALID{Z} {pat_invalid}   {DIM}ecarts : {pat_classes[:40]}{Z}")
elif seq:
    print(f"  {mark(pattern_ok)} Motif joue         {gaps_ok}/{gaps_total} ecarts   "
          f"{DIM}— la suite est une rotation cyclique de celle du motif{Z}")
else:
    print(f"  {mark(pattern_ok)} Train regulier     {gaps_ok}/{gaps_total} ecarts   "
          f"{DIM}— un par step, a 5 % de la mediane ; le motif charge est ignore{Z}")
print(f"  {mark(silent_before_play)} Silence au demarrage  premier front a {first_ms:.0f} ms   "
      f"{DIM}— PLAY relache a {play_ms:.0f} ms ; le module demarre a l arret{Z}")
if not cvstep:
    print(f"  {mark(same and coincident)} Six lignes en phase "
          f"{'meme compte, fronts < 200 us' if (same and coincident) else 'DESACCORD'}")
if not ratchet and not raw_edges:
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

phase_ok = True if cvstep else (same and coincident)
ok = (all_lines and pattern_ok and phase_ok and jit_ok and duty_ok
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
    elif cvstep:
        print(f"\n  Les six sorties jouent chacune leur instance, lue au step decale par le CV STEP.")
        print(f"  {DIM}Le chemin CV -> zone STEP -> readStep -> contenu -> broche est exerce sur les six.{Z}")
    elif cvpattern and pat_course == "cvpattern":
        print(f"\n  Au step ou l'index PATTERN change, le contenu et le ratchet viennent "
              f"du meme template.")
        print(f"  {DIM}P35 est tenue sur les broches, sur la fixture OLD -> NEW.{Z}")
    elif cvpattern:
        print(f"\n  La fixture {pat_course.upper()} joue ce que son template porte.")
    elif seq:
        print(f"\n  Les six sorties jouent le motif ecrit, en phase, au bon tempo.")
        print(f"  {DIM}Le chemin contenu du pattern -> sortie est exerce de bout en bout.{Z}")
    else:
        print(f"\n  Les six sorties emettent le train CLOCK, en phase, au bon tempo.")
        print(f"  {DIM}Le motif est dans la banque et n'est pas joue : c'est le{Z}")
        print(f"  {DIM}comportement de l'original.{Z}")
elif cvpattern and pat_invalid:
    print(f"\n  {ERR}INVALID{Z} — la course ne permet pas de conclure : {pat_invalid}")
    sys.exit(5)
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
