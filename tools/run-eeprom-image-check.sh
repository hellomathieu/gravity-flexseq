#!/usr/bin/env bash
# Checks the bytes tools/eeprom-image.cpp actually emits on stdout.
#
# The generator cannot read back its own output, so a truncated stream leaves
# its internal coverage check green. This script is the external oracle: it
# counts what arrives and compares it to LITERAL expectations. It calls no
# FlexSeq function, so a defect in the production codec cannot make the
# expectation follow it.
#
# Levers, each one turning a criterion red:
#   TRUNCATE=304          cuts the stream before the analysis
#   MUTATE_BANK=<i>       flips one byte of the bank record i of the per-channel image
#   MUTATE_FIXTURE=1      copies the ratchet of the OLD fixture pattern into NEW
#   MUTATE_SOURCE=<n>     writes n into the clock source byte of the images
#
# Exit codes: 0 all green, 1 a criterion failed, 2 the harness could not run.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

TRUNCATE="${TRUNCATE:-0}"
MUTATE_BANK="${MUTATE_BANK:--1}"
MUTATE_FIXTURE="${MUTATE_FIXTURE:-0}"
MUTATE_SOURCE="${MUTATE_SOURCE:--1}"

GEN="$WORK/eeprom-image"
if ! c++ -std=gnu++11 -I"$ROOT/include" -o "$GEN" \
     "$ROOT/tools/eeprom-image.cpp" "$ROOT"/src/domain/*.cpp 2> "$WORK/build.log"; then
  printf 'le generateur ne compile pas :\n'
  cat "$WORK/build.log"
  exit 2
fi

emit() {
  local out="$1"
  shift
  if ! "$GEN" "$@" > "$out" 2> "$WORK/gen.err"; then
    printf 'le generateur a refuse (%s) : %s\n' "$*" "$(head -1 "$WORK/gen.err")"
    exit 2
  fi
  if [ "$TRUNCATE" -gt 0 ]; then
    head -c "$TRUNCATE" "$out" > "$out.cut" && mv "$out.cut" "$out"
  fi
}

emit "$WORK/v2.bin" --mode seq --steps 0,3,4,9,15
emit "$WORK/v4.bin" --mode seq --steps 0,3,4,9,15 --format 4
emit "$WORK/pc4.bin" --mode seq --per-channel --format 4
# The P35 fixture: OLD carries the TRIPLET at step 4, NEW does not, both have
# step 4 active, and the six channels select OLD.
emit "$WORK/fx4.bin" --mode seq --format 4 --selected 8 \
     --bank 8:0,4:4/7 --bank 15:0,4
emit "$WORK/ext4.bin" --mode seq --steps 0,3,4,9,15 --format 4 --clock-source 1

python3 - "$WORK/v2.bin" "$WORK/v4.bin" "$WORK/pc4.bin" "$WORK/fx4.bin" \
         "$WORK/ext4.bin" \
         "$MUTATE_BANK" "$MUTATE_FIXTURE" "$MUTATE_SOURCE" <<'PYEOF'
import sys

(v2_path, v4_path, pc4_path, fx4_path, ext4_path,
 mutate_bank, mutate_fixture, mutate_source) = sys.argv[1:9]
mutate_bank = int(mutate_bank)
mutate_fixture = int(mutate_fixture)
mutate_source = int(mutate_source)

# ------------------------------------------------------------------
# Every number below is a LITERAL. Nothing here is read from FlexSeq.
# ------------------------------------------------------------------
V2_SIZE = 304
V2_VERSION = 2

V4_SIZE = 434
V4_VERSION = 4

BANK_AT = 1
BANK_RECORD = 23
BANK_COUNT = 16
STEP_BYTES = 5
RATCHET_BYTES = 18

# The --steps 0,3,4,9,15 content of the shared pattern 0.
STEPS_MASK = 0x8219

# PER_CHANNEL content: channel c plays the bank pattern c.
PER_CHANNEL_MASKS = [0x000001, 0x000006, 0x000038, 0x0003C0, 0x007C00, 0x0084A5]
PER_CHANNEL_RATCHETS = [(0, 2), (1, 3), (3, 4), (6, 6), (10, 7), (15, 2)]

CHANNELS_AT = 369
CHANNEL_RECORD = 9
CHANNEL_COUNT = 6

GLOBAL_AT = 423
GLOBAL_CLOCK_SOURCE_AT = 2
SOURCE_INTERNAL = 0
SOURCE_EXTERNAL_PPQN_24 = 1

FIXTURE_OLD = 8
FIXTURE_NEW = 15
FIXTURE_STEP = 4
FIXTURE_MASK = 0x000011
TRIPLET = 7
NONE = 0

passed = 0
failed = 0

def ok(name, detail=""):
    global passed
    print("  \033[32m✅\033[0m %-38s %s" % (name, detail))
    passed += 1

def bad(name, detail=""):
    global failed
    print("  \033[31m❌\033[0m %-38s %s" % (name, detail))
    failed += 1

def read(path):
    with open(path, "rb") as handle:
        return bytearray(handle.read())

def record(image, index):
    at = BANK_AT + index * BANK_RECORD
    return image[at:at + BANK_RECORD]

def step_mask(rec):
    value = 0
    for index in range(STEP_BYTES):
        value |= rec[index] << (8 * index)
    return value

def ratchet_at(rec, step):
    byte = rec[STEP_BYTES + step // 2]
    return (byte >> 4) if (step % 2) else (byte & 0x0F)

def has_ratchet(rec):
    return any(rec[STEP_BYTES + offset] for offset in range(RATCHET_BYTES))

v2 = read(v2_path)
v4 = read(v4_path)
pc4 = read(pc4_path)
fx4 = read(fx4_path)
ext4 = read(ext4_path)

if mutate_fixture and len(fx4) == V4_SIZE:
    src = BANK_AT + FIXTURE_OLD * BANK_RECORD + STEP_BYTES + FIXTURE_STEP // 2
    dst = BANK_AT + FIXTURE_NEW * BANK_RECORD + STEP_BYTES + FIXTURE_STEP // 2
    fx4[dst] = fx4[src]
    print("  levier : fixture, ratchet de OLD copie dans NEW, octet %d -> %d" % (src, dst))

if mutate_bank >= 0 and len(pc4) > BANK_AT + mutate_bank * BANK_RECORD:
    at = BANK_AT + mutate_bank * BANK_RECORD
    pc4[at] ^= 0xFF
    print("  levier : pattern %d du banc, octet %d inverse" % (mutate_bank, at))

if mutate_source >= 0:
    at = GLOBAL_AT + GLOBAL_CLOCK_SOURCE_AT
    for image in (v4, ext4):
        if len(image) > at:
            image[at] = mutate_source
    print("  levier : octet de source d horloge %d force a %d" % (at, mutate_source))

print()
print("========== IMAGE EEPROM, OCTETS RECUS ==========")

if len(v2) == V2_SIZE:
    ok("longueur du format 2", "%d octets" % len(v2))
else:
    bad("longueur du format 2", "%d octets au lieu de %d" % (len(v2), V2_SIZE))

if len(v2) > 0 and v2[0] == V2_VERSION:
    ok("version du format 2", "octet 0 = %d" % v2[0])
else:
    bad("version du format 2", "octet 0 = %s" % (v2[0] if v2 else "absent"))

for label, image in (("format 4", v4), ("format 4 --per-channel", pc4),
                     ("format 4 fixture P35", fx4),
                     ("format 4 --clock-source", ext4)):
    if len(image) == V4_SIZE:
        ok("longueur du %s" % label, "%d octets" % len(image))
    else:
        bad("longueur du %s" % label, "%d octets au lieu de %d" % (len(image), V4_SIZE))
    if len(image) > 0 and image[0] == V4_VERSION:
        ok("version du %s" % label, "octet 0 = %d" % image[0])
    else:
        bad("version du %s" % label, "octet 0 = %s" % (image[0] if image else "absent"))

if failed:
    print()
    print("  la suite de l analyse suppose une image complete : elle est arretee.")
    print("=" * 48)
    print("  VERDICT : FAIL — %d critere(s) vert(s), %d rouge(s)" % (passed, failed))
    sys.exit(1)

# --- the plain image: pattern 0 carries --steps, the rest is empty ----------
if step_mask(record(v4, 0)) == STEPS_MASK:
    ok("pattern 0 de l image simple", "masque %06x" % STEPS_MASK)
else:
    bad("pattern 0 de l image simple", "masque %06x au lieu de %06x"
        % (step_mask(record(v4, 0)), STEPS_MASK))

others = [i for i in range(1, BANK_COUNT)
          if step_mask(record(v4, i)) != 0 or has_ratchet(record(v4, i))]
if others:
    bad("les quinze autres patterns", "pattern %d porte du contenu" % others[0])
else:
    ok("les quinze autres patterns", "vides : le generateur ne seme jamais l usine")

selected = [v4[CHANNELS_AT + c * CHANNEL_RECORD] for c in range(CHANNEL_COUNT)]
if all(sel == 0 for sel in selected):
    ok("selection de l image simple", "les six canaux jouent le pattern 0")
else:
    bad("selection de l image simple", "octets 0 des records de canal : %s" % selected)

# --- the per-channel image: pattern c carries the channel c content ---------
wrong_masks = []
wrong_ratchets = []
wrong_canonical = []
for index in range(CHANNEL_COUNT):
    rec = record(pc4, index)
    if step_mask(rec) != PER_CHANNEL_MASKS[index]:
        wrong_masks.append((index, step_mask(rec), PER_CHANNEL_MASKS[index]))
    step, code = PER_CHANNEL_RATCHETS[index]
    if ratchet_at(rec, step) != code:
        wrong_ratchets.append((index, ratchet_at(rec, step), code))
    if rec[STEP_BYTES - 1] & 0xF0:
        wrong_canonical.append(index)

if wrong_masks:
    first = wrong_masks[0]
    bad("masques des six patterns par canal",
        "pattern %d : %06x au lieu de %06x" % (first[0], first[1], first[2]))
else:
    ok("masques des six patterns par canal", "un contenu distinct par pattern")

if wrong_ratchets:
    first = wrong_ratchets[0]
    bad("ratchets des six patterns par canal",
        "pattern %d : %d au lieu de %d" % (first[0], first[1], first[2]))
else:
    ok("ratchets des six patterns par canal", "un ratchet attendu par pattern")

if wrong_canonical:
    bad("bits 36 a 39 du banc", "pattern %d les porte" % wrong_canonical[0])
else:
    ok("bits 36 a 39 du banc", "canoniques a zero sur les six")

empty_tail = [i for i in range(CHANNEL_COUNT, BANK_COUNT)
              if step_mask(record(pc4, i)) != 0 or has_ratchet(record(pc4, i))]
if empty_tail:
    bad("les dix patterns restants", "pattern %d porte du contenu" % empty_tail[0])
else:
    ok("les dix patterns restants", "vides")

selected = [pc4[CHANNELS_AT + c * CHANNEL_RECORD] for c in range(CHANNEL_COUNT)]
if selected == list(range(CHANNEL_COUNT)):
    ok("selection par canal", "le canal c joue le pattern c")
else:
    bad("selection par canal", "octets 0 des records de canal : %s" % selected)

# --- the P35 fixture: two patterns, same active step, different ratchets ----
old = record(fx4, FIXTURE_OLD)
new = record(fx4, FIXTURE_NEW)

if step_mask(old) == FIXTURE_MASK and step_mask(new) == FIXTURE_MASK:
    ok("fixture : meme contenu", "steps 0 et %d actifs des deux cotes" % FIXTURE_STEP)
else:
    bad("fixture : meme contenu", "OLD %06x, NEW %06x" % (step_mask(old), step_mask(new)))

old_r = ratchet_at(old, FIXTURE_STEP)
new_r = ratchet_at(new, FIXTURE_STEP)
if old_r == TRIPLET:
    ok("fixture : ratchet de OLD", "TRIPLET (%d) au step %d" % (TRIPLET, FIXTURE_STEP))
else:
    bad("fixture : ratchet de OLD", "%d au lieu de %d" % (old_r, TRIPLET))
if new_r == NONE:
    ok("fixture : ratchet de NEW", "NONE au step %d" % FIXTURE_STEP)
else:
    bad("fixture : ratchet de NEW", "%d au lieu de %d" % (new_r, NONE))

if old != new and old[:STEP_BYTES] == new[:STEP_BYTES]:
    ok("fixture : OLD != NEW", "par les ratchets seulement")
else:
    bad("fixture : OLD != NEW", "identiques" if old == new else "les steps different aussi")

untouched = [i for i in range(BANK_COUNT) if i not in (FIXTURE_OLD, FIXTURE_NEW)
             and (step_mask(record(fx4, i)) != 0 or has_ratchet(record(fx4, i)))]
if untouched:
    bad("fixture : les autres patterns", "pattern %d porte du contenu" % untouched[0])
else:
    ok("fixture : les autres patterns", "les 14 autres records sont vides")

selected = [fx4[CHANNELS_AT + c * CHANNEL_RECORD] for c in range(CHANNEL_COUNT)]
if all(sel == FIXTURE_OLD for sel in selected):
    ok("fixture : --selected", "les six canaux selectionnent %d" % FIXTURE_OLD)
else:
    bad("fixture : --selected", "octets 0 des records de canal : %s" % selected)

# --- lot XCLK : the persisted clock source ----------------------------------
at = GLOBAL_AT + GLOBAL_CLOCK_SOURCE_AT
if v4[at] == SOURCE_INTERNAL:
    ok("source d horloge par defaut", "octet %d = %d, INT" % (at, v4[at]))
else:
    bad("source d horloge par defaut", "octet %d = %d au lieu de %d"
        % (at, v4[at], SOURCE_INTERNAL))

if ext4[at] == SOURCE_EXTERNAL_PPQN_24:
    ok("--clock-source 1", "octet %d = %d, EXT PPQN 24" % (at, ext4[at]))
else:
    bad("--clock-source 1", "octet %d = %d au lieu de %d"
        % (at, ext4[at], SOURCE_EXTERNAL_PPQN_24))

diff = [i for i in range(min(len(v4), len(ext4))) if v4[i] != ext4[i]]
if diff == [at]:
    ok("--clock-source n a qu un effet", "un seul octet differe, %d" % at)
else:
    bad("--clock-source n a qu un effet", "octets differents : %s" % diff[:8])

print("=" * 48)
if failed:
    print("  VERDICT : FAIL — %d critere(s) vert(s), %d rouge(s)" % (passed, failed))
    sys.exit(1)
print("  VERDICT : PASS — %d criteres verts, 0 rouge" % passed)
sys.exit(0)
PYEOF
