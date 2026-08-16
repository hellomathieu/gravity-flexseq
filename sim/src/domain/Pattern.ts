/**
 * Pattern — modele de reference TypeScript.
 *
 * Port comportemental fidele de `flexseq::Pattern` (C++ / AVR), voir
 * gravity-flexseq/src/domain/Pattern.cpp et include/flexseq/Pattern.h.
 *
 * IMPORTANT : ce modele reproduit le COMPORTEMENT, pas la representation
 * memoire. Cote firmware, Pattern est packe sur 7 B (packedSteps[3] +
 * tripletStarts[3] + baseLength) ; cette contrainte `sizeof == 7` reste
 * verifiee cote C++ uniquement. Ici on privilegie la lisibilite.
 *
 * La cible de parite est fonctionnelle : les memes cas de test doivent
 * passer cote TS et cote C++.
 */
export class Pattern {
  static readonly MIN_PATTERN_LENGTH = 1;
  static readonly MAX_PATTERN_LENGTH = 24;
  static readonly DEFAULT_PATTERN_LENGTH = 16;
  static readonly DEFAULT_TOTAL_STEPS = 24;

  private readonly steps: boolean[];
  private readonly tripletStarts: boolean[];
  private baseLength: number;

  constructor() {
    this.steps = new Array<boolean>(Pattern.DEFAULT_TOTAL_STEPS).fill(false);
    this.tripletStarts = new Array<boolean>(Pattern.DEFAULT_TOTAL_STEPS).fill(false);
    this.baseLength = Pattern.DEFAULT_PATTERN_LENGTH;
  }

  private static isValidIndex(index: number): boolean {
    return Number.isInteger(index) && index >= 0 && index < Pattern.DEFAULT_TOTAL_STEPS;
  }

  /**
   * Retourne l'etat du step, ou `null` si l'index est hors bornes.
   * Mirroir de `bool readStep(index, bool& active)` : `null` correspond au
   * retour `false` du C++ (l'appelant n'obtient aucune valeur).
   */
  readStep(index: number): boolean | null {
    if (!Pattern.isValidIndex(index)) {
      return null;
    }
    return this.steps[index] ?? false;
  }

  /** Ecrit un step. Retourne `false` (sans mutation) si l'index est hors bornes. */
  writeStep(index: number, active: boolean): boolean {
    if (!Pattern.isValidIndex(index)) {
      return false;
    }
    this.steps[index] = active;
    return true;
  }

  getBaseLength(): number {
    return this.baseLength;
  }

  /** Definit la longueur de base (1..24). Retourne `false` sans mutation si invalide. */
  setBaseLength(length: number): boolean {
    if (
      !Number.isInteger(length) ||
      length < Pattern.MIN_PATTERN_LENGTH ||
      length > Pattern.MAX_PATTERN_LENGTH
    ) {
      return false;
    }
    this.baseLength = length;
    return true;
  }

  /** Efface tous les steps ET les triolets, en conservant la longueur. */
  clear(): void {
    this.steps.fill(false);
    this.clearTriplets();
  }

  /**
   * Ajoute un groupe ternaire demarrant a `startIndex` (occupe start, +1, +2).
   * Regles (identiques au C++) :
   *  - start doit tenir dans la grille : start <= DEFAULT_TOTAL_STEPS - 3 (soit <= 21) ;
   *  - le groupe doit tenir dans la longueur active : start + 3 <= baseLength ;
   *  - pas de chevauchement : aucun start existant T avec |start - T| <= 2
   *    (couvre aussi le doublon exact).
   */
  addTriplet(startIndex: number): boolean {
    if (!Number.isInteger(startIndex) || startIndex < 0) {
      return false;
    }
    if (startIndex > Pattern.DEFAULT_TOTAL_STEPS - 3) {
      return false;
    }
    if (startIndex + 3 > this.baseLength) {
      return false;
    }

    const first = Math.max(0, startIndex - 2);
    const last = Math.min(Pattern.DEFAULT_TOTAL_STEPS - 3, startIndex + 2);
    for (let existing = first; existing <= last; ++existing) {
      if (this.tripletStarts[existing]) {
        return false;
      }
    }

    this.tripletStarts[startIndex] = true;
    return true;
  }

  /** Retire le groupe ternaire demarrant a `startIndex`. Retourne `false` si absent/hors bornes. */
  removeTriplet(startIndex: number): boolean {
    if (!Pattern.isValidIndex(startIndex)) {
      return false;
    }
    if (!this.tripletStarts[startIndex]) {
      return false;
    }
    this.tripletStarts[startIndex] = false;
    return true;
  }

  /** Vrai si un groupe ternaire demarre exactement a `index`. */
  isTripletStart(index: number): boolean {
    if (!Pattern.isValidIndex(index)) {
      return false;
    }
    return this.tripletStarts[index] ?? false;
  }

  /** Vrai si `index` appartient a un groupe ternaire (start, +1 ou +2). */
  isTripletStep(index: number): boolean {
    if (!Pattern.isValidIndex(index)) {
      return false;
    }
    for (let start = 0; start <= Pattern.DEFAULT_TOTAL_STEPS - 3; ++start) {
      if (!this.tripletStarts[start]) {
        continue;
      }
      if (index >= start && index < start + 3) {
        return true;
      }
    }
    return false;
  }

  /** Efface uniquement les groupes ternaires (steps et longueur conserves). */
  clearTriplets(): void {
    this.tripletStarts.fill(false);
  }
}
