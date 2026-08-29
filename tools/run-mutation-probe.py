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
# QUATRE GARDES, chacun ne du meme genre d'erreur :
#   1. un motif ABSENT du code est une ERREUR (sortie 2), jamais un survivant.
#     Sans cela, un mutant qui ne s'applique plus se lit comme un mutant tue ;
#   1bis. un motif present PLUSIEURS FOIS est la meme erreur (sortie 2). Le
#      remplacement ne porte que sur la premiere occurrence, donc un motif
#      ambigu mute une cible que le mutant ne vise pas : le lot B3.3 a ainsi
#      mute `templateByte` en croyant muter `factoryTemplateByte`, et la
#      mutation s'est lue comme non detectee. Exiger exactement une occurrence ;
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
    ("cpp: the play path reads the shared template instead of the channel instance",
     "src/domain/SequencerEngine.cpp",
     "const Pattern* SequencerEngine::patternForChannel(uint8_t channel) const {\n"
     "    return instanceForChannel(channel);\n}",
     "const Pattern* SequencerEngine::patternForChannel(uint8_t channel) const {\n"
     "    if (!validChannel(channel) || bank_ == nullptr) {\n"
     "        return nullptr;\n    }\n"
     "    return bank_->getPattern(channels_[channel].selectedPattern);\n}", "cpp-all"),
    ("cpp: the effective length derivation never runs (ADR 0009)",
     "src/domain/SequencerEngine.cpp",
     "    channels_[channel].baseLength = length;\n"
     "    refreshEffectiveLength(channel);\n"
     "    return true;",
     "    channels_[channel].baseLength = length;\n"
     "    return true;", "cpp-all"),
    ("cpp: the edit path writes into the shared template instead of the instance",
     "src/domain/SequencerEngine.cpp",
     "Pattern* SequencerEngine::patternForChannel(uint8_t channel) {\n"
     "    return instanceForChannel(channel);\n}",
     "Pattern* SequencerEngine::patternForChannel(uint8_t channel) {\n"
     "    if (!validChannel(channel) || bank_ == nullptr) {\n"
     "        return nullptr;\n    }\n"
     "    return bank_->getPattern(channels_[channel].selectedPattern);\n}", "cpp-all"),
    ("cpp: the scheduler writes the whole template record in one advance (B4b.6.2b)",
     "include/flexseq/Persistence.h",
     "            storage.write(image.templateAddressAt(templateIndex_, templateCursor_),\n"
     "                          image.templateByteAt(templateChannel_, templateIndex_,\n"
     "                                               templateCursor_));\n"
     "            ++templateCursor_;",
     "            while (templateCursor_ < Image::TEMPLATE_RECORD_SIZE) {\n"
     "                storage.write(image.templateAddressAt(templateIndex_, templateCursor_),\n"
     "                              image.templateByteAt(templateChannel_, templateIndex_,\n"
     "                                                   templateCursor_));\n"
     "                ++templateCursor_;\n            }", "cpp-all"),
    ("cpp: the image scan goes before the template request (B4b.6.2b)",
     "include/flexseq/Persistence.h",
     "        if (templateIndex_ != NO_TEMPLATE) {\n"
     "            storage.write(image.templateAddressAt",
     "        if (templateIndex_ != NO_TEMPLATE && !dirty_) {\n"
     "            storage.write(image.templateAddressAt", "cpp-all"),
    ("ts: the scheduler writes the whole template record in one advance (B4b.6.2b)",
     "sim/src/domain/Persistence.ts",
     "      ++this.templateCursor;\n"
     "      if (this.templateCursor >= image.templateRecordSize) {",
     "      while (this.templateCursor < image.templateRecordSize) {\n"
     "        storage.write(\n"
     "          image.templateAddressAt(this.templateIndex, this.templateCursor),\n"
     "          image.templateByteAt(this.templateChannel, this.templateIndex, this.templateCursor),\n"
     "        );\n"
     "        ++this.templateCursor;\n      }\n"
     "      if (this.templateCursor >= image.templateRecordSize) {", "ts-all"),
    ("ts: the image scan goes before the template request (B4b.6.2b)",
     "sim/src/domain/Persistence.ts",
     "    if (this.templateIndex !== PersistenceScheduler.NO_TEMPLATE) {\n"
     "      storage.write(",
     "    if (this.templateIndex !== PersistenceScheduler.NO_TEMPLATE && !this.dirtyFlag) {\n"
     "      storage.write(", "ts-all"),
    ("cpp: saveTemplate serialises the effective length (B4b.6.2)",
     "include/flexseq/Persistence.h",
     "        const uint8_t length = engine_.getBaseLength(channel);",
     "        const uint8_t length = engine_.getEffectiveLength(channel);", "cpp-all"),
    ("cpp: the freeze lets the eighth factory slot be written (B4b.6.2)",
     "include/flexseq/Persistence.h",
     "        if (index < persist::v3::FROZEN_TEMPLATE_COUNT\n"
     "            || index >= persist::v3::TEMPLATE_COUNT) {",
     "        if (index < persist::v3::FROZEN_TEMPLATE_COUNT - 1\n"
     "            || index >= persist::v3::TEMPLATE_COUNT) {", "cpp-all"),
    ("ts: saveTemplate serialises the effective length (B4b.6.2)",
     "sim/src/domain/Persistence.ts",
     "    const length = this.engine.getBaseLength(channel);",
     "    const length = this.engine.getEffectiveLength(channel);", "ts-all"),
    ("ts: the freeze lets the eighth factory slot be written (B4b.6.2)",
     "sim/src/domain/Persistence.ts",
     "    if (index < V3_FROZEN_TEMPLATE_COUNT || index >= V3_TEMPLATE_COUNT) return false;",
     "    if (index < V3_FROZEN_TEMPLATE_COUNT - 1 || index >= V3_TEMPLATE_COUNT) return false;",
     "ts-all"),
    ("cpp: loadTemplate restores the length through the manual entry point (B4b.6.1)",
     "include/flexseq/Persistence.h",
     "        (void)engine_.setBaseLengthFromStorage(\n"
     "            channel,\n"
     "            storage.read(persist::v3::templateAddress(index,\n"
     "                                                      persist::v3::RECORD_LENGTH_AT)));",
     "        (void)engine_.setBaseLength(\n"
     "            channel,\n"
     "            storage.read(persist::v3::templateAddress(index,\n"
     "                                                      persist::v3::RECORD_LENGTH_AT)));",
     "cpp-all"),
    ("cpp: loadTemplate ignores the template length (B4b.6.1)",
     "include/flexseq/Persistence.h",
     "        (void)engine_.setBaseLengthFromStorage(\n"
     "            channel,\n"
     "            storage.read(persist::v3::templateAddress(index,\n"
     "                                                      persist::v3::RECORD_LENGTH_AT)));\n"
     "        engine_.setSelectedPattern(channel, index);",
     "        engine_.setSelectedPattern(channel, index);",
     "cpp-all"),
    ("ts: loadTemplate restores the length through the manual entry point (B4b.6.1)",
     "sim/src/domain/Persistence.ts",
     "    this.engine.setBaseLengthFromStorage(\n"
     "      channel,\n"
     "      storage.read(v3TemplateAddress(index, V3_RECORD_LENGTH_AT)),\n"
     "    );",
     "    this.engine.setBaseLength(\n"
     "      channel,\n"
     "      storage.read(v3TemplateAddress(index, V3_RECORD_LENGTH_AT)),\n"
     "    );",
     "ts-all"),
    ("ts: loadTemplate ignores the template length (B4b.6.1)",
     "sim/src/domain/Persistence.ts",
     "    this.engine.setBaseLengthFromStorage(\n"
     "      channel,\n"
     "      storage.read(v3TemplateAddress(index, V3_RECORD_LENGTH_AT)),\n"
     "    );\n"
     "    this.engine.setSelectedPattern(channel, index);",
     "    this.engine.setSelectedPattern(channel, index);",
     "ts-all"),
    ("ts: the effective length derivation never runs (ADR 0009)",
     "sim/src/domain/SequencerEngine.ts",
     "    c.baseLength = length;\n"
     "    this.refreshEffectiveLength(channel);\n"
     "    return true;\n  }\n\n  /**\n   * Definit baseLength depuis le STOCKAGE",
     "    c.baseLength = length;\n"
     "    return true;\n  }\n\n  /**\n   * Definit baseLength depuis le STOCKAGE", "ts-all"),
    ("ts: the stored bound falls back to the interface ceiling (ADR 0009)",
     "sim/src/domain/SequencerEngine.ts",
     "export const MAX_STORED_LENGTH = Pattern.DEFAULT_TOTAL_STEPS;",
     "export const MAX_STORED_LENGTH = MAX_LENGTH;", "ts-all"),
    ("cpp: the stored bound falls back to the interface ceiling (ADR 0009)",
     "include/flexseq/SequencerEngine.h",
     "    static constexpr uint8_t MAX_STORED_LENGTH = Pattern::DEFAULT_TOTAL_STEPS;",
     "    static constexpr uint8_t MAX_STORED_LENGTH = MAX_LENGTH;", "cpp-all"),
    ("cpp: the channel record restores the base through the manual entry point (ADR 0009)",
     "src/domain/Persistence.cpp",
     "        case 1: engine.setBaseLengthFromStorage(channel, value); break;",
     "        case 1: engine.setBaseLength(channel, value); break;", "cpp-all"),
    ("ts: the play path reads the shared template instead of the channel instance",
     "sim/src/domain/SequencerEngine.ts",
     "  patternForChannel(channel: number): Pattern | null {\n"
     "    return this.instanceForChannel(channel);\n  }",
     "  patternForChannel(channel: number): Pattern | null {\n"
     "    const c = this.channels[channel];\n"
     "    if (!c || !this.bank) return null;\n"
     "    return this.bank.getPattern(c.selectedPattern);\n  }", "ts-instances"),
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
    ("cpp: the version 2 channel record is 10 bytes instead of 9",
     "include/flexseq/Persistence.h",
     "constexpr uint8_t CHANNEL_RECORD = 9;\nconstexpr uint16_t CHANNELS_OFFSET = PATTERNS_OFFSET + PATTERNS_SIZE;",
     "constexpr uint8_t CHANNEL_RECORD = 10;\nconstexpr uint16_t CHANNELS_OFFSET = PATTERNS_OFFSET + PATTERNS_SIZE;",
     "cpp"),
    ("cpp: the offset cap moves one byte too far",
     "include/flexseq/SequencerEngine.h",
     "static constexpr uint8_t MAX_OFFSET = 255;", "static constexpr uint8_t MAX_OFFSET = 254;", "cpp-all"),
    ("cpp: the length cap goes up to 36 steps",
     "include/flexseq/SequencerEngine.h",
     "    static constexpr uint8_t MAX_LENGTH = 24;",
     "    static constexpr uint8_t MAX_LENGTH = 36;", "cpp-all"),
    ("cpp: the length cap falls to 20 steps",
     "include/flexseq/SequencerEngine.h",
     "    static constexpr uint8_t MAX_LENGTH = 24;",
     "    static constexpr uint8_t MAX_LENGTH = 20;", "cpp-all"),
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
     "    for (uint8_t index = 0; index < FACTORY_PATTERN_COUNT; ++index) {\n"
     "        Pattern* pattern = bank.getPattern(index);\n"
     "        if (pattern == nullptr) {\n"
     "            continue;\n"
     "        }\n"
     "        pattern->clear();\n"
     "        pattern->setLowStepMask(factoryStepMask(index));",
     "    for (uint8_t index = 0; index < PATTERN_COUNT; ++index) {\n"
     "        Pattern* pattern = bank.getPattern(index);\n"
     "        if (pattern == nullptr) {\n"
     "            continue;\n"
     "        }\n"
     "        pattern->clear();\n"
     "        pattern->setLowStepMask(factoryStepMask(index % FACTORY_PATTERN_COUNT));",
     "cpp-factory"),
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

    ("cpp: the step byte count truncates instead of rounding up",
     "include/flexseq/Pattern.h",
     "    static constexpr uint8_t STEP_BYTES = (DEFAULT_TOTAL_STEPS + 7) / 8;",
     "    static constexpr uint8_t STEP_BYTES = DEFAULT_TOTAL_STEPS / 8;", "cpp-pattern"),
    ("cpp: the pattern capacity falls back to 32 steps",
     "include/flexseq/Pattern.h",
     "    static constexpr uint8_t DEFAULT_TOTAL_STEPS = 36;",
     "    static constexpr uint8_t DEFAULT_TOTAL_STEPS = 32;", "cpp-pattern"),
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
    ("cpp: clear() leaves the last two step bytes untouched",
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
     "  static readonly DEFAULT_TOTAL_STEPS = 36;",
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
    ("cpp: the v3 step bytes truncate instead of rounding up",
     "include/flexseq/Persistence.h",
     "constexpr uint8_t STEP_BYTES = Pattern::STEP_BYTES;",
     "constexpr uint8_t STEP_BYTES = Pattern::DEFAULT_TOTAL_STEPS / 8;", "cpp"),
    ("cpp: the v3 ratchet bytes stay at the 32-step count",
     "include/flexseq/Persistence.h",
     "constexpr uint8_t RATCHET_BYTES = Pattern::RATCHET_BYTES;",
     "constexpr uint8_t RATCHET_BYTES = 16;", "cpp"),
    ("cpp: the v3 template record loses its length byte",
     "include/flexseq/Persistence.h",
     "constexpr uint8_t LENGTH_BYTES = 1;",
     "constexpr uint8_t LENGTH_BYTES = 0;", "cpp"),
    ("cpp: the v3 instance record carries a length byte it must not have",
     "include/flexseq/Persistence.h",
     "constexpr uint8_t INSTANCE_RECORD = CONTENT_BYTES;",
     "constexpr uint8_t INSTANCE_RECORD = CONTENT_BYTES + LENGTH_BYTES;", "cpp"),
    ("cpp: the v3 ratchets overlap the last step byte",
     "include/flexseq/Persistence.h",
     "constexpr uint8_t RECORD_RATCHETS_AT = RECORD_STEPS_AT + STEP_BYTES;",
     "constexpr uint8_t RECORD_RATCHETS_AT = RECORD_STEPS_AT + STEP_BYTES - 1;", "cpp"),
    ("cpp: the v3 instance zone leaves a one-byte gap",
     "include/flexseq/Persistence.h",
     "constexpr uint16_t INSTANCES_OFFSET = TEMPLATES_OFFSET + TEMPLATES_SIZE;",
     "constexpr uint16_t INSTANCES_OFFSET = TEMPLATES_OFFSET + TEMPLATES_SIZE + 1;", "cpp"),
    ("cpp: the v3 global zone falls back to the three bytes of version 2",
     "include/flexseq/Persistence.h",
     "constexpr uint16_t GLOBAL_SIZE = 5;",
     "constexpr uint16_t GLOBAL_SIZE = 3;", "cpp"),
    ("cpp: v3 stores the A bank only",
     "include/flexseq/Persistence.h",
     "constexpr uint8_t TEMPLATE_COUNT = PATTERN_COUNT;",
     "constexpr uint8_t TEMPLATE_COUNT = 8;", "cpp"),
    ("cpp: v3 stores one instance per template instead of per channel",
     "include/flexseq/Persistence.h",
     "constexpr uint8_t INSTANCE_COUNT = SequencerEngine::CHANNEL_COUNT;",
     "constexpr uint8_t INSTANCE_COUNT = PATTERN_COUNT;", "cpp"),
    ("cpp: the v3 channel record drops the two reserved CV bytes",
     "include/flexseq/Persistence.h",
     "constexpr uint8_t CHANNEL_RECORD = 9;\nconstexpr uint16_t CHANNELS_OFFSET = INSTANCES_OFFSET + INSTANCES_SIZE;",
     "constexpr uint8_t CHANNEL_RECORD = 7;\nconstexpr uint16_t CHANNELS_OFFSET = INSTANCES_OFFSET + INSTANCES_SIZE;", "cpp"),
    ("cpp: the v3 prefs zone forgets one CV calibration channel",
     "include/flexseq/Persistence.h",
     "constexpr uint16_t PREFS_SIZE = 6;\n\nconstexpr uint16_t TOTAL_SIZE = PREFS_OFFSET + PREFS_SIZE;",
     "constexpr uint16_t PREFS_SIZE = 4;\n\nconstexpr uint16_t TOTAL_SIZE = PREFS_OFFSET + PREFS_SIZE;", "cpp"),
    ("cpp: v3 goes back to the 586 bytes of the option set aside",
     "include/flexseq/Persistence.h",
     "constexpr uint16_t TOTAL_SIZE = PREFS_OFFSET + PREFS_SIZE;",
     "constexpr uint16_t TOTAL_SIZE = 586;", "cpp"),
    ("cpp: the v3 version byte collides with version 2",
     "include/flexseq/Persistence.h",
     "constexpr uint8_t FORMAT_VERSION = 3;",
     "constexpr uint8_t FORMAT_VERSION = 2;", "cpp"),

    ("ts: the v3 step bytes divide as floats instead of flooring",
     "sim/src/domain/Persistence.ts",
     "const V3_STEP_BYTES = Math.floor((Pattern.DEFAULT_TOTAL_STEPS + 7) / 8);",
     "const V3_STEP_BYTES = (Pattern.DEFAULT_TOTAL_STEPS + 7) / 8;", "ts"),
    ("ts: the v3 step bytes truncate instead of rounding up",
     "sim/src/domain/Persistence.ts",
     "const V3_STEP_BYTES = Math.floor((Pattern.DEFAULT_TOTAL_STEPS + 7) / 8);",
     "const V3_STEP_BYTES = Math.floor(Pattern.DEFAULT_TOTAL_STEPS / 8);", "ts"),
    ("ts: the v3 ratchet bytes stay at the 32-step count",
     "sim/src/domain/Persistence.ts",
     "const V3_RATCHET_BYTES = Math.floor(Pattern.DEFAULT_TOTAL_STEPS / 2);",
     "const V3_RATCHET_BYTES = 16;", "ts"),
    ("ts: the v3 template record loses its length byte",
     "sim/src/domain/Persistence.ts",
     "const V3_LENGTH_BYTES = 1;",
     "const V3_LENGTH_BYTES = 0;", "ts"),
    ("ts: the v3 instance record carries a length byte it must not have",
     "sim/src/domain/Persistence.ts",
     "const V3_INSTANCE_RECORD = V3_CONTENT_BYTES;",
     "const V3_INSTANCE_RECORD = V3_CONTENT_BYTES + V3_LENGTH_BYTES;", "ts"),
    ("ts: the v3 ratchets overlap the last step byte",
     "sim/src/domain/Persistence.ts",
     "const V3_RECORD_RATCHETS_AT = V3_RECORD_STEPS_AT + V3_STEP_BYTES;",
     "const V3_RECORD_RATCHETS_AT = V3_RECORD_STEPS_AT + V3_STEP_BYTES - 1;", "ts"),
    ("ts: the v3 instance zone leaves a one-byte gap",
     "sim/src/domain/Persistence.ts",
     "const V3_INSTANCES_OFFSET = V3_TEMPLATES_OFFSET + V3_TEMPLATES_SIZE;",
     "const V3_INSTANCES_OFFSET = V3_TEMPLATES_OFFSET + V3_TEMPLATES_SIZE + 1;", "ts"),
    ("ts: the v3 global zone falls back to the three bytes of version 2",
     "sim/src/domain/Persistence.ts",
     "const V3_GLOBAL_SIZE = 5;",
     "const V3_GLOBAL_SIZE = 3;", "ts"),
    ("ts: v3 stores the A bank only",
     "sim/src/domain/Persistence.ts",
     "const V3_TEMPLATE_COUNT = PATTERN_COUNT;",
     "const V3_TEMPLATE_COUNT = 8;", "ts"),
    ("ts: v3 stores one instance per template instead of per channel",
     "sim/src/domain/Persistence.ts",
     "const V3_INSTANCE_COUNT = CHANNEL_COUNT;",
     "const V3_INSTANCE_COUNT = PATTERN_COUNT;", "ts"),
    ("ts: the v3 channel record drops the two reserved CV bytes",
     "sim/src/domain/Persistence.ts",
     "const V3_CHANNEL_RECORD = 9;",
     "const V3_CHANNEL_RECORD = 7;", "ts"),
    ("ts: the v3 prefs zone forgets one CV calibration channel",
     "sim/src/domain/Persistence.ts",
     "const V3_PREFS_SIZE = 6;",
     "const V3_PREFS_SIZE = 4;", "ts"),
    ("ts: v3 goes back to the 586 bytes of the option set aside",
     "sim/src/domain/Persistence.ts",
     "const V3_TOTAL_SIZE = V3_PREFS_OFFSET + V3_PREFS_SIZE;",
     "const V3_TOTAL_SIZE = 586;", "ts"),
    ("ts: the v3 version byte collides with version 2",
     "sim/src/domain/Persistence.ts",
     "  FORMAT_VERSION: 3,",
     "  FORMAT_VERSION: 2,", "ts"),
    ('cpp: the v3 emit mask on the last step byte disappears',
     'src/domain/Persistence.cpp',
     '        return (offset == STEP_BYTES - 1)\n                   ? static_cast<uint8_t>(raw & LAST_STEP_BYTE_MASK)\n                   : raw;',
     '        return raw;', 'cpp'),
    ('cpp: the v3 load mask on the last step byte disappears',
     'src/domain/Persistence.cpp',
     '        const uint8_t kept = (offset == STEP_BYTES - 1)\n                                 ? static_cast<uint8_t>(value & LAST_STEP_BYTE_MASK)\n                                 : value;',
     '        const uint8_t kept = value;', 'cpp'),
    ('cpp: the v3 emit mask lands on the wrong step byte',
     'src/domain/Persistence.cpp',
     '        return (offset == STEP_BYTES - 1)\n                   ? static_cast<uint8_t>(raw & LAST_STEP_BYTE_MASK)',
     '        return (offset == 0)\n                   ? static_cast<uint8_t>(raw & LAST_STEP_BYTE_MASK)', 'cpp'),
    ('cpp: the v3 load mask lands on the wrong step byte',
     'src/domain/Persistence.cpp',
     '        const uint8_t kept = (offset == STEP_BYTES - 1)',
     '        const uint8_t kept = (offset == 0)', 'cpp'),
    ('cpp: the v3 codec stops normalising an invalid ratchet nibble',
     'src/domain/Persistence.cpp',
     '                           sanitisedRatchetNibbles(value));',
     '                           value);', 'cpp'),
    ('cpp: the v3 ratchet zone is read one byte off',
     'src/domain/Persistence.cpp',
     '    return pattern.ratchetByte(static_cast<uint8_t>(offset - STEP_BYTES));',
     '    return pattern.ratchetByte(static_cast<uint8_t>(offset - STEP_BYTES + 1));', 'cpp'),
    ('cpp: the v3 step and ratchet boundary slips one byte',
     'src/domain/Persistence.cpp',
     'uint8_t contentByte(const Pattern& pattern, uint8_t offset) {\n    if (offset < STEP_BYTES) {',
     'uint8_t contentByte(const Pattern& pattern, uint8_t offset) {\n    if (offset < STEP_BYTES - 1) {', 'cpp'),
    ('cpp: the v3 length clamp loses its floor',
     'src/domain/Persistence.cpp',
     '        if (length < MIN_TEMPLATE_LENGTH) {\n            return MIN_TEMPLATE_LENGTH;\n        }\n',
     '', 'cpp'),
    ('cpp: the v3 length clamp loses its ceiling',
     'src/domain/Persistence.cpp',
     '        if (length > MAX_TEMPLATE_LENGTH) {\n            return MAX_TEMPLATE_LENGTH;\n        }\n',
     '', 'cpp'),
    ('cpp: the v3 record accepts any stored length',
     'src/domain/Persistence.cpp',
     '        if (value < MIN_TEMPLATE_LENGTH || value > MAX_TEMPLATE_LENGTH) {\n            return false;\n        }\n',
     '', 'cpp'),
    ('cpp: a refused length is applied and reported as accepted',
     'src/domain/Persistence.cpp',
     '        if (value < MIN_TEMPLATE_LENGTH || value > MAX_TEMPLATE_LENGTH) {\n            return false;\n        }',
     '        if (value < MIN_TEMPLATE_LENGTH || value > MAX_TEMPLATE_LENGTH) {\n            length = value;\n            return true;\n        }', 'cpp'),
    ('cpp: the stored length is bounded by the engine cap instead of the format',
     'src/domain/Persistence.cpp',
     '        if (value < MIN_TEMPLATE_LENGTH || value > MAX_TEMPLATE_LENGTH) {',
     '        if (value < MIN_TEMPLATE_LENGTH || value > SequencerEngine::MAX_LENGTH) {', 'cpp'),
    ('cpp: the length byte slips to offset 22 on load',
     'src/domain/Persistence.cpp',
     'bool applyTemplateByte(Pattern& pattern, uint8_t& length, uint8_t offset, uint8_t value) {\n    if (offset == RECORD_LENGTH_AT) {',
     'bool applyTemplateByte(Pattern& pattern, uint8_t& length, uint8_t offset, uint8_t value) {\n    if (offset == RECORD_LENGTH_AT - 1) {', 'cpp'),
    ('cpp: the length byte slips to offset 22 on emit',
     'src/domain/Persistence.cpp',
     'uint8_t templateByte(const Pattern& pattern, uint8_t length, uint8_t offset) {\n    if (offset == RECORD_LENGTH_AT) {',
     'uint8_t templateByte(const Pattern& pattern, uint8_t length, uint8_t offset) {\n    if (offset == RECORD_LENGTH_AT - 1) {', 'cpp'),
    ('cpp: the format length bound becomes the engine cap',
     'include/flexseq/Persistence.h',
     'constexpr uint8_t MAX_TEMPLATE_LENGTH = Pattern::DEFAULT_TOTAL_STEPS;',
     'constexpr uint8_t MAX_TEMPLATE_LENGTH = SequencerEngine::MAX_LENGTH;', 'cpp'),
    ('cpp: the format length floor falls to zero',
     'include/flexseq/Persistence.h',
     'constexpr uint8_t MIN_TEMPLATE_LENGTH = 1;',
     'constexpr uint8_t MIN_TEMPLATE_LENGTH = 0;', 'cpp'),
    ('ts: the v3 emit reads a refused step as active',
     'sim/src/domain/Persistence.ts',
     '      if (pattern.readStep(offset * 8 + bit) === true) packed |= 1 << bit;',
     '      if (pattern.readStep(offset * 8 + bit) !== false) packed |= 1 << bit;', 'ts'),
    ('ts: the v3 emit loses the eighth bit of a step byte',
     'sim/src/domain/Persistence.ts',
     '    let packed = 0;\n    for (let bit = 0; bit < 8; ++bit) {',
     '    let packed = 0;\n    for (let bit = 0; bit < 7; ++bit) {', 'ts'),
    ('ts: the v3 load loses the eighth bit of a step byte',
     'sim/src/domain/Persistence.ts',
     '  if (offset < V3_STEP_BYTES) {\n    for (let bit = 0; bit < 8; ++bit) {\n      pattern.writeStep(offset * 8 + bit, (value & (1 << bit)) !== 0);',
     '  if (offset < V3_STEP_BYTES) {\n    for (let bit = 0; bit < 7; ++bit) {\n      pattern.writeStep(offset * 8 + bit, (value & (1 << bit)) !== 0);', 'ts'),
    ('ts: the v3 step and ratchet boundary slips one byte on emit',
     'sim/src/domain/Persistence.ts',
     'function v3ContentByte(pattern: Pattern, offset: number): number {\n  if (offset < V3_STEP_BYTES) {',
     'function v3ContentByte(pattern: Pattern, offset: number): number {\n  if (offset < V3_STEP_BYTES - 1) {', 'ts'),
    ('ts: the v3 step and ratchet boundary slips one byte on load',
     'sim/src/domain/Persistence.ts',
     'function v3ApplyContentByte(pattern: Pattern, offset: number, value: number): void {\n  if (offset < V3_STEP_BYTES) {',
     'function v3ApplyContentByte(pattern: Pattern, offset: number, value: number): void {\n  if (offset < V3_STEP_BYTES - 1) {', 'ts'),
    ('ts: the v3 ratchet zone is read one byte off',
     'sim/src/domain/Persistence.ts',
     '  const pair = offset - V3_STEP_BYTES;\n  const low = pattern.getRatchet(pair * 2) & 0x0f;',
     '  const pair = offset - V3_STEP_BYTES + 1;\n  const low = pattern.getRatchet(pair * 2) & 0x0f;', 'ts'),
    ('ts: the two ratchet nibbles swap places on emit',
     'sim/src/domain/Persistence.ts',
     '  return (high << 4) | low;\n}',
     '  return (low << 4) | high;\n}', 'ts'),
    ('ts: the v3 codec stops normalising the high ratchet nibble',
     'sim/src/domain/Persistence.ts',
     '  pattern.setRatchet(pair * 2 + 1, isValidRatchet(high) ? high : RATCHET_NONE);',
     '  pattern.setRatchet(pair * 2 + 1, high);', 'ts'),
    ('ts: the v3 codec stops normalising an invalid ratchet nibble',
     'sim/src/domain/Persistence.ts',
     '  pattern.setRatchet(pair * 2, isValidRatchet(low) ? low : RATCHET_NONE);',
     '  pattern.setRatchet(pair * 2, low);', 'ts'),
    ('ts: the v3 length clamp loses its floor',
     'sim/src/domain/Persistence.ts',
     '    if (length < V3_MIN_TEMPLATE_LENGTH) return V3_MIN_TEMPLATE_LENGTH;\n',
     '', 'ts'),
    ('ts: the v3 length clamp loses its ceiling',
     'sim/src/domain/Persistence.ts',
     '    if (length > V3_MAX_TEMPLATE_LENGTH) return V3_MAX_TEMPLATE_LENGTH;\n',
     '', 'ts'),
    ('ts: the v3 record accepts any stored length',
     'sim/src/domain/Persistence.ts',
     '    if (value < V3_MIN_TEMPLATE_LENGTH || value > V3_MAX_TEMPLATE_LENGTH) return false;\n',
     '', 'ts'),
    ('ts: a refused length is applied and reported as accepted',
     'sim/src/domain/Persistence.ts',
     '    if (value < V3_MIN_TEMPLATE_LENGTH || value > V3_MAX_TEMPLATE_LENGTH) return false;',
     '    if (value < V3_MIN_TEMPLATE_LENGTH || value > V3_MAX_TEMPLATE_LENGTH) {\n      length.value = value;\n      return true;\n    }', 'ts'),
    ('ts: the stored length is bounded by the engine cap instead of the format',
     'sim/src/domain/Persistence.ts',
     '    if (value < V3_MIN_TEMPLATE_LENGTH || value > V3_MAX_TEMPLATE_LENGTH) return false;',
     '    if (value < V3_MIN_TEMPLATE_LENGTH || value > 24) return false;', 'ts'),
    ('ts: the length byte slips to offset 22 on emit',
     'sim/src/domain/Persistence.ts',
     'function v3TemplateByte(pattern: Pattern, length: number, offset: number): number {\n  if (offset === V3_RECORD_LENGTH_AT) {',
     'function v3TemplateByte(pattern: Pattern, length: number, offset: number): number {\n  if (offset === V3_RECORD_LENGTH_AT - 1) {', 'ts'),
    ('ts: the length byte slips to offset 22 on load',
     'sim/src/domain/Persistence.ts',
     '): boolean {\n  if (offset === V3_RECORD_LENGTH_AT) {',
     '): boolean {\n  if (offset === V3_RECORD_LENGTH_AT - 1) {', 'ts'),
    ('ts: the v3 format length bound becomes the engine cap',
     'sim/src/domain/Persistence.ts',
     'const V3_MAX_TEMPLATE_LENGTH = Pattern.DEFAULT_TOTAL_STEPS;',
     'const V3_MAX_TEMPLATE_LENGTH = 24;', 'ts'),
    ('ts: the v3 format length floor falls to zero',
     'sim/src/domain/Persistence.ts',
     'const V3_MIN_TEMPLATE_LENGTH = 1;',
     'const V3_MIN_TEMPLATE_LENGTH = 0;', 'ts'),
    ("cpp: the factory record ignores its template index",
     "src/domain/Persistence.cpp",
     "    if (index >= TEMPLATE_COUNT) {\n        return 0;\n    }\n    if (offset == RECORD_LENGTH_AT) {",
     "    if (offset == RECORD_LENGTH_AT) {", "cpp-factory"),
    ("cpp: the factory record reports its length above offset 22",
     "src/domain/Persistence.cpp",
     "    if (offset == RECORD_LENGTH_AT) {\n        return FACTORY_TEMPLATE_LENGTH;",
     "    if (offset >= RECORD_LENGTH_AT) {\n        return FACTORY_TEMPLATE_LENGTH;", "cpp-factory"),
    ("cpp: the factory record serialises its mask big-endian",
     "src/domain/Persistence.cpp",
     "    const uint8_t shift = static_cast<uint8_t>((offset - RECORD_STEPS_AT) * 8);",
     "    const uint8_t shift = static_cast<uint8_t>((FACTORY_MASK_BYTES - 1 - (offset - RECORD_STEPS_AT)) * 8);",
     "cpp-factory"),
    ("cpp: the factory record declares one step too many",
     "src/domain/Persistence.cpp",
     "        return FACTORY_TEMPLATE_LENGTH;", "        return FACTORY_TEMPLATE_LENGTH + 1;", "cpp-factory"),
    ("cpp: the factory record keeps only the low byte of its mask",
     "src/domain/Persistence.cpp",
     "    if (offset >= RECORD_STEPS_AT + FACTORY_MASK_BYTES) {",
     "    if (offset >= RECORD_STEPS_AT + 1) {", "cpp-factory"),
    ("ts: the factory record ignores its template index",
     "sim/src/domain/Persistence.ts",
     "  if (index < 0 || index >= V3_TEMPLATE_COUNT) return 0;\n", "", "ts-factory"),
    ("ts: the factory record reports its length above offset 22",
     "sim/src/domain/Persistence.ts",
     "  if (offset === V3_RECORD_LENGTH_AT) return V3_FACTORY_TEMPLATE_LENGTH;",
     "  if (offset >= V3_RECORD_LENGTH_AT) return V3_FACTORY_TEMPLATE_LENGTH;", "ts-factory"),
    ("ts: the factory record serialises its mask big-endian",
     "sim/src/domain/Persistence.ts",
     "  const shift = (offset - V3_RECORD_STEPS_AT) * 8;",
     "  const shift = (FACTORY_MASK_BYTES - 1 - (offset - V3_RECORD_STEPS_AT)) * 8;", "ts-factory"),
    ("ts: the factory record declares one step too many",
     "sim/src/domain/Persistence.ts",
     "return V3_FACTORY_TEMPLATE_LENGTH;", "return V3_FACTORY_TEMPLATE_LENGTH + 1;", "ts-factory"),
    ("ts: the factory record keeps only the low byte of its mask",
     "sim/src/domain/Persistence.ts",
     "  if (offset >= V3_RECORD_STEPS_AT + FACTORY_MASK_BYTES) return 0;",
     "  if (offset >= V3_RECORD_STEPS_AT + 1) return 0;", "ts-factory"),
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
    "ts-instances": (["npx", "vitest", "run", "test/TriggerSequencer.test.ts",
                      "test/SequencerEngine.test.ts", "test/UiController.test.ts"],
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
        seen = original.count(old)
        if seen == 0:
            print(f"  {ERR}❌ MOTIF ABSENT{Z}  {label}")
            print(f"     {DIM}{rel} a change : ce mutant ne s'applique plus, il ne prouve rien.{Z}")
            print(f"     {DIM}Corriger la liste, jamais l'ignorer.{Z}")
            return 2
        if seen > 1:
            print(f"  {ERR}❌ MOTIF AMBIGU{Z}  {label}")
            print(f"     {DIM}{rel} contient ce motif {seen} fois ; seule la premiere serait mutee.{Z}")
            print(f"     {DIM}Elargir le motif jusqu'a ce qu'il ne designe qu'une cible.{Z}")
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
