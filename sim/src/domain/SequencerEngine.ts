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

interface ChannelState {
  selectedPattern: number; // index 0..15 dans la banque partagee
  effectiveLength: number;
  subdiv: number; // valeur SUBDIV (libGravity) ; determine ticksPerStep
  ticksPerStep: number;
  localStep: number; // position locale, dans [0, effectiveLength)
  acc: number; // ticks accumules dans le step courant, dans [0, ticksPerStep)
  stepped: boolean; // a franchi une frontiere de step lors du dernier advance()
}

export class SequencerEngine {
  private phase = 0; // masterPhase, en ticks (uint32)
  private running = false;
  private readonly channels: ChannelState[];

  constructor(channelCount: number = CHANNEL_COUNT) {
    this.channels = Array.from({ length: channelCount }, () => ({
      selectedPattern: 0,
      effectiveLength: DEFAULT_LENGTH,
      subdiv: DEFAULT_SUBDIV,
      ticksPerStep: TICKS_PER_SIXTEENTH,
      localStep: 0,
      acc: 0,
      stepped: false,
    }));
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
    for (const c of this.channels) {
      c.localStep = 0;
      c.acc = 0;
    }
  }

  /**
   * Avance la phase maitre de `ticks` (defaut 1) et fait progresser la phase
   * locale de chaque channel. No-op si le moteur est arrete.
   */
  advance(ticks = 1): void {
    for (const c of this.channels) c.stepped = false; // report only THIS advance
    if (!this.running) return;
    if (!Number.isInteger(ticks) || ticks < 0) return;

    this.phase = (this.phase + ticks) % PHASE_MODULO;

    for (const c of this.channels) {
      c.acc += ticks;
      while (c.acc >= c.ticksPerStep) {
        c.acc -= c.ticksPerStep;
        c.localStep = (c.localStep + 1) % c.effectiveLength;
        c.stepped = true;
      }
    }
  }

  // --- Etat par channel --------------------------------------------------

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
    if (c.localStep >= length) c.localStep %= length;
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
    if (c.acc >= ticks) c.acc %= ticks;
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
    if (c.acc >= ticks) c.acc %= ticks;
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
