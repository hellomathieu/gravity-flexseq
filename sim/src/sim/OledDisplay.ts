/**
 * OledDisplay — apercu fidele de l'ecran OLED du Gravity (128 x 64, 1-bit).
 *
 * Geometrie reprise du POC Wokwi (`flexseq-oled-playground/sketch.ino`), qui
 * fait foi : 24 steps en 2 lignes de 12, pas horizontal de 10 px, grille
 * centree, glyphes 5x5 identiques au firmware d'origine, cadre de selection
 * 9x9, step courant marque par le pixel central inverse.
 *
 * Vocabulaire (voir la legende du PRD) :
 *   o  anneau 5x5      step inactif
 *   *  disque 5x5      step actif
 *   ^  triangle 5x5    step actif en TRIOLET (3 declenchements sur 2 unites)
 *   .  1 pixel         position au-dela de LENGTH
 *   chiffre sous le step : ratchet 2/3/4/6 (N declenchements dans le step)
 *   barre verticale dans la gouttiere : separation de mesure (GRAPHIQUE seule)
 */
import type { CellView } from "./PatternView.js";
import { GLYPH_HEIGHT, textPixels, textWidth } from "./oledFont.js";
import { RATCHET_NONE, RATCHET_TRIPLET } from "../domain/Pattern.js";

export const OLED_W = 128;
export const OLED_H = 64;

const PAPER = "#f4f4f2";
const INK = "#111";

// --- Geometrie (sketch.ino) -------------------------------------------------
const PER_ROW = 12;
const COL_SPACING = 10;
const GRID_WIDTH = (PER_ROW - 1) * COL_SPACING; // 110
const COL_X0 = Math.floor((OLED_W - GRID_WIDTH + 1) / 2); // 9
const ROW_CY = [20, 38] as const; // centres verticaux des 2 lignes
const GLYPH_HALF = 2; // glyphe 5x5
const SELECT_HALF = 4; // cadre 9x9
const SELECT_SIZE = 9;
// Chiffre de ratchet : dessine a la main en 3x5 px (et non via la police, deux
// fois plus encombrante). Loge sous le glyphe, SOUS le cadre du curseur.
const DIGIT_W = 3;
const DIGIT_H = 5;
const DIGIT_DY = 5;

/** Chiffres 3x5 des seuls ratchets affichables. Bit 2 = colonne de gauche. */
const DIGITS: Record<number, readonly number[]> = {
  2: [0b111, 0b001, 0b111, 0b100, 0b111],
  3: [0b111, 0b001, 0b111, 0b001, 0b111],
  4: [0b101, 0b101, 0b111, 0b001, 0b001],
  6: [0b111, 0b100, 0b111, 0b101, 0b111],
};
// La barre de mesure DEPASSE le cadre 9x9 du curseur : sinon un curseur voisin
// l'absorbe visuellement.
const BAR_HALF_H = 6;

const TITLE_TOP = 2;
const HEADER_LINE_Y = 11;
const HEADER_LINE_X = 4;
const HEADER_LINE_W = 120;

const GRID_BOTTOM_Y = ROW_CY[1]! + DIGIT_DY + 4;
const FOOTER_TOP = OLED_H - GLYPH_HEIGHT;
const FOOTER_X = HEADER_LINE_X;

export interface StepCenter {
  x: number;
  y: number;
}

function colX(index: number): number {
  return COL_X0 + (index % PER_ROW) * COL_SPACING;
}

function rowOf(index: number): number {
  return Math.floor(index / PER_ROW);
}

/** Centres des 24 positions (index 0..23), 2 lignes de 12. Pur, deterministe. */
export function stepCenters(): StepCenter[] {
  const centers: StepCenter[] = [];
  for (let i = 0; i < 24; ++i) centers.push({ x: colX(i), y: ROW_CY[rowOf(i)]! });
  return centers;
}

export interface OledModel {
  title: string;
  cells: CellView[];
  /** Step en cours d'edition : cadre 9x9. -1 pour masquer. */
  cursor: number;
  /** Step joue : pixel central inverse. -1 pour masquer. */
  playhead?: number;
  /**
   * Separation de mesure : barre tous les N steps (0 = aucune). GRAPHIQUE
   * uniquement — n'a aucun effet sur la duree des steps ni sur la SUBDIV.
   */
  barLength?: number;
  /** Pied de page d'EDIT PATTERN : channel et tempo. Absent = rien dessine. */
  footer?: string;
}

/** Sous-ensemble de CanvasRenderingContext2D utilise (testable/mockable). */
export type OledCtx = Pick<CanvasRenderingContext2D, "fillRect"> & {
  fillStyle: string;
};

function px(ctx: OledCtx, x: number, y: number): void {
  ctx.fillRect(x, y, 1, 1);
}

function hline(ctx: OledCtx, x: number, y: number, w: number): void {
  ctx.fillRect(x, y, w, 1);
}

function blitText(ctx: OledCtx, text: string, x: number, y: number): void {
  for (const p of textPixels(text)) px(ctx, x + p.x, y + p.y);
}

// --- Glyphes 5x5 ------------------------------------------------------------

/** Anneau : step inactif. */
function drawRing(ctx: OledCtx, cx: number, cy: number): void {
  const x = cx - GLYPH_HALF;
  const y = cy - GLYPH_HALF;
  hline(ctx, x + 1, y, 3);
  px(ctx, x, y + 1);
  px(ctx, x + 4, y + 1);
  px(ctx, x, y + 2);
  px(ctx, x + 4, y + 2);
  px(ctx, x, y + 3);
  px(ctx, x + 4, y + 3);
  hline(ctx, x + 1, y + 4, 3);
}

/** Disque plein : step actif. */
function drawDisc(ctx: OledCtx, cx: number, cy: number): void {
  const x = cx - GLYPH_HALF;
  const y = cy - GLYPH_HALF;
  hline(ctx, x + 1, y, 3);
  hline(ctx, x, y + 1, 5);
  hline(ctx, x, y + 2, 5);
  hline(ctx, x, y + 3, 5);
  hline(ctx, x + 1, y + 4, 3);
}

/** Triangle plein : step actif en TRIOLET (3 declenchements sur 2 unites). */
function drawTriangle(ctx: OledCtx, cx: number, cy: number): void {
  const y = cy - GLYPH_HALF;
  px(ctx, cx, y);
  hline(ctx, cx - 1, y + 1, 3);
  hline(ctx, cx - 1, y + 2, 3);
  hline(ctx, cx - 2, y + 3, 5);
  hline(ctx, cx - 2, y + 4, 5);
}

function frame(ctx: OledCtx, cx: number, cy: number): void {
  const x = cx - SELECT_HALF;
  const y = cy - SELECT_HALF;
  hline(ctx, x, y, SELECT_SIZE);
  hline(ctx, x, y + SELECT_SIZE - 1, SELECT_SIZE);
  ctx.fillRect(x, y, 1, SELECT_SIZE);
  ctx.fillRect(x + SELECT_SIZE - 1, y, 1, SELECT_SIZE);
}

export function drawOled(ctx: OledCtx, model: OledModel): void {
  ctx.fillStyle = PAPER;
  ctx.fillRect(0, 0, OLED_W, OLED_H);
  ctx.fillStyle = INK;

  // En-tete : titre centre + filet.
  const title = model.title.toUpperCase();
  blitText(ctx, title, Math.round((OLED_W - textWidth(title)) / 2), TITLE_TOP);
  hline(ctx, HEADER_LINE_X, HEADER_LINE_Y, HEADER_LINE_W);

  // Separations de mesure : barre dans la gouttiere, jamais en bord de ligne.
  const bar = model.barLength ?? 0;
  if (bar > 0) {
    for (let k = bar; k < 24; k += bar) {
      if (k % PER_ROW === 0) continue;
      const bx = colX(k) - Math.floor(COL_SPACING / 2);
      const cy = ROW_CY[rowOf(k)]!;
      for (let y = cy - BAR_HALF_H; y <= cy + BAR_HALF_H; ++y) px(ctx, bx, y);
    }
  }

  for (const cell of model.cells) {
    const cx = colX(cell.index);
    const cy = ROW_CY[rowOf(cell.index)]!;

    if (cell.kind === "beyond") {
      px(ctx, cx, cy); // au-dela de LENGTH : simple point
      continue;
    }

    if (cell.kind === "active") {
      if (cell.ratchet === RATCHET_TRIPLET) drawTriangle(ctx, cx, cy);
      else drawDisc(ctx, cx, cy);
    } else {
      drawRing(ctx, cx, cy);
    }

    // Ratchet chiffre sous le step (le triolet a deja son triangle).
    if (cell.ratchet !== RATCHET_NONE && cell.ratchet !== RATCHET_TRIPLET) {
      const rows = DIGITS[cell.ratchet];
      if (rows) {
        for (let r = 0; r < DIGIT_H; ++r) {
          const bits = rows[r]!;
          for (let col = 0; col < DIGIT_W; ++col) {
            if (bits & (1 << (DIGIT_W - 1 - col))) px(ctx, cx - 1 + col, cy + DIGIT_DY + r);
          }
        }
      }
    }
  }

  // Cadre d'edition autour du step courant.
  if (model.cursor >= 0 && model.cursor < 24) {
    frame(ctx, colX(model.cursor), ROW_CY[rowOf(model.cursor)]!);
  }

  if (model.footer !== undefined && model.footer !== "") {
    blitText(ctx, model.footer.toUpperCase(), FOOTER_X, FOOTER_TOP);
  }

  // Step joue : pixel central inverse (blanc sur un step actif, noir sinon).
  const head = model.playhead;
  if (head !== undefined && head >= 0 && head < 24) {
    const cell = model.cells.find((c) => c.index === head);
    if (cell && cell.kind !== "beyond") {
      ctx.fillStyle = cell.kind === "active" ? PAPER : INK;
      px(ctx, colX(head), ROW_CY[rowOf(head)]!);
      ctx.fillStyle = INK;
    }
  }
}

/** Le pied tient STRICTEMENT sous le dernier pixel que la grille peut poser. */
export const FOOTER_GEOMETRY = { top: FOOTER_TOP, x: FOOTER_X, gridBottom: GRID_BOTTOM_Y };
