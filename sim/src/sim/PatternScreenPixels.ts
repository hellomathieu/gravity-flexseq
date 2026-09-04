/**
 * PatternScreenPixels — le rendu de l ecran EDIT PATTERN, PIXEL PAR PIXEL,
 * miroir de `drawPatternScreen` de `include/flexseq/PatternScreen.h`.
 *
 * Meme raison d etre que `MainScreenPixels` : donner a la vue TypeScript de quoi
 * contredire le panneau. La geometrie vit ici et elle est gardee par
 * `test/vectors/screen_geometry_vectors.tsv`, famille `E` (ADR 0012).
 */
import { OLED_H, OLED_W } from "./OledDisplay.js";
import { VELVETSCREEN, glyphFor, textWidth } from "./oledFont.js";
import { RATCHET_NONE, RATCHET_2, RATCHET_3, RATCHET_4, RATCHET_6, RATCHET_TRIPLET } from "../domain/Pattern.js";

export const PER_ROW = 12;
export const GRID_ROWS = 3;
export const GRID_STEPS = PER_ROW * GRID_ROWS;
export const COL_SPACING = 10;
export const GRID_WIDTH = (PER_ROW - 1) * COL_SPACING;
export const COL_X0 = Math.floor((OLED_W - GRID_WIDTH + 1) / 2);
export const ROW_CY_0 = 18;
export const ROW_SPACING = 18;
export const GLYPH_HALF = 2;
export const SELECT_HALF = 4;
export const SELECT_SIZE = 9;
export const DIGIT_W = 3;
export const DIGIT_H = 5;
export const DIGIT_DY = 5;
export const BAR_HALF_H = 6;
export const BAR_HEIGHT = 2 * BAR_HALF_H + 1;
export const TITLE_BASELINE_Y = 7;
export const HEADER_LINE_X = 4;
export const HEADER_LINE_Y = 10;
export const HEADER_LINE_W = 120;
export const SEP_LABEL_X = 102;
export const SEP_VALUE_X = 120;
export const SEP_LABEL_W = 14;
export const LAST_ROW_CY = ROW_CY_0 + (GRID_ROWS - 1) * ROW_SPACING;
export const GRID_BOTTOM_Y = LAST_ROW_CY + DIGIT_DY + DIGIT_H - 1;

export function colX(index: number): number {
  return COL_X0 + (index % PER_ROW) * COL_SPACING;
}

export function rowOf(index: number): number {
  return Math.floor(index / PER_ROW);
}

export function rowCY(index: number): number {
  return ROW_CY_0 + rowOf(index) * ROW_SPACING;
}

/**
 * Les chiffres 3x5 des seuls ratchets affichables. Les cinq rangees de trois
 * pixels sont empilees dans un entier de quinze bits, rangee 0 en tete, comme
 * dans le firmware — pas une table, pour la meme raison qu en C++ : elle y
 * couterait de la RAM.
 */
export function digitBits(code: number): number {
  if (code === RATCHET_2) return 0b111001111100111;
  if (code === RATCHET_3) return 0b111001111001111;
  if (code === RATCHET_4) return 0b101101111001001;
  if (code === RATCHET_6) return 0b111100111101111;
  return 0;
}

export interface PatternScreenPixelModel {
  title: string | null;
  steps: ReadonlyArray<boolean>;
  ratchets: ReadonlyArray<number>;
  length: number;
  cursor: number;
  playhead: number;
  barLength: number;
  sepSelected: boolean;
  sepOpen: boolean;
}

class Ink {
  private readonly on = new Set<number>();
  private colour = 1;

  setDrawColor(c: number): void {
    this.colour = c;
  }

  plot(x: number, y: number): void {
    if (x < 0 || y < 0 || x >= OLED_W || y >= OLED_H) return;
    const key = y * OLED_W + x;
    if (this.colour === 1) this.on.add(key);
    else this.on.delete(key);
  }

  drawHLine(x: number, y: number, len: number): void {
    for (let i = 0; i < len; ++i) this.plot(x + i, y);
  }

  drawVLine(x: number, y: number, len: number): void {
    for (let i = 0; i < len; ++i) this.plot(x, y + i);
  }

  drawBox(x: number, y: number, w: number, h: number): void {
    for (let r = 0; r < h; ++r) this.drawHLine(x, y + r, w);
  }

  drawFrame(x: number, y: number, w: number, h: number): void {
    this.drawHLine(x, y, w);
    this.drawHLine(x, y + h - 1, w);
    this.drawVLine(x, y, h);
    this.drawVLine(x + w - 1, y, h);
  }

  drawStr(x: number, baseline: number, text: string): void {
    let cx = x;
    for (const ch of text) {
      const g = glyphFor(ch, VELVETSCREEN);
      if (!g) continue;
      const top = baseline - g.h - g.yoff;
      for (let r = 0; r < g.h; ++r) {
        const row = g.rows[r] ?? "";
        for (let c = 0; c < g.w; ++c) {
          if (row[c] === "1") this.plot(cx + c + g.xoff, top + r);
        }
      }
      cx += g.advance;
    }
  }

  count(): number {
    return this.on.size;
  }

  rowCounts(): number[] {
    const rows = new Array<number>(OLED_H).fill(0);
    for (const key of this.on) rows[Math.floor(key / OLED_W)]! += 1;
    return rows;
  }

  pixels(): Set<string> {
    const out = new Set<string>();
    for (const key of this.on) out.add(`${key % OLED_W},${Math.floor(key / OLED_W)}`);
    return out;
  }
}

function drawRing(ink: Ink, cx: number, cy: number): void {
  const x = cx - GLYPH_HALF;
  const y = cy - GLYPH_HALF;
  ink.drawHLine(x + 1, y, 3);
  for (let r = 1; r <= 3; ++r) {
    ink.plot(x, y + r);
    ink.plot(x + 4, y + r);
  }
  ink.drawHLine(x + 1, y + 4, 3);
}

function drawDisc(ink: Ink, cx: number, cy: number): void {
  const x = cx - GLYPH_HALF;
  const y = cy - GLYPH_HALF;
  ink.drawHLine(x + 1, y, 3);
  ink.drawHLine(x, y + 1, 5);
  ink.drawHLine(x, y + 2, 5);
  ink.drawHLine(x, y + 3, 5);
  ink.drawHLine(x + 1, y + 4, 3);
}

function drawTriangle(ink: Ink, cx: number, cy: number): void {
  const y = cy - GLYPH_HALF;
  ink.plot(cx, y);
  ink.drawHLine(cx - 1, y + 1, 3);
  ink.drawHLine(cx - 1, y + 2, 3);
  ink.drawHLine(cx - 2, y + 3, 5);
  ink.drawHLine(cx - 2, y + 4, 5);
}

function drawRatchetDigit(ink: Ink, cx: number, cy: number, code: number): void {
  const bits = digitBits(code);
  if (bits === 0) return;
  const x0 = cx - 1;
  const y0 = cy + DIGIT_DY;
  for (let r = 0; r < DIGIT_H; ++r) {
    const row = (bits >> (DIGIT_W * (DIGIT_H - 1 - r))) & 0b111;
    for (let col = 0; col < DIGIT_W; ++col) {
      if (row & (1 << (DIGIT_W - 1 - col))) ink.plot(x0 + col, y0 + r);
    }
  }
}

export interface Render {
  pixels: Set<string>;
  count: number;
  rows: number[];
}

export function renderPatternScreen(model: PatternScreenPixelModel): Render {
  const ink = new Ink();

  if (model.title !== null) {
    const w = textWidth(model.title, VELVETSCREEN);
    ink.drawStr(Math.floor((OLED_W - w) / 2), TITLE_BASELINE_Y, model.title);
  }
  ink.drawHLine(HEADER_LINE_X, HEADER_LINE_Y, HEADER_LINE_W);

  {
    const sep = model.barLength === 0 ? "-" : String(model.barLength);
    const base = TITLE_BASELINE_Y;
    const h = 5;
    if (model.sepSelected && !model.sepOpen) {
      ink.drawBox(SEP_LABEL_X - 1, base - h - 1, SEP_LABEL_W + 2, h + 2);
      ink.setDrawColor(0);
      ink.drawStr(SEP_LABEL_X, base, "SEP");
      ink.setDrawColor(1);
    } else {
      ink.drawStr(SEP_LABEL_X, base, "SEP");
    }
    if (model.sepSelected && model.sepOpen) {
      ink.drawFrame(SEP_VALUE_X - 1, base - h - 1, textWidth(sep, VELVETSCREEN) + 2, h + 2);
    }
    ink.drawStr(SEP_VALUE_X, base, sep);
  }

  if (model.barLength > 0) {
    for (let k = model.barLength; k < GRID_STEPS; k += model.barLength) {
      if (k % PER_ROW === 0) continue;
      const cy = rowCY(k);
      ink.drawVLine(colX(k) - COL_SPACING / 2, cy - BAR_HALF_H, BAR_HEIGHT);
    }
  }

  for (let i = 0; i < GRID_STEPS; ++i) {
    const cx = colX(i);
    const cy = rowCY(i);
    if (i >= model.length) {
      ink.plot(cx, cy);
      continue;
    }
    const active = model.steps[i] ?? false;
    const ratchet = model.ratchets[i] ?? RATCHET_NONE;
    if (active) {
      if (ratchet === RATCHET_TRIPLET) drawTriangle(ink, cx, cy);
      else drawDisc(ink, cx, cy);
    } else {
      drawRing(ink, cx, cy);
    }
    if (ratchet !== RATCHET_NONE && ratchet !== RATCHET_TRIPLET) {
      drawRatchetDigit(ink, cx, cy, ratchet);
    }
  }

  if (model.cursor >= 0 && model.cursor < GRID_STEPS) {
    ink.drawFrame(
      colX(model.cursor) - SELECT_HALF,
      rowCY(model.cursor) - SELECT_HALF,
      SELECT_SIZE,
      SELECT_SIZE,
    );
  }

  if (model.playhead >= 0 && model.playhead < GRID_STEPS && model.playhead < model.length) {
    const active = model.steps[model.playhead] ?? false;
    ink.setDrawColor(active ? 0 : 1);
    ink.plot(colX(model.playhead), rowCY(model.playhead));
    ink.setDrawColor(1);
  }

  return { pixels: ink.pixels(), count: ink.count(), rows: ink.rowCounts() };
}
