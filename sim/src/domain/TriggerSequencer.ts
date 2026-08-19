/**
 * TriggerSequencer — couche de decision pure (miroir de flexseq/TriggerSequencer.h).
 *
 * Donne, pour chaque channel, s'il doit emettre un trigger MAINTENANT : il vient
 * de franchir l'onset d'un step ACTIF de son pattern selectionne. Appeler
 * `triggered()` juste apres `SequencerEngine.advance()` (moment ou `onsetCount()`
 * est valide). Aucune dependance materielle : cote firmware C++ un `true` devient
 * une impulsion DigitalOutput ; cote simulateur, un flash visuel.
 */
import type { PatternBank } from "./PatternBank.js";
import type { SequencerEngine } from "./SequencerEngine.js";

export class TriggerSequencer {
  private readonly bank: PatternBank;
  private readonly engine: SequencerEngine;

  constructor(bank: PatternBank, engine: SequencerEngine) {
    this.bank = bank;
    this.engine = engine;
  }

  /** Nombre d'impulsions dues pour le dernier advance() (ratchets inclus). */
  triggerCount(channel: number): number {
    const onsets = this.engine.onsetCount(channel);
    if (!onsets) return 0;
    return this.activeStep(channel) ? onsets : 0;
  }

  triggered(channel: number): boolean {
    return this.triggerCount(channel) > 0;
  }

  private activeStep(channel: number): boolean {
    const patternIndex = this.engine.getSelectedPattern(channel);
    if (patternIndex < 0) return false;

    const pattern = this.bank.getPattern(patternIndex);
    if (!pattern) return false;

    const step = this.engine.effectiveStep(channel);
    if (step < 0) return false;

    return pattern.readStep(step) === true;
  }
}
