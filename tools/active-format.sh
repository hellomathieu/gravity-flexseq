# Resout le FORMAT DE PERSISTANCE ACTIF a partir du binaire teste.
#
# Pourquoi ce fichier existe. run-stack-probe.sh decoupait Persistence.h avant
# `namespace v3 {` et prenait les constantes qui restaient ; les trois autres
# sondes ecrivaient `--format 3` en dur. Les deux formes SUPPOSENT le format au
# lieu de le lire. Le 2026-08-28 la premiere a rendu un rouge qui ne disait rien
# du firmware, apres l'activation de la v3.
#
# La chaine est : binaire -> image liee -> constantes du compilateur. Le
# generateur d'image reste ignorant de l'ELF ; le script decide, le generateur
# execute.
#
# Deux images liees, ou aucune, ARRETENT l'appelant sans verdict. Un format
# indecidable n'est jamais un defaut du firmware.
#
# FLEXSEQ_FORMAT_FORCE=<2|3> est le levier de contre-epreuve. Il falsifie
# l'hypothese de L'OUTILLAGE, jamais le firmware : le binaire n'est ni
# recompile ni modifie. L'oracle croit alors a un autre format, et le resultat
# doit rougir.
#
# Exporte : FLEXSEQ_FORMAT_VERSION, FLEXSEQ_IMAGE_SIZE, FLEXSEQ_SCAN_SIZE,
# FLEXSEQ_VERSION_OFFSET, FLEXSEQ_BASE_ADDRESS, FLEXSEQ_FORMAT_DETECTED.
#
# Usage :
#   . "$ROOT/tools/active-format.sh"
#   flexseq_resolve_active_format "$ROOT" "$ELF" "$WORK" || exit 1

flexseq_format_die() {
  printf '  ❌ %s\n' "$1" >&2
}

flexseq_resolve_active_format() {
  fmt_root="$1"; fmt_elf="$2"; fmt_work="$3"

  [ -f "$fmt_elf" ] || { flexseq_format_die "binaire absent : $fmt_elf"; return 1; }

  fmt_nm="$(command -v avr-nm || echo "$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-nm")"
  [ -x "$fmt_nm" ] || { flexseq_format_die "avr-nm introuvable."; return 127; }

  fmt_symbols="$("$fmt_nm" --demangle "$fmt_elf" 2>"$fmt_work/nm.log")" || {
    cat "$fmt_work/nm.log" >&2
    flexseq_format_die "avr-nm n'a pas pu lire $fmt_elf. Le format actif n'est pas indecidable : il n'a pas ete cherche."
    return 1
  }
  fmt_v3="$(printf '%s\n' "$fmt_symbols" | grep -c 'flexseq::PersistentImageV3::')"
  fmt_v2="$(printf '%s\n' "$fmt_symbols" | grep -c 'flexseq::PersistentImage::')"
  case "$fmt_v3:$fmt_v2" in
    0:0) flexseq_format_die "format actif INDECIDABLE : le binaire ne porte aucun symbole d'image de persistance. Ne pas rendre de verdict sur une supposition."; return 1 ;;
    0:*) fmt_major=2 ;;
    *:0) fmt_major=3 ;;
    *)   flexseq_format_die "format actif AMBIGU : le binaire lie les deux images (v3 $fmt_v3 symboles, v2 $fmt_v2). Ne pas rendre de verdict."; return 1 ;;
  esac
  FLEXSEQ_FORMAT_DETECTED="$fmt_major"

  if [ -n "${FLEXSEQ_FORMAT_FORCE:-}" ]; then
    case "$FLEXSEQ_FORMAT_FORCE" in
      2|3) ;;
      *) flexseq_format_die "FLEXSEQ_FORMAT_FORCE accepte 2 ou 3, lu \"$FLEXSEQ_FORMAT_FORCE\""; return 1 ;;
    esac
    fmt_major="$FLEXSEQ_FORMAT_FORCE"
  fi

  fmt_bin="$fmt_work/persistence_format"
  if ! c++ -std=c++17 -I"$fmt_root/include" -DACTIVE_FORMAT_V"$fmt_major"=1 \
       -o "$fmt_bin" "$fmt_root/tools/persistence-format.cpp" > "$fmt_work/format.log" 2>&1; then
    cat "$fmt_work/format.log" >&2
    flexseq_format_die "compilation de tools/persistence-format.cpp en echec"
    return 1
  fi
  fmt_out="$("$fmt_bin")" || { flexseq_format_die "tools/persistence-format.cpp n'a rien rendu"; return 1; }

  FLEXSEQ_FORMAT_VERSION="$(printf '%s\n' "$fmt_out" | sed -n 's/^FORMAT_VERSION=//p')"
  FLEXSEQ_IMAGE_SIZE="$(printf '%s\n' "$fmt_out" | sed -n 's/^IMAGE_SIZE=//p')"
  FLEXSEQ_SCAN_SIZE="$(printf '%s\n' "$fmt_out" | sed -n 's/^SCAN_SIZE=//p')"
  FLEXSEQ_VERSION_OFFSET="$(printf '%s\n' "$fmt_out" | sed -n 's/^VERSION_OFFSET=//p')"
  FLEXSEQ_BASE_ADDRESS="$(printf '%s\n' "$fmt_out" | sed -n 's/^BASE_ADDRESS=//p')"

  for fmt_name in FLEXSEQ_FORMAT_VERSION FLEXSEQ_IMAGE_SIZE FLEXSEQ_SCAN_SIZE \
                  FLEXSEQ_VERSION_OFFSET FLEXSEQ_BASE_ADDRESS; do
    eval "fmt_value=\$$fmt_name"
    case "$fmt_value" in
      ''|*[!0-9]*) flexseq_format_die "constante $fmt_name absente ou non numerique : \"$fmt_value\""; return 1 ;;
    esac
  done

  [ "$FLEXSEQ_FORMAT_VERSION" = "$fmt_major" ] || {
    flexseq_format_die "le format compile annonce la version $FLEXSEQ_FORMAT_VERSION la ou l'outillage demandait la v$fmt_major. Ne pas rendre de verdict."
    return 1
  }
  return 0
}

flexseq_report_active_format() {
  fmt_ok="${1:-}"; fmt_dim="${2:-}"; fmt_0="${3:-}"
  if [ -n "${FLEXSEQ_FORMAT_FORCE:-}" ]; then
    printf '  ⚠  FORMAT FORCE           %sversion %s, %s o physiques — le binaire lie la v%s ; oracle volontairement faux%s\n' \
      "$fmt_dim" "$FLEXSEQ_FORMAT_VERSION" "$FLEXSEQ_IMAGE_SIZE" "$FLEXSEQ_FORMAT_DETECTED" "$fmt_0"
  else
    printf '  %s✅%s format actif           %sversion %s, %s o physiques, %s balayes — lu sur firmware.elf%s\n' \
      "$fmt_ok" "$fmt_0" "$fmt_dim" "$FLEXSEQ_FORMAT_VERSION" "$FLEXSEQ_IMAGE_SIZE" \
      "$FLEXSEQ_SCAN_SIZE" "$fmt_0"
  fi
}
