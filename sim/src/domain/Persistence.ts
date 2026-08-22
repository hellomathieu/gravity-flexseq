import { PATTERN_COUNT, type PatternBank } from "./PatternBank.js";
import { isValidRatchet, RATCHET_NONE, type Pattern } from "./Pattern.js";
import { CHANNEL_COUNT, DEFAULT_BAR_LENGTH, DEFAULT_LENGTH, type SequencerEngine } from "./SequencerEngine.js";
import { DEFAULT_SUBDIV, SUBDIVS } from "./subdiv.js";
import { DEFAULT_TEMPO, type UiController } from "./UiController.js";

export const BASE_ADDRESS = 384;
export const FORMAT_VERSION = 1;
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
export const CHANNEL_RECORD = 6;
export const CHANNELS_OFFSET = PATTERNS_OFFSET + PATTERNS_SIZE;
export const CHANNELS_SIZE = CHANNEL_COUNT * CHANNEL_RECORD;
export const GLOBAL_OFFSET = CHANNELS_OFFSET + CHANNELS_SIZE;
export const GLOBAL_SIZE = 3;
export const PREFS_OFFSET = GLOBAL_OFFSET + GLOBAL_SIZE;
export const PREFS_SIZE = 6;
export const TOTAL_SIZE = HEADER_SIZE + PATTERNS_SIZE + CHANNELS_SIZE + GLOBAL_SIZE + PREFS_SIZE;

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

export class PersistentImage {
  static readonly SIZE = TOTAL_SIZE;

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
    const rel = index - PREFS_OFFSET;
    if (rel === 0) return this.preferences.rotateScreen;
    if (rel === 1) return this.preferences.reverseEncoder;
    const channel = Math.floor((rel - 2) / 2);
    const offset = this.preferences.cvCalibration[channel] ?? 0;
    return (rel - 2) % 2 === 0 ? offset & 0xff : (offset >> 8) & 0xff;
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
    const rel = index - PREFS_OFFSET;
    if (rel === 0) {
      this.preferences.rotateScreen = value ? 1 : 0;
      return;
    }
    if (rel === 1) {
      this.preferences.reverseEncoder = value ? 1 : 0;
      return;
    }
    const channel = Math.floor((rel - 2) / 2);
    const current = this.preferences.cvCalibration[channel] ?? 0;
    const raw = current < 0 ? current + 0x10000 : current;
    const merged = (rel - 2) % 2 === 0 ? (raw & 0xff00) | value : (raw & 0x00ff) | (value << 8);
    this.preferences.cvCalibration[channel] = toSigned16(merged & 0xffff);
  }

  resetToDefaults(): void {
    for (let index = 0; index < PATTERN_COUNT; ++index) {
      this.bank.getPattern(index)?.clear();
    }
    for (let channel = 0; channel < CHANNEL_COUNT; ++channel) {
      this.engine.setSelectedPattern(channel, 0);
      this.engine.setEffectiveLength(channel, DEFAULT_LENGTH);
      this.engine.setSubdiv(channel, DEFAULT_SUBDIV);
      this.engine.setBarLength(channel, DEFAULT_BAR_LENGTH);
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
    switch (offset) {
      case 0: {
        const selected = this.engine.getSelectedPattern(channel);
        return selected < 0 ? 0 : selected;
      }
      case 1:
        return this.engine.getEffectiveLength(channel);
      case 2: {
        const index = SUBDIVS.indexOf(this.engine.getSubdiv(channel));
        return index < 0 ? SUBDIVS.indexOf(DEFAULT_SUBDIV) : index;
      }
      case 3: {
        const bar = this.engine.getBarLength(channel);
        return bar < 0 ? 0 : bar;
      }
      default:
        return 0;
    }
  }

  private applyChannelByte(channel: number, offset: number, value: number): void {
    switch (offset) {
      case 0:
        this.engine.setSelectedPattern(channel, value);
        break;
      case 1:
        this.engine.setEffectiveLength(channel, value);
        break;
      case 2:
        this.engine.setSubdiv(channel, SUBDIVS[value] ?? DEFAULT_SUBDIV);
        break;
      case 3:
        this.engine.setBarLength(channel, value);
        break;
      default:
        break;
    }
  }
}

export class PersistenceScheduler {
  private lastChangeMs = 0;
  private cursorIndex = 0;
  private dirtyFlag = false;
  private writingFlag = false;

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

  advance(storage: Storage, image: PersistentImage, nowMs: number): boolean {
    if (!this.writingFlag) {
      if (!this.quietElapsed(nowMs)) return false;
      this.writingFlag = true;
      this.cursorIndex = 0;
    }
    while (this.cursorIndex < PersistentImage.SIZE) {
      const address = BASE_ADDRESS + this.cursorIndex;
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

  load(storage: Storage, image: PersistentImage): boolean {
    if (storage.read(BASE_ADDRESS + HEADER_OFFSET) !== FORMAT_VERSION) {
      image.resetToDefaults();
      return false;
    }
    for (let index = HEADER_SIZE; index < PersistentImage.SIZE; ++index) {
      image.applyByte(index, storage.read(BASE_ADDRESS + index));
    }
    return true;
  }
}
