import { Pattern, ratchetFitsStep, RATCHET_CODES } from "./Pattern.js";
import {
  CV_SOURCE_1,
  CV_SOURCE_2,
  MOD_CHOICE_COUNT,
  modChoiceAt,
  modIndexOf,
} from "./CvDestination.js";
import {
  BAR_LENGTHS,
  CHANNEL_COUNT,
  CHANNEL_MODE_COUNT,
  ChannelMode,
  MAX_LENGTH,
  MAX_SKIP_CHANCE_SETTING,
  MIN_LENGTH,
  type SequencerEngine,
} from "./SequencerEngine.js";
import { PATTERN_COUNT } from "./PatternBank.js";
import { PatternAction, PATTERN_ACTION_COUNT } from "./PatternAction.js";
import { DEFAULT_SUBDIV, SUBDIVS } from "./subdiv.js";
import type { Transport } from "./Transport.js";

export enum UiEvent {
  Rotate,
  Press,
  LongPress,
  ShiftRotate,
  ShiftPress,
  ShiftLongPress,
  PlayPress,
  ShiftPlayPress,
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
  SkipChance,
  Mode,
  Offset,
  Mod,
  Config,
  Slot,
}

export const TAB_COUNT = 9;
export const TAB_CLOCK = 0;
export const TAB_FIRST_CHANNEL = 1;
export const TAB_PATTERNS = 7;
export const TAB_SETTINGS = 8;

export const FIRST_WRITABLE_TEMPLATE = 8;

export const CLOCK_TAB_FIELDS = 2;
export const PATTERNS_TAB_FIELDS = 1;
export const CHANNEL_TAB_FIELDS = 3;
export const SEQ_CHANNEL_TAB_FIELDS = 4;
export const CONFIG_PAGE_FIELDS = 3;

// PRD 5.0 amendement 1bis : la grande valeur est la PREMIERE position, parce
// qu elle est la premiere valeur a choisir avant d editer un pattern.
export const SEQ_FIELD_INDEX_PATTERN = 0;
export const SEQ_FIELD_INDEX_MODE = 1;
export const SEQ_FIELD_INDEX_EDIT_ENTRY = 2;
export const SEQ_FIELD_INDEX_CONFIG = 3;

export const CONFIG_FIELD_INDEX_LENGTH = 0;
export const CONFIG_FIELD_INDEX_SUBDIV = 1;
export const CONFIG_FIELD_INDEX_MOD = 2;

export const PATTERNS_FIELD_INDEX_EDIT_ENTRY = 0;

export const CLOCK_SOURCE_COUNT = 6;
export const CLOCK_SOURCE_INTERNAL = 0;
export const MIN_TEMPO = 20;
export const MAX_TEMPO = 300;
export const DEFAULT_TEMPO = 120;

export const STEP_COUNT = 36;

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
  private slot = FIRST_WRITABLE_TEMPLATE;
  private header = false;
  private configPage = false;
  private open = false;
  private currentTempo = DEFAULT_TEMPO;
  private source = 0;
  private rev = 0;
  private action: PatternAction = PatternAction.Load;

  constructor(
    private readonly engine: SequencerEngine,
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

  get isOnHeader(): boolean {
    return this.header;
  }

  get isOnConfigPage(): boolean {
    return this.configPage;
  }

  get isLegacyModeTab(): boolean {
    const channel = this.selectedChannel;
    if (channel < 0) return false;
    const mode = this.engine.getChannelMode(channel);
    return mode === ChannelMode.CLOCK || mode === ChannelMode.RANDOM;
  }

  get patternAction(): PatternAction {
    return this.action;
  }

  // SAVE n apparait que si la copie du canal differe du template qu il a charge
  // — PRD 12.9 point 5. Un moteur non cable n a pas de drapeau : il ne propose
  // alors que LOAD.
  get patternActionCount(): number {
    const state = this.engine.modulatedPatterns();
    const channel = this.selectedChannel;
    if (state === null || channel < 0) {
      return 1;
    }
    return state.isDirty(channel) ? PATTERN_ACTION_COUNT : 1;
  }

  get fieldCount(): number {
    if (this.tab === TAB_CLOCK) return CLOCK_TAB_FIELDS;
    if (this.isChannelTab) {
      if (this.configPage) return CONFIG_PAGE_FIELDS;
      // La grande valeur n existe qu en SEQ : les deux autres modes ne lisent
      // aucun pattern.
      return this.isLegacyModeTab ? CHANNEL_TAB_FIELDS : SEQ_CHANNEL_TAB_FIELDS;
    }
    if (this.tab === TAB_PATTERNS) return PATTERNS_TAB_FIELDS;
    return 0;
  }

  fieldAt(index: number): UiField {
    if (index < 0 || index >= this.fieldCount) return UiField.None;
    if (this.tab === TAB_CLOCK) {
      return index === 0 ? UiField.Tempo : UiField.ClockSource;
    }
    if (this.tab === TAB_PATTERNS) {
      // Une seule ligne : l emplacement se change par SHIFT plus rotation
      // depuis la barre, comme le pattern d un canal.
      return UiField.EditEntry;
    }
    if (this.configPage) {
      switch (index) {
        case CONFIG_FIELD_INDEX_LENGTH:
          return UiField.Length;
        case CONFIG_FIELD_INDEX_SUBDIV:
          return UiField.Subdiv;
        default:
          return UiField.Mod;
      }
    }
    if (this.isLegacyModeTab) {
      const mode = this.engine.getChannelMode(this.selectedChannel);
      switch (index) {
        case 0:
          return UiField.Mode;
        case 1:
          return mode === ChannelMode.CLOCK ? UiField.Offset : UiField.Subdiv;
        default:
          return UiField.Mod;
      }
    }
    switch (index) {
      case SEQ_FIELD_INDEX_PATTERN:
        return UiField.Pattern;
      case SEQ_FIELD_INDEX_MODE:
        return UiField.Mode;
      case SEQ_FIELD_INDEX_EDIT_ENTRY:
        return UiField.EditEntry;
      default:
        return UiField.Config;
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

  get slotCursor(): number {
    return this.slot;
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
    if (event === UiEvent.ShiftPress || event === UiEvent.ShiftPlayPress) return;

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
    if (event === UiEvent.ShiftRotate) {
      this.adjustFieldValue(this.mainField, delta);
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
        if (this.open && this.field === UiField.Pattern) {
          // Le champ ouvert choisit une ACTION. Le NUMERO du template se nomme
          // par SHIFT plus rotation, et ce geste ne change pas.
          this.action = clampIndex(this.action, oneStep(delta), this.patternActionCount);
        } else if (this.open) {
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
          this.header = false;
        } else if (this.field === UiField.Config) {
          this.configPage = true;
          this.fieldCursor = CONFIG_FIELD_INDEX_LENGTH;
        } else if (this.field !== UiField.None) {
          this.open = true;
          this.action = PatternAction.Load;
        }
        break;
      case UiEvent.LongPress:
        if (this.open) {
          this.open = false;
        } else if (this.configPage) {
          this.configPage = false;
          this.fieldCursor = SEQ_FIELD_INDEX_CONFIG;
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
      case UiEvent.Rotate: {
        const s = oneStep(delta);
        if (this.header) {
          if (this.open) {
            this.adjustFieldValue(UiField.BarLength, delta);
          } else if (s > 0) {
            this.header = false;
            this.step = 0;
          }
        } else if (s < 0 && this.step === 0) {
          this.header = true;
        } else {
          this.step = wrapIndex(this.step, s, STEP_COUNT);
        }
        break;
      }
      case UiEvent.Press:
        if (this.header) this.open = !this.open;
        else this.toggleStep();
        break;
      case UiEvent.LongPress:
        if (this.header) {
          this.header = false;
          this.open = false;
          this.step = 0;
        } else {
          this.currentLevel = UiLevel.Tab;
          this.open = false;
        }
        break;
      case UiEvent.ShiftRotate:
        if (!this.header) this.adjustRatchet(delta);
        break;
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
    return this.engine.patternForChannel(channel);
  }

  get mainField(): UiField {
    if (this.tab === TAB_CLOCK) return UiField.Tempo;
    if (this.tab === TAB_PATTERNS) return UiField.Slot;
    const channel = this.selectedChannel;
    if (channel < 0) return UiField.None;
    switch (this.engine.getChannelMode(channel)) {
      case ChannelMode.CLOCK:
        return UiField.Subdiv;
      case ChannelMode.RANDOM:
        return UiField.SkipChance;
      default:
        return UiField.Pattern;
    }
  }

  private adjustField(delta: number): void {
    this.adjustFieldValue(this.field, delta);
  }

  private adjustFieldValue(target: UiField, raw: number): void {
    const channel = this.selectedChannel;
    const delta = oneStep(raw);
    switch (target) {
      case UiField.Tempo:
        this.currentTempo = clampRange(this.currentTempo + delta, MIN_TEMPO, MAX_TEMPO);
        break;
      case UiField.ClockSource:
        this.source = clampIndex(this.source, delta, CLOCK_SOURCE_COUNT);
        break;
      case UiField.Slot:
        this.slot = clampRange(this.slot + delta, FIRST_WRITABLE_TEMPLATE, PATTERN_COUNT - 1);
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
        const current = this.engine.getBaseLength(channel);
        this.engine.setBaseLength(channel, clampRange(current + delta, MIN_LENGTH, MAX_LENGTH));
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
      case UiField.Mode: {
        if (channel < 0) break;
        const current = this.engine.getChannelMode(channel) as number;
        this.engine.setChannelMode(channel, clampIndex(current, delta, CHANNEL_MODE_COUNT));
        // ⚠️ La position 0 change de sens avec le mode : elle porte MODE hors
        // SEQ et la grande valeur en SEQ. Sans ce recalage le curseur resterait
        // immobile pendant que le champ sous lui changerait.
        this.fieldCursor = this.isLegacyModeTab ? 0 : SEQ_FIELD_INDEX_MODE;
        break;
      }
      case UiField.Mod: {
        if (channel < 0) break;
        if (this.isLegacyModeTab) break;
        const current = modIndexOf(
          this.engine.getCvDestination(channel, CV_SOURCE_1),
          this.engine.getCvDestination(channel, CV_SOURCE_2),
        );
        const [first, second] = modChoiceAt(
          clampIndex(current < 0 ? 0 : current, delta, MOD_CHOICE_COUNT),
        );
        this.engine.setCvDestination(channel, CV_SOURCE_1, first);
        this.engine.setCvDestination(channel, CV_SOURCE_2, second);
        break;
      }
      case UiField.Offset: {
        if (channel < 0) break;
        const candidate = this.engine.getOffset(channel) + delta;
        this.engine.setOffset(channel, candidate < 0 ? 0 : candidate);
        break;
      }
      case UiField.SkipChance: {
        if (channel < 0) break;
        const current = this.engine.getSkipChance(channel);
        this.engine.setSkipChance(channel, clampIndex(current, delta, MAX_SKIP_CHANCE_SETTING + 1));
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
    if (pattern.readStep(this.step) !== true) return;
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
    // L'original ne demarre et n'arrete que l'horloge INTERNE : en source
    // externe ou MIDI, c'est la source qui commande le transport, et PLAY
    // reste inerte (Interactions.ino:372).
    if (this.source !== CLOCK_SOURCE_INTERNAL) return;
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
