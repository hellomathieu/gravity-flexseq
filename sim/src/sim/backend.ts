/**
 * Seam backend du Gravity Simulator.
 *
 * L'UI ne parle qu'a cette interface : elle est ainsi decouplee de la source
 * des donnees. Aujourd'hui un seul backend (`TsReferenceBackend`, modele TS de
 * reference) ; demain un `Avr8jsBackend` lira l'etat du firmware C++ reel
 * execute par avr8js, en implementant la meme interface.
 *
 * Les vues renvoyees sont des valeurs (CellView[]), pas des objets domaine :
 * un backend AVR n'expose pas de `Pattern` TypeScript.
 */
import { PatternStore } from "../domain/PatternStore.js";
import { viewPattern, type CellView } from "./PatternView.js";

export interface SimBackend {
  readonly channelCount: number;
  readonly patternsPerChannel: number;

  getLength(channel: number, patternIndex: number): number;
  view(channel: number, patternIndex: number): CellView[];

  toggleStep(channel: number, patternIndex: number, index: number): void;
  setLength(channel: number, patternIndex: number, length: number): boolean;
  /** Ajoute le triolet s'il est absent (et valide), le retire s'il est present. */
  toggleTriplet(channel: number, patternIndex: number, startIndex: number): boolean;
}

/** Backend de reference : lit/ecrit directement le modele TypeScript. */
export class TsReferenceBackend implements SimBackend {
  readonly channelCount = PatternStore.CHANNEL_COUNT;
  readonly patternsPerChannel = PatternStore.PATTERN_PER_CHANNEL;

  private readonly store: PatternStore;

  constructor(store: PatternStore = new PatternStore()) {
    this.store = store;
  }

  getLength(channel: number, patternIndex: number): number {
    return this.store.getPattern(channel, patternIndex)?.getBaseLength() ?? 0;
  }

  view(channel: number, patternIndex: number): CellView[] {
    const pattern = this.store.getPattern(channel, patternIndex);
    return pattern ? viewPattern(pattern) : [];
  }

  toggleStep(channel: number, patternIndex: number, index: number): void {
    const pattern = this.store.getPattern(channel, patternIndex);
    if (!pattern) return;
    const current = pattern.readStep(index);
    if (current === null) return;
    pattern.writeStep(index, !current);
  }

  setLength(channel: number, patternIndex: number, length: number): boolean {
    return this.store.getPattern(channel, patternIndex)?.setBaseLength(length) ?? false;
  }

  toggleTriplet(channel: number, patternIndex: number, startIndex: number): boolean {
    const pattern = this.store.getPattern(channel, patternIndex);
    if (!pattern) return false;
    return pattern.isTripletStart(startIndex)
      ? pattern.removeTriplet(startIndex)
      : pattern.addTriplet(startIndex);
  }
}
