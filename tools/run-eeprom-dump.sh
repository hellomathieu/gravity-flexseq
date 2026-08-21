#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENVNAME="${ENVNAME:-eepromdump}"
OUT="${OUT:-$ROOT/../gravity-module-backup/gravity-original-eeprom.hex}"
FLASH="${FLASH:-$(dirname "$OUT")/gravity-original-flash.hex}"
WINDOW="${WINDOW:-30}"
SKIP_UPLOAD="${SKIP_UPLOAD:-0}"
FORCE="${FORCE:-0}"

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
    cat <<USAGE
Sauvegarde l'EEPROM du module Gravity, que le bootloader ne sait pas lire.

    ./tools/run-eeprom-dump.sh

Televerse env:eepromdump, capture les dumps Intel HEX emis sur le port serie,
puis verifie la capture avant de l'ecrire. Trois criteres, tous bloquants :

  1. toutes les sommes de controle valides, les 1024 octets couverts ;
  2. la capture DIFFERE des 1024 premiers octets de la flash — c'est ainsi
     qu'on detecte un bootloader qui rend du contenu de flash sans le dire ;
  3. deux dumps consecutifs identiques.

Reglages par variable d'environnement :

  PORT=/dev/cu.usbserial-XXXX   force le port (sinon detection automatique)
  OUT=<chemin>                  fichier de sortie
                                defaut : $OUT
  FLASH=<chemin>                dump de flash servant au critere 2
  WINDOW=<secondes>             duree de capture (defaut 30)
  SKIP_UPLOAD=1                 ne pas televerser, le module tourne deja
  FORCE=1                       ecraser un fichier de sortie existant

Le fichier de sortie reste HORS du depot : c'est le firmware GPLv3 du
fabricant et les reglages du proprietaire.
USAGE
    exit 0
fi

find_pio() {
    if command -v pio >/dev/null 2>&1; then
        command -v pio
    elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then
        echo "$HOME/.platformio/penv/bin/pio"
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

if [ -e "$OUT" ] && [ "$FORCE" != "1" ]; then
    echo "  ❌ $OUT existe deja."
    echo "     Une sauvegarde valide ne doit pas etre ecrasee par une execution"
    echo "     douteuse. Relancer avec FORCE=1 pour la remplacer."
    exit 1
fi

PORT_PATH="$(detect_port)"
case "$PORT_PATH" in
    AUCUN)
        echo "  ❌ aucun port serie USB. Le module est-il branche ?"
        echo "     Les ports presents : $(echo /dev/cu.* | tr ' ' '\n' | tr '\n' ' ')"
        exit 1
        ;;
    PLUSIEURS*)
        echo "  ❌ plusieurs ports candidats : ${PORT_PATH#PLUSIEURS }"
        echo "     Choisir avec PORT=<chemin>."
        exit 1
        ;;
esac
echo "  ✅ port                   $PORT_PATH"

if [ "$SKIP_UPLOAD" != "1" ]; then
    PIO="$(find_pio)"
    if [ -z "$PIO" ]; then
        echo "  ❌ pio introuvable, ni sur le PATH ni dans ~/.platformio/penv/bin/"
        echo "     Installation : https://docs.platformio.org/en/latest/core/installation/"
        exit 127
    fi
    PORT_PATH="$PORT_PATH" python3 - <<'QUIET'
import os
import sys
import termios
import time

QUIET_S = 2.0
CAP_S = 45.0

fd = os.open(os.environ["PORT_PATH"], os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
try:
    termios.tcflush(fd, termios.TCIOFLUSH)
    discarded = 0
    last_byte_at = time.time()
    deadline = time.time() + CAP_S
    while time.time() < deadline:
        try:
            chunk = os.read(fd, 4096)
        except BlockingIOError:
            chunk = b""
        if chunk:
            discarded += len(chunk)
            last_byte_at = time.time()
        else:
            if time.time() - last_byte_at >= QUIET_S:
                break
            time.sleep(0.05)
    else:
        print(f"  ❌ le port parle encore apres {CAP_S:.0f} s, {discarded} octets jetes")
        print("     Un autre programme occupe-t-il le port ?")
        sys.exit(1)
    termios.tcflush(fd, termios.TCIOFLUSH)
    print(f"  ✅ ligne au repos        {discarded} octet(s) jete(s)")
finally:
    os.close(fd)
QUIET
    if ! "$PIO" run -e "$ENVNAME" -t upload --upload-port "$PORT_PATH" >/tmp/eeprom-upload.$$ 2>&1; then
        echo "  ❌ televersement echoue :"
        tail -20 /tmp/eeprom-upload.$$
        rm -f /tmp/eeprom-upload.$$
        exit 1
    fi
    VERIFIED="$(grep -o '[0-9]* bytes of flash verified' /tmp/eeprom-upload.$$ | head -1 || true)"
    rm -f /tmp/eeprom-upload.$$
    echo "  ✅ televersement         env:$ENVNAME, ${VERIFIED:-ok}"
else
    echo "  ⏭  televersement         ignore (SKIP_UPLOAD=1)"
fi

OUT="$OUT" FLASH="$FLASH" PORT_PATH="$PORT_PATH" WINDOW="$WINDOW" python3 - <<'PYTHON'
import os
import sys
import termios
import time

PORT = os.environ["PORT_PATH"]
OUT = os.environ["OUT"]
FLASH = os.environ["FLASH"]
WINDOW = float(os.environ["WINDOW"])
END = ":00000001FF"
EEPROM_BYTES = 1024


def capture(port, window):
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        attrs = termios.tcgetattr(fd)
        cc = list(attrs[6])
        cc[termios.VMIN] = 0
        cc[termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, [
            0, 0,
            termios.CS8 | termios.CREAD | termios.CLOCAL,
            0, termios.B9600, termios.B9600, cc,
        ])
        time.sleep(0.2)
        termios.tcflush(fd, termios.TCIFLUSH)
        data = bytearray()
        deadline = time.time() + window
        while time.time() < deadline:
            try:
                chunk = os.read(fd, 4096)
            except BlockingIOError:
                chunk = b""
            if chunk:
                data += chunk
            else:
                time.sleep(0.05)
        return data
    finally:
        os.close(fd)


def split_dumps(raw):
    text = raw.decode("ascii", errors="replace").replace("\r", "\n")
    dumps, current = [], None
    for line in (ln.strip() for ln in text.split("\n")):
        if not line:
            continue
        if line.startswith(":10000000"):
            current = [line]
        elif current is not None:
            current.append(line)
            if line == END:
                dumps.append(current)
                current = None
    return dumps


def parse(lines):
    data, bad = {}, 0
    for line in lines:
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


def load_file(path):
    with open(path) as handle:
        return parse([ln.strip() for ln in handle])[0]


failures = []
dumps = split_dumps(capture(PORT, WINDOW))
print(f"  {'✅' if dumps else '❌'} dumps complets        {len(dumps)}")
if not dumps:
    print("     Rien de lisible. Verifier que le firmware televerse est bien")
    print("     env:eepromdump, et que rien d'autre n'occupe le port.")
    sys.exit(1)

data, bad = parse(dumps[-1])
if bad == 0 and len(data) == EEPROM_BYTES:
    print(f"  ✅ critere 1 integrite   {len(dumps[-1]) - 1} enregistrements, 0 somme invalide, {len(data)} octets")
else:
    failures.append("integrite")
    print(f"  ❌ critere 1 integrite   {bad} somme(s) invalide(s), {len(data)} octets sur {EEPROM_BYTES}")

if os.path.exists(FLASH):
    flash = load_file(FLASH)
    same = sum(1 for a in range(EEPROM_BYTES) if data.get(a) == flash.get(a))
    if same == EEPROM_BYTES:
        failures.append("copie de la flash")
        print(f"  ❌ critere 2 vs flash    {same}/{EEPROM_BYTES} identiques — c'est la FLASH, pas l'EEPROM")
    else:
        print(f"  ✅ critere 2 vs flash    {same}/{EEPROM_BYTES} identiques, donc distinct")
else:
    failures.append("critere 2 non evalue")
    print(f"  ❌ critere 2 vs flash    {FLASH} absent, critere NON evalue")

if len(dumps) >= 2:
    if dumps[-1] == dumps[-2]:
        print(f"  ✅ critere 3 stabilite   deux derniers dumps identiques")
    else:
        failures.append("instabilite")
        print(f"  ❌ critere 3 stabilite   les deux derniers dumps different")
else:
    failures.append("critere 3 non evalue")
    print(f"  ❌ critere 3 stabilite   un seul dump, augmenter WINDOW")

if failures:
    print()
    print(f"  ❌ ECHEC : {', '.join(failures)}. Rien n'a ete ecrit.")
    sys.exit(1)

os.makedirs(os.path.dirname(os.path.abspath(OUT)), exist_ok=True)
with open(OUT, "w", encoding="ascii") as handle:
    for line in dumps[-1]:
        handle.write(line + "\n")

erased = sum(1 for v in data.values() if v == 0xFF)
print()
print(f"  bpm enregistre : {data[0] | (data[1] << 8)}   (adresse 0, saveState() du firmware d'origine)")
print(f"  cellules jamais ecrites : {erased} sur {EEPROM_BYTES} ({100 * erased / EEPROM_BYTES:.1f} %)")
print(f"  ecrit : {OUT}")
PYTHON
