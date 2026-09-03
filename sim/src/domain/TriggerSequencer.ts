/**
 * TriggerSequencer — couche de decision pure (miroir de flexseq/TriggerSequencer.h).
 *
 * Le comportement depend du mode du channel (PRD 4.2). Appeler `update()` juste
 * apres `SequencerEngine.advance()` : c'est le seul moment ou `onsetCount()` est
 * valide, et c'est la que le tirage de RANDOM est consomme, une fois par step.
 */
import { ChannelMode, type SequencerEngine } from "./SequencerEngine.js";
import { Prng } from "./Prng.js";

export const SKIP_DRAW_BOUND = 10;

/**
 * Un pas porte au plus six onsets : RATCHET_6 donne cinq sous-declenchements
 * plus celui du pas. Une dette au-dela veut dire que la boucle a plus d'un pas
 * entier de retard, et les onsets anciens ont perdu leur sens musical.
 */
export const MAX_OWED = 6;

export class TriggerSequencer {
  private readonly engine: SequencerEngine;
  private readonly prng = new Prng();
  private counts: number[] = [];
  private owed: number[] = [];

  constructor(engine: SequencerEngine) {
    this.engine = engine;
  }

  seed(value: number): void {
    this.prng.seed(value);
  }

  update(): void {
    const next: number[] = [];
    for (let ch = 0; ch < this.engine.channelCount(); ++ch) {
      const decided = this.decide(ch);
      next[ch] = decided;
      const total = (this.owed[ch] ?? 0) + decided;
      this.owed[ch] = total > MAX_OWED ? MAX_OWED : total;
    }
    this.counts = next;
  }

  triggerCount(channel: number): number {
    return this.counts[channel] ?? 0;
  }

  triggered(channel: number): boolean {
    return this.triggerCount(channel) > 0;
  }

  /**
   * Ce qui reste du a la sortie. Une sortie ne se rearme qu'une fois par
   * impulsion, donc un drainage qui porte plusieurs onsets laisse une dette.
   */
  owedTriggers(channel: number): number {
    if (channel < 0 || channel >= this.engine.channelCount()) return 0;
    return this.owed[channel] ?? 0;
  }

  takeTrigger(channel: number): boolean {
    if (channel < 0 || channel >= this.engine.channelCount()) return false;
    const owed = this.owed[channel] ?? 0;
    if (owed === 0) return false;
    this.owed[channel] = owed - 1;
    return true;
  }

  private decide(channel: number): number {
    const onsets = this.engine.onsetCount(channel);
    if (!onsets) return 0;
    switch (this.engine.getChannelMode(channel)) {
      case ChannelMode.CLOCK:
        return onsets;
      case ChannelMode.RANDOM:
        return this.keptByChance(channel) ? onsets : 0;
      default:
        return this.activeStep(channel) ? onsets : 0;
    }
  }

  private keptByChance(channel: number): boolean {
    const draw = this.prng.below(SKIP_DRAW_BOUND) + 1;
    return draw > this.engine.getSkipChance(channel);
  }

  private activeStep(channel: number): boolean {
    const pattern = this.engine.patternForChannel(channel);
    if (!pattern) return false;

    const step = this.engine.currentReadStep(channel);
    if (step < 0) return false;

    return pattern.readStep(step) === true;
  }
}
