import { Pattern, ratchetFitsStep, RATCHET_CODES } from "./Pattern.js";
import type { PatternBank } from "./PatternBank.js";
import {
  BAR_LENGTHS,
  CHANNEL_COUNT,
  MAX_LENGTH,
  MIN_LENGTH,
  type SequencerEngine,
} from "./SequencerEngine.js";
import { PATTERN_COUNT } from "./PatternBank.js";
import { DEFAULT_SUBDIV, SUBDIVS } from "./subdiv.js";
import type { Transport } from "./Transport.js";

export enum UiEvent {
  Rotate,
  RotateHeld,
  Press,
  LongPress,
  ShiftRotate,
  ShiftPress,
  ShiftLongPress,
  PlayPress,
}

export enum UiLevel {
  TabBar,
  Tab,
  Edit,
}

export enum UiField {
  None,
  Tempo,
  ClockSource,
  Pattern,
  Length,
  Subdiv,
  BarLength,
  EditEntry,
}

export const TAB_COUNT = 8;
export const TAB_CLOCK = 0;
export const TAB_FIRST_CHANNEL = 1;
export const TAB_SETTINGS = 7;

export const CLOCK_TAB_FIELDS = 2;
export const CHANNEL_TAB_FIELDS = 5;

export const CLOCK_SOURCE_COUNT = 6;
export const MIN_TEMPO = 30;
export const MAX_TEMPO = 300;
export const DEFAULT_TEMPO = 120;

export const STEP_COUNT = Pattern.DEFAULT_TOTAL_STEPS;

function wrapIndex(current: number, delta: number, count: number): number {
  if (count === 0) return 0;
  const value = (current + delta) % count;
  return value < 0 ? value + count : value;
}

function clampRange(value: number, low: number, high: number): number {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

function oneStep(delta: number): number {
  if (delta > 0) return 1;
  if (delta < 0) return -1;
  return 0;
}

function clampIndex(current: number, delta: number, count: number): number {
  if (count === 0) return 0;
  return clampRange(current + delta, 0, count - 1);
}

export class UiController {
  private currentLevel = UiLevel.TabBar;
  private tab = TAB_FIRST_CHANNEL;
  private fieldCursor = 0;
  private step = 0;
  private open = false;
  private currentTempo = DEFAULT_TEMPO;
  private source = 0;
  private rev = 0;

  constructor(
    private readonly engine: SequencerEngine,
    private readonly bank: PatternBank,
    private readonly transport: Transport,
  ) {}

  get revision(): number {
    return this.rev;
  }

  get level(): UiLevel {
    return this.currentLevel;
  }

  get currentTab(): number {
    return this.tab;
  }

  get isChannelTab(): boolean {
    return this.tab >= TAB_FIRST_CHANNEL && this.tab < TAB_FIRST_CHANNEL + CHANNEL_COUNT;
  }

  get selectedChannel(): number {
    return this.isChannelTab ? this.tab - TAB_FIRST_CHANNEL : -1;
  }

  get fieldCount(): number {
    if (this.tab === TAB_CLOCK) return CLOCK_TAB_FIELDS;
    if (this.isChannelTab) return CHANNEL_TAB_FIELDS;
    return 0;
  }

  fieldAt(index: number): UiField {
    if (index < 0 || index >= this.fieldCount) return UiField.None;
    if (this.tab === TAB_CLOCK) {
      return index === 0 ? UiField.Tempo : UiField.ClockSource;
    }
    switch (index) {
      case 0:
        return UiField.Pattern;
      case 1:
        return UiField.Length;
      case 2:
        return UiField.Subdiv;
      case 3:
        return UiField.BarLength;
      default:
        return UiField.EditEntry;
    }
  }

  get field(): UiField {
    return this.fieldAt(this.fieldCursor);
  }

  get cursor(): number {
    return this.fieldCursor;
  }

  get fieldOpen(): boolean {
    return this.open;
  }

  get stepCursor(): number {
    return this.step;
  }

  get tempo(): number {
    return this.currentTempo;
  }

  get clockSource(): number {
    return this.source;
  }

  setTempo(bpm: number): boolean {
    if (bpm < MIN_TEMPO || bpm > MAX_TEMPO) return false;
    this.currentTempo = bpm;
    this.rev = (this.rev + 1) & 0xff;
    return true;
  }

  setClockSource(source: number): boolean {
    if (source < 0 || source >= CLOCK_SOURCE_COUNT) return false;
    this.source = source;
    this.rev = (this.rev + 1) & 0xff;
    return true;
  }

  handle(event: UiEvent, delta = 0): void {
    this.rev = (this.rev + 1) & 0xff;
    if (event === UiEvent.PlayPress) {
      this.togglePlay();
      return;
    }
    if (event === UiEvent.ShiftPress) return;

    switch (this.currentLevel) {
      case UiLevel.TabBar:
        this.handleTabBar(event, delta);
        break;
      case UiLevel.Tab:
        this.handleTab(event, delta);
        break;
      case UiLevel.Edit:
        this.handleEdit(event, delta);
        break;
    }
  }

  private handleTabBar(event: UiEvent, delta: number): void {
    if (event === UiEvent.Rotate) {
      this.tab = wrapIndex(this.tab, oneStep(delta), TAB_COUNT);
      this.fieldCursor = 0;
      this.open = false;
      return;
    }
    if (event === UiEvent.Press && this.fieldCount > 0) {
      this.currentLevel = UiLevel.Tab;
      this.fieldCursor = 0;
      this.open = false;
    }
  }

  private handleTab(event: UiEvent, delta: number): void {
    switch (event) {
      case UiEvent.Rotate:
        if (this.open) {
          this.adjustField(delta);
        } else {
          this.fieldCursor = wrapIndex(this.fieldCursor, oneStep(delta), this.fieldCount);
        }
        break;
      case UiEvent.ShiftRotate:
        this.adjustField(delta);
        break;
      case UiEvent.Press:
        if (this.open) {
          this.open = false;
        } else if (this.field === UiField.EditEntry) {
          this.currentLevel = UiLevel.Edit;
          this.step = 0;
        } else if (this.field !== UiField.None) {
          this.open = true;
        }
        break;
      case UiEvent.LongPress:
        if (this.open) {
          this.open = false;
        } else {
          this.currentLevel = UiLevel.TabBar;
        }
        break;
      default:
        break;
    }
  }

  private handleEdit(event: UiEvent, delta: number): void {
    switch (event) {
      case UiEvent.Rotate:
        this.step = wrapIndex(this.step, oneStep(delta), STEP_COUNT);
        break;
      case UiEvent.RotateHeld:
        this.adjustRatchet(delta);
        break;
      case UiEvent.Press:
        this.toggleStep();
        break;
      case UiEvent.LongPress:
        this.currentLevel = UiLevel.Tab;
        this.open = false;
        break;
      case UiEvent.ShiftRotate: {
        const channel = this.selectedChannel;
        if (channel >= 0) {
          this.tab = TAB_FIRST_CHANNEL + wrapIndex(channel, oneStep(delta), CHANNEL_COUNT);
        }
        break;
      }
      case UiEvent.ShiftLongPress:
        this.clearPattern();
        break;
      default:
        break;
    }
  }

  private currentPattern(): Pattern | null {
    const channel = this.selectedChannel;
    if (channel < 0) return null;
    const index = this.engine.getSelectedPattern(channel);
    if (index < 0) return null;
    return this.bank.getPattern(index);
  }

  private adjustField(accelerated: number): void {
    const channel = this.selectedChannel;
    const delta = this.field === UiField.Tempo ? accelerated : oneStep(accelerated);
    switch (this.field) {
      case UiField.Tempo:
        this.currentTempo = clampRange(this.currentTempo + delta, MIN_TEMPO, MAX_TEMPO);
        break;
      case UiField.ClockSource:
        this.source = clampIndex(this.source, delta, CLOCK_SOURCE_COUNT);
        break;
      case UiField.Pattern: {
        if (channel < 0) break;
        const current = this.engine.getSelectedPattern(channel);
        if (current < 0) break;
        this.engine.setSelectedPattern(channel, clampIndex(current, delta, PATTERN_COUNT));
        break;
      }
      case UiField.Length: {
        if (channel < 0) break;
        const current = this.engine.getEffectiveLength(channel);
        this.engine.setEffectiveLength(channel, clampRange(current + delta, MIN_LENGTH, MAX_LENGTH));
        break;
      }
      case UiField.Subdiv: {
        if (channel < 0) break;
        let index = SUBDIVS.indexOf(this.engine.getSubdiv(channel));
        if (index < 0) index = SUBDIVS.indexOf(DEFAULT_SUBDIV);
        const next = clampIndex(index, delta, SUBDIVS.length);
        this.engine.setSubdiv(channel, SUBDIVS[next]!);
        break;
      }
      case UiField.BarLength: {
        if (channel < 0) break;
        let index = BAR_LENGTHS.indexOf(this.engine.getBarLength(channel));
        if (index < 0) index = 0;
        const next = clampIndex(index, delta, BAR_LENGTHS.length);
        this.engine.setBarLength(channel, BAR_LENGTHS[next]!);
        break;
      }
      default:
        break;
    }
  }

  private adjustRatchet(delta: number): void {
    const pattern = this.currentPattern();
    if (pattern === null) return;
    const channel = this.selectedChannel;
    if (channel < 0) return;
    const step = oneStep(delta);
    if (step === 0) return;
    const ticks = this.engine.getTicksPerStep(channel);

    let index = RATCHET_CODES.indexOf(pattern.getRatchet(this.step));
    if (index < 0) index = 0;

    let cursor = index;
    for (let tried = 0; tried < RATCHET_CODES.length; ++tried) {
      const candidate = clampIndex(cursor, step, RATCHET_CODES.length);
      if (candidate === cursor) return;
      cursor = candidate;
      if (ratchetFitsStep(RATCHET_CODES[cursor]!, ticks)) {
        pattern.setRatchet(this.step, RATCHET_CODES[cursor]!);
        this.engine.refreshTiming(channel);
        return;
      }
    }
  }

  private togglePlay(): void {
    if (this.engine.isRunning) {
      this.transport.stop();
    } else {
      this.transport.start();
    }
  }

  private toggleStep(): void {
    const pattern = this.currentPattern();
    if (pattern === null) return;
    const active = pattern.readStep(this.step);
    if (active === null) return;
    pattern.writeStep(this.step, !active);
  }

  private clearPattern(): void {
    const pattern = this.currentPattern();
    if (pattern === null) return;
    pattern.clear();
    this.engine.refreshTiming();
  }
}
