/**
 * Représentation géométrique des valeurs SUBDIV en notes de musique (1-bit),
 * pour l'écran OLED (CONFIG PATTERN).
 *
 * 13 des 25 valeurs SUBDIV correspondent à une note dessinable :
 *  - 7 notes « pures » (puissances de 2) : ronde, blanche, noire, croche,
 *    double-, triple-, quadruple-croche ;
 *  - 2 notes pointées : /3 = blanche pointée, /6 = ronde pointée ;
 *  - 4 triolets : x3, x6, x12, x24 = note + indicateur « 3 ».
 * Les 12 autres valeurs (/5, /7, /8, /9…/128) n'ont pas de note simple : le
 * rendu retombe alors sur le label texte `/N`·`xN` (subdivLabel).
 *
 * Ce module est PUR (mapping + bitmaps déterministes), testé en miroir du futur
 * portage C++/velvetscreen (le firmware n'a pas encore de rendu OLED).
 */

export type NoteType =
  | "ronde"
  | "blanche"
  | "noire"
  | "croche"
  | "double"
  | "triple"
  | "quadruple";

export interface NoteGlyph {
  note: NoteType;
  /** Note pointée (durée × 1,5). */
  dotted: boolean;
  /** Triolet : la note porte un indicateur « 3 ». */
  triplet: boolean;
}

interface NoteShape {
  filled: boolean; // tête pleine (noire+) vs creuse (ronde/blanche)
  stem: boolean; // hampe
  flags: number; // nombre de crochets (0..4)
}

const NOTE_SHAPE: Record<NoteType, NoteShape> = {
  ronde: { filled: false, stem: false, flags: 0 },
  blanche: { filled: false, stem: true, flags: 0 },
  noire: { filled: true, stem: true, flags: 0 },
  croche: { filled: true, stem: true, flags: 1 },
  double: { filled: true, stem: true, flags: 2 },
  triple: { filled: true, stem: true, flags: 3 },
  quadruple: { filled: true, stem: true, flags: 4 },
};

/** Dimensions de la cellule bitmap d'une note. */
export const GLYPH_W = 8;
export const GLYPH_H = 12;

/**
 * Mapping SUBDIV (valeur libGravity) → note dessinable, ou null si aucune note
 * simple (rendu texte). Voir Subdiv.h / subdiv.ts pour la convention `/N`·`xN`.
 */
const GLYPH_MAP: Record<number, NoteGlyph> = {
  [-24]: { note: "quadruple", dotted: false, triplet: true },
  [-16]: { note: "quadruple", dotted: false, triplet: false },
  [-12]: { note: "triple", dotted: false, triplet: true },
  [-8]: { note: "triple", dotted: false, triplet: false },
  [-6]: { note: "double", dotted: false, triplet: true },
  [-4]: { note: "double", dotted: false, triplet: false },
  [-3]: { note: "croche", dotted: false, triplet: true },
  [-2]: { note: "croche", dotted: false, triplet: false },
  [1]: { note: "noire", dotted: false, triplet: false },
  [2]: { note: "blanche", dotted: false, triplet: false },
  [3]: { note: "blanche", dotted: true, triplet: false },
  [4]: { note: "ronde", dotted: false, triplet: false },
  [6]: { note: "ronde", dotted: true, triplet: false },
};

/** Note géométrique d'une valeur SUBDIV, ou null si elle se rend en texte. */
export function subdivGlyph(subdiv: number): NoteGlyph | null {
  return GLYPH_MAP[subdiv] ?? null;
}

/**
 * Bitmap 1-bit (GLYPH_H × GLYPH_W) d'une note : tête + hampe + crochets, plus
 * le point d'augmentation si `dotted`. L'indicateur « 3 » du triolet est rendu
 * séparément (au-dessus de la note) et n'est PAS inclus ici.
 */
export function makeNoteBitmap(note: NoteType, dotted = false): number[][] {
  const g: number[][] = Array.from({ length: GLYPH_H }, () =>
    Array<number>(GLYPH_W).fill(0),
  );
  const shape = NOTE_SHAPE[note];

  const head: ReadonlyArray<readonly [number, number]> = shape.filled
    ? [[1, 9], [2, 9], [3, 9], [0, 10], [1, 10], [2, 10], [3, 10], [0, 11], [1, 11], [2, 11]]
    : [[1, 9], [2, 9], [3, 9], [0, 10], [3, 10], [1, 11], [2, 11]];
  for (const [x, y] of head) g[y]![x] = 1;

  if (shape.stem) for (let y = 1; y <= 10; ++y) g[y]![4] = 1;

  for (let i = 0; i < shape.flags; ++i) {
    const b = 1 + i * 2;
    g[b]![5] = 1;
    g[b]![6] = 1;
    g[b + 1]![6] = 1;
  }

  // Point d'augmentation : 2×2 à droite de la tête (les notes pointées n'ont
  // pas de crochet, donc l'espace est libre).
  if (dotted) {
    g[9]![5] = 1;
    g[9]![6] = 1;
    g[10]![5] = 1;
    g[10]![6] = 1;
  }

  return g;
}
