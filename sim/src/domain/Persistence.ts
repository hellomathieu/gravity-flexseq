import {
  FACTORY_MASK_BYTES,
  FACTORY_STEP_COUNT,
  factoryStepMask,
} from "./FactoryPatterns.js";
import { PATTERN_COUNT, type PatternBank } from "./PatternBank.js";
import { isValidRatchet, Pattern, RATCHET_NONE } from "./Pattern.js";
import {
  CHANNEL_COUNT,
  ChannelMode,
  DEFAULT_BAR_LENGTH,
  DEFAULT_CHANNEL_MODE,
  DEFAULT_LENGTH,
  type SequencerEngine,
} from "./SequencerEngine.js";
import { DEFAULT_SUBDIV, SUBDIVS } from "./subdiv.js";
import { DEFAULT_TEMPO, type UiController } from "./UiController.js";

export const BASE_ADDRESS = 384;
export const FORMAT_VERSION = 2;
export const EEPROM_SIZE = 1024;
export const ORIGINAL_FIRMWARE_LAST = 320;
export const QUIET_MS = 3000;

export const HEADER_OFFSET = 0;
export const HEADER_SIZE = 1;
export const PATTERN_STEP_BYTES = 3;
export const PATTERN_RATCHET_BYTES = 12;
export const PATTERN_RECORD = PATTERN_STEP_BYTES + PATTERN_RATCHET_BYTES;
export const PATTERNS_OFFSET = HEADER_OFFSET + HEADER_SIZE;
export const PATTERNS_SIZE = PATTERN_COUNT * PATTERN_RECORD;
export const CHANNEL_RECORD = 9;
export const CHANNELS_OFFSET = PATTERNS_OFFSET + PATTERNS_SIZE;
export const CHANNELS_SIZE = CHANNEL_COUNT * CHANNEL_RECORD;
export const GLOBAL_OFFSET = CHANNELS_OFFSET + CHANNELS_SIZE;
export const GLOBAL_SIZE = 3;
export const PREFS_OFFSET = GLOBAL_OFFSET + GLOBAL_SIZE;
export const PREFS_SIZE = 6;
export const TOTAL_SIZE = HEADER_SIZE + PATTERNS_SIZE + CHANNELS_SIZE + GLOBAL_SIZE + PREFS_SIZE;

const V3_STEP_BYTES = Math.floor((Pattern.DEFAULT_TOTAL_STEPS + 7) / 8);
const V3_RATCHET_BYTES = Math.floor(Pattern.DEFAULT_TOTAL_STEPS / 2);
const V3_CONTENT_BYTES = V3_STEP_BYTES + V3_RATCHET_BYTES;
const V3_LENGTH_BYTES = 1;

const V3_RECORD_STEPS_AT = 0;
const V3_RECORD_RATCHETS_AT = V3_RECORD_STEPS_AT + V3_STEP_BYTES;
const V3_RECORD_LENGTH_AT = V3_RECORD_RATCHETS_AT + V3_RATCHET_BYTES;

const V3_TEMPLATE_RECORD = V3_CONTENT_BYTES + V3_LENGTH_BYTES;
const V3_INSTANCE_RECORD = V3_CONTENT_BYTES;
const V3_TEMPLATE_COUNT = PATTERN_COUNT;
/** PRD 5.0 : A1 a A8 portent le contenu d'usine de l'original et ne changent jamais. */
const V3_FROZEN_TEMPLATE_COUNT = 8;
const V3_INSTANCE_COUNT = CHANNEL_COUNT;

const V3_HEADER_OFFSET = 0;
const V3_HEADER_SIZE = 1;
const V3_TEMPLATES_OFFSET = V3_HEADER_OFFSET + V3_HEADER_SIZE;
const V3_TEMPLATES_SIZE = V3_TEMPLATE_COUNT * V3_TEMPLATE_RECORD;
const V3_INSTANCES_OFFSET = V3_TEMPLATES_OFFSET + V3_TEMPLATES_SIZE;
const V3_INSTANCES_SIZE = V3_INSTANCE_COUNT * V3_INSTANCE_RECORD;
const V3_CHANNEL_RECORD = 9;
const V3_CHANNELS_OFFSET = V3_INSTANCES_OFFSET + V3_INSTANCES_SIZE;
const V3_CHANNELS_SIZE = CHANNEL_COUNT * V3_CHANNEL_RECORD;
const V3_GLOBAL_OFFSET = V3_CHANNELS_OFFSET + V3_CHANNELS_SIZE;
const V3_GLOBAL_SIZE = 5;
const V3_PREFS_OFFSET = V3_GLOBAL_OFFSET + V3_GLOBAL_SIZE;
const V3_PREFS_SIZE = 6;
const V3_TOTAL_SIZE = V3_PREFS_OFFSET + V3_PREFS_SIZE;

const V3_IMAGE_INSTANCES_AT = 0;
const V3_IMAGE_CHANNELS_AT = V3_IMAGE_INSTANCES_AT + V3_INSTANCES_SIZE;
const V3_IMAGE_GLOBAL_AT = V3_IMAGE_CHANNELS_AT + V3_CHANNELS_SIZE;
const V3_IMAGE_PREFS_AT = V3_IMAGE_GLOBAL_AT + V3_GLOBAL_SIZE;
const V3_IMAGE_VERSION_AT = V3_IMAGE_PREFS_AT + V3_PREFS_SIZE;
const V3_IMAGE_SIZE = V3_IMAGE_VERSION_AT + V3_HEADER_SIZE;

function v3TemplateAddress(index: number, offset: number): number {
  return BASE_ADDRESS + V3_TEMPLATES_OFFSET + index * V3_TEMPLATE_RECORD + offset;
}

const V3_MIN_TEMPLATE_LENGTH = 1;
const V3_MAX_TEMPLATE_LENGTH = Pattern.DEFAULT_TOTAL_STEPS;

const V3_FACTORY_TEMPLATE_LENGTH = FACTORY_STEP_COUNT;

export interface TemplateLength {
  value: number;
}

function v3ContentByte(pattern: Pattern, offset: number): number {
  if (offset < V3_STEP_BYTES) {
    let packed = 0;
    for (let bit = 0; bit < 8; ++bit) {
      if (pattern.readStep(offset * 8 + bit) === true) packed |= 1 << bit;
    }
    return packed;
  }
  const pair = offset - V3_STEP_BYTES;
  const low = pattern.getRatchet(pair * 2) & 0x0f;
  const high = pattern.getRatchet(pair * 2 + 1) & 0x0f;
  return (high << 4) | low;
}

function v3ApplyContentByte(pattern: Pattern, offset: number, value: number): void {
  if (offset < V3_STEP_BYTES) {
    for (let bit = 0; bit < 8; ++bit) {
      pattern.writeStep(offset * 8 + bit, (value & (1 << bit)) !== 0);
    }
    return;
  }
  const low = value & 0x0f;
  const high = (value >> 4) & 0x0f;
  const pair = offset - V3_STEP_BYTES;
  pattern.setRatchet(pair * 2, isValidRatchet(low) ? low : RATCHET_NONE);
  pattern.setRatchet(pair * 2 + 1, isValidRatchet(high) ? high : RATCHET_NONE);
}

function v3TemplateByte(pattern: Pattern, length: number, offset: number): number {
  if (offset === V3_RECORD_LENGTH_AT) {
    if (length < V3_MIN_TEMPLATE_LENGTH) return V3_MIN_TEMPLATE_LENGTH;
    if (length > V3_MAX_TEMPLATE_LENGTH) return V3_MAX_TEMPLATE_LENGTH;
    return length;
  }
  return v3ContentByte(pattern, offset);
}

function v3ApplyTemplateByte(
  pattern: Pattern,
  length: TemplateLength,
  offset: number,
  value: number,
): boolean {
  if (offset === V3_RECORD_LENGTH_AT) {
    if (value < V3_MIN_TEMPLATE_LENGTH || value > V3_MAX_TEMPLATE_LENGTH) return false;
    length.value = value;
    return true;
  }
  v3ApplyContentByte(pattern, offset, value);
  return true;
}

function v3FactoryTemplateByte(index: number, offset: number): number {
  if (index < 0 || index >= V3_TEMPLATE_COUNT) return 0;
  if (offset < 0) return 0;
  if (offset === V3_RECORD_LENGTH_AT) return V3_FACTORY_TEMPLATE_LENGTH;
  if (offset >= V3_RECORD_STEPS_AT + FACTORY_MASK_BYTES) return 0;
  const mask = factoryStepMask(index);
  const shift = (offset - V3_RECORD_STEPS_AT) * 8;
  return (mask >> shift) & 0xff;
}

export const v3 = {
  FORMAT_VERSION: 3,

  STEP_BYTES: V3_STEP_BYTES,
  RATCHET_BYTES: V3_RATCHET_BYTES,
  CONTENT_BYTES: V3_CONTENT_BYTES,
  LENGTH_BYTES: V3_LENGTH_BYTES,

  RECORD_STEPS_AT: V3_RECORD_STEPS_AT,
  RECORD_RATCHETS_AT: V3_RECORD_RATCHETS_AT,
  RECORD_LENGTH_AT: V3_RECORD_LENGTH_AT,

  TEMPLATE_RECORD: V3_TEMPLATE_RECORD,
  INSTANCE_RECORD: V3_INSTANCE_RECORD,
  TEMPLATE_COUNT: V3_TEMPLATE_COUNT,
  FROZEN_TEMPLATE_COUNT: V3_FROZEN_TEMPLATE_COUNT,
  INSTANCE_COUNT: V3_INSTANCE_COUNT,

  HEADER_OFFSET: V3_HEADER_OFFSET,
  HEADER_SIZE: V3_HEADER_SIZE,
  TEMPLATES_OFFSET: V3_TEMPLATES_OFFSET,
  TEMPLATES_SIZE: V3_TEMPLATES_SIZE,
  INSTANCES_OFFSET: V3_INSTANCES_OFFSET,
  INSTANCES_SIZE: V3_INSTANCES_SIZE,
  CHANNEL_RECORD: V3_CHANNEL_RECORD,
  CHANNELS_OFFSET: V3_CHANNELS_OFFSET,
  CHANNELS_SIZE: V3_CHANNELS_SIZE,
  GLOBAL_OFFSET: V3_GLOBAL_OFFSET,
  GLOBAL_SIZE: V3_GLOBAL_SIZE,
  PREFS_OFFSET: V3_PREFS_OFFSET,
  PREFS_SIZE: V3_PREFS_SIZE,

  GLOBAL_TEMPO_LO_AT: 0,
  GLOBAL_TEMPO_HI_AT: 1,
  GLOBAL_CLOCK_SOURCE_AT: 2,
  GLOBAL_MOD_AT: 3,
  GLOBAL_RANGE_AT: 4,

  IMAGE_INSTANCES_AT: V3_IMAGE_INSTANCES_AT,
  IMAGE_CHANNELS_AT: V3_IMAGE_CHANNELS_AT,
  IMAGE_GLOBAL_AT: V3_IMAGE_GLOBAL_AT,
  IMAGE_PREFS_AT: V3_IMAGE_PREFS_AT,
  IMAGE_VERSION_AT: V3_IMAGE_VERSION_AT,
  IMAGE_SIZE: V3_IMAGE_SIZE,

  TOTAL_SIZE: V3_TOTAL_SIZE,
  LAST_ADDRESS: BASE_ADDRESS + V3_TOTAL_SIZE - 1,

  MIN_TEMPLATE_LENGTH: V3_MIN_TEMPLATE_LENGTH,
  MAX_TEMPLATE_LENGTH: V3_MAX_TEMPLATE_LENGTH,
  FACTORY_TEMPLATE_LENGTH: V3_FACTORY_TEMPLATE_LENGTH,

  templateAddress: v3TemplateAddress,
  contentByte: v3ContentByte,
  applyContentByte: v3ApplyContentByte,
  templateByte: v3TemplateByte,
  applyTemplateByte: v3ApplyTemplateByte,
  factoryTemplateByte: v3FactoryTemplateByte,
} as const;

export interface Preferences {
  rotateScreen: number;
  reverseEncoder: number;
  cvCalibration: [number, number];
}

export function defaultPreferences(): Preferences {
  return { rotateScreen: 1, reverseEncoder: 1, cvCalibration: [0, 0] };
}

export interface Storage {
  read(address: number): number;
  write(address: number, value: number): void;
}

function toSigned16(value: number): number {
  return value >= 0x8000 ? value - 0x10000 : value;
}

function channelRecordByte(engine: SequencerEngine, channel: number, offset: number): number {
  switch (offset) {
    case 0: {
      const selected = engine.getSelectedPattern(channel);
      return selected < 0 ? 0 : selected;
    }
    case 1:
      return engine.getBaseLength(channel);
    case 2: {
      const index = SUBDIVS.indexOf(engine.getSubdiv(channel));
      return index < 0 ? SUBDIVS.indexOf(DEFAULT_SUBDIV) : index;
    }
    case 3: {
      const bar = engine.getBarLength(channel);
      return bar < 0 ? 0 : bar;
    }
    case 4:
      return engine.getChannelMode(channel);
    case 5:
      return engine.getOffset(channel) & 0xff;
    case 6:
      return engine.getSkipChance(channel);
    default:
      return 0;
  }
}

function applyChannelRecordByte(
  engine: SequencerEngine,
  channel: number,
  offset: number,
  value: number,
): void {
  switch (offset) {
    case 0:
      engine.setSelectedPattern(channel, value);
      break;
    case 1:
      engine.setBaseLengthFromStorage(channel, value);
      break;
    case 2:
      engine.setSubdiv(channel, SUBDIVS[value] ?? DEFAULT_SUBDIV);
      break;
    case 3:
      engine.setBarLength(channel, value);
      break;
    case 4:
      engine.setChannelMode(channel, value as ChannelMode);
      break;
    case 5:
      engine.setOffset(channel, value);
      break;
    case 6:
      engine.setSkipChance(channel, value);
      break;
    default:
      break;
  }
}

function prefsRecordByte(prefs: Preferences, rel: number): number {
  if (rel === 0) return prefs.rotateScreen;
  if (rel === 1) return prefs.reverseEncoder;
  const channel = Math.floor((rel - 2) / 2);
  const offset = prefs.cvCalibration[channel] ?? 0;
  return (rel - 2) % 2 === 0 ? offset & 0xff : (offset >> 8) & 0xff;
}

function applyPrefsRecordByte(prefs: Preferences, rel: number, value: number): void {
  if (rel === 0) {
    prefs.rotateScreen = value ? 1 : 0;
    return;
  }
  if (rel === 1) {
    prefs.reverseEncoder = value ? 1 : 0;
    return;
  }
  const channel = Math.floor((rel - 2) / 2);
  const current = prefs.cvCalibration[channel] ?? 0;
  const raw = current < 0 ? current + 0x10000 : current;
  const merged = (rel - 2) % 2 === 0 ? (raw & 0xff00) | value : (raw & 0x00ff) | (value << 8);
  prefs.cvCalibration[channel] = toSigned16(merged & 0xffff);
}

export interface ScannedImage {
  readonly size: number;
  readonly versionIndex: number;
  byteAt(index: number): number;
  applyByte(index: number, value: number): void;
  addressAt(index: number): number;
  resetToDefaults(): void;
  readonly templateRecordSize: number;
  canWriteTemplate(channel: number, index: number): boolean;
  templateAddressAt(index: number, offset: number): number;
  templateByteAt(channel: number, index: number, offset: number): number;
}

export class PersistentImage implements ScannedImage {
  static readonly SIZE = TOTAL_SIZE;
  static readonly VERSION_INDEX = HEADER_OFFSET;

  /** La version 2 n'a pas de zone de templates : toute demande est refusee. */
  readonly templateRecordSize = 1;
  canWriteTemplate(): boolean {
    return false;
  }
  templateAddressAt(): number {
    return BASE_ADDRESS;
  }
  templateByteAt(): number {
    return 0;
  }

  readonly size = PersistentImage.SIZE;
  readonly versionIndex = PersistentImage.VERSION_INDEX;

  addressAt(index: number): number {
    return BASE_ADDRESS + index;
  }

  constructor(
    private readonly bank: PatternBank,
    private readonly engine: SequencerEngine,
    private readonly ui: UiController,
    readonly preferences: Preferences,
  ) {}

  byteAt(index: number): number {
    if (index === HEADER_OFFSET) return FORMAT_VERSION;
    if (index < CHANNELS_OFFSET) {
      const rel = index - PATTERNS_OFFSET;
      return this.patternByte(Math.floor(rel / PATTERN_RECORD), rel % PATTERN_RECORD);
    }
    if (index < GLOBAL_OFFSET) {
      const rel = index - CHANNELS_OFFSET;
      return this.channelByte(Math.floor(rel / CHANNEL_RECORD), rel % CHANNEL_RECORD);
    }
    if (index < PREFS_OFFSET) {
      const rel = index - GLOBAL_OFFSET;
      if (rel === 0) return this.ui.tempo & 0xff;
      if (rel === 1) return (this.ui.tempo >> 8) & 0xff;
      return this.ui.clockSource;
    }
    return prefsRecordByte(this.preferences, index - PREFS_OFFSET);
  }

  applyByte(index: number, value: number): void {
    if (index === HEADER_OFFSET) return;
    if (index < CHANNELS_OFFSET) {
      const rel = index - PATTERNS_OFFSET;
      this.applyPatternByte(Math.floor(rel / PATTERN_RECORD), rel % PATTERN_RECORD, value);
      return;
    }
    if (index < GLOBAL_OFFSET) {
      const rel = index - CHANNELS_OFFSET;
      this.applyChannelByte(Math.floor(rel / CHANNEL_RECORD), rel % CHANNEL_RECORD, value);
      return;
    }
    if (index < PREFS_OFFSET) {
      const rel = index - GLOBAL_OFFSET;
      if (rel === 0) {
        this.ui.setTempo((this.ui.tempo & 0xff00) | value);
        return;
      }
      if (rel === 1) {
        this.ui.setTempo((this.ui.tempo & 0x00ff) | (value << 8));
        return;
      }
      this.ui.setClockSource(value);
      return;
    }
    applyPrefsRecordByte(this.preferences, index - PREFS_OFFSET, value);
  }

  resetToDefaults(): void {
    for (let index = 0; index < PATTERN_COUNT; ++index) {
      this.bank.getPattern(index)?.clear();
    }
    for (let channel = 0; channel < CHANNEL_COUNT; ++channel) {
      this.engine.setSelectedPattern(channel, 0);
      this.engine.setBaseLength(channel, DEFAULT_LENGTH);
      this.engine.setSubdiv(channel, DEFAULT_SUBDIV);
      this.engine.setBarLength(channel, DEFAULT_BAR_LENGTH);
      this.engine.setChannelMode(channel, DEFAULT_CHANNEL_MODE);
      this.engine.setOffset(channel, 0);
      this.engine.setSkipChance(channel, 0);
    }
    this.ui.setTempo(DEFAULT_TEMPO);
    this.ui.setClockSource(0);
    const fresh = defaultPreferences();
    this.preferences.rotateScreen = fresh.rotateScreen;
    this.preferences.reverseEncoder = fresh.reverseEncoder;
    this.preferences.cvCalibration[0] = fresh.cvCalibration[0];
    this.preferences.cvCalibration[1] = fresh.cvCalibration[1];
    this.engine.refreshTiming();
  }

  private pattern(index: number): Pattern | null {
    return this.bank.getPattern(index);
  }

  private patternByte(index: number, offset: number): number {
    const p = this.pattern(index);
    if (p === null) return 0;
    if (offset < PATTERN_STEP_BYTES) {
      let packed = 0;
      for (let bit = 0; bit < 8; ++bit) {
        if (p.readStep(offset * 8 + bit) === true) packed |= 1 << bit;
      }
      return packed;
    }
    const pair = offset - PATTERN_STEP_BYTES;
    const low = p.getRatchet(pair * 2) & 0x0f;
    const high = p.getRatchet(pair * 2 + 1) & 0x0f;
    return (high << 4) | low;
  }

  private applyPatternByte(index: number, offset: number, value: number): void {
    const p = this.pattern(index);
    if (p === null) return;
    if (offset < PATTERN_STEP_BYTES) {
      for (let bit = 0; bit < 8; ++bit) {
        p.writeStep(offset * 8 + bit, (value & (1 << bit)) !== 0);
      }
      return;
    }
    const low = value & 0x0f;
    const high = (value >> 4) & 0x0f;
    const pair = offset - PATTERN_STEP_BYTES;
    p.setRatchet(pair * 2, isValidRatchet(low) ? low : RATCHET_NONE);
    p.setRatchet(pair * 2 + 1, isValidRatchet(high) ? high : RATCHET_NONE);
  }

  private channelByte(channel: number, offset: number): number {
    return channelRecordByte(this.engine, channel, offset);
  }

  private applyChannelByte(channel: number, offset: number, value: number): void {
    applyChannelRecordByte(this.engine, channel, offset, value);
  }
}

export class PersistentImageV3 implements ScannedImage {
  static readonly SIZE = V3_IMAGE_SIZE;
  static readonly VERSION_INDEX = V3_IMAGE_VERSION_AT;

  readonly size = PersistentImageV3.SIZE;
  readonly versionIndex = PersistentImageV3.VERSION_INDEX;

  constructor(
    private readonly engine: SequencerEngine,
    private readonly ui: UiController,
    readonly preferences: Preferences,
  ) {}

  addressAt(index: number): number {
    let offset: number;
    if (index === V3_IMAGE_VERSION_AT) {
      offset = V3_HEADER_OFFSET;
    } else if (index < V3_IMAGE_CHANNELS_AT) {
      offset = V3_INSTANCES_OFFSET + (index - V3_IMAGE_INSTANCES_AT);
    } else if (index < V3_IMAGE_GLOBAL_AT) {
      offset = V3_CHANNELS_OFFSET + (index - V3_IMAGE_CHANNELS_AT);
    } else if (index < V3_IMAGE_PREFS_AT) {
      offset = V3_GLOBAL_OFFSET + (index - V3_IMAGE_GLOBAL_AT);
    } else {
      offset = V3_PREFS_OFFSET + (index - V3_IMAGE_PREFS_AT);
    }
    return BASE_ADDRESS + offset;
  }

  byteAt(index: number): number {
    if (index === V3_IMAGE_VERSION_AT) return v3.FORMAT_VERSION;
    if (index < V3_IMAGE_CHANNELS_AT) {
      const rel = index - V3_IMAGE_INSTANCES_AT;
      const instance = this.engine.instanceForChannel(Math.floor(rel / V3_INSTANCE_RECORD));
      if (instance === null) return 0;
      return v3ContentByte(instance, rel % V3_INSTANCE_RECORD);
    }
    if (index < V3_IMAGE_GLOBAL_AT) {
      const rel = index - V3_IMAGE_CHANNELS_AT;
      return channelRecordByte(
        this.engine,
        Math.floor(rel / V3_CHANNEL_RECORD),
        rel % V3_CHANNEL_RECORD,
      );
    }
    if (index < V3_IMAGE_PREFS_AT) {
      const rel = index - V3_IMAGE_GLOBAL_AT;
      if (rel === v3.GLOBAL_TEMPO_LO_AT) return this.ui.tempo & 0xff;
      if (rel === v3.GLOBAL_TEMPO_HI_AT) return (this.ui.tempo >> 8) & 0xff;
      if (rel === v3.GLOBAL_CLOCK_SOURCE_AT) return this.ui.clockSource;
      return 0;
    }
    return prefsRecordByte(this.preferences, index - V3_IMAGE_PREFS_AT);
  }

  applyByte(index: number, value: number): void {
    if (index === V3_IMAGE_VERSION_AT) return;
    if (index < V3_IMAGE_CHANNELS_AT) {
      const rel = index - V3_IMAGE_INSTANCES_AT;
      const instance = this.engine.instanceForChannel(Math.floor(rel / V3_INSTANCE_RECORD));
      if (instance !== null) {
        v3ApplyContentByte(instance, rel % V3_INSTANCE_RECORD, value);
      }
      return;
    }
    if (index < V3_IMAGE_GLOBAL_AT) {
      const rel = index - V3_IMAGE_CHANNELS_AT;
      applyChannelRecordByte(
        this.engine,
        Math.floor(rel / V3_CHANNEL_RECORD),
        rel % V3_CHANNEL_RECORD,
        value,
      );
      return;
    }
    if (index < V3_IMAGE_PREFS_AT) {
      const rel = index - V3_IMAGE_GLOBAL_AT;
      if (rel === v3.GLOBAL_TEMPO_LO_AT) {
        this.ui.setTempo((this.ui.tempo & 0xff00) | value);
        return;
      }
      if (rel === v3.GLOBAL_TEMPO_HI_AT) {
        this.ui.setTempo((this.ui.tempo & 0x00ff) | (value << 8));
        return;
      }
      if (rel === v3.GLOBAL_CLOCK_SOURCE_AT) {
        this.ui.setClockSource(value);
      }
      return;
    }
    applyPrefsRecordByte(this.preferences, index - V3_IMAGE_PREFS_AT, value);
  }

  seedFactoryTemplates(storage: Storage): void {
    for (let index = 0; index < V3_TEMPLATE_COUNT; ++index) {
      for (let offset = 0; offset < V3_TEMPLATE_RECORD; ++offset) {
        storage.write(v3TemplateAddress(index, offset), v3FactoryTemplateByte(index, offset));
      }
    }
  }

  readonly templateRecordSize = V3_TEMPLATE_RECORD;

  canWriteTemplate(channel: number, index: number): boolean {
    if (!Number.isInteger(index)) return false;
    if (index < V3_FROZEN_TEMPLATE_COUNT || index >= V3_TEMPLATE_COUNT) return false;
    return this.engine.instanceForChannel(channel) !== null;
  }

  templateAddressAt(index: number, offset: number): number {
    return v3TemplateAddress(index, offset);
  }

  templateByteAt(channel: number, index: number, offset: number): number {
    void index;
    const instance = this.engine.instanceForChannel(channel);
    if (instance === null) return 0;
    return v3TemplateByte(instance, this.engine.getBaseLength(channel), offset);
  }

  saveTemplate(storage: Storage, channel: number, index: number): boolean {
    if (!Number.isInteger(index)) return false;
    if (index < V3_FROZEN_TEMPLATE_COUNT || index >= V3_TEMPLATE_COUNT) return false;
    const instance = this.engine.instanceForChannel(channel);
    if (instance === null) return false;
    const length = this.engine.getBaseLength(channel);
    for (let offset = 0; offset < V3_TEMPLATE_RECORD; ++offset) {
      storage.write(v3TemplateAddress(index, offset), v3TemplateByte(instance, length, offset));
    }
    return true;
  }

  loadTemplate(storage: Storage, channel: number, index: number): boolean {
    if (!Number.isInteger(index) || index < 0 || index >= V3_TEMPLATE_COUNT) return false;
    const instance = this.engine.instanceForChannel(channel);
    if (instance === null) return false;
    for (let offset = 0; offset < V3_CONTENT_BYTES; ++offset) {
      v3ApplyContentByte(instance, offset, storage.read(v3TemplateAddress(index, offset)));
    }
    // PRD 11.1 : une longueur hors plage est refusee, le contenu deja lu reste.
    this.engine.setBaseLengthFromStorage(
      channel,
      storage.read(v3TemplateAddress(index, V3_RECORD_LENGTH_AT)),
    );
    this.engine.setSelectedPattern(channel, index);
    return true;
  }

  loadTemplatesIntoInstances(storage: Storage): void {
    for (let channel = 0; channel < CHANNEL_COUNT; ++channel) {
      const instance = this.engine.instanceForChannel(channel);
      const selected = this.engine.getSelectedPattern(channel);
      if (instance === null || selected < 0) continue;
      for (let offset = 0; offset < V3_CONTENT_BYTES; ++offset) {
        v3ApplyContentByte(instance, offset, storage.read(v3TemplateAddress(selected, offset)));
      }
    }
  }

  resetToDefaults(): void {
    for (let channel = 0; channel < CHANNEL_COUNT; ++channel) {
      this.engine.instanceForChannel(channel)?.clear();
      this.engine.setSelectedPattern(channel, 0);
      this.engine.setBaseLength(channel, DEFAULT_LENGTH);
      this.engine.setSubdiv(channel, DEFAULT_SUBDIV);
      this.engine.setBarLength(channel, DEFAULT_BAR_LENGTH);
      this.engine.setChannelMode(channel, DEFAULT_CHANNEL_MODE);
      this.engine.setOffset(channel, 0);
      this.engine.setSkipChance(channel, 0);
    }
    this.ui.setTempo(DEFAULT_TEMPO);
    this.ui.setClockSource(0);
    const fresh = defaultPreferences();
    this.preferences.rotateScreen = fresh.rotateScreen;
    this.preferences.reverseEncoder = fresh.reverseEncoder;
    this.preferences.cvCalibration[0] = fresh.cvCalibration[0];
    this.preferences.cvCalibration[1] = fresh.cvCalibration[1];
    this.engine.refreshTiming();
  }
}

export class PersistenceScheduler {
  static readonly NO_TEMPLATE = 0xff;

  private lastChangeMs = 0;
  private cursorIndex = 0;
  private dirtyFlag = false;
  private writingFlag = false;
  private templateChannel = 0;
  private templateIndex = PersistenceScheduler.NO_TEMPLATE;
  private templateCursor = 0;

  requestTemplateWrite(image: ScannedImage, channel: number, index: number): boolean {
    if (this.templateIndex !== PersistenceScheduler.NO_TEMPLATE) return false;
    if (!image.canWriteTemplate(channel, index)) return false;
    this.templateChannel = channel;
    this.templateIndex = index;
    this.templateCursor = 0;
    return true;
  }

  get isWritingTemplate(): boolean {
    return this.templateIndex !== PersistenceScheduler.NO_TEMPLATE;
  }

  markDirty(nowMs: number): void {
    this.dirtyFlag = true;
    this.lastChangeMs = nowMs;
    this.writingFlag = false;
    this.cursorIndex = 0;
  }

  get isDirty(): boolean {
    return this.dirtyFlag;
  }

  get isWriting(): boolean {
    return this.writingFlag;
  }

  get cursor(): number {
    return this.cursorIndex;
  }

  quietElapsed(nowMs: number): boolean {
    return this.dirtyFlag && nowMs - this.lastChangeMs >= QUIET_MS;
  }

  advance(storage: Storage, image: ScannedImage, nowMs: number): boolean {
    // Une ecriture de template est une COMMANDE, pas un anti-rebond : elle passe
    // avant le balayage et n'attend aucun delai de calme. Un octet par appel.
    if (this.templateIndex !== PersistenceScheduler.NO_TEMPLATE) {
      storage.write(
        image.templateAddressAt(this.templateIndex, this.templateCursor),
        image.templateByteAt(this.templateChannel, this.templateIndex, this.templateCursor),
      );
      ++this.templateCursor;
      if (this.templateCursor >= image.templateRecordSize) {
        this.templateIndex = PersistenceScheduler.NO_TEMPLATE;
      }
      return true;
    }
    if (!this.writingFlag) {
      if (!this.quietElapsed(nowMs)) return false;
      this.writingFlag = true;
      this.cursorIndex = 0;
    }
    while (this.cursorIndex < image.size) {
      const address = image.addressAt(this.cursorIndex);
      const wanted = image.byteAt(this.cursorIndex);
      ++this.cursorIndex;
      if (storage.read(address) !== wanted) {
        storage.write(address, wanted);
        return true;
      }
    }
    this.writingFlag = false;
    this.dirtyFlag = false;
    return false;
  }

  load(storage: Storage, image: ScannedImage): boolean {
    if (storage.read(image.addressAt(image.versionIndex))
        !== image.byteAt(image.versionIndex)) {
      image.resetToDefaults();
      return false;
    }
    for (let index = 0; index < image.size; ++index) {
      if (index === image.versionIndex) continue;
      image.applyByte(index, storage.read(image.addressAt(index)));
    }
    return true;
  }
}

export function bootstrap(
  storage: Storage,
  image: PersistentImageV3,
  scheduler: PersistenceScheduler,
  nowMs: number,
): boolean {
  if (scheduler.load(storage, image)) return true;
  image.seedFactoryTemplates(storage);
  image.loadTemplatesIntoInstances(storage);
  scheduler.markDirty(nowMs);
  return false;
}
