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
# Usage :
#   ./tools/run-libgravity-tests.sh            # suite complete + verdict
#   ./tools/run-libgravity-tests.sh -l         # liste les tests
#   VERBOSE=1 ./tools/run-libgravity-tests.sh  # sortie pio detaillee
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

# -----------------------------------------------------------------------------
# Reference auditee : assertions dont l'echec EST le comportement documente.
# Identifiees par nom de test, jamais par numero de ligne (trop fragile).
# Chaque entree renvoie a une anomalie listee dans CLAUDE.md.
# -----------------------------------------------------------------------------
EXPECTED_FAILURES=$(cat <<'LIST'
test_init_starts_output_off
test_negative_to_positive_crossing_is_rising_edge
test_negative_to_positive_crossing_with_positive_threshold_is_rising_edge
test_new_encoder_has_safe_initial_state
test_new_encoder_process_without_movement_does_not_report_rotation
test_reinit_turns_active_output_off
test_release_during_debounce_is_reported_after_stable_window
LIST
)

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
[ "${VERBOSE:-0}" = "1" ] && ARGS+=("-vvv")

if [ "${1:-}" = "-l" ] || [ "${1:-}" = "--list" ]; then
  ARGS+=("--list-tests")
  echo "==> $PIO ${ARGS[*]}"
  exec "$PIO" "${ARGS[@]}"
fi

LOG="$(mktemp -t flexseq-carac)"
trap 'rm -f "$LOG"' EXIT

echo "==> $PIO ${ARGS[*]}"
"$PIO" "${ARGS[@]}" 2>&1 | tee "$LOG"

# -----------------------------------------------------------------------------
# Echec dur : un test qui ne compile plus. C'est exactement la panne silencieuse
# de f0d6134, ou la perte de deux -I avait rendu ces tests non compilables.
# -----------------------------------------------------------------------------
if grep -qE 'fatal error|Building stage has failed' "$LOG"; then
  echo
  echo "❌ COMPILATION — au moins un test de caracterisation ne compile plus."
  grep -E 'fatal error|error:' "$LOG" | head -10
  exit 1
fi

# `pio test` sur platform=native rapporte le code de sortie du binaire Unity
# (= nombre d'assertions en echec) comme un signal : « SIGINT (Interrupt: 2) »
# pour 2 echecs, « SIGHUP (Hangup: 1) » pour 1. Ce n'est pas un crash, et le
# statut ERRORED qui en decoule n'est donc pas exploitable : on lit les
# assertions elles-memes.
OBSERVED="$(sed -nE 's/^[^:]+:[0-9]+: ([A-Za-z0-9_]+):.*\[FAILED\].*/\1/p' "$LOG" | sort -u)"
EXPECTED="$(printf '%s\n' "$EXPECTED_FAILURES" | sed '/^$/d' | sort -u)"

NEW="$(comm -13 <(printf '%s\n' "$EXPECTED") <(printf '%s\n' "$OBSERVED"))"
FIXED="$(comm -23 <(printf '%s\n' "$EXPECTED") <(printf '%s\n' "$OBSERVED"))"

echo
echo "========== CARACTERISATION libGravity @ 9be88be1f4 =========="
printf "  anomalies auditees attendues : %s\n" "$(printf '%s\n' "$EXPECTED" | grep -c . )"
printf "  assertions en echec observees : %s\n" "$(printf '%s\n' "$OBSERVED" | grep -c . )"

status=0

if [ -n "$NEW" ]; then
  echo
  echo "❌ ECHEC INATTENDU — assertion en echec hors reference auditee :"
  printf '     %s\n' $NEW
  echo "     -> soit une regression, soit une anomalie non encore documentee."
  echo "     -> documenter dans CLAUDE.md puis ajouter a EXPECTED_FAILURES."
  status=1
fi

if [ -n "$FIXED" ]; then
  echo
  echo "❌ ANOMALIE DISPARUE — assertion auditee qui passe desormais :"
  printf '     %s\n' $FIXED
  echo "     -> la dependance a change, ou le test a ete affaibli."
  echo "     -> verifier le commit epingle avant de retirer l'entree."
  status=1
fi

if [ "$status" = "0" ]; then
  echo
  echo "✅ Conforme au comportement audite : les 7 anomalies connues sont"
  echo "   reproduites, aucune de plus, aucune de moins."
fi
echo "============================================================"

exit "$status"
