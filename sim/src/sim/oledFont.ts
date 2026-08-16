/**
 * oledFont — police bitmap `velvetscreen` du firmware Gravity, decodee au
 * pixel pres depuis le format U8g2 (voir tools/decode-velvetscreen.py) et
 * figee dans velvetscreen.font.json. Aucun decodeur U8g2 au runtime.
 *
 * Ce module est PUR (aucun DOM) : il expose la position des pixels allumes
 * pour un glyphe ou un texte, ce qui le rend testable sans navigateur.
 * `OledDisplay` se charge du blit sur le canvas.
 *
 * Les glyphes de pastille du sequenceur sont, comme dans le firmware :
 *   'q' = step actif (disque plein) · 'p' = step inactif (anneau).
 */
import fontData from "./velvetscreen.font.json";

export interface Glyph {
  w: number;
  h: number;
  xoff: number;
  yoff: number;
  advance: number;
  rows: string[];
}

export interface Pixel {
  x: number;
  y: number;
}

const GLYPHS = (fontData as { glyphs: Record<string, Glyph> }).glyphs;

/** Hauteur commune des majuscules/chiffres velvetscreen. */
export const GLYPH_HEIGHT = 5;

/** L'espace a un advance aberrant dans l'atlas decode ; on force une valeur nette. */
const SPACE_ADVANCE = 3;

export function glyphFor(ch: string): Glyph | undefined {
  return GLYPHS[String(ch.charCodeAt(0))];
}

function advanceOf(ch: string, g: Glyph | undefined): number {
  if (ch === " ") return SPACE_ADVANCE;
  return g && g.advance > 0 ? g.advance : SPACE_ADVANCE;
}

/** Pixels allumes d'un seul glyphe, coordonnees relatives a son coin haut-gauche. */
export function glyphPixels(ch: string): Pixel[] {
  const g = glyphFor(ch);
  const px: Pixel[] = [];
  if (!g) return px;
  for (let r = 0; r < g.h; ++r) {
    const row = g.rows[r] ?? "";
    for (let c = 0; c < g.w; ++c) {
      if (row[c] === "1") px.push({ x: c + g.xoff, y: r });
    }
  }
  return px;
}

/** Pixels allumes d'un texte (coin haut-gauche a l'origine), avec inter-lettrage. */
export function textPixels(text: string, tracking = 1): Pixel[] {
  const px: Pixel[] = [];
  let cx = 0;
  for (const ch of text) {
    const g = glyphFor(ch);
    if (g) {
      for (let r = 0; r < g.h; ++r) {
        const row = g.rows[r] ?? "";
        for (let c = 0; c < g.w; ++c) {
          if (row[c] === "1") px.push({ x: cx + c + g.xoff, y: r });
        }
      }
    }
    cx += advanceOf(ch, g) + tracking;
  }
  return px;
}

/** Largeur en pixels d'un texte rendu par textPixels (meme inter-lettrage). */
export function textWidth(text: string, tracking = 1): number {
  let w = 0;
  for (const ch of text) w += advanceOf(ch, glyphFor(ch)) + tracking;
  return w > 0 ? w - tracking : 0;
}
