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
import { PatternAction } from "./PatternAction.js";
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

// PRD 5.0 amendement 1quater : la grande valeur reprend la PREMIERE position,
// et elle porte le NOM du template.
export const SEQ_FIELD_INDEX_PATTERN = 0;
export const SEQ_FIELD_INDEX_MODE = 1;
export const SEQ_FIELD_INDEX_EDIT_ENTRY = 2;
export const SEQ_FIELD_INDEX_CONFIG = 3;

// La sentinelle « aucun choix en cours » : l ecran montre alors ce que le canal
// joue. Elle evite un choix PAR CANAL, un changement d onglet la reposant.
export const NO_BROWSE = -1;

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
  private ask = false;
  private yes = false;
  private browse = NO_BROWSE;
  private pending: PatternAction = PatternAction.None;
  private pendingChannel = 0;

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

  // PRD 5.0 amendement 1quater. Le nom AFFICHE est un choix : SHIFT plus une
  // rotation le change, et la rotation dans le champ ouvert aussi. Il n atteint
  // selectedPattern qu au chargement.
  get displayedPattern(): number {
    if (this.browse !== NO_BROWSE) return this.browse;
    const channel = this.selectedChannel;
    return channel < 0 ? NO_BROWSE : this.engine.getSelectedPattern(channel);
  }

  // La question qui precede un chargement destructeur, et le mot choisi.
  get patternAskPending(): boolean {
    return this.ask;
  }

  get patternAnswerIsYes(): boolean {
    return this.yes;
  }

  // Le controleur POSE une demande, il ne l execute pas : ADR 0002 lui interdit
  // de connaitre le Storage. Le service la consomme, une seule fois.
  takePatternAction(): { action: PatternAction; channel: number } | null {
    if (this.pending === PatternAction.None) return null;
    const taken = { action: this.pending, channel: this.pendingChannel };
    this.pending = PatternAction.None;
    return taken;
  }

  // SHIFT plus une rotation, et la rotation dans le champ ouvert, deplacent le
  // meme choix. PRD 5.0 amendement 1quater : ce geste ne charge RIEN.
  private browsePattern(delta: number): void {
    const shown = this.displayedPattern;
    if (shown < 0) return;
    this.browse = clampIndex(shown, oneStep(delta), PATTERN_COUNT);
  }

  // Le chargement du nom choisi. selectedPattern ne bouge qu ICI : c est le seul
  // endroit ou le canal se met a jouer autre chose.
  private postPatternLoad(): void {
    const channel = this.selectedChannel;
    const wanted = this.displayedPattern;
    if (channel < 0 || wanted < 0) return;
    this.engine.setSelectedPattern(channel, wanted);
    this.browse = NO_BROWSE;
    this.pending = PatternAction.Load;
    this.pendingChannel = channel;
  }

  // Un appui court dans le champ ouvert. Rend true quand le champ doit RESTER
  // ouvert, ce qui n arrive que lorsque la question vient d etre posee.
  private pressInPatternField(): boolean {
    if (this.ask) {
      this.ask = false;
      if (this.yes) {
        this.postPatternLoad();
      } else {
        // NO rend son nom au template que le canal joue.
        this.browse = NO_BROWSE;
      }
      return false;
    }
    if (this.channelCopyHasChanged()) {
      // ⚠️ NO est arme le premier : un appui court de trop ne doit rien detruire.
      this.ask = true;
      this.yes = false;
      return true;
    }
    this.postPatternLoad();
    return false;
  }

  private channelCopyHasChanged(): boolean {
    const state = this.engine.modulatedPatterns();
    const channel = this.selectedChannel;
    if (state === null || channel < 0) return false;
    return state.isDirty(channel);
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
      // ⚠️ Le choix en cours appartient au canal qu on quitte.
      this.browse = NO_BROWSE;
      this.ask = false;
      return;
    }
    if (event === UiEvent.ShiftRotate) {
      // Le geste de l original : il choisit la valeur du champ principal. Sur un
      // canal en SEQ ce champ est le PATTERN, et ce choix ne charge RIEN.
      if (this.mainField === UiField.Pattern) {
        this.browsePattern(delta);
      } else {
        this.adjustFieldValue(this.mainField, delta);
      }
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
          if (this.ask) {
            // Deux mots seulement : une rotation passe de l un a l autre.
            this.yes = !this.yes;
          } else {
            this.browsePattern(delta);
          }
        } else if (this.open) {
          this.adjustField(delta);
        } else {
          this.fieldCursor = wrapIndex(this.fieldCursor, oneStep(delta), this.fieldCount);
        }
        break;
      case UiEvent.ShiftRotate:
        if (this.field === UiField.Pattern) {
          this.browsePattern(delta);
        } else {
          this.adjustField(delta);
        }
        break;
      case UiEvent.Press:
        if (this.open) {
          if (this.field === UiField.Pattern && this.pressInPatternField()) break;
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
          this.ask = false;
          this.yes = false;
        }
        break;
      case UiEvent.LongPress:
        if (this.open) {
          this.ask = false;
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
        // ⚠️ LE RECALAGE REDEVIENT NECESSAIRE sous PRD 5.0 amendement 1quater :
        // la position 0 porte MODE hors SEQ et la GRANDE VALEUR en SEQ. Sans lui
        // le curseur resterait immobile pendant que le champ sous lui
        // changerait, et un appui court ouvrirait le champ du pattern au lieu
        // de MODE.
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
