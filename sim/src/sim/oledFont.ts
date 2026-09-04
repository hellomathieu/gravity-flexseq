/**
 * oledFont — les DEUX polices bitmap du firmware Gravity, decodees au pixel
 * pres depuis le format U8g2 (voir tools/decode-original-font.py) et figees
 * dans velvetscreen.font.json et stkl.font.json. Aucun decodeur U8g2 au
 * runtime.
 *
 * Les deux atlas sont decodes depuis src/hal/OriginalFonts.cpp, donc depuis
 * les MEMES octets que le firmware embarque.
 *
 * L avance d un glyphe est celle de l atlas, et rien ne s y ajoute : u8g2
 * avance de `delta_x` et pas d un pixel de plus. Les avances de l atlas
 * portent deja le pixel de separation — `A` mesure 4 de large pour une avance
 * de 5.
 *
 * Ce module est PUR (aucun DOM) : il expose la position des pixels allumes
 * pour un glyphe ou un texte, ce qui le rend testable sans navigateur.
 * `OledDisplay` se charge du blit sur le canvas.
 *
 * Les glyphes de pastille du sequenceur sont, comme dans le firmware :
 *   'q' = step actif (disque plein) · 'p' = step inactif (anneau).
 */
import velvetscreenData from "./velvetscreen.font.json";
import stklData from "./stkl.font.json";

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

export type Font = Record<string, Glyph>;

export const VELVETSCREEN: Font =
  (velvetscreenData as { glyphs: Font }).glyphs;
export const STK_L: Font = (stklData as { glyphs: Font }).glyphs;

/** Hauteur commune des majuscules/chiffres velvetscreen. */
export const GLYPH_HEIGHT = 5;

/** Hauteur des chiffres de stkL, le gros parametre de l original. */
export const STK_L_HEIGHT = 23;

/** Avance de repli d un caractere que la police ne porte pas. */
const FALLBACK_ADVANCE = 3;

export function glyphFor(ch: string, font: Font = VELVETSCREEN): Glyph | undefined {
  return font[String(ch.charCodeAt(0))];
}

function advanceOf(g: Glyph | undefined): number {
  return g ? g.advance : FALLBACK_ADVANCE;
}

/** Pixels allumes d'un seul glyphe, coordonnees relatives a son coin haut-gauche. */
export function glyphPixels(ch: string, font: Font = VELVETSCREEN): Pixel[] {
  const g = glyphFor(ch, font);
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
export function textPixels(text: string, font: Font = VELVETSCREEN): Pixel[] {
  const px: Pixel[] = [];
  let cx = 0;
  for (const ch of text) {
    const g = glyphFor(ch, font);
    if (g) {
      for (let r = 0; r < g.h; ++r) {
        const row = g.rows[r] ?? "";
        for (let c = 0; c < g.w; ++c) {
          if (row[c] === "1") px.push({ x: cx + c + g.xoff, y: r });
        }
      }
    }
    cx += advanceOf(g);
  }
  return px;
}

/**
 * Largeur en pixels d un texte, exactement comme `u8g2_GetStrWidth`.
 *
 * ⚠️ **Ce n est PAS la somme des avances.** u8g2 retire l avance du DERNIER
 * glyphe et ajoute a la place sa largeur d encre reelle plus son decalage
 * horizontal — `u8g2_font.c` lignes 1361 a 1368. Pour `MOD:` la somme des
 * avances vaut 18 et u8g2 rend 17, parce que le deux-points mesure 1 pixel
 * d encre pour une avance de 2. L ecart d un pixel se voit sur chaque pave de
 * curseur et sur chaque texte centre.
 *
 * L ajustement est saute quand le dernier glyphe n a aucune encre — un espace
 * final, par exemple. C est le garde `glyph_width != 0` de u8g2.
 */
export function textWidth(text: string, font: Font = VELVETSCREEN): number {
  let w = 0;
  let last: Glyph | undefined;
  for (const ch of text) {
    const g = glyphFor(ch, font);
    if (!g) continue;
    w += g.advance;
    last = g;
  }
  if (last !== undefined && last.w !== 0) {
    w -= last.advance;
    w += last.w + last.xoff;
  }
  return w;
}

/** Somme des avances, ce que rend `u8g2_DrawStr` — distinct de textWidth. */
export function textAdvance(text: string, font: Font = VELVETSCREEN): number {
  let w = 0;
  for (const ch of text) w += advanceOf(glyphFor(ch, font));
  return w;
}
