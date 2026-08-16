/**
 * OledDisplay — apercu fidele de l'ecran OLED du Gravity (128 x 64, 1-bit).
 *
 * Reproduit l'ecran EDIT PATTERN avec les elements REELS du firmware
 * (voir Gravity/UI.ino) :
 *   - police bitmap `velvetscreen` decodee au pixel pres (titre + pastilles) ;
 *   - pastilles 'q' = step actif (disque plein) / 'p' = step inactif (anneau) ;
 *   - curseur = cadre carre autour du step courant.
 *
 * Adaptation FlexSeq (PRD) : 24 positions en 2 lignes de 12 (le firmware
 * d'origine en montre 16 en 2x8) et `•` discret au-dela de LENGTH. L'espacement
 * est resserre pour loger 12 pastilles par ligne dans 128 px.
 *
 * Le canvas logique fait 128x64 px reels ; l'agrandissement CSS se fait en
 * `image-rendering: pixelated` pour garder l'aspect blocky de l'OLED.
 */
import type { CellView } from "./PatternView.js";
import { glyphPixels, textPixels, textWidth, GLYPH_HEIGHT } from "./oledFont.js";

export const OLED_W = 128;
export const OLED_H = 64;

const MARGIN_X = 11;
const PER_ROW = 12;
const ROW_Y = [30, 48] as const;
const CURSOR_SIDE = 9;
const TITLE_TOP = 2;
const SEPARATOR_Y = 11;

export interface StepCenter {
  x: number;
  y: number;
}

/** Centres des 24 positions (index 0..23), 2 lignes de 12. Pur, deterministe. */
export function stepCenters(): StepCenter[] {
  const gap = (OLED_W - 2 * MARGIN_X) / (PER_ROW - 1);
  const centers: StepCenter[] = [];
  for (let i = 0; i < 24; ++i) {
    const row = Math.floor(i / PER_ROW);
    const col = i % PER_ROW;
    centers.push({ x: Math.round(MARGIN_X + col * gap), y: ROW_Y[row]! });
  }
  return centers;
}

export interface OledModel {
  title: string;
  cells: CellView[];
  cursor: number;
}

/** Sous-ensemble de CanvasRenderingContext2D utilise (testable/mockable). */
export type OledCtx = Pick<CanvasRenderingContext2D, "fillRect"> & {
  fillStyle: string;
};

function px(ctx: OledCtx, x: number, y: number): void {
  ctx.fillRect(x, y, 1, 1);
}

function boxOutline(ctx: OledCtx, x: number, y: number, side: number): void {
  ctx.fillRect(x, y, side, 1);
  ctx.fillRect(x, y + side - 1, side, 1);
  ctx.fillRect(x, y, 1, side);
  ctx.fillRect(x + side - 1, y, 1, side);
}

function drawGlyphCentered(ctx: OledCtx, ch: string, cx: number, cy: number): void {
  const half = (GLYPH_HEIGHT - 1) >> 1; // 5px -> 2
  for (const p of glyphPixels(ch)) px(ctx, cx - half + p.x, cy - half + p.y);
}

export function drawOled(ctx: OledCtx, model: OledModel): void {
  // Fond "papier" clair, encre noire (comme la maquette).
  ctx.fillStyle = "#f4f4f2";
  ctx.fillRect(0, 0, OLED_W, OLED_H);
  ctx.fillStyle = "#111";

  // Titre centre en police velvetscreen + ligne de separation.
  const title = model.title.toUpperCase();
  const x0 = Math.round((OLED_W - textWidth(title)) / 2);
  for (const p of textPixels(title)) px(ctx, x0 + p.x, TITLE_TOP + p.y);
  ctx.fillRect(6, SEPARATOR_Y, OLED_W - 12, 1);

  const centers = stepCenters();

  for (const cell of model.cells) {
    const c = centers[cell.index];
    if (!c) continue;

    // Indicateur de groupe ternaire, au-dessus du step.
    if (cell.tripletStep) {
      ctx.fillRect(c.x - 3, c.y - 6, 7, 1);
    }

    if (cell.kind === "active") {
      drawGlyphCentered(ctx, "q", c.x, c.y);
    } else if (cell.kind === "inactive") {
      drawGlyphCentered(ctx, "p", c.x, c.y);
    } else {
      px(ctx, c.x, c.y); // au-dela de LENGTH : point discret
    }
  }

  // Curseur : cadre carre autour du step courant.
  const cur = centers[model.cursor];
  if (cur) {
    boxOutline(ctx, cur.x - (CURSOR_SIDE >> 1), cur.y - (CURSOR_SIDE >> 1), CURSOR_SIDE);
  }
}
