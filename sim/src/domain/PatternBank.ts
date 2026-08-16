/**
 * PatternBank — banque de 16 patterns PARTAGES (A1..A8, B1..B8).
 *
 * Remplace l'ancien PatternStore (6x16 prive par channel). Conforme au
 * firmware Sitka original : les 16 patterns (seqA1..seqB8) sont partages, et
 * chaque channel choisit lequel jouer via son `selectedPattern` (voir
 * SequencerEngine). Editer un pattern se repercute sur TOUS les channels qui
 * le referencent.
 *
 * NB memoire (C++/AVR, hors modele TS) : 16 x 6 B ≈ 96 B, contre 672 B pour
 * l'ancien 6x16 — soit ~560 B de RAM liberes sur l'ATmega328P.
 */
import { Pattern } from "./Pattern.js";

export const PATTERN_COUNT = 16;

export class PatternBank {
  private readonly patterns: Pattern[];

  constructor() {
    this.patterns = Array.from({ length: PATTERN_COUNT }, () => new Pattern());
  }

  private static isValidIndex(index: number): boolean {
    return Number.isInteger(index) && index >= 0 && index < PATTERN_COUNT;
  }

  /** Retourne le Pattern partage `index` (0..15), ou `null` hors bornes. */
  getPattern(index: number): Pattern | null {
    if (!PatternBank.isValidIndex(index)) return null;
    return this.patterns[index]!;
  }
}
