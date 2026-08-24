#!/usr/bin/env python3
#
# Verifie que les assertions du domaine TUENT les defauts qu'elles pretendent
# attraper. Une assertion verte ne prouve rien tant qu'elle n'a pas ete rouge :
# ce harnais introduit un defaut connu dans le code de production, lance la suite
# concernee, et exige un echec.
#
# POURQUOI IL EST VERSIONNE. Les series precedentes vivaient dans un repertoire
# temporaire et ont disparu avec leur session : le lot 9 a mesure 20/20 sans que
# rien ne permette de le rejouer. La liste des mutants est donc du code du depot,
# et elle grandit lot par lot.
#
# TROIS GARDES, chacun ne du meme genre d'erreur :
#   1. un motif ABSENT du code est une ERREUR (sortie 2), jamais un survivant.
#     Sans cela, un mutant qui ne s'applique plus se lit comme un mutant tue ;
#   2. chaque course porte un DELAI MAXIMUM. Un mutant qui retire un garde de
#      boucle transforme `while` en boucle infinie, et le harnais attendrait
#      indefiniment. Un depassement compte le mutant comme TUE ;
#   3. le fichier est restaure dans un `finally` ET sur signal. Une interruption
#      a deja laisse un mutant dans le code source, deux fois de suite.
#
# UNE ASSERTION NE DOIT PAS SE COMPARER A LA CONSTANTE QU'ELLE TESTE. Un mutant
# a survecu le 2026-08-23 parce que le test comparait a `MAX_OFFSET` : muter la
# constante mutait aussi l'attente. Ecrire la valeur en clair.
#
# Usage :
#   ./tools/run-mutation-probe.py            toute la serie
#   ./tools/run-mutation-probe.py --list     la liste, sans rien executer
#   ./tools/run-mutation-probe.py --only cpp   ou --only ts
#   TIMEOUT=600 ./tools/run-mutation-probe.py
#
# Sortie 0 si tous les mutants sont tues, 1 s'il en survit un, 2 si un motif est
# absent du code, 127 si un outil manque.

import os
import signal
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TIMEOUT = int(os.environ.get("TIMEOUT", "240"))

TTY = sys.stdout.isatty()
OK, ERR, DIM, B, Z = ("\033[32m", "\033[31m", "\033[2m", "\033[1m", "\033[0m") if TTY else ("",) * 5

# (etiquette, chemin relatif, motif, remplacement, suite)
MUTANTS = [
    ("cpp: channel byte 4 stops reporting the mode",
     "src/domain/Persistence.cpp",
     "        case 4:\n            return static_cast<uint8_t>(engine_.getChannelMode(channel));",
     "        case 4:\n            return 0;", "cpp"),
    ("cpp: channel byte 5 stops reporting the offset",
     "src/domain/Persistence.cpp",
     "        case 5:\n            return static_cast<uint8_t>(engine_.getOffset(channel) & 0xFF);",
     "        case 5:\n            return 0;", "cpp"),
    ("cpp: channel byte 6 stops reporting the skip chance",
     "src/domain/Persistence.cpp",
     "        case 6:\n            return engine_.getSkipChance(channel);",
     "        case 6:\n            return 0;", "cpp"),
    ("cpp: a reserved CV byte reports something",
     "src/domain/Persistence.cpp",
     "        case 6:\n            return engine_.getSkipChance(channel);\n        default:\n            return 0;",
     "        case 6:\n            return engine_.getSkipChance(channel);\n        default:\n            return 1;", "cpp"),
    ("cpp: loading the mode byte becomes a no-op",
     "src/domain/Persistence.cpp",
     "        case 4:\n            engine_.setChannelMode(channel, static_cast<ChannelMode>(value));\n            break;",
     "        case 4:\n            break;", "cpp"),
    ("cpp: loading the offset byte becomes a no-op",
     "src/domain/Persistence.cpp",
     "        case 5:\n            engine_.setOffset(channel, value);\n            break;",
     "        case 5:\n            break;", "cpp"),
    ("cpp: loading the skip chance byte becomes a no-op",
     "src/domain/Persistence.cpp",
     "        case 6:\n            engine_.setSkipChance(channel, value);\n            break;",
     "        case 6:\n            break;", "cpp"),
    ("cpp: a stored CV target reaches the mode instead of being ignored",
     "src/domain/Persistence.cpp",
     "        case 6:\n            engine_.setSkipChance(channel, value);\n            break;\n        default:\n            break;",
     "        case 6:\n            engine_.setSkipChance(channel, value);\n            break;\n        default:\n"
     "            engine_.setChannelMode(channel, static_cast<ChannelMode>(value & 1));\n            break;", "cpp"),
    ("cpp: the defaults stop resetting the mode",
     "src/domain/Persistence.cpp",
     "        engine_.setChannelMode(channel, DEFAULT_CHANNEL_MODE);\n", "", "cpp"),
    ("cpp: the defaults stop resetting the offset",
     "src/domain/Persistence.cpp",
     "        engine_.setOffset(channel, 0);\n", "", "cpp"),
    ("cpp: the defaults stop resetting the skip chance",
     "src/domain/Persistence.cpp",
     "        engine_.setSkipChance(channel, 0);\n", "", "cpp"),
    ("cpp: the version byte stays at 1",
     "include/flexseq/Persistence.h",
     "constexpr uint8_t FORMAT_VERSION = 2;", "constexpr uint8_t FORMAT_VERSION = 1;", "cpp"),
    ("cpp: the channel record is 10 bytes instead of 9",
     "include/flexseq/Persistence.h",
     "constexpr uint8_t CHANNEL_RECORD = 9;", "constexpr uint8_t CHANNEL_RECORD = 10;", "cpp"),
    ("cpp: the offset cap moves one byte too far",
     "include/flexseq/SequencerEngine.h",
     "static constexpr uint8_t MAX_OFFSET = 255;", "static constexpr uint8_t MAX_OFFSET = 254;", "cpp-all"),
    ("cpp: the offset cap disappears from setOffset",
     "src/domain/SequencerEngine.cpp",
     "    c.offset = offset > MAX_OFFSET ? MAX_OFFSET : static_cast<uint8_t>(offset);",
     "    c.offset = static_cast<uint8_t>(offset);", "cpp-all"),
    ("cpp: the step limit stops clamping the offset",
     "src/domain/SequencerEngine.cpp",
     "    if (c.offset > limit) {\n        c.offset = static_cast<uint8_t>(limit);\n    }\n", "", "cpp-all"),

    ("ts: channel byte 4 stops reporting the mode",
     "sim/src/domain/Persistence.ts",
     "      case 4:\n        return this.engine.getChannelMode(channel);",
     "      case 4:\n        return 0;", "ts"),
    ("ts: channel byte 5 stops reporting the offset",
     "sim/src/domain/Persistence.ts",
     "      case 5:\n        return this.engine.getOffset(channel) & 0xff;",
     "      case 5:\n        return 0;", "ts"),
    ("ts: channel byte 6 stops reporting the skip chance",
     "sim/src/domain/Persistence.ts",
     "      case 6:\n        return this.engine.getSkipChance(channel);",
     "      case 6:\n        return 0;", "ts"),
    ("ts: a reserved CV byte reports something",
     "sim/src/domain/Persistence.ts",
     "      case 6:\n        return this.engine.getSkipChance(channel);\n      default:\n        return 0;",
     "      case 6:\n        return this.engine.getSkipChance(channel);\n      default:\n        return 1;", "ts"),
    ("ts: loading the mode byte becomes a no-op",
     "sim/src/domain/Persistence.ts",
     "      case 4:\n        this.engine.setChannelMode(channel, value as ChannelMode);\n        break;",
     "      case 4:\n        break;", "ts"),
    ("ts: loading the offset byte becomes a no-op",
     "sim/src/domain/Persistence.ts",
     "      case 5:\n        this.engine.setOffset(channel, value);\n        break;",
     "      case 5:\n        break;", "ts"),
    ("ts: loading the skip chance byte becomes a no-op",
     "sim/src/domain/Persistence.ts",
     "      case 6:\n        this.engine.setSkipChance(channel, value);\n        break;",
     "      case 6:\n        break;", "ts"),
    ("ts: a stored CV target reaches the mode instead of being ignored",
     "sim/src/domain/Persistence.ts",
     "      case 6:\n        this.engine.setSkipChance(channel, value);\n        break;\n      default:\n        break;",
     "      case 6:\n        this.engine.setSkipChance(channel, value);\n        break;\n      default:\n"
     "        this.engine.setChannelMode(channel, (value & 1) as ChannelMode);\n        break;", "ts"),
    ("ts: the defaults stop resetting the mode",
     "sim/src/domain/Persistence.ts",
     "      this.engine.setChannelMode(channel, DEFAULT_CHANNEL_MODE);\n", "", "ts"),
    ("ts: the defaults stop resetting the offset",
     "sim/src/domain/Persistence.ts",
     "      this.engine.setOffset(channel, 0);\n", "", "ts"),
    ("ts: the defaults stop resetting the skip chance",
     "sim/src/domain/Persistence.ts",
     "      this.engine.setSkipChance(channel, 0);\n", "", "ts"),
    ("ts: the version byte stays at 1",
     "sim/src/domain/Persistence.ts",
     "export const FORMAT_VERSION = 2;", "export const FORMAT_VERSION = 1;", "ts"),
    ("ts: the channel record is 10 bytes instead of 9",
     "sim/src/domain/Persistence.ts",
     "export const CHANNEL_RECORD = 9;", "export const CHANNEL_RECORD = 10;", "ts"),
    ("ts: the offset cap moves one byte too far",
     "sim/src/domain/SequencerEngine.ts",
     "export const MAX_OFFSET = 255;", "export const MAX_OFFSET = 254;", "ts"),
    ("ts: the offset cap disappears from setOffset",
     "sim/src/domain/SequencerEngine.ts",
     "    c.offset = Math.min(offset, MAX_OFFSET);", "    c.offset = offset;", "ts"),
    ("ts: the step limit stops clamping the offset",
     "sim/src/domain/SequencerEngine.ts",
     "    const limit = c.ticksPerStep - 1;\n    if (c.offset > limit) c.offset = limit;",
     "    const limit = c.ticksPerStep - 1;\n    void limit;", "ts"),

    # --- Lot 20 : la matrice ratchet x cadence ---------------------------------
    ("cpp: a triplet stops spanning two steps",
     "src/domain/Pattern.cpp",
     "    return (code == RATCHET_TRIPLET) ? 2 : 1;",
     "    return 1;", "cpp-ratchet"),
    ("cpp: a triplet fires twice instead of three times",
     "src/domain/Pattern.cpp",
     "        return 3; // three triggers, spread over two step durations",
     "        return 2; // three triggers, spread over two step durations", "cpp-ratchet"),
    ("cpp: the slot floor disappears",
     "src/domain/SequencerEngine.cpp",
     "    if (!ratchetFitsStep(code, c.ticksPerStep)) {\n        triggers = 1;\n    }\n",
     "", "cpp-ratchet"),
    ("cpp: the slot floor drops to one tick",
     "include/flexseq/Pattern.h",
     "constexpr uint8_t MIN_SLOT_TICKS = 2;", "constexpr uint8_t MIN_SLOT_TICKS = 1;", "cpp-ratchet"),
    ("cpp: every ratchet is declared to fit",
     "src/domain/Pattern.cpp",
     "    return stepTicks / triggers >= MIN_SLOT_TICKS;", "    return true;", "cpp-ratchet"),
    ("cpp: a multiplied subdiv gives one tick too many",
     "include/flexseq/Subdiv.h",
     "    return static_cast<uint16_t>(QUARTER_TICKS / mult);",
     "    return static_cast<uint16_t>(QUARTER_TICKS / mult + 1);", "cpp-ratchet"),
    ("cpp: an inactive step emits its ratchet",
     "include/flexseq/TriggerSequencer.h",
     "                return activeStep(channel) ? onsets : 0;",
     "                return onsets;", "cpp-ratchet"),
    ("cpp: switching a step off clears its ratchet",
     "src/domain/Pattern.cpp",
     "        packedSteps[byteIndex] &= static_cast<uint8_t>(~mask);",
     "        packedSteps[byteIndex] &= static_cast<uint8_t>(~mask);\n        setRatchet(index, RATCHET_NONE);", "cpp-ratchet"),
    ("cpp: clearing the pattern spares the ratchets",
     "src/domain/Pattern.cpp",
     "    clearRatchets();\n}", "}", "cpp-ratchet"),
    ("cpp: the sub-onset returns to the truncated form",
     "src/domain/SequencerEngine.cpp",
     "        (static_cast<uint32_t>(stepTicks) * k) / triggers);",
     "        (static_cast<uint32_t>(stepTicks) / triggers) * k);", "cpp-ratchet"),
    ("cpp: the choice list stops skipping a refused ratchet",
     "src/domain/UiController.cpp",
     "        if (ratchetFitsStep(ratchetAtIndex(cursor), ticks)) {",
     "        if (true) {", "cpp-ratchet"),

    ("cpp: the debt is overwritten instead of accumulated",
     "include/flexseq/TriggerSequencer.h",
     "            const uint16_t total =\n                static_cast<uint16_t>(owed_[ch]) + counts_[ch];",
     "            const uint16_t total = counts_[ch];", "cpp-debt"),
    ("cpp: the debt loses its cap",
     "include/flexseq/TriggerSequencer.h",
     "            owed_[ch] = total > MAX_OWED ? MAX_OWED : static_cast<uint8_t>(total);",
     "            owed_[ch] = static_cast<uint8_t>(total);", "cpp-debt"),
    ("cpp: taking a trigger does not pay the debt",
     "include/flexseq/TriggerSequencer.h",
     "        --owed_[channel];\n        return true;",
     "        return true;", "cpp-debt"),

    ("ts: a triplet stops spanning two steps",
     "sim/src/domain/Pattern.ts",
     "  return code === RATCHET_TRIPLET ? 2 : 1;", "  return 1;", "ts-ratchet"),
    ("ts: the slot floor disappears",
     "sim/src/domain/SequencerEngine.ts",
     "    if (!ratchetFitsStep(code, c.ticksPerStep)) triggers = 1;\n", "", "ts-ratchet"),
    ("ts: the slot floor drops to one tick",
     "sim/src/domain/Pattern.ts",
     "export const MIN_SLOT_TICKS = 2;", "export const MIN_SLOT_TICKS = 1;", "ts-ratchet"),
    ("ts: every ratchet is declared to fit",
     "sim/src/domain/Pattern.ts",
     "  return Math.floor(stepTicks / triggers) >= MIN_SLOT_TICKS;", "  return true;", "ts-ratchet"),
    ("ts: an inactive step emits its ratchet",
     "sim/src/domain/TriggerSequencer.ts",
     "        return this.activeStep(channel) ? onsets : 0;",
     "        return onsets;", "ts-ratchet"),
    ("ts: switching a step off clears its ratchet",
     "sim/src/domain/Pattern.ts",
     "    this.steps[index] = active;",
     "    this.steps[index] = active;\n    if (!active) this.ratchets[index] = RATCHET_NONE;", "ts-ratchet"),
    ("ts: the sub-onset returns to the truncated form",
     "sim/src/domain/SequencerEngine.ts",
     "  return Math.floor((stepTicks * k) / triggers);",
     "  return Math.floor(stepTicks / triggers) * k;", "ts-ratchet"),
    ("ts: the choice list stops skipping a refused ratchet",
     "sim/src/domain/UiController.ts",
     "      if (ratchetFitsStep(RATCHET_CODES[cursor]!, ticks)) {",
     "      if (true) {", "ts-ratchet"),
    ("ts: a triplet fires twice instead of three times",
     "sim/src/domain/Pattern.ts",
     "  if (code === RATCHET_TRIPLET) return 3;",
     "  if (code === RATCHET_TRIPLET) return 2;", "ts-ratchet"),
    ("ts: a multiplied subdiv gives one tick too many",
     "sim/src/domain/subdiv.ts",
     "  return QUARTER_TICKS / mult;", "  return QUARTER_TICKS / mult + 1;", "ts-ratchet"),
    ("ts: clearing the pattern spares the ratchets",
     "sim/src/domain/Pattern.ts",
     "    this.clearRatchets();\n", "", "ts-ratchet"),
    ("cpp: a rate change stops waiting for the beat",
     "src/domain/SequencerEngine.cpp",
     "    if (!running_ || onBeat()) {",
     "    if (true) {", "cpp-subdiv"),
    ("cpp: the beat crossing is always declared",
     "src/domain/SequencerEngine.cpp",
     "    const bool beatCrossed = beat >= PPQN;",
     "    const bool beatCrossed = true;", "cpp-subdiv"),
    ("cpp: the phase is no longer re-derived on a rate change",
     "src/domain/SequencerEngine.cpp",
     "            c.acc = alignedAcc(c.stepTicks, ticks);\n", "", "cpp-subdiv"),
    ("cpp: the re-derived phase becomes a plain zero",
     "src/domain/SequencerEngine.cpp",
     "    return static_cast<uint16_t>((target + stepTicks - back) % stepTicks);",
     "    return 0;", "cpp-subdiv"),
    ("cpp: a global reset drops the pending rate",
     "src/domain/SequencerEngine.cpp",
     "        if (channels_[ch].pendingTicks > 0) {",
     "        if (false) {", "cpp-subdiv"),

    ("ts: a rate change stops waiting for the beat",
     "sim/src/domain/SequencerEngine.ts",
     "    if (!this.running || this.onBeat()) {",
     "    if (true) {", "ts-subdiv"),
    ("ts: the beat crossing is always declared",
     "sim/src/domain/SequencerEngine.ts",
     "    const beatCrossed = beat >= PPQN;",
     "    const beatCrossed = true;", "ts-subdiv"),
    ("ts: the phase is no longer re-derived on a rate change",
     "sim/src/domain/SequencerEngine.ts",
     "        c.acc = this.alignedAcc(c.stepTicks, ticks);\n", "", "ts-subdiv"),
    ("ts: the re-derived phase becomes a plain zero",
     "sim/src/domain/SequencerEngine.ts",
     "    return (target + stepTicks - back) % stepTicks;",
     "    return 0;", "ts-subdiv"),
    ("ts: a global reset drops the pending rate",
     "sim/src/domain/SequencerEngine.ts",
     "      if (c.pendingTicks > 0) {",
     "      if (false) {", "ts-subdiv"),
    ("cpp: the main parameter ignores the channel mode",
     "src/domain/UiController.cpp",
     "        case MODE_CLOCK:  return FIELD_SUBDIV;",
     "        case MODE_CLOCK:  return FIELD_PATTERN;", "cpp-ui"),
    ("cpp: SHIFT plus a rotation does nothing on the tab bar again",
     "src/domain/UiController.cpp",
     "        case EVENT_SHIFT_ROTATE:\n            adjustFieldValue(mainField(), delta);\n            break;\n",
     "", "cpp-ui"),
    ("cpp: the ratchet no longer needs an active step",
     "src/domain/UiController.cpp",
     "    if (!pattern->readStep(stepCursor_, active) || !active) {",
     "    if (false) {", "cpp-ui"),
    ("cpp: SHIFT plus PLAY toggles the transport",
     "src/domain/UiController.cpp",
     "    if (event == EVENT_PLAY_PRESS) {",
     "    if (event == EVENT_PLAY_PRESS || event == EVENT_SHIFT_PLAY_PRESS) {", "cpp-ui"),
    ("cpp: the acceleration comes back on every field",
     "src/domain/UiController.cpp",
     "    const int8_t delta = oneStep(raw);",
     "    const int8_t delta = raw;", "cpp-ui"),

    ("ts: the main parameter ignores the channel mode",
     "sim/src/domain/UiController.ts",
     "      case ChannelMode.CLOCK:\n        return UiField.Subdiv;",
     "      case ChannelMode.CLOCK:\n        return UiField.Pattern;", "ts-ui"),
    ("ts: SHIFT plus a rotation does nothing on the tab bar again",
     "sim/src/domain/UiController.ts",
     "    if (event === UiEvent.ShiftRotate) {\n      this.adjustFieldValue(this.mainField, delta);\n      return;\n    }\n",
     "", "ts-ui"),
    ("ts: the ratchet no longer needs an active step",
     "sim/src/domain/UiController.ts",
     "    if (pattern.readStep(this.step) !== true) return;\n", "", "ts-ui"),
    ("ts: SHIFT plus PLAY toggles the transport",
     "sim/src/domain/UiController.ts",
     "    if (event === UiEvent.PlayPress) {",
     "    if (event === UiEvent.PlayPress || event === UiEvent.ShiftPlayPress) {", "ts-ui"),
    ("ts: the acceleration comes back on every field",
     "sim/src/domain/UiController.ts",
     "    const delta = oneStep(raw);",
     "    const delta = raw;", "ts-ui"),
    ("cpp: a rotation while the encoder is held no longer suppresses the long press",
     "src/hal/InputAdapter.cpp",
     "    if (rotatedWhileEncoderHeld) {\n        ++suppressedLong;\n        return;\n    }\n",
     "", "cpp-adapter"),
    ("cpp: a rotation while SHIFT is held no longer suppresses the long press",
     "src/hal/InputAdapter.cpp",
     "    if (rotatedWhileShiftHeld) {\n        ++suppressedLong;\n        return;\n    }\n",
     "", "cpp-adapter"),
    ("cpp: the encoder guard is never armed",
     "src/hal/InputAdapter.cpp",
     "    gravity.encoder.AttachPressRotateHandler(onRotateWhileEncoderHeld);\n",
     "", "cpp-adapter"),
    ("cpp: the encoder guard leaks to the next hold",
     "src/hal/InputAdapter.cpp",
     "    if (encoderLongPress.Change() != Button::CHANGE_UNCHANGED\n        && encoderLongPress.Change() != Button::CHANGE_PRESSED) {\n        rotatedWhileEncoderHeld = false;\n    }\n",
     "", "cpp-adapter"),
    ("cpp: a rotation stops arming the SHIFT guard",
     "src/hal/InputAdapter.cpp",
     "    if (shift) {\n        rotatedWhileShiftHeld = true;\n    }\n",
     "", "cpp-adapter"),
    ("ts: the debt is overwritten instead of accumulated",
     "sim/src/domain/TriggerSequencer.ts",
     "      const total = (this.owed[ch] ?? 0) + decided;",
     "      const total = decided;", "ts-debt"),
    ("ts: the debt loses its cap",
     "sim/src/domain/TriggerSequencer.ts",
     "      this.owed[ch] = total > MAX_OWED ? MAX_OWED : total;",
     "      this.owed[ch] = total;", "ts-debt"),
]

SUITES = {
    "cpp-debt": (["./tools/run-cpp-tests.sh", "test_trigger_sequencer"], ROOT),
    "ts-debt": (["npx", "vitest", "run", "test/TriggerSequencer.test.ts"],
                os.path.join(ROOT, "sim")),
    "cpp": (["./tools/run-cpp-tests.sh", "test_persistence"], ROOT),
    "cpp-all": (["./tools/run-cpp-tests.sh"], ROOT),
    "ts": (["npx", "vitest", "run", "test/Persistence.test.ts", "test/SequencerEngine.test.ts"],
           os.path.join(ROOT, "sim")),
    "cpp-ratchet": (["./tools/run-cpp-tests.sh", "test_ratchet_matrix"], ROOT),
    "ts-ratchet": (["npx", "vitest", "run", "test/RatchetMatrix.test.ts"],
                   os.path.join(ROOT, "sim")),
    "cpp-ui": (["./tools/run-cpp-tests.sh", "test_ui_controller"], ROOT),
    "cpp-adapter": (["./tools/run-adapter-tests.sh"], ROOT),
    "ts-ui": (["npx", "vitest", "run", "test/UiController.test.ts"],
              os.path.join(ROOT, "sim")),
    "cpp-subdiv": (["./tools/run-cpp-tests.sh", "test_subdiv_phase"], ROOT),
    "ts-subdiv": (["npx", "vitest", "run", "test/SubdivPhase.test.ts"],
                  os.path.join(ROOT, "sim")),
}

_restore = {}


def restore_all(*_):
    for path, text in _restore.items():
        with open(path, "w") as handle:
            handle.write(text)
    _restore.clear()


def on_signal(signum, _frame):
    restore_all()
    print(f"\n  {ERR}interrompu — le code source est restaure{Z}")
    sys.exit(130)


def selected(argv):
    if "--only" in argv:
        want = argv[argv.index("--only") + 1]
        return [m for m in MUTANTS if m[0].startswith(want + ":")]
    return list(MUTANTS)


def main():
    argv = sys.argv[1:]
    mutants = selected(argv)

    if "--list" in argv:
        for label, rel, _, _, suite in mutants:
            print(f"  {label}   {DIM}{rel} [{suite}]{Z}")
        print(f"\n  {len(mutants)} mutants")
        return 0

    for name in ("cpp", "ts"):
        if any(m[4].startswith(name) for m in mutants):
            command, cwd = SUITES["cpp" if name == "cpp" else "ts"]
            if not os.path.exists(os.path.join(cwd, command[0])) and command[0] != "npx":
                print(f"  {ERR}suite introuvable : {command[0]}{Z}")
                return 127

    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    print(f"{B}=================== MUTATIONS DU DOMAINE ==================={Z}")
    killed = survived = 0
    for label, rel, old, new, suite in mutants:
        path = os.path.join(ROOT, rel)
        original = open(path).read()
        if old not in original:
            print(f"  {ERR}❌ MOTIF ABSENT{Z}  {label}")
            print(f"     {DIM}{rel} a change : ce mutant ne s'applique plus, il ne prouve rien.{Z}")
            print(f"     {DIM}Corriger la liste, jamais l'ignorer.{Z}")
            return 2

        command, cwd = SUITES[suite]
        _restore[path] = original
        try:
            with open(path, "w") as handle:
                handle.write(original.replace(old, new, 1))
            try:
                done = subprocess.run(command, cwd=cwd, capture_output=True,
                                      text=True, timeout=TIMEOUT)
                code = done.returncode
            except subprocess.TimeoutExpired:
                code = "timeout"
        finally:
            restore_all()

        if code == "timeout":
            print(f"  {OK}✅ tue{Z} {DIM}(boucle infinie, delai {TIMEOUT} s){Z}  {label}")
            killed += 1
        elif code != 0:
            print(f"  {OK}✅ tue{Z}      {label}")
            killed += 1
        else:
            print(f"  {ERR}❌ SURVIVANT{Z} {label}")
            survived += 1

    total = killed + survived
    print(f"{B}============================================================{Z}")
    if survived:
        print(f"  {ERR}{killed}/{total} tues — {survived} survivant(s).{Z}")
        print(f"  {DIM}Un survivant designe une assertion manquante, ou une assertion qui se{Z}")
        print(f"  {DIM}compare a la constante qu'elle teste.{Z}")
        return 1
    print(f"  {OK}{killed}/{total} tues.{Z}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
