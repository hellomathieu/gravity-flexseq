/**
 * SequencerEngine — coeur du modele temporel FlexSeq (modele de reference TS).
 *
 * Decisions validees (Phase 2) :
 *  - masterPhase = compteur MONOTONE de ticks a 96 PPQN (resolution interne
 *    libGravity ; SUBDIV historiques exacts). 1/16 non fige : chaque channel
 *    divise par son propre `ticksPerStep`.
 *  - Type uint32 (deborde apres ~221 j a 140 BPM -> pas de normalisation).
 *  - masterPhase independant de LENGTH / du pattern. Reset global -> 0 ;
 *    stop() conserve la phase.
 *
 * PROJECTION -> effectiveStep : PHASE LOCALE LISSEE (decision 2026-08-16).
 * Chaque channel maintient une position locale (`localStep`) + un accumulateur
 * de ticks intra-step. `advance()` fait franchir les frontieres de step et
 * incremente localStep en bouclant sur effectiveLength. Un changement de
 * LENGTH ne fait PAS sauter le playhead : localStep est conserve, et n'est
 * replie (`% newLength`) que s'il tombe hors de la nouvelle longueur. C'est le
 * role de la `phase locale` / `phaseOffset` prevue par le PRD (par opposition
 * a la projection `masterPhase % length` ancree a l'origine globale, qui
 * sautait a chaque edition de LENGTH).
 *
 * Consequence assumee : effectiveStep depend de l'historique des changements
 * de LENGTH (path-dependent). Un reset global realigne tous les channels a 0.
 *
 * HORS PERIMETRE (differe) : couche Transport (clock->progression), grille
 * METER/SUBDIV/MEASURES, modulation CV. `ticksPerStep` reste provisoirement a
 * l'equivalent 1/16.
 */

import { subdivToTicks, DEFAULT_SUBDIV } from "./subdiv.js";
import type { PatternBank } from "./PatternBank.js";
import { ratchetSpan, ratchetTriggers, RATCHET_NONE } from "./Pattern.js";

/** Separation de mesure : une barre tous les N steps. GRAPHIQUE uniquement. */
export const BAR_NONE = 0;
export const DEFAULT_BAR_LENGTH = 4;
/** Seules les valeurs qui divisent une ligne de 12 sans reste. */
export const BAR_LENGTHS: readonly number[] = [BAR_NONE, 2, 3, 4, 6];

/** Ticks par noire de l'horloge interne libGravity (resolution de reference). */
export const PPQN = 96;

/** 1/16 de note a 96 PPQN = 96 / 4. Valeur provisoire de ticksPerStep. */
export const TICKS_PER_SIXTEENTH = PPQN / 4; // 24

/** Bornes de longueur, alignees sur le domaine Pattern. */
export const MIN_LENGTH = 1;
export const MAX_LENGTH = 24;
export const DEFAULT_LENGTH = 16;

/** masterPhase est un uint32 : il boucle a 2^32 ticks. */
const PHASE_MODULO = 0x1_0000_0000;

export const CHANNEL_COUNT = 6;

/** Nombre de patterns partages selectionnables par channel (voir PatternBank). */
export const PATTERN_COUNT = 16;

/** Modes de channel du firmware d'origine (PRD 4.2). */
export enum ChannelMode {
  CLOCK = 0,
  RANDOM = 1,
  SEQ = 2,
}

export const CHANNEL_MODE_COUNT = 3;

/** Defaut d'usine de l'original : les six channels sont en CLOCK. */
export const DEFAULT_CHANNEL_MODE = ChannelMode.CLOCK;

export const MAX_OFFSET = 255;

/** Chance de SAUT d'un step en dixiemes : 0 jamais, 10 toujours. */
export const MAX_SKIP_CHANCE = 10;

interface ChannelState {
  selectedPattern: number; // index 0..15 dans la banque partagee
  effectiveLength: number;
  subdiv: number; // valeur SUBDIV (libGravity) ; determine ticksPerStep
  ticksPerStep: number;
  barLength: number; // separation de mesure (graphique), en steps
  mode: ChannelMode;
  offset: number;
  skipChance: number;
  /** Timing du step COURANT, recalcule a chaque frontiere (pas de division en boucle). */
  stepTicks: number; // ticksPerStep x span
  slotTicks: number; // stepTicks / triggers
  triggers: number; // declenchements dans ce step
  subOnset: number; // sous-declenchements deja emis
  localStep: number; // position locale, dans [0, effectiveLength)
  acc: number; // ticks accumules dans le step courant, dans [0, ticksPerStep)
  stepped: boolean; // a franchi une frontiere de step lors du dernier advance()
}

export class SequencerEngine {
  private phase = 0; // masterPhase, en ticks (uint32)
  private running = false;
  private bank: PatternBank | null = null;
  private readonly onsets: number[];
  private readonly channels: ChannelState[];

  constructor(channelCount: number = CHANNEL_COUNT) {
    this.channels = Array.from({ length: channelCount }, () => ({
      selectedPattern: 0,
      effectiveLength: DEFAULT_LENGTH,
      subdiv: DEFAULT_SUBDIV,
      ticksPerStep: subdivToTicks(DEFAULT_SUBDIV),
      barLength: DEFAULT_BAR_LENGTH,
      mode: DEFAULT_CHANNEL_MODE,
      offset: 0,
      skipChance: 0,
      stepTicks: subdivToTicks(DEFAULT_SUBDIV),
      slotTicks: subdivToTicks(DEFAULT_SUBDIV),
      triggers: 1,
      subOnset: 0,
      localStep: 0,
      acc: 0,
      stepped: false,
    }));
    this.onsets = new Array<number>(this.channels.length).fill(0);
    for (let ch = 0; ch < this.channels.length; ++ch) this.refreshStepTiming(ch);
  }

  /**
   * Recalcule la duree et le nombre de declenchements du step courant a partir
   * de son code de ratchet. Appele a chaque frontiere de step et sur tout
   * changement de cadence : le chemin chaud ne fait donc aucune division.
   */
  private refreshStepTiming(ch: number, resetSubOnset = true): void {
    const c = this.channels[ch];
    if (!c) return;

    let code = RATCHET_NONE;
    if (this.bank && c.mode === ChannelMode.SEQ) {
      const pattern = this.bank.getPattern(c.selectedPattern);
      if (pattern) code = pattern.getRatchet(c.localStep);
    }

    const span = ratchetSpan(code);
    let triggers = ratchetTriggers(code);
    c.stepTicks = c.ticksPerStep * span;

    // Un sous-slot doit tomber sur un tick entier ; sinon le ratchet est ignore
    // pour cette combinaison (repli documente, aucune derive).
    if (triggers > 1 && c.stepTicks % triggers !== 0) triggers = 1;

    c.triggers = triggers;
    c.slotTicks = c.stepTicks / triggers;
    if (resetSubOnset) {
      c.subOnset = 0;
    } else if (c.subOnset >= c.triggers) {
      // Conserve les declenchements deja emis dans ce step : une edition ne
      // doit pas les refaire jouer.
      c.subOnset = c.triggers - 1;
    }
  }

  /**
   * Relit le ratchet du step courant apres une modification du CONTENU du
   * pattern. Sans cela l'edition ne serait prise en compte qu'au passage
   * suivant sur ce step.
   */
  refreshTiming(channel?: number): void {
    if (channel === undefined) {
      for (let ch = 0; ch < this.channels.length; ++ch) this.refreshStepTiming(ch, false);
      return;
    }
    if (this.channel(channel)) this.refreshStepTiming(channel, false);
  }

  /**
   * Banque partagee optionnelle. Une fois fournie, un step appartenant a un
   * groupe ternaire LOCAL du pattern selectionne dure `ticksPerStep / 3` : les
   * trois steps tiennent dans la duree d'UN step (ils passent plus vite). Sans
   * banque, la duree de step reste uniforme.
   */
  setPatternBank(bank: PatternBank | null): void {
    this.bank = bank;
    for (let ch = 0; ch < this.channels.length; ++ch) this.refreshStepTiming(ch);
  }

  /**
   * Duree en ticks du step COURANT du channel (ticksPerStep, ou son tiers dans
   * un groupe ternaire). 0 si le channel est invalide.
   */
  currentStepTicks(channel: number): number {
    return this.channel(channel)?.stepTicks ?? 0;
  }

  /** Nombre de declenchements emis par le step courant. */
  currentStepTriggers(channel: number): number {
    return this.channel(channel)?.triggers ?? 0;
  }

  /** Declenchements emis lors du DERNIER advance() (1 par step, N si ratchet). */
  onsetCount(channel: number): number {
    return Number.isInteger(channel) ? (this.onsets[channel] ?? 0) : 0;
  }

  /** Separation de mesure du channel (en steps), ou -1 si invalide. */
  getBarLength(channel: number): number {
    return this.channel(channel)?.barLength ?? -1;
  }

  /** Definit la separation de mesure (graphique). Rejette hors de BAR_LENGTHS. */
  setBarLength(channel: number, steps: number): boolean {
    const c = this.channel(channel);
    if (!c) return false;
    if (!BAR_LENGTHS.includes(steps)) return false;
    c.barLength = steps;
    return true;
  }

  // --- Transport ---------------------------------------------------------

  get masterPhase(): number {
    return this.phase;
  }

  get isRunning(): boolean {
    return this.running;
  }

  /** Demarre la progression (running = true). Ne touche pas a la phase. */
  start(): void {
    this.running = true;
  }

  /** Arrete la progression sans reset : phase et positions locales conservees. */
  stop(): void {
    this.running = false;
  }

  /** Reset global : masterPhase a 0 et realignement de tous les channels. */
  reset(): void {
    this.phase = 0;
    for (let ch = 0; ch < this.channels.length; ++ch) {
      const c = this.channels[ch]!;
      c.localStep = 0;
      c.acc = 0;
      this.refreshStepTiming(ch);
    }
  }

  /**
   * Avance la phase maitre de `ticks` (defaut 1) et fait progresser la phase
   * locale de chaque channel. No-op si le moteur est arrete.
   */
  advance(ticks = 1): void {
    for (const c of this.channels) c.stepped = false; // report only THIS advance
    this.onsets.fill(0);
    if (!this.running) return;
    if (!Number.isInteger(ticks) || ticks < 0) return;

    this.phase = (this.phase + ticks) % PHASE_MODULO;

    for (let ch = 0; ch < this.channels.length; ++ch) {
      const c = this.channels[ch]!;
      c.acc += ticks;

      for (;;) {
        if (c.mode === ChannelMode.CLOCK) {
          if (c.subOnset === 0 && c.offset > 0 && c.acc >= c.offset) {
            c.subOnset = 1;
            this.onsets[ch] = (this.onsets[ch] ?? 0) + 1;
            continue;
          }
        } else if (c.subOnset + 1 < c.triggers && c.acc >= c.slotTicks * (c.subOnset + 1)) {
          c.subOnset += 1;
          this.onsets[ch] = (this.onsets[ch] ?? 0) + 1;
          continue;
        }
        if (c.stepTicks > 0 && c.acc >= c.stepTicks) {
          c.acc -= c.stepTicks;
          c.localStep = (c.localStep + 1) % c.effectiveLength;
          c.stepped = true;
          this.refreshStepTiming(ch); // nouveau step -> nouvelle duree
          if (c.mode !== ChannelMode.CLOCK || c.offset === 0) {
            this.onsets[ch] = (this.onsets[ch] ?? 0) + 1;
          }
          continue;
        }
        break;
      }
    }
  }

  // --- Etat par channel --------------------------------------------------

  channelCount(): number {
    return this.channels.length;
  }

  private channel(index: number): ChannelState | undefined {
    return Number.isInteger(index) ? this.channels[index] : undefined;
  }

  /** Index du pattern partage joue par ce channel (0..15), ou -1 si invalide. */
  getSelectedPattern(channel: number): number {
    return this.channel(channel)?.selectedPattern ?? -1;
  }

  /** Choisit le pattern partage joue par ce channel. Rejette si invalide. */
  setSelectedPattern(channel: number, index: number): boolean {
    const c = this.channel(channel);
    if (!c) return false;
    if (!Number.isInteger(index) || index < 0 || index >= PATTERN_COUNT) return false;
    c.selectedPattern = index;
    this.refreshStepTiming(channel);
    return true;
  }

  getEffectiveLength(channel: number): number {
    return this.channel(channel)?.effectiveLength ?? 0;
  }

  /**
   * Definit effectiveLength (1..24). Rejette sans mutation si invalide.
   * Phase locale LISSEE : localStep est conserve, et n'est replie sur la
   * nouvelle longueur que s'il tombe hors bornes (pas de saut sinon).
   */
  setEffectiveLength(channel: number, length: number): boolean {
    const c = this.channel(channel);
    if (!c) return false;
    if (!Number.isInteger(length) || length < MIN_LENGTH || length > MAX_LENGTH) {
      return false;
    }
    c.effectiveLength = length;
    if (c.localStep >= length) {
      c.localStep %= length;
      this.refreshStepTiming(channel);
    }
    return true;
  }

  getTicksPerStep(channel: number): number {
    return this.channel(channel)?.ticksPerStep ?? 0;
  }

  /** Definit ticksPerStep (> 0). Rejette sans mutation si invalide. */
  setTicksPerStep(channel: number, ticks: number): boolean {
    const c = this.channel(channel);
    if (!c) return false;
    if (!Number.isInteger(ticks) || ticks < 1) return false;
    c.ticksPerStep = ticks;
    this.refreshStepTiming(channel);
    this.clampOffset(c);
    if (c.acc >= c.stepTicks) c.acc %= c.stepTicks;
    return true;
  }

  // --- Modes, offset, chance de saut (PRD 4.2) ---------------------------

  private clampOffset(c: ChannelState): void {
    if (c.offset >= c.ticksPerStep) c.offset = c.ticksPerStep - 1;
    if (c.offset > MAX_OFFSET) c.offset = MAX_OFFSET;
  }

  getChannelMode(channel: number): ChannelMode {
    return this.channel(channel)?.mode ?? DEFAULT_CHANNEL_MODE;
  }

  setChannelMode(channel: number, mode: ChannelMode): boolean {
    const c = this.channel(channel);
    if (!c) return false;
    if (!Number.isInteger(mode) || mode < 0 || mode >= CHANNEL_MODE_COUNT) return false;
    if (c.mode === mode) return true;
    c.mode = mode;
    this.refreshStepTiming(channel);
    if (c.acc >= c.stepTicks) c.acc %= c.stepTicks;
    return true;
  }

  getOffset(channel: number): number {
    return this.channel(channel)?.offset ?? 0;
  }

  setOffset(channel: number, offset: number): boolean {
    const c = this.channel(channel);
    if (!c) return false;
    if (!Number.isInteger(offset) || offset < 0) return false;
    c.offset = offset;
    this.clampOffset(c);
    return true;
  }

  getSkipChance(channel: number): number {
    return this.channel(channel)?.skipChance ?? 0;
  }

  setSkipChance(channel: number, tenths: number): boolean {
    const c = this.channel(channel);
    if (!c) return false;
    if (!Number.isInteger(tenths) || tenths < 0 || tenths > MAX_SKIP_CHANCE) return false;
    c.skipChance = tenths;
    return true;
  }

  // --- Derivation --------------------------------------------------------

  /** Valeur SUBDIV du channel (libGravity), ou 0 si invalide. */
  getSubdiv(channel: number): number {
    return this.channel(channel)?.subdiv ?? 0;
  }

  /**
   * Definit la SUBDIV du channel : met a jour subdiv ET ticksPerStep via la
   * table officielle. Rejette (sans mutation) une SUBDIV invalide.
   */
  setSubdiv(channel: number, subdiv: number): boolean {
    const c = this.channel(channel);
    if (!c) return false;
    const ticks = subdivToTicks(subdiv);
    if (ticks < 1) return false;
    c.subdiv = subdiv;
    c.ticksPerStep = ticks;
    this.refreshStepTiming(channel);
    this.clampOffset(c);
    if (c.acc >= c.stepTicks) c.acc %= c.stepTicks;
    return true;
  }

  /** Position logique du channel (localStep), dans [0, effectiveLength). -1 si invalide. */
  effectiveStep(channel: number): number {
    return this.channel(channel)?.localStep ?? -1;
  }

  /**
   * Vrai si le channel a franchi au moins une frontiere de step lors du DERNIER
   * advance() (onset). Sert a emettre les triggers. Remis a zero au debut de
   * chaque advance().
   */
  hasStepped(channel: number): boolean {
    return this.channel(channel)?.stepped ?? false;
  }
}
