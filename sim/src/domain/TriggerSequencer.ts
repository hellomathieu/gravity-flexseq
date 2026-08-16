/**
 * TriggerSequencer — couche de decision pure (miroir de flexseq/TriggerSequencer.h).
 *
 * Donne, pour chaque channel, s'il doit emettre un trigger MAINTENANT : il vient
 * de franchir l'onset d'un step ACTIF de son pattern selectionne. Appeler
 * `triggered()` juste apres `SequencerEngine.advance()` (moment ou `hasStepped()`
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

  triggered(channel: number): boolean {
    if (!this.engine.hasStepped(channel)) return false;

    const patternIndex = this.engine.getSelectedPattern(channel);
    if (patternIndex < 0) return false;

    const pattern = this.bank.getPattern(patternIndex);
    if (!pattern) return false;

    const step = this.engine.effectiveStep(channel);
    if (step < 0) return false;

    return pattern.readStep(step) === true;
  }
}
