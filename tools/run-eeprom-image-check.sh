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
#   MUTATE_INSTANCE=<c>   flips one byte of the instance record of channel c
#   MUTATE_TEMPLATE=<i>   flips one byte of the template record i
#   MUTATE_FIXTURE=1      copies the ratchet of the OLD fixture template into NEW
#   MUTATE_SOURCE=<n>     writes n into the clock source byte of the images
#
# Exit codes: 0 all green, 1 a criterion failed, 2 the harness could not run.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

TRUNCATE="${TRUNCATE:-0}"
MUTATE_INSTANCE="${MUTATE_INSTANCE:--1}"
MUTATE_TEMPLATE="${MUTATE_TEMPLATE:--1}"
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
emit "$WORK/v3.bin" --mode seq --steps 0,3,4,9,15 --format 3
emit "$WORK/pc3.bin" --mode seq --per-channel --format 3
# The P35 fixture of lot STEP: OLD carries the TRIPLET at step 4, NEW does not,
# both have step 4 active, and the six channels select OLD.
emit "$WORK/fx3.bin" --mode seq --format 3 --selected 8 \
     --template 8:0,4:4/7 --template 15:0,4
emit "$WORK/ext3.bin" --mode seq --steps 0,3,4,9,15 --format 3 --clock-source 1

python3 - "$WORK/v2.bin" "$WORK/v3.bin" "$WORK/pc3.bin" "$WORK/fx3.bin" \
         "$WORK/ext3.bin" \
         "$MUTATE_INSTANCE" "$MUTATE_TEMPLATE" "$MUTATE_FIXTURE" \
         "$MUTATE_SOURCE" <<'PYEOF'
import sys

(v2_path, v3_path, pc3_path, fx3_path, ext3_path,
 mutate_instance, mutate_template, mutate_fixture,
 mutate_source) = sys.argv[1:10]
mutate_instance = int(mutate_instance)
mutate_template = int(mutate_template)
mutate_fixture = int(mutate_fixture)
mutate_source = int(mutate_source)

# ------------------------------------------------------------------
# Every number below is a LITERAL. Nothing here is read from FlexSeq.
# ------------------------------------------------------------------
V2_SIZE = 304
V2_VERSION = 2

V3_SIZE = 588
V3_VERSION = 3

TEMPLATES_AT = 1
TEMPLATE_RECORD = 24
TEMPLATE_COUNT = 16
CONTENT_BYTES = 23
STEP_BYTES = 5
RATCHET_BYTES = 18

INSTANCES_AT = 385
INSTANCE_RECORD = 23
INSTANCE_COUNT = 6

FACTORY_LENGTH = 16
FACTORY_MASKS = [
    0x9111, 0x0810, 0x1249, 0xCCCC, 0xEEEE, 0x5454, 0x7FBF, 0xB733,
    0, 0, 0, 0, 0, 0, 0, 0,
]

# PER_CHANNEL content, plus the marker step 18 + c that the instance carries.
INSTANCE_MASKS = [
    0x000001 | (1 << 18),
    0x000006 | (1 << 19),
    0x000038 | (1 << 20),
    0x0003C0 | (1 << 21),
    0x007C00 | (1 << 22),
    0x0084A5 | (1 << 23),
]
INSTANCE_RATCHETS = [(0, 2), (1, 3), (3, 4), (6, 6), (10, 7), (15, 2)]

CHANNELS_AT = 523
CHANNEL_RECORD = 9
CHANNEL_COUNT = 6

GLOBAL_AT = 577
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

def step_mask(record):
    value = 0
    for index in range(STEP_BYTES):
        value |= record[index] << (8 * index)
    return value

def ratchet_at(record, step):
    byte = record[STEP_BYTES + step // 2]
    return (byte >> 4) if (step % 2) else (byte & 0x0F)

v2 = read(v2_path)
v3 = read(v3_path)
pc3 = read(pc3_path)
fx3 = read(fx3_path)
ext3 = read(ext3_path)

if mutate_fixture and len(fx3) == V3_SIZE:
    src = TEMPLATES_AT + FIXTURE_OLD * TEMPLATE_RECORD + STEP_BYTES + FIXTURE_STEP // 2
    dst = TEMPLATES_AT + FIXTURE_NEW * TEMPLATE_RECORD + STEP_BYTES + FIXTURE_STEP // 2
    fx3[dst] = fx3[src]
    print("  levier : fixture, ratchet de OLD copie dans NEW, octet %d -> %d" % (src, dst))

if mutate_template >= 0 and len(pc3) > TEMPLATES_AT + mutate_template * TEMPLATE_RECORD:
    at = TEMPLATES_AT + mutate_template * TEMPLATE_RECORD
    pc3[at] ^= 0xFF
    print("  levier : template %d, octet %d inverse" % (mutate_template, at))

if mutate_instance >= 0 and len(pc3) > INSTANCES_AT + mutate_instance * INSTANCE_RECORD:
    at = INSTANCES_AT + mutate_instance * INSTANCE_RECORD
    pc3[at] ^= 0xFF
    print("  levier : instance %d, octet %d inverse" % (mutate_instance, at))

if mutate_source >= 0:
    at = GLOBAL_AT + GLOBAL_CLOCK_SOURCE_AT
    for image in (v3, ext3):
        if len(image) > at:
            image[at] = mutate_source
    print("  levier : octet de source d horloge %d force a %d" % (at, mutate_source))

print()
print("========== IMAGE EEPROM, OCTETS RECUS ==========")

# --- longueurs et versions -----------------------------------------
if len(v2) == V2_SIZE:
    ok("longueur du format 2", "%d octets" % len(v2))
else:
    bad("longueur du format 2", "%d octets au lieu de %d" % (len(v2), V2_SIZE))

if len(v2) > 0 and v2[0] == V2_VERSION:
    ok("version du format 2", "octet 0 = %d" % v2[0])
else:
    bad("version du format 2", "octet 0 = %s" % (v2[0] if v2 else "absent"))

for label, image in (("format 3", v3), ("format 3 --per-channel", pc3),
                     ("format 3 fixture P35", fx3),
                     ("format 3 --clock-source", ext3)):
    if len(image) == V3_SIZE:
        ok("longueur du %s" % label, "%d octets" % len(image))
    else:
        bad("longueur du %s" % label, "%d octets au lieu de %d" % (len(image), V3_SIZE))
    if len(image) > 0 and image[0] == V3_VERSION:
        ok("version du %s" % label, "octet 0 = %d" % image[0])
    else:
        bad("version du %s" % label, "octet 0 = %s" % (image[0] if image else "absent"))

if failed:
    print()
    print("  la suite de l analyse suppose une image complete : elle est arretee.")
    print("=" * 48)
    print("  VERDICT : FAIL — %d critere(s) vert(s), %d rouge(s)" % (passed, failed))
    sys.exit(1)

# --- les seize records de template ---------------------------------
wrong_masks = []
wrong_tails = []
wrong_lengths = []
wrong_canonical = []
for index in range(TEMPLATE_COUNT):
    at = TEMPLATES_AT + index * TEMPLATE_RECORD
    record = pc3[at:at + TEMPLATE_RECORD]
    if step_mask(record) != FACTORY_MASKS[index]:
        wrong_masks.append((index, step_mask(record), FACTORY_MASKS[index]))
    if any(record[STEP_BYTES + offset] for offset in range(RATCHET_BYTES)):
        wrong_tails.append(index)
    if record[CONTENT_BYTES] != FACTORY_LENGTH:
        wrong_lengths.append((index, record[CONTENT_BYTES]))
    if record[STEP_BYTES - 1] & 0xF0:
        wrong_canonical.append(index)

if wrong_masks:
    first = wrong_masks[0]
    bad("masques d usine des templates",
        "template %d : %06x au lieu de %06x" % (first[0], first[1], first[2]))
else:
    ok("masques d usine des templates", "16 records, A1..A8 puis huit vides")

if wrong_tails:
    bad("ratchets des templates d usine", "template %d en porte un" % wrong_tails[0])
else:
    ok("ratchets des templates d usine", "les 18 octets sont a zero")

if wrong_lengths:
    bad("longueur des records de template",
        "template %d : %d au lieu de %d" % (wrong_lengths[0][0], wrong_lengths[0][1],
                                            FACTORY_LENGTH))
else:
    ok("longueur des records de template", "16 sur les seize records")

if wrong_canonical:
    bad("bits 36 a 39 des templates", "template %d les porte" % wrong_canonical[0])
else:
    ok("bits 36 a 39 des templates", "canoniques a zero sur les seize")

# --- les six records d instance, --per-channel ----------------------
wrong_inst = []
wrong_ratchet = []
wrong_inst_canonical = []
for channel in range(INSTANCE_COUNT):
    at = INSTANCES_AT + channel * INSTANCE_RECORD
    record = pc3[at:at + INSTANCE_RECORD]
    if step_mask(record) != INSTANCE_MASKS[channel]:
        wrong_inst.append((channel, step_mask(record), INSTANCE_MASKS[channel]))
    step, code = INSTANCE_RATCHETS[channel]
    if ratchet_at(record, step) != code:
        wrong_ratchet.append((channel, ratchet_at(record, step), code))
    if record[STEP_BYTES - 1] & 0xF0:
        wrong_inst_canonical.append(channel)

if wrong_inst:
    first = wrong_inst[0]
    bad("masques des six instances",
        "canal %d : %06x au lieu de %06x" % (first[0], first[1], first[2]))
else:
    ok("masques des six instances", "contenu du canal plus le marqueur 18 + c")

if wrong_ratchet:
    first = wrong_ratchet[0]
    bad("ratchets des six instances",
        "canal %d : %d au lieu de %d" % (first[0], first[1], first[2]))
else:
    ok("ratchets des six instances", "un ratchet attendu par canal")

if wrong_inst_canonical:
    bad("bits 36 a 39 des instances", "canal %d les porte" % wrong_inst_canonical[0])
else:
    ok("bits 36 a 39 des instances", "canoniques a zero sur les six")

# --- la fixture P35 : deux templates, meme step actif, ratchets differents ---
def template_record(image, index):
    at = TEMPLATES_AT + index * TEMPLATE_RECORD
    return image[at:at + TEMPLATE_RECORD]

old = template_record(fx3, FIXTURE_OLD)
new = template_record(fx3, FIXTURE_NEW)

if old[CONTENT_BYTES] == FACTORY_LENGTH and new[CONTENT_BYTES] == FACTORY_LENGTH:
    ok("fixture : longueurs", "OLD et NEW valent %d" % FACTORY_LENGTH)
else:
    bad("fixture : longueurs", "OLD %d, NEW %d" % (old[CONTENT_BYTES], new[CONTENT_BYTES]))

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

untouched = [i for i in range(TEMPLATE_COUNT) if i not in (FIXTURE_OLD, FIXTURE_NEW)
             and template_record(fx3, i) != template_record(pc3, i)]
if untouched:
    bad("fixture : templates d usine", "template %d modifie" % untouched[0])
else:
    ok("fixture : templates d usine", "les 14 autres records identiques a l usine")

selected = [fx3[CHANNELS_AT + c * CHANNEL_RECORD] for c in range(CHANNEL_COUNT)]
if all(sel == FIXTURE_OLD for sel in selected):
    ok("fixture : --selected", "les six canaux selectionnent %d" % FIXTURE_OLD)
else:
    bad("fixture : --selected", "octets 0 des records de canal : %s" % selected)

instances_empty = all(step_mask(fx3[INSTANCES_AT + c * INSTANCE_RECORD:
                                    INSTANCES_AT + (c + 1) * INSTANCE_RECORD]) == 0
                      for c in range(INSTANCE_COUNT))
if instances_empty:
    ok("fixture : instances vides", "la banque, pas le template : un canal non module se tait")
else:
    bad("fixture : instances vides", "une instance porte du contenu")

# --- l image sans --per-channel ne porte pas les marqueurs ----------
plain_carries_marker = []
for channel in range(INSTANCE_COUNT):
    at = INSTANCES_AT + channel * INSTANCE_RECORD
    if step_mask(v3[at:at + INSTANCE_RECORD]) & (1 << (18 + channel)):
        plain_carries_marker.append(channel)

if plain_carries_marker:
    bad("le marqueur appartient a --per-channel",
        "canal %d le porte sans le drapeau" % plain_carries_marker[0])
else:
    ok("le marqueur appartient a --per-channel", "absent des six instances sans le drapeau")

# --- lot XCLK : la source d horloge persistee ------------------------
at = GLOBAL_AT + GLOBAL_CLOCK_SOURCE_AT
if v3[at] == SOURCE_INTERNAL:
    ok("source d horloge par defaut", "octet %d = %d, INT" % (at, v3[at]))
else:
    bad("source d horloge par defaut", "octet %d = %d au lieu de %d"
        % (at, v3[at], SOURCE_INTERNAL))

if ext3[at] == SOURCE_EXTERNAL_PPQN_24:
    ok("--clock-source 1", "octet %d = %d, EXT PPQN 24" % (at, ext3[at]))
else:
    bad("--clock-source 1", "octet %d = %d au lieu de %d"
        % (at, ext3[at], SOURCE_EXTERNAL_PPQN_24))

diff = [i for i in range(min(len(v3), len(ext3))) if v3[i] != ext3[i]]
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
