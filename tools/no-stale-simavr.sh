# Refuse de mesurer quand une course anterieure du simulateur est encore vivante.
#
# Pourquoi ce fichier existe. Le 2026-09-09 la sonde de gestes a rendu FAIL sur
# deux criteres, et le rapport a designe un changement du rendu. Deux courses
# rejouees dans un environnement de processus verifie propre rendent PASS sur le
# MEME commit. Le code n'etait donc pas la cause, et la cause n'est toujours pas
# etablie. Ce qui est etabli est plus etroit, et il suffit pour agir : la sonde
# ne controle pas son environnement d'execution, et elle ne le verifie pas.
#
# C'est la regle de methode que docs/open-risks.md porte deja : un outil ne doit
# pas dependre d'un etat qu'il ne controle pas. Un harnais reste vivant apres une
# course interrompue, il garde le port, le fichier de travail ou le temps CPU, et
# la course suivante mesure autre chose que le firmware.
#
# La sortie 6 est reservee a ce refus. Elle ne se confond ni avec 1 (un defaut du
# firmware), ni avec 5 (une mesure non evaluable a cause de l'etat du firmware),
# ni avec 2 ou 4 (un argument refuse). Le refus NOMME sa cause, donc il ne peut
# pas etre lu comme un defaut du module.
#
# Usage, apres la definition de ROOT :
#   . "$ROOT/tools/no-stale-simavr.sh"
#   refuse_stale_simavr
#
# ALLOW_STALE_SIMAVR=1 leve le refus, pour une course parallele deliberee.

FLEXSEQ_SIMAVR_HARNESSES="blocking_probe cv_capture_probe drift_probe
eeprom_boundary_probe gesture_probe screen_dump stack_probe trigger_probe"

refuse_stale_simavr() {
  if [ -n "${ALLOW_STALE_SIMAVR:-}" ]; then
    return 0
  fi
  stale=""
  for harness in $FLEXSEQ_SIMAVR_HARNESSES; do
    for pid in $(pgrep -x "$harness" 2>/dev/null); do
      if [ "$pid" != "$$" ]; then
        stale="$stale $harness($pid)"
      fi
    done
  done
  if [ -n "$stale" ]; then
    printf '\n  ❌ REFUS : une course anterieure du simulateur est encore vivante\n' >&2
    printf '     %s\n' "$stale" >&2
    printf '     Cette mesure porterait sur un environnement partage, donc elle\n' >&2
    printf '     ne dirait rien du firmware. Arrete ce processus, puis relance.\n' >&2
    printf '     ALLOW_STALE_SIMAVR=1 leve ce refus pour une course deliberee.\n\n' >&2
    exit 6
  fi
}
