/**
 * MainScreenPixels — le rendu de l ecran principal, PIXEL PAR PIXEL, miroir de
 * `drawMainScreen` de `include/flexseq/MainScreen.h`.
 *
 * Ce module est PUR : il rend un ensemble de pixels allumes, pas un canvas. Il
 * existe pour une seule raison, et c est la ligne 89 de `docs/open-risks.md` :
 * tant que la vue TypeScript dessinait sa propre disposition, rien ne pouvait
 * la confronter au panneau. Ici la comparaison est un nombre.
 *
 * La geometrie vient de `MainScreenDisplay`, dont les valeurs sont gardees par
 * `test/vectors/screen_geometry_vectors.tsv` (ADR 0012).
 *
 * Convention verticale de u8g2, ADR 0012 : l encre d un glyphe de hauteur `h`
 * dessine a la ligne de base `b` occupe les rangees `b - h - yoff` a
 * `b - 1 - yoff`. La rangee `b` reste vide.
 */
import { OLED_H, OLED_W } from "./OledDisplay.js";
import {
  LINE_0_BASELINE_Y,
  LINE_LABEL_X,
  LINE_SPACING_Y,
  LINE_VALUE_X,
  MAIN_CENTRE_X,
  MAIN_LABEL_BASELINE_Y,
  MAIN_VALUE_BASELINE_Y,
  HEADLINE_BASELINE_Y,
  HEADLINE_BOX_H,
  HEADLINE_BOX_W,
  HEADLINE_BOX_X,
  HEADLINE_BOX_Y,
  COL_LEFT_X,
  COL_RIGHT_X,
  COL_W,
  ROW_A_BOX_Y,
  ROW_B_BOX_Y,
  ROW_BOX_H,
  RULE_W,
  RULE_X,
  RULE_Y,
  TAB_BASELINE_Y,
  TAB_BOX_H,
  TAB_BOX_Y,
  TAB_COUNT,
  TAB_SLOT_W,
  TAB_TOP_Y,
  TEXT_INSET,
  GLYPH_SIZE,
  tabCentreX,
  tabSlotX,
  barLabel,
  headlineOf,
  patternName,
  sourceLabel,
  subdivLabel,
} from "./MainScreenDisplay.js";
import { MainParameter, type MainScreenModel } from "../domain/MainScreenModel.js";
import { ChannelMode } from "../domain/SequencerEngine.js";
import { STK_L, VELVETSCREEN, glyphFor, textWidth, type Font } from "./oledFont.js";

export const LBL_MODE = "MODE:";
export const LBL_OFFSET = "OFFSET:";
export const LBL_SUBDIV_FIELD = "SUBDIV:";
export const LBL_MOD = "MOD:";
export const LBL_OFF = "OFF";
export const LBL_SUBDIVISION = "SUBDIVISION";
export const LBL_SKIP_CHANCE = "SKIP CHANCE";
export const LBL_LENGTH = "LENGTH:";
export const LBL_PATTERN = "PATTERN";

const VELVETSCREEN_HEIGHT = 5;
const STK_L_HEIGHT = 23;

class Ink {
  private readonly on = new Set<number>();
  private colour = 1;

  setDrawColor(c: number): void {
    this.colour = c;
  }

  private plot(x: number, y: number): void {
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

  drawStr(x: number, baseline: number, text: string, font: Font): number {
    let cx = x;
    for (const ch of text) {
      const g = glyphFor(ch, font);
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
    return cx - x;
  }

  pixels(): Set<string> {
    const out = new Set<string>();
    for (const key of this.on) {
      out.add(`${key % OLED_W},${Math.floor(key / OLED_W)}`);
    }
    return out;
  }

  count(): number {
    return this.on.size;
  }

  rowCounts(): number[] {
    const rows = new Array<number>(OLED_H).fill(0);
    for (const key of this.on) rows[Math.floor(key / OLED_W)]! += 1;
    return rows;
  }
}

function writeUnsigned(value: number): string {
  return String(value);
}

function skipText(skipChance: number): string {
  return `${skipChance}0%`;
}

export function mainValueOf(model: MainScreenModel): string {
  if (model.configPage || model.mainParameter === MainParameter.Pattern) {
    return patternName(model.patternIndex);
  }
  if (model.mainParameter === MainParameter.None) return "";
  if (model.mainParameter === MainParameter.SkipChance) return skipText(model.skipChance);
  return subdivLabel(model.subdiv);
}

export function mainLabelOf(model: MainScreenModel): string {
  if (model.configPage || model.mainParameter === MainParameter.Pattern) return LBL_PATTERN;
  return model.mainParameter === MainParameter.SkipChance ? LBL_SKIP_CHANCE : LBL_SUBDIVISION;
}

function modeText(mode: ChannelMode): string {
  if (mode === ChannelMode.CLOCK) return "CLOCK";
  if (mode === ChannelMode.RANDOM) return "RAND";
  return "SEQ";
}

export function configLine(model: MainScreenModel, index: number): [string, string] {
  if (index === 0) return [LBL_LENGTH, String(model.length)];
  if (index === 1) return [LBL_SUBDIV_FIELD, subdivLabel(model.subdiv)];
  return [LBL_MOD, LBL_OFF];
}

export function legacyLine(model: MainScreenModel, index: number): [string, string] {
  if (model.configPage) return configLine(model, index);
  if (index === 0) return [LBL_MODE, modeText(model.mode)];
  if (index === 1) {
    if (model.mode === ChannelMode.CLOCK) {
      return [LBL_OFFSET, `${writeUnsigned(model.offset)}/${writeUnsigned(model.stepTicks)}`];
    }
    return [LBL_SUBDIV_FIELD, subdivLabel(model.subdiv)];
  }
  return [LBL_MOD, LBL_OFF];
}

function drawClockGlyph(ink: Ink, cx: number, cy: number): void {
  const x = cx - 3;
  const y = cy - 3;
  ink.drawFrame(x, y, GLYPH_SIZE, GLYPH_SIZE);
  ink.drawHLine(x + 4, y + 2, 2);
  ink.drawHLine(x + 4, y + 3, 2);
  ink.drawHLine(x + 3, y + 2, 1);
}

function drawSettingsGlyph(ink: Ink, cx: number, cy: number): void {
  ink.drawBox(cx - 2, cy - 2, 5, 5);
}

function drawLabelledField(
  ink: Ink,
  x: number,
  y: number,
  label: string,
  value: string | null,
  framed: boolean,
  inverted: boolean,
): void {
  if (inverted) {
    ink.drawBox(x, y, COL_W, ROW_BOX_H);
    ink.setDrawColor(0);
  } else if (framed) {
    ink.drawFrame(x, y, COL_W, ROW_BOX_H);
  }
  const textX = x + TEXT_INSET;
  const baseline = y + 7;
  const used = ink.drawStr(textX, baseline, label, VELVETSCREEN);
  if (value !== null) ink.drawStr(textX + used + 4, baseline, value, VELVETSCREEN);
  if (inverted) ink.setDrawColor(1);
}

function drawLegacyChannel(ink: Ink, model: MainScreenModel): void {
  const value = mainValueOf(model);
  if (value !== "") {
    const w = textWidth(value, STK_L);
    ink.drawStr(MAIN_CENTRE_X - Math.floor(w / 2), MAIN_VALUE_BASELINE_Y, value, STK_L);
  }

  const mainLabel = mainLabelOf(model);
  const lw = textWidth(mainLabel, VELVETSCREEN);
  ink.drawStr(MAIN_CENTRE_X - Math.floor(lw / 2), MAIN_LABEL_BASELINE_Y, mainLabel, VELVETSCREEN);

  for (let line = 0; line < 3; ++line) {
    const base = LINE_0_BASELINE_Y + line * LINE_SPACING_Y;
    const [text, lineValue] = legacyLine(model, line);
    const onCursor = model.insideTab && model.cursor === line;
    const labelW = textWidth(text, VELVETSCREEN);
    if (onCursor && !model.fieldOpen) {
      ink.drawBox(
        LINE_LABEL_X - 1,
        base - VELVETSCREEN_HEIGHT - 1,
        labelW + 2,
        VELVETSCREEN_HEIGHT + 2,
      );
      ink.setDrawColor(0);
      ink.drawStr(LINE_LABEL_X, base, text, VELVETSCREEN);
      ink.setDrawColor(1);
    } else {
      ink.drawStr(LINE_LABEL_X, base, text, VELVETSCREEN);
    }
    if (lineValue !== "") {
      if (onCursor && model.fieldOpen) {
        const w = textWidth(lineValue, VELVETSCREEN);
        ink.drawFrame(
          LINE_VALUE_X - 2,
          base - VELVETSCREEN_HEIGHT - 2,
          w + 4,
          VELVETSCREEN_HEIGHT + 4,
        );
      }
      ink.drawStr(LINE_VALUE_X, base, lineValue, VELVETSCREEN);
    }
  }
}

export interface Render {
  pixels: Set<string>;
  count: number;
  rows: number[];
}

export function renderMainScreen(model: MainScreenModel): Render {
  const ink = new Ink();
  const legacy = (model.configPage || model.legacyLayout)
    && model.tab !== 0 && model.tab !== TAB_COUNT - 1;
  const cursorOnHeadline = model.insideTab && model.cursor === 0;

  if (!legacy) {
    const headline = headlineOf(model);
    if (headline !== "") {
      const w = textWidth(headline, VELVETSCREEN);
      ink.drawStr(
        Math.floor((OLED_W - w) / 2),
        HEADLINE_BASELINE_Y,
        headline,
        VELVETSCREEN,
      );
    }
    if (cursorOnHeadline) {
      ink.drawFrame(HEADLINE_BOX_X, HEADLINE_BOX_Y, HEADLINE_BOX_W, HEADLINE_BOX_H);
      if (model.fieldOpen) {
        ink.drawFrame(
          HEADLINE_BOX_X + 1,
          HEADLINE_BOX_Y + 1,
          HEADLINE_BOX_W - 2,
          HEADLINE_BOX_H - 2,
        );
      }
    }
  }

  if (model.tab === 0) {
    drawLabelledField(
      ink,
      COL_LEFT_X,
      ROW_A_BOX_Y,
      "SRC",
      sourceLabel(model.clockSource),
      model.insideTab && model.cursor === 1,
      model.insideTab && model.cursor === 1 && model.fieldOpen,
    );
  } else if (legacy) {
    drawLegacyChannel(ink, model);
  } else if (model.tab !== TAB_COUNT - 1) {
    drawLabelledField(ink, COL_LEFT_X, ROW_A_BOX_Y, "LEN", writeUnsigned(model.length),
      model.insideTab && model.cursor === 1,
      model.insideTab && model.cursor === 1 && model.fieldOpen);
    drawLabelledField(ink, COL_RIGHT_X, ROW_A_BOX_Y, "SUB", subdivLabel(model.subdiv),
      model.insideTab && model.cursor === 2,
      model.insideTab && model.cursor === 2 && model.fieldOpen);
    drawLabelledField(ink, COL_LEFT_X, ROW_B_BOX_Y, "SEP", barLabel(model.barLength),
      model.insideTab && model.cursor === 3,
      model.insideTab && model.cursor === 3 && model.fieldOpen);
    drawLabelledField(ink, COL_RIGHT_X, ROW_B_BOX_Y, "EDIT", null,
      model.insideTab && model.cursor === 4, false);
  }

  ink.drawHLine(RULE_X, RULE_Y, RULE_W);

  for (let tab = 0; tab < TAB_COUNT; ++tab) {
    const selected = tab === model.tab;
    if (selected) {
      ink.drawBox(tabSlotX(tab), TAB_BOX_Y, TAB_SLOT_W, TAB_BOX_H);
      ink.setDrawColor(0);
    }
    const cx = tabCentreX(tab);
    if (tab === 0) drawClockGlyph(ink, cx, TAB_TOP_Y + 3);
    else if (tab === TAB_COUNT - 1) drawSettingsGlyph(ink, cx, TAB_TOP_Y + 3);
    else ink.drawStr(cx - 2, TAB_BASELINE_Y, String(tab), VELVETSCREEN);
    if (selected) ink.setDrawColor(1);
  }

  return { pixels: ink.pixels(), count: ink.count(), rows: ink.rowCounts() };
}

export { STK_L_HEIGHT, VELVETSCREEN_HEIGHT };
