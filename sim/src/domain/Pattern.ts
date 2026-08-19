/**
 * Pattern — CONTENU d'un pattern (modele de reference TypeScript).
 *
 * Un Pattern ne contient QUE son contenu musical — 24 steps binaires + un code
 * de RATCHET par step. Il est PARTAGE : plusieurs channels peuvent referencer
 * le meme Pattern (via leur `selectedPattern`), comme le firmware Sitka
 * original (seqA1..seqB8 partages, channel.seqPattern selecteur).
 *
 * La LONGUEUR n'est PAS une propriete du Pattern : elle est par channel (etat
 * d'execution, voir SequencerEngine). La separation de mesure non plus : c'est
 * une aide de lecture par channel, purement graphique.
 *
 * NB memoire (C++/AVR, hors modele TS) : 15 octets
 * (packedSteps[3] + packedRatchets[12], un quartet par step).
 */

/**
 * Code de ratchet par step. Un step reste UNE position de grille ; le ratchet
 * dit combien de declenchements il emet et combien de temps il dure.
 *
 *  - NONE : 1 declenchement, 1 unite de temps
 *  - 2/3/4/6 : N declenchements DANS la duree d'un step (duree inchangee)
 *  - TRIPLET : 3 declenchements sur DEUX unites (« un triolet de noires vaut
 *    une blanche ») — seul code qui ETIRE le temps et decale la suite.
 *
 * Le ratchet 5 est volontairement absent : a 96 PPQN (2^5 x 3) un cinquieme
 * n'est exact que sur 2 des 25 valeurs de SUBDIV.
 */
export const RATCHET_NONE = 0;
export const RATCHET_2 = 2;
export const RATCHET_3 = 3;
export const RATCHET_4 = 4;
export const RATCHET_6 = 6;
export const RATCHET_TRIPLET = 7;

/** Codes de ratchet stockables, dans l'ordre d'edition. */
export const RATCHET_CODES: readonly number[] = [
  RATCHET_NONE, RATCHET_2, RATCHET_3, RATCHET_4, RATCHET_6, RATCHET_TRIPLET,
];

export function isValidRatchet(code: number): boolean {
  return RATCHET_CODES.includes(code);
}

/** Nombre de declenchements emis par un step portant ce code (>= 1). */
export function ratchetTriggers(code: number): number {
  if (code === RATCHET_TRIPLET) return 3;
  if (code === RATCHET_2 || code === RATCHET_3 || code === RATCHET_4 || code === RATCHET_6) {
    return code;
  }
  return 1;
}

/** Nombre d'unites de temps occupees par le step (1, ou 2 pour le triolet). */
export function ratchetSpan(code: number): number {
  return code === RATCHET_TRIPLET ? 2 : 1;
}

/** Libelle court affiche sous le step (vide si aucun ratchet). */
export function ratchetLabel(code: number): string {
  if (code === RATCHET_TRIPLET) return "3T";
  if (code === RATCHET_NONE) return "";
  return String(code);
}
export class Pattern {
  static readonly DEFAULT_TOTAL_STEPS = 24;

  private readonly steps: boolean[];
  private readonly ratchets: number[];

  constructor() {
    this.steps = new Array<boolean>(Pattern.DEFAULT_TOTAL_STEPS).fill(false);
    this.ratchets = new Array<number>(Pattern.DEFAULT_TOTAL_STEPS).fill(RATCHET_NONE);
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
    this.clearRatchets();
  }

  /**
   * Ajoute un groupe ternaire demarrant a `startIndex` (occupe start, +1, +2).
   * Regles (independantes de la longueur) :
   *  - le groupe tient dans la grille : startIndex <= DEFAULT_TOTAL_STEPS - 3 ;
   *  - pas de chevauchement : aucun depart existant T avec |startIndex - T| <= 2.
   */
  /** Code de ratchet du step, ou RATCHET_NONE si l'index est hors bornes. */
  getRatchet(index: number): number {
    if (!Pattern.isValidIndex(index)) return RATCHET_NONE;
    return this.ratchets[index] ?? RATCHET_NONE;
  }

  /** Definit le ratchet d'un step. Rejette un index ou un code invalide. */
  setRatchet(index: number, code: number): boolean {
    if (!Pattern.isValidIndex(index) || !isValidRatchet(code)) return false;
    this.ratchets[index] = code;
    return true;
  }

  clearRatchets(): void {
    this.ratchets.fill(RATCHET_NONE);
  }

}
