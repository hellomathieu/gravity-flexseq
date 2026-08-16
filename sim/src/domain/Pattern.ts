/**
 * Pattern — CONTENU d'un pattern (modele de reference TypeScript).
 *
 * Depuis la ré-architecture "banque partagee" (2026-08-16), un Pattern ne
 * contient QUE son contenu musical — 24 steps binaires + groupes ternaires
 * locaux. Il est PARTAGE : plusieurs channels peuvent referencer le meme
 * Pattern (via leur `selectedPattern`), comme le firmware Sitka original
 * (seqA1..seqB8 partages, channel.seqPattern selecteur).
 *
 * La LONGUEUR n'est PLUS une propriete du Pattern : elle est par channel
 * (etat d'execution, voir SequencerEngine). Par consequent la validite d'un
 * triolet ne depend plus de la longueur — un groupe ternaire est valide sur la
 * grille de 24 (depart <= 21, sans chevauchement), conformement au PRD
 * (« triolets independants de LENGTH »).
 *
 * NB memoire (C++/AVR, hors modele TS) : le Pattern packe visera desormais
 * 6 octets (packedSteps[3] + tripletStarts[3], sans baseLength).
 */
export class Pattern {
  static readonly DEFAULT_TOTAL_STEPS = 24;

  private readonly steps: boolean[];
  private readonly tripletStarts: boolean[];

  constructor() {
    this.steps = new Array<boolean>(Pattern.DEFAULT_TOTAL_STEPS).fill(false);
    this.tripletStarts = new Array<boolean>(Pattern.DEFAULT_TOTAL_STEPS).fill(false);
  }

  private static isValidIndex(index: number): boolean {
    return Number.isInteger(index) && index >= 0 && index < Pattern.DEFAULT_TOTAL_STEPS;
  }

  /** Etat du step, ou `null` si l'index est hors bornes. */
  readStep(index: number): boolean | null {
    if (!Pattern.isValidIndex(index)) return null;
    return this.steps[index] ?? false;
  }

  /** Ecrit un step. Retourne `false` (sans mutation) si l'index est hors bornes. */
  writeStep(index: number, active: boolean): boolean {
    if (!Pattern.isValidIndex(index)) return false;
    this.steps[index] = active;
    return true;
  }

  /** Efface tous les steps ET les triolets. */
  clear(): void {
    this.steps.fill(false);
    this.clearTriplets();
  }

  /**
   * Ajoute un groupe ternaire demarrant a `startIndex` (occupe start, +1, +2).
   * Regles (independantes de la longueur) :
   *  - le groupe tient dans la grille : startIndex <= DEFAULT_TOTAL_STEPS - 3 ;
   *  - pas de chevauchement : aucun depart existant T avec |startIndex - T| <= 2.
   */
  addTriplet(startIndex: number): boolean {
    if (!Number.isInteger(startIndex) || startIndex < 0) return false;
    if (startIndex > Pattern.DEFAULT_TOTAL_STEPS - 3) return false;

    const first = Math.max(0, startIndex - 2);
    const last = Math.min(Pattern.DEFAULT_TOTAL_STEPS - 3, startIndex + 2);
    for (let existing = first; existing <= last; ++existing) {
      if (this.tripletStarts[existing]) return false;
    }

    this.tripletStarts[startIndex] = true;
    return true;
  }

  /** Retire le groupe ternaire demarrant a `startIndex`. `false` si absent/hors bornes. */
  removeTriplet(startIndex: number): boolean {
    if (!Pattern.isValidIndex(startIndex)) return false;
    if (!this.tripletStarts[startIndex]) return false;
    this.tripletStarts[startIndex] = false;
    return true;
  }

  /** Vrai si un groupe ternaire demarre exactement a `index`. */
  isTripletStart(index: number): boolean {
    if (!Pattern.isValidIndex(index)) return false;
    return this.tripletStarts[index] ?? false;
  }

  /** Vrai si `index` appartient a un groupe ternaire (start, +1 ou +2). */
  isTripletStep(index: number): boolean {
    if (!Pattern.isValidIndex(index)) return false;
    for (let start = 0; start <= Pattern.DEFAULT_TOTAL_STEPS - 3; ++start) {
      if (!this.tripletStarts[start]) continue;
      if (index >= start && index < start + 3) return true;
    }
    return false;
  }

  /** Efface uniquement les groupes ternaires. */
  clearTriplets(): void {
    this.tripletStarts.fill(false);
  }
}
