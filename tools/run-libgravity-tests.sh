#!/usr/bin/env bash
#
# Tests de CARACTERISATION de la dependance libGravity, figee au commit
# 9be88be1f4 (env PlatformIO `native_libgravity`).
#
# Ce ne sont PAS des tests d'acceptation FlexSeq. Ils decrivent le comportement
# REEL de la dependance, anomalies incluses. Certains echouent donc par
# construction : c'est le resultat attendu, et l'affaiblir serait masquer un
# defaut de la dependance (regle CLAUDE.md).
#
# Ce que ce script verifie reellement : que l'ensemble des assertions en echec
# est EXACTEMENT celui qui a ete audite. Un rouge attendu n'est pas un signal ;
# un rouge qui CHANGE en est un. D'ou la comparaison a une reference explicite.
#
#   - une assertion auditee qui se met a passer  -> la dependance a bouge
#   - une assertion inattendue qui echoue        -> regression ou nouvelle anomalie
#   - un test qui ne compile plus                -> echec dur (cas de f0d6134)
#
# Le transcript brut de `pio test` est volontairement MASQUE par defaut : il perd
# ses couleurs des que la sortie est capturee, et ses lignes « ERRORED » et
# « Program received signal SIGINT » sont trompeuses (voir plus bas). Le script
# lit les assertions et produit son propre rapport.
#
# Usage :
#   ./tools/run-libgravity-tests.sh            # rapport + verdict
#   ./tools/run-libgravity-tests.sh -l         # liste les tests
#   VERBOSE=1 ./tools/run-libgravity-tests.sh  # + transcript pio brut
#   VERBOSE=2 ./tools/run-libgravity-tests.sh  # + transcript pio en -vvv
#
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# Localise pio : PATH d'abord, sinon l'installation PlatformIO par defaut.
if command -v pio >/dev/null 2>&1; then
  PIO="pio"
elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then
  PIO="$HOME/.platformio/penv/bin/pio"
else
  echo "erreur : 'pio' introuvable (ni dans le PATH, ni dans ~/.platformio/penv/bin)." >&2
  echo "Installe PlatformIO Core : https://docs.platformio.org/en/latest/core/installation/" >&2
  exit 127
fi

# Couleurs seulement si la sortie est un terminal.
if [ -t 1 ]; then
  C_OK=$'\033[32m'; C_WARN=$'\033[33m'; C_ERR=$'\033[31m'
  C_DIM=$'\033[2m';  C_B=$'\033[1m';    C_0=$'\033[0m'
else
  C_OK=""; C_WARN=""; C_ERR=""; C_DIM=""; C_B=""; C_0=""
fi

# -----------------------------------------------------------------------------
# Reference auditee : assertions dont l'echec EST le comportement documente.
# Identifiees par nom de test, jamais par numero de ligne (trop fragile).
# Format : <nom du test>|<anomalie libGravity documentee dans CLAUDE.md>
# -----------------------------------------------------------------------------
EXPECTED=$(cat <<'LIST'
test_negative_to_positive_crossing_is_rising_edge|AnalogInput::IsRisingEdge — croisement negatif -> positif rate (etat signe/non signe)
test_negative_to_positive_crossing_with_positive_threshold_is_rising_edge|AnalogInput::IsRisingEdge — idem avec un seuil positif
test_new_encoder_has_safe_initial_state|Encoder — etat initial non sur
test_new_encoder_process_without_movement_does_not_report_rotation|Encoder — faux premier mouvement rapporte
test_init_starts_output_off|DigitalOutput::Init — ne garantit pas l'etat OFF
test_reinit_turns_active_output_off|DigitalOutput::Init — ne coupe pas une sortie deja active
test_release_during_debounce_is_reported_after_stable_window|Button — relachement perdu dans la fenetre de debounce
LIST
)
EXPECTED_NAMES="$(printf '%s\n' "$EXPECTED" | sed '/^$/d' | cut -d'|' -f1 | sort -u)"

# Les en-tetes libGravity sont lues depuis les libdeps de l'env AVR : il doit
# avoir ete construit au moins une fois.
LIBDEPS="$REPO_ROOT/.pio/libdeps/nanoatmega328/libGravity/src"
if [ ! -d "$LIBDEPS" ]; then
  echo "libdeps libGravity absentes ($LIBDEPS) -> construction de l'env AVR..."
  if ! "$PIO" run -e nanoatmega328; then
    echo "erreur : impossible de recuperer libGravity via l'env nanoatmega328." >&2
    exit 1
  fi
fi

ARGS=("test" "-e" "native_libgravity")
[ "${VERBOSE:-0}" = "2" ] && ARGS+=("-vvv")

if [ "${1:-}" = "-l" ] || [ "${1:-}" = "--list" ]; then
  ARGS+=("--list-tests")
  echo "==> $PIO ${ARGS[*]}"
  exec "$PIO" "${ARGS[@]}"
fi

LOG="$(mktemp -t flexseq-carac)"
trap 'rm -f "$LOG"' EXIT

echo "${C_DIM}==> $PIO ${ARGS[*]}${C_0}"
if [ "${VERBOSE:-0}" = "0" ]; then
  "$PIO" "${ARGS[@]}" >"$LOG" 2>&1
else
  "$PIO" "${ARGS[@]}" 2>&1 | tee "$LOG"
fi

# -----------------------------------------------------------------------------
# Echec dur : un test qui ne compile plus. C'est exactement la panne silencieuse
# de f0d6134, ou la perte de deux -I avait rendu ces tests non compilables.
# -----------------------------------------------------------------------------
if grep -qE 'fatal error|Building stage has failed' "$LOG"; then
  echo
  echo "${C_ERR}${C_B}❌ COMPILATION — au moins un test de caracterisation ne compile plus.${C_0}"
  grep -E 'fatal error|error:' "$LOG" | head -10
  exit 1
fi

# Assertions, normalisees en « module nom verdict ».
ASSERTIONS="$(sed -nE 's#^[[:space:]]*test/([^/]+)/[^:]+:[0-9]+: ([A-Za-z0-9_]+):?.*\[(PASSED|FAILED)\].*#\1 \2 \3#p' "$LOG")"

if [ -z "$ASSERTIONS" ]; then
  echo
  echo "${C_ERR}${C_B}❌ Aucune assertion lue dans la sortie de pio — format inattendu.${C_0}"
  echo "   Relancer avec VERBOSE=1 pour voir le transcript brut."
  exit 1
fi

OBSERVED="$(printf '%s\n' "$ASSERTIONS" | awk '$3=="FAILED"{print $2}' | sort -u)"
NEW="$(comm -13 <(printf '%s\n' "$EXPECTED_NAMES") <(printf '%s\n' "$OBSERVED"))"
FIXED="$(comm -23 <(printf '%s\n' "$EXPECTED_NAMES") <(printf '%s\n' "$OBSERVED"))"

n_total=$(printf '%s\n' "$ASSERTIONS" | grep -c .)
n_fail=$(printf '%s\n' "$ASSERTIONS" | awk '$3=="FAILED"' | grep -c .)
n_pass=$(( n_total - n_fail ))

echo
echo "${C_B}========== CARACTERISATION libGravity @ 9be88be1f4 ==========${C_0}"
echo

# Recap par module, dans l'ordre d'execution. Un module est :
#   ✅ aucune assertion en echec
#   ⚠️  uniquement des anomalies auditees (resultat attendu)
#   ❌ au moins un echec hors reference
printf '%s\n' "$ASSERTIONS" | awk '!seen[$1]++{print $1}' | while read -r mod; do
  m_all=$(printf '%s\n' "$ASSERTIONS" | awk -v m="$mod" '$1==m' | grep -c .)
  m_fail_names=$(printf '%s\n' "$ASSERTIONS" | awk -v m="$mod" '$1==m && $3=="FAILED"{print $2}')
  m_fail=$(printf '%s\n' "$m_fail_names" | grep -c .)
  m_ok=$(( m_all - m_fail ))
  m_unexpected=0
  for n in $m_fail_names; do
    printf '%s\n' "$EXPECTED_NAMES" | grep -qx "$n" || m_unexpected=$((m_unexpected + 1))
  done
  if [ "$m_unexpected" -gt 0 ]; then
    [ "$m_unexpected" -gt 1 ] && w="echecs INATTENDUS" || w="echec INATTENDU"
    printf '  %s❌ %-22s %2d/%-2d  %d %s%s\n' \
      "$C_ERR" "$mod" "$m_ok" "$m_all" "$m_unexpected" "$w" "$C_0"
  elif [ "$m_fail" -gt 0 ]; then
    [ "$m_fail" -gt 1 ] && w="anomalies auditees" || w="anomalie auditee"
    printf '  %s⚠  %-22s %2d/%-2d  %d %s%s\n' \
      "$C_WARN" "$mod" "$m_ok" "$m_all" "$m_fail" "$w" "$C_0"
  else
    printf '  %s✅ %-22s %2d/%-2d%s\n' "$C_OK" "$mod" "$m_ok" "$m_all" "$C_0"
  fi
done

# Detail des anomalies reproduites, avec ce qu'elles documentent.
if [ -n "$OBSERVED" ]; then
  echo
  echo "  ${C_DIM}Anomalies reproduites — echec ATTENDU, ne pas corriger :${C_0}"
  printf '%s\n' "$EXPECTED" | sed '/^$/d' | while IFS='|' read -r n label; do
    printf '%s\n' "$OBSERVED" | grep -qx "$n" || continue
    printf '    %s·%s %s\n' "$C_WARN" "$C_0" "$label"
    printf '      %s%s%s\n' "$C_DIM" "$n" "$C_0"
  done
fi

status=0

if [ -n "$NEW" ]; then
  echo
  echo "  ${C_ERR}${C_B}❌ ECHEC INATTENDU — hors reference auditee :${C_0}"
  printf '     %s\n' $NEW
  echo "     -> soit une regression, soit une anomalie non encore documentee."
  echo "     -> documenter dans CLAUDE.md puis ajouter a EXPECTED dans ce script."
  status=1
fi

if [ -n "$FIXED" ]; then
  echo
  echo "  ${C_ERR}${C_B}❌ ANOMALIE DISPARUE — assertion auditee qui passe desormais :${C_0}"
  printf '     %s\n' $FIXED
  echo "     -> la dependance a change, ou le test a ete affaibli."
  echo "     -> verifier le commit epingle avant de retirer l'entree."
  status=1
fi

echo
if [ "$status" = "0" ]; then
  printf '  %s%s✅ Conforme au comportement audite%s : %d anomalies auditees reproduites,\n' \
    "$C_OK" "$C_B" "$C_0" "$(printf '%s\n' "$EXPECTED_NAMES" | grep -c .)"
  printf '     aucune de plus, aucune de moins. %s(%d assertions, %d vertes)%s\n' \
    "$C_DIM" "$n_total" "$n_pass" "$C_0"
fi
echo "${C_B}=============================================================${C_0}"

# Rappel : sur platform=native, `pio test` presente le code de sortie du binaire
# Unity (= nombre d'assertions en echec) comme un signal — « SIGINT
# (Interrupt: 2) » pour 2 echecs, « SIGHUP (Hangup: 1) » pour 1. Ce n'est pas un
# crash, et le statut ERRORED qui en decoule n'est pas exploitable : c'est
# pourquoi ce script lit les assertions plutot que le tableau de pio.
#
# Son total ne l'est pas davantage : pio annonce « 69 test cases » pour 65
# assertions reelles, en ajoutant une pseudo-assertion par module ERRORED.
# Les chiffres rapportes ici sont comptes sur les assertions elles-memes.

exit "$status"
