/**
 * PatternStore — modele de reference TypeScript.
 *
 * Port comportemental de `flexseq::PatternStore` (voir
 * gravity-flexseq/src/domain/PatternStore.cpp et include/flexseq/PatternStore.h).
 *
 * Contrat : 6 channels x 16 patterns, acces borne. `getPattern` retourne la
 * MEME instance a chaque appel (semantique reference, miroir du `Pattern*`
 * cote C++) : muter le Pattern retourne modifie bien le store.
 *
 * Rappel packing (C++/AVR uniquement) : sizeof(PatternStore) == 672 B
 * (6 x 16 x 7). Cette contrainte n'est PAS modelisee ici.
 */
import { Pattern } from "./Pattern.js";

export class PatternStore {
  static readonly CHANNEL_COUNT = 6;
  static readonly PATTERN_PER_CHANNEL = 16;

  private readonly patterns: Pattern[][];

  constructor() {
    this.patterns = Array.from(
      { length: PatternStore.CHANNEL_COUNT },
      () =>
        Array.from(
          { length: PatternStore.PATTERN_PER_CHANNEL },
          () => new Pattern(),
        ),
    );
  }

  private static isValidChannel(channel: number): boolean {
    return (
      Number.isInteger(channel) &&
      channel >= 0 &&
      channel < PatternStore.CHANNEL_COUNT
    );
  }

  private static isValidPatternIndex(patternIndex: number): boolean {
    return (
      Number.isInteger(patternIndex) &&
      patternIndex >= 0 &&
      patternIndex < PatternStore.PATTERN_PER_CHANNEL
    );
  }

  /**
   * Retourne le Pattern (channel, patternIndex), ou `null` hors bornes.
   * Miroir de `Pattern* getPattern(...)` : `null` == retour `nullptr`.
   */
  getPattern(channel: number, patternIndex: number): Pattern | null {
    if (
      !PatternStore.isValidChannel(channel) ||
      !PatternStore.isValidPatternIndex(patternIndex)
    ) {
      return null;
    }
    return this.patterns[channel]![patternIndex]!;
  }
}
