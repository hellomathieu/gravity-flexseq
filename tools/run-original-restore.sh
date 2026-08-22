#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BACKUP="${BACKUP:-$ROOT/../gravity-module-backup/gravity-original-flash.hex}"
TRIMMED="${TRIMMED:-$(dirname "$BACKUP")/gravity-original-flash-app.hex}"
APP_END="${APP_END:-0x6BFF}"
BOOT_START="${BOOT_START:-0x7E00}"
FLASH_BYTES="${FLASH_BYTES:-32768}"
DRY_RUN=0
MUTATE="${MUTATE:-}"

for arg in "$@"; do
    case "$arg" in
        -h|--help)
            cat <<USAGE
Restaure le firmware d'origine du module Gravity depuis la sauvegarde de flash.

    ./tools/run-original-restore.sh --dry-run   # rogne et verifie, sans ecrire
    ./tools/run-original-restore.sh             # rogne, verifie, televerse, relit

Le HEX est rogne a 0x0000-$APP_END. C'est OBLIGATOIRE : la sauvegarde couvre les
32 ko de flash, bootloader inclus, et televerser le bootloader PAR le bootloader
le detruit. La restauration ne touche pas l'EEPROM, donc les patterns et les
reglages du proprietaire survivent.

Quatre criteres, tous bloquants, et un critere non evaluable compte pour un
echec :

  1. la sauvegarde est integre : sommes de controle valides, $FLASH_BYTES octets ;
  2. la zone rognee au-dessus de l'application est VIERGE — sinon le rognage
     perdrait du contenu reel ;
  3. le HEX rogne relu est identique a la sauvegarde sur 0x0000-$APP_END ;
  4. la relecture du module est identique a la sauvegarde sur 0x0000-$APP_END,
     et le bootloader est intact.

Reglages par variable d'environnement :

  PORT=/dev/cu.usbserial-XXXX   force le port (sinon detection automatique)
  BACKUP=<chemin>               sauvegarde de flash
                                defaut : $BACKUP
  TRIMMED=<chemin>              HEX rogne produit
  APP_END=<adresse>             derniere adresse d'application (defaut 0x6BFF)
  MUTATE=<adresse>              ecrit un octet dans la zone vierge : rend le
                                critere 2 rouge, pour exercer le chemin d'echec

Retour a FlexSeq ensuite :

    pio run -e nanoatmega328 -t upload
USAGE
            exit 0
            ;;
        --dry-run) DRY_RUN=1 ;;
        *) echo "  ❌ argument inconnu : $arg"; exit 2 ;;
    esac
done

find_pio() {
    if command -v pio >/dev/null 2>&1; then
        command -v pio
    elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then
        echo "$HOME/.platformio/penv/bin/pio"
    fi
}

find_avrdude() {
    if [ -x "$HOME/.platformio/packages/tool-avrdude/bin/avrdude" ]; then
        echo "$HOME/.platformio/packages/tool-avrdude/bin/avrdude"
    elif command -v avrdude >/dev/null 2>&1; then
        command -v avrdude
    fi
}

detect_port() {
    if [ -n "${PORT:-}" ]; then
        echo "$PORT"
        return
    fi
    local found=()
    local candidate
    for candidate in /dev/cu.*; do
        [ -e "$candidate" ] || continue
        case "$candidate" in
            *usbserial*|*usbmodem*|*wchusbserial*) found+=("$candidate") ;;
        esac
    done
    if [ "${#found[@]}" -eq 1 ]; then
        echo "${found[0]}"
    elif [ "${#found[@]}" -eq 0 ]; then
        echo "AUCUN"
    else
        printf 'PLUSIEURS %s\n' "${found[*]}"
    fi
}

if [ ! -e "$BACKUP" ]; then
    echo "  ❌ sauvegarde absente : $BACKUP"
    echo "     Sans elle il n'y a rien a restaurer. Voir CLAUDE.md, section"
    echo "     « EEPROM backup », pour la sauvegarde d'EEPROM ; celle de flash"
    echo "     se lit par le bootloader (avrdude -U flash:r:)."
    exit 1
fi

BACKUP="$BACKUP" TRIMMED="$TRIMMED" APP_END="$APP_END" BOOT_START="$BOOT_START" \
FLASH_BYTES="$FLASH_BYTES" MUTATE="$MUTATE" python3 - <<'PYTHON'
import os
import sys

BACKUP = os.environ["BACKUP"]
TRIMMED = os.environ["TRIMMED"]
APP_END = int(os.environ["APP_END"], 0)
BOOT_START = int(os.environ["BOOT_START"], 0)
FLASH_BYTES = int(os.environ["FLASH_BYTES"])
MUTATE = os.environ["MUTATE"]
PER_RECORD = 32


def parse(lines):
    data, bad = {}, 0
    for line in lines:
        line = line.strip()
        if not line.startswith(":"):
            continue
        raw = bytes.fromhex(line[1:])
        if ((0 - (sum(raw[:-1]) & 0xFF)) & 0xFF) != raw[-1]:
            bad += 1
        if raw[3] == 0:
            address = (raw[1] << 8) | raw[2]
            for offset, value in enumerate(raw[4:4 + raw[0]]):
                data[address + offset] = value
    return data, bad


def emit(data, path, last):
    with open(path, "w", encoding="ascii") as handle:
        for base in range(0, last + 1, PER_RECORD):
            payload = bytes(data.get(base + i, 0xFF) for i in range(PER_RECORD))
            header = bytes((PER_RECORD, base >> 8 & 0xFF, base & 0xFF, 0))
            record = header + payload
            checksum = (0 - (sum(record) & 0xFF)) & 0xFF
            handle.write(":" + (record + bytes((checksum,))).hex().upper() + "\n")
        handle.write(":00000001FF\n")


failures = []
with open(BACKUP) as handle:
    data, bad = parse(handle)

if bad == 0 and len(data) == FLASH_BYTES:
    print(f"  ✅ critere 1 integrite   0 somme invalide, {len(data)} octets")
else:
    print(f"  ❌ critere 1 integrite   {bad} somme(s) invalide(s), {len(data)} octets sur {FLASH_BYTES}")
    print()
    print("  ❌ ECHEC : integrite. Les criteres suivants ne sont pas evaluables")
    print("     sur une sauvegarde incomplete. Rien n'a ete produit ni televerse.")
    sys.exit(1)

if MUTATE:
    where = int(MUTATE, 0)
    data[where] = 0x00
    print(f"  ⚠  MUTATE                octet 0x00 ecrit a {hex(where)}")

trimmed_region = [a for a in range(APP_END + 1, BOOT_START) if data.get(a, 0xFF) != 0xFF]
if not trimmed_region:
    print(f"  ✅ critere 2 zone vierge {hex(APP_END + 1)}..{hex(BOOT_START - 1)}, {BOOT_START - APP_END - 1} octets, tous a 0xFF")
else:
    failures.append("zone rognee non vierge")
    print(f"  ❌ critere 2 zone vierge {len(trimmed_region)} octet(s) ecrit(s) entre {hex(APP_END + 1)} et {hex(BOOT_START - 1)}")
    print(f"     Premier : {hex(trimmed_region[0])}. Rogner a {hex(APP_END)} perdrait du contenu reel.")

boot_written = sum(1 for a in range(BOOT_START, FLASH_BYTES) if data.get(a, 0xFF) != 0xFF)
print(f"     bootloader {hex(BOOT_START)}..{hex(FLASH_BYTES - 1)} : {boot_written} octet(s) ecrit(s), EXCLU du televersement")

if failures:
    print()
    print(f"  ❌ ECHEC : {', '.join(failures)}. Rien n'a ete produit ni televerse.")
    sys.exit(1)

os.makedirs(os.path.dirname(os.path.abspath(TRIMMED)), exist_ok=True)
emit(data, TRIMMED, APP_END)

with open(TRIMMED) as handle:
    reread, reread_bad = parse(handle)
differences = [a for a in range(APP_END + 1) if reread.get(a) != data.get(a)]
if reread_bad == 0 and not differences and len(reread) == APP_END + 1:
    print(f"  ✅ critere 3 relecture   {len(reread)} octets, identiques a la sauvegarde")
else:
    print(f"  ❌ critere 3 relecture   {reread_bad} somme(s) invalide(s), {len(differences)} difference(s), {len(reread)} octets")
    print()
    print("  ❌ ECHEC : le HEX rogne ne reproduit pas la sauvegarde. Rien n'a ete televerse.")
    sys.exit(1)

print(f"     ecrit : {TRIMMED}")
PYTHON

if [ "$DRY_RUN" = "1" ]; then
    echo
    echo "  ⏭  televersement         ignore (--dry-run)"
    echo "     Relancer sans --dry-run, module branche, pour restaurer."
    exit 0
fi

PORT_PATH="$(detect_port)"
case "$PORT_PATH" in
    AUCUN)
        echo "  ❌ aucun port serie USB. Le module est-il branche ?"
        exit 1
        ;;
    PLUSIEURS*)
        echo "  ❌ plusieurs ports candidats : ${PORT_PATH#PLUSIEURS }"
        echo "     Choisir avec PORT=<chemin>."
        exit 1
        ;;
esac
echo "  ✅ port                   $PORT_PATH"

AVRDUDE="$(find_avrdude)"
if [ -z "$AVRDUDE" ]; then
    echo "  ❌ avrdude introuvable, ni dans les paquets PlatformIO ni sur le PATH"
    exit 127
fi
CONF="$(dirname "$AVRDUDE")/../avrdude.conf"
CONF_ARG=()
[ -e "$CONF" ] && CONF_ARG=(-C "$CONF")

READBACK="$(mktemp -t gravity-readback).hex"
trap 'rm -f "$READBACK"' EXIT

if ! "$AVRDUDE" "${CONF_ARG[@]}" -p atmega328p -c arduino -P "$PORT_PATH" -b 115200 \
     -D -U "flash:w:$TRIMMED:i" >/tmp/gravity-restore.$$ 2>&1; then
    echo "  ❌ televersement echoue :"
    tail -20 /tmp/gravity-restore.$$
    rm -f /tmp/gravity-restore.$$
    exit 1
fi
rm -f /tmp/gravity-restore.$$
echo "  ✅ televersement         $(basename "$TRIMMED"), sans effacement de puce (-D)"

if ! "$AVRDUDE" "${CONF_ARG[@]}" -p atmega328p -c arduino -P "$PORT_PATH" -b 115200 \
     -U "flash:r:$READBACK:i" >/tmp/gravity-readback.$$ 2>&1; then
    echo "  ❌ relecture du module echouee :"
    tail -20 /tmp/gravity-readback.$$
    rm -f /tmp/gravity-readback.$$
    exit 1
fi
rm -f /tmp/gravity-readback.$$

BACKUP="$BACKUP" READBACK="$READBACK" APP_END="$APP_END" BOOT_START="$BOOT_START" \
FLASH_BYTES="$FLASH_BYTES" python3 - <<'PYTHON'
import os
import sys

APP_END = int(os.environ["APP_END"], 0)
BOOT_START = int(os.environ["BOOT_START"], 0)
FLASH_BYTES = int(os.environ["FLASH_BYTES"])


def parse(path):
    data = {}
    for line in open(path):
        line = line.strip()
        if not line.startswith(":"):
            continue
        raw = bytes.fromhex(line[1:])
        if raw[3] == 0:
            address = (raw[1] << 8) | raw[2]
            for offset, value in enumerate(raw[4:4 + raw[0]]):
                data[address + offset] = value
    return data


backup = parse(os.environ["BACKUP"])
module = parse(os.environ["READBACK"])

app = [a for a in range(APP_END + 1) if module.get(a) != backup.get(a)]
if not app:
    print(f"  ✅ critere 4 application {APP_END + 1} octets identiques a la sauvegarde")
else:
    print(f"  ❌ critere 4 application {len(app)} difference(s), premiere a {hex(app[0])}")

boot = [a for a in range(BOOT_START, FLASH_BYTES) if module.get(a) != backup.get(a)]
if not boot:
    print(f"  ✅ bootloader intact     {FLASH_BYTES - BOOT_START} octets identiques")
else:
    print(f"  ❌ bootloader altere     {len(boot)} difference(s), premiere a {hex(boot[0])}")

if app or boot:
    print()
    print("  ❌ ECHEC : le module ne porte pas la sauvegarde.")
    sys.exit(1)

print()
print("  Reste a verifier a l'oeil, et c'est la partie qu'aucun outil ne couvre :")
print("    - le firmware d'origine demarre et affiche son interface ;")
print("    - les huit patterns sont la, bpm a 120, reglages de channels intacts.")
print()
print("  Retour a FlexSeq :  pio run -e nanoatmega328 -t upload")
PYTHON
