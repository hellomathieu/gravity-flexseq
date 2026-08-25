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
     "    if (code == RATCHET_TRIPLET) {\n        return 3;\n    }",
     "    if (code == RATCHET_TRIPLET) {\n        return 2;\n    }", "cpp-ratchet"),
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

    ("cpp: PLAY drives the transport outside the internal clock",
     "src/domain/UiController.cpp",
     "    if (clockSource_ != CLOCK_SOURCE_INTERNAL) {\n        return;\n    }\n",
     "", "cpp-ui"),
    ("ts: PLAY drives the transport outside the internal clock",
     "sim/src/domain/UiController.ts",
     "    if (this.source !== CLOCK_SOURCE_INTERNAL) return;\n", "", "ts-ui"),

    ("cpp: a factory pattern loses a step",
     "src/domain/FactoryPatterns.cpp", "    0x9111,", "    0x9110,", "cpp-factory"),
    ("cpp: a reload of the factory patterns no longer erases",
     "src/domain/FactoryPatterns.cpp", "        pattern->clear();\n", "", "cpp-factory"),
    ("cpp: the factory content spills into the B bank",
     "src/domain/FactoryPatterns.cpp",
     "    for (uint8_t index = 0; index < FACTORY_PATTERN_COUNT; ++index) {",
     "    for (uint8_t index = 0; index < PATTERN_COUNT; ++index) {", "cpp-factory"),
    ("cpp: the step mask writes only its low byte",
     "src/domain/Pattern.cpp",
     "    packedSteps[1] = static_cast<uint8_t>((bits >> 8) & 0xFF);\n", "", "cpp-pattern"),

    ("ts: a factory pattern loses a step",
     "sim/src/domain/FactoryPatterns.ts", "0x9111,", "0x9110,", "ts-factory"),
    ("ts: a reload of the factory patterns no longer erases",
     "sim/src/domain/FactoryPatterns.ts", "    pattern.clear();\n", "", "ts-factory"),

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

    ("cpp: reading a step stops at the old 24-step bound",
     "src/domain/Pattern.cpp",
     "bool Pattern::readStep(uint8_t index, bool& active) const {\n    if (index >= DEFAULT_TOTAL_STEPS) {",
     "bool Pattern::readStep(uint8_t index, bool& active) const {\n    if (index >= 24) {", "cpp-pattern"),
    ("cpp: writing a step stops at the old 24-step bound",
     "src/domain/Pattern.cpp",
     "bool Pattern::writeStep(uint8_t index, bool active) {\n    if (index >= DEFAULT_TOTAL_STEPS) {",
     "bool Pattern::writeStep(uint8_t index, bool active) {\n    if (index >= 24) {", "cpp-pattern"),
    ("cpp: a ratchet stops being storable above step 23",
     "src/domain/Pattern.cpp",
     "    if (index >= DEFAULT_TOTAL_STEPS || !isValidRatchet(code)) {",
     "    if (index >= 24 || !isValidRatchet(code)) {", "cpp-pattern"),
    ("cpp: clear() leaves the fourth step byte untouched",
     "src/domain/Pattern.cpp",
     "    for (uint8_t i = 0; i < STEP_BYTES; ++i) {",
     "    for (uint8_t i = 0; i < 3; ++i) {", "cpp-pattern"),
    ("cpp: clearRatchets() leaves the ratchets of the steps above 23",
     "src/domain/Pattern.cpp",
     "    for (uint8_t i = 0; i < RATCHET_BYTES; ++i) {",
     "    for (uint8_t i = 0; i < 12; ++i) {", "cpp-pattern"),
    ("cpp: the step cursor follows the pattern instead of the grid",
     "include/flexseq/UiController.h",
     "    static constexpr uint8_t STEP_COUNT = 24;",
     "    static constexpr uint8_t STEP_COUNT = Pattern::DEFAULT_TOTAL_STEPS;", "cpp-ui"),
    ("cpp: the grid follows the pattern instead of its two rows",
     "include/flexseq/PatternScreen.h",
     "constexpr uint8_t GRID_STEPS = PER_ROW * GRID_ROWS;",
     "constexpr uint8_t GRID_STEPS = Pattern::DEFAULT_TOTAL_STEPS;", "cpp-screen"),
    ("ts: the pattern goes back to 24 steps",
     "sim/src/domain/Pattern.ts",
     "  static readonly DEFAULT_TOTAL_STEPS = 32;",
     "  static readonly DEFAULT_TOTAL_STEPS = 24;", "ts-pattern"),
    ("ts: the step cursor follows the pattern instead of the grid",
     "sim/src/domain/UiController.ts",
     "export const STEP_COUNT = 24;",
     "export const STEP_COUNT = Pattern.DEFAULT_TOTAL_STEPS;", "ts-ui"),
    ("ts: the projected grid follows the pattern instead of its two rows",
     "sim/src/sim/PatternView.ts",
     "export const GRID_STEPS = 24;",
     "export const GRID_STEPS = Pattern.DEFAULT_TOTAL_STEPS;", "ts-view"),

    ("ts drift: the slope estimator always answers zero",
     "sim/src/analysis/driftEstimator.ts",
     "  const slope = sxy / sxx;",
     "  const slope = 0;", "ts-drift"),
    ("ts drift: a bounded jitter is reported as a drift",
     "sim/src/analysis/driftEstimator.ts",
     "  const drifting = olsLarge && olsSignificant && tsLarge;",
     "  const drifting = stats.max !== stats.min;", "ts-drift"),
    ("ts drift: the verdict falls back to first against last",
     "sim/src/analysis/driftEstimator.ts",
     "  const drifting = olsLarge && olsSignificant && tsLarge;",
     "  const drifting = stats.last !== stats.first;", "ts-drift"),
    ("ts drift: a missing onset is no longer detected",
     "sim/src/analysis/driftEstimator.ts",
     "    if (i + 1 < expected.length && at >= expected[i + 1]!) {\n      dropped.push(i);\n      ++i;\n      continue;\n    }",
     "", "ts-drift"),
    ("ts drift: an unexpected onset is no longer detected",
     "sim/src/analysis/driftEstimator.ts",
     "    if (at + toleranceTicks < expected[i]!) {\n      unexpected.push(j);\n      ++j;\n      continue;\n    }",
     "    if (false) {\n      unexpected.push(j);\n      ++j;\n      continue;\n    }", "ts-drift"),
    ("ts drift: the sub-onset rounds instead of truncating",
     "sim/src/analysis/driftEstimator.ts",
     "  return Math.floor((stepTicks * k) / triggers);",
     "  return Math.round((stepTicks * k) / triggers);", "ts-drift"),
    ("ts drift: the sub-onset divides before multiplying",
     "sim/src/analysis/driftEstimator.ts",
     "  return Math.floor((stepTicks * k) / triggers);",
     "  return Math.floor(stepTicks / triggers) * k;", "ts-drift"),
    ("ts drift: Theil-Sen answers the mean instead of the median",
     "sim/src/analysis/driftEstimator.ts",
     "  return m % 2 === 1 ? slopes[(m - 1) / 2]! : (slopes[m / 2 - 1]! + slopes[m / 2]!) / 2;",
     "  return slopes.reduce((s, v) => s + v, 0) / m;", "ts-drift"),
    ("ts drift: the onset budget forgets the pending onsets",
     "sim/src/analysis/driftEstimator.ts",
     "  const residual = budget.expected - (budget.emitted + budget.pending + budget.dropped);",
     "  const residual = budget.expected - (budget.emitted + budget.dropped);", "ts-drift"),
    ("ts drift: the standard deviation forgets its square root",
     "sim/src/analysis/driftEstimator.ts",
     "    stdDev: Math.sqrt(variance),",
     "    stdDev: variance,", "ts-drift"),

    ("ts drift: recovery is always announced",
     "sim/src/analysis/driftEstimator.ts",
     "  const recovered = lastAffectedIndex < 0 || ys[n - 1]! === nominal;",
     "  const recovered = true;", "ts-drift"),
    ("ts drift: recovery is judged on the maximum instead of the return",
     "sim/src/analysis/driftEstimator.ts",
     "  const recovered = lastAffectedIndex < 0 || ys[n - 1]! === nominal;",
     "  const recovered = maxError === 0;", "ts-drift"),
    ("ts drift: the recovery length is off by one",
     "sim/src/analysis/driftEstimator.ts",
     "    lastAffectedIndex < 0 ? 0 : recovered ? lastAffectedIndex - firstAffectedIndex + 1 : n - firstAffectedIndex;",
     "    lastAffectedIndex < 0 ? 0 : recovered ? lastAffectedIndex - firstAffectedIndex : n - firstAffectedIndex;", "ts-drift"),
    ("ts drift: the maximum error is read from the last sample only",
     "sim/src/analysis/driftEstimator.ts",
     "    if (deviation > maxError) maxError = deviation;",
     "    maxError = deviation;", "ts-drift"),
    ("ts drift: the persistent offset is always zero",
     "sim/src/analysis/driftEstimator.ts",
     "    persistentOffset: recovered ? 0 : ys[n - 1]! - nominal,",
     "    persistentOffset: 0,", "ts-drift"),
    ("ts drift: the affected onsets are not counted",
     "sim/src/analysis/driftEstimator.ts",
     "      ++affectedCount;\n      if (firstAffectedIndex < 0) firstAffectedIndex = i;",
     "      if (firstAffectedIndex < 0) firstAffectedIndex = i;", "ts-drift"),

    ("ts reconcile: an edge before its onset is accepted instead of counted as extra",
     "sim/src/analysis/reconcile.ts",
     "    if (observed[j]! < expected[i]!) {",
     "    if (false) {", "ts-reconcile"),
    ("ts reconcile: the ambiguity is resolved silently by taking the first candidate",
     "sim/src/analysis/reconcile.ts",
     "    } else if (feasible.length > 1) {\n      ambiguous = 1;\n      ambiguousRange = [feasible[0]!, feasible[feasible.length - 1]!];",
     "    } else if (feasible.length > 1) {\n      missingPositions = [feasible[0]!];\n      ambiguousRange = null;", "ts-reconcile"),
    ("ts reconcile: several losses are no longer flagged ambiguous",
     "sim/src/analysis/reconcile.ts",
     "  } else if (missing > 1) {\n    ambiguous = missing;",
     "  } else if (missing > 1) {\n    ambiguous = 0;", "ts-reconcile"),
    ("ts reconcile: the plausible delay bound is ignored",
     "sim/src/analysis/reconcile.ts",
     "    ok = ok && delay >= 0 && delay <= maxDelay;\n    prefixOk[i + 1] = ok;",
     "    ok = ok && delay >= 0;\n    prefixOk[i + 1] = ok;", "ts-reconcile"),
    ("ts reconcile: the debt peak is read as the final deficit",
     "sim/src/analysis/reconcile.ts",
     "      if (deficit > maxDeficit) maxDeficit = deficit;",
     "      maxDeficit = deficit;", "ts-reconcile"),
    ("ts reconcile: a late onset is no longer counted as late",
     "sim/src/analysis/reconcile.ts",
     "    if (pair.delayTicks >= 1) ++late;",
     "    if (pair.delayTicks >= 1000000) ++late;", "ts-reconcile"),
    ("ts reconcile: the raw difference follows the pairing instead of the counts",
     "sim/src/analysis/reconcile.ts",
     "    rawDifference: expected.length - observed.length,",
     "    rawDifference: unpaired.length,", "ts-reconcile"),

    ("ts gestures: the ratchet recipe stops checking that the step is active",
     "sim/src/analysis/gestureRecipes.ts",
     "    if (pattern.readStep(step) !== true) {",
     "    if (false) {", "ts-gestures"),
    ("ts gestures: the ratchet recipe ignores the rate guard",
     "sim/src/analysis/gestureRecipes.ts",
     "    if (!ratchetFitsStep(code, ticks)) {",
     "    if (false) {", "ts-gestures"),
    ("ts gestures: selecting a field rotates one detent too many",
     "sim/src/analysis/gestureRecipes.ts",
     "      if (this.ui.field === field) return;\n      this.rotate(1);",
     "      if (this.ui.field === field) { this.rotate(1); return; }\n      this.rotate(1);", "ts-gestures"),
    ("ts gestures: going back climbs two levels instead of one",
     "sim/src/analysis/gestureRecipes.ts",
     "  backOneLevel(): void {\n    this.longPress();\n  }",
     "  backOneLevel(): void {\n    this.longPress();\n    this.longPress();\n  }", "ts-gestures"),
    ("ts gestures: a value is set by a plain rotation instead of SHIFT plus rotation",
     "sim/src/analysis/gestureRecipes.ts",
     "  shiftRotate(detents: number): void {\n    const step = detents >= 0 ? 1 : -1;\n    for (let i = 0; i < Math.abs(detents); ++i) this.emit(UiEvent.ShiftRotate, step);",
     "  shiftRotate(detents: number): void {\n    const step = detents >= 0 ? 1 : -1;\n    for (let i = 0; i < Math.abs(detents); ++i) this.emit(UiEvent.Rotate, step);", "ts-gestures"),
    ("ts gestures: toggling a step forgets to move the cursor first",
     "sim/src/analysis/gestureRecipes.ts",
     "  toggleStep(step: number): void {\n    this.moveStepCursor(step);\n    this.press();",
     "  toggleStep(step: number): void {\n    this.press();", "ts-gestures"),
]

SUITES = {
    "cpp-factory": (["./tools/run-cpp-tests.sh", "test_factory_patterns"], ROOT),
    "cpp-pattern": (["./tools/run-cpp-tests.sh", "test_pattern"], ROOT),
    "ts-factory": (["npx", "vitest", "run", "test/FactoryPatterns.test.ts"],
                   os.path.join(ROOT, "sim")),
    "cpp-debt": (["./tools/run-cpp-tests.sh", "test_trigger_sequencer"], ROOT),
    "ts-debt": (["npx", "vitest", "run", "test/TriggerSequencer.test.ts"],
                os.path.join(ROOT, "sim")),
    "cpp": (["./tools/run-cpp-tests.sh", "test_persistence"], ROOT),
    "cpp-all": (["./tools/run-cpp-tests.sh"], ROOT),
    "ts": (["npx", "vitest", "run", "test/Persistence.test.ts", "test/SequencerEngine.test.ts"],
           os.path.join(ROOT, "sim")),
    "cpp-ratchet": (["./tools/run-cpp-tests.sh", "test_ratchet_matrix"], ROOT),
    "cpp-screen": (["./tools/run-cpp-tests.sh", "test_pattern_screen"], ROOT),
    "ts-pattern": (["npx", "vitest", "run", "test/Pattern.test.ts"],
                   os.path.join(ROOT, "sim")),
    "ts-view": (["npx", "vitest", "run", "test/PatternView.test.ts"],
                os.path.join(ROOT, "sim")),
    "ts-drift": (["npx", "vitest", "run", "test/driftEstimator.test.ts"],
                 os.path.join(ROOT, "sim")),
    "ts-reconcile": (["npx", "vitest", "run", "test/reconcile.test.ts"],
                     os.path.join(ROOT, "sim")),
    "ts-gestures": (["npx", "vitest", "run", "test/gestureRecipes.test.ts"],
                    os.path.join(ROOT, "sim")),
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
