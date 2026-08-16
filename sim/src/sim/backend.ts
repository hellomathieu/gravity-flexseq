/**
 * Seam backend du Gravity Simulator.
 *
 * L'UI ne parle qu'a cette interface : elle est ainsi decouplee de la source
 * des donnees. Aujourd'hui `TsReferenceBackend` (modele TS) ; demain un
 * `Avr8jsBackend` lira l'etat du firmware C++ execute par avr8js.
 *
 * Modele : une BANQUE de 16 patterns partages + un SequencerEngine qui detient
 * l'etat d'execution par channel (pattern selectionne, longueur, phase locale).
 * Editer le pattern d'un channel modifie le pattern PARTAGE : les autres
 * channels qui le referencent voient le meme contenu.
 */
import { PatternBank, PATTERN_COUNT } from "../domain/PatternBank.js";
import { SequencerEngine, CHANNEL_COUNT } from "../domain/SequencerEngine.js";
import { viewPattern, type CellView } from "./PatternView.js";

export interface SimBackend {
  readonly channelCount: number;
  readonly patternCount: number;

  // --- selection & edition (par channel, sur le pattern partage) ---
  getSelectedPattern(channel: number): number;
  setSelectedPattern(channel: number, index: number): void;
  getLength(channel: number): number;
  setLength(channel: number, length: number): boolean;
  view(channel: number): CellView[];
  toggleStep(channel: number, index: number): void;
  /** Ajoute le triolet s'il est absent (et valide), le retire s'il est present. */
  toggleTriplet(channel: number, startIndex: number): boolean;

  // --- transport & modele temporel ---
  play(): void;
  pause(): void;
  resetPhase(): void;
  isPlaying(): boolean;
  advanceTicks(ticks: number): void;
  masterPhase(): number;
  effectiveStep(channel: number): number;
}

/** Backend de reference : banque partagee + moteur, en TypeScript. */
export class TsReferenceBackend implements SimBackend {
  readonly channelCount = CHANNEL_COUNT;
  readonly patternCount = PATTERN_COUNT;

  private readonly bank = new PatternBank();
  private readonly engine = new SequencerEngine();

  private patternOf(channel: number) {
    return this.bank.getPattern(this.engine.getSelectedPattern(channel));
  }

  // --- selection & edition ----------------------------------------------

  getSelectedPattern(channel: number): number {
    return this.engine.getSelectedPattern(channel);
  }

  setSelectedPattern(channel: number, index: number): void {
    this.engine.setSelectedPattern(channel, index);
  }

  getLength(channel: number): number {
    return this.engine.getEffectiveLength(channel);
  }

  setLength(channel: number, length: number): boolean {
    return this.engine.setEffectiveLength(channel, length);
  }

  view(channel: number): CellView[] {
    const pattern = this.patternOf(channel);
    return pattern ? viewPattern(pattern, this.engine.getEffectiveLength(channel)) : [];
  }

  toggleStep(channel: number, index: number): void {
    const pattern = this.patternOf(channel);
    if (!pattern) return;
    const current = pattern.readStep(index);
    if (current === null) return;
    pattern.writeStep(index, !current);
  }

  toggleTriplet(channel: number, startIndex: number): boolean {
    const pattern = this.patternOf(channel);
    if (!pattern) return false;
    return pattern.isTripletStart(startIndex)
      ? pattern.removeTriplet(startIndex)
      : pattern.addTriplet(startIndex);
  }

  // --- transport & modele temporel --------------------------------------

  play(): void {
    this.engine.start();
  }

  pause(): void {
    this.engine.stop();
  }

  resetPhase(): void {
    this.engine.reset();
  }

  isPlaying(): boolean {
    return this.engine.isRunning;
  }

  advanceTicks(ticks: number): void {
    this.engine.advance(ticks);
  }

  masterPhase(): number {
    return this.engine.masterPhase;
  }

  effectiveStep(channel: number): number {
    return this.engine.effectiveStep(channel);
  }
}
