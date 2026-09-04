export {
  MainParameter,
  mainScreenModelOf,
  type MainScreenModel,
} from "../domain/MainScreenModel.js";
import type { MainScreenModel } from "../domain/MainScreenModel.js";
import { GLYPH_HEIGHT, textPixels, textWidth } from "./oledFont.js";
import { INK, OLED_H, OLED_W, PAPER, type OledCtx } from "./OledDisplay.js";

export const TAB_COUNT = 8;
export const TAB_SLOT_W = OLED_W / TAB_COUNT;
export const TAB_BASELINE_Y = OLED_H - 1;
export const TAB_TOP_Y = TAB_BASELINE_Y - (GLYPH_HEIGHT - 1);
export const TAB_BOX_Y = OLED_H - 8;
export const TAB_BOX_H = 8;

export const RULE_Y = 52;
export const RULE_X = 4;
export const RULE_W = 120;

export const HEADLINE_BOX_X = 2;
export const HEADLINE_BOX_Y = 1;
export const HEADLINE_BOX_W = OLED_W - 2 * HEADLINE_BOX_X;
export const HEADLINE_BOX_H = 10;
export const HEADLINE_SCALE = 1;

export const ROW_A_BOX_Y = 14;
export const ROW_B_BOX_Y = 22;
export const ROW_BOX_H = 8;
export const COL_LEFT_X = 2;
export const COL_RIGHT_X = 66;
export const COL_W = 60;
export const TEXT_INSET = 2;

export const CLOCK_SOURCE_LABELS = ["INT", "EXT24", "EXT4", "EXT2", "EXT1", "MIDI"] as const;


export function tabSlotX(tab: number): number {
  return tab * TAB_SLOT_W;
}

export function tabCentreX(tab: number): number {
  return tabSlotX(tab) + TAB_SLOT_W / 2;
}

export function patternName(index: number): string {
  if (index < 0) return "--";
  return (index < 8 ? "A" : "B") + String((index % 8) + 1);
}

export function subdivLabel(subdiv: number): string {
  if (subdiv === 0) return "?";
  return subdiv < 0 ? `x${-subdiv}` : `/${subdiv}`;
}

export function barLabel(steps: number): string {
  return steps === 0 ? "-" : String(steps);
}

export function sourceLabel(source: number): string {
  return CLOCK_SOURCE_LABELS[source] ?? "MIDI";
}

export function headlineOf(model: MainScreenModel): string {
  if (model.tab === 0) return String(model.tempo);
  if (model.tab === TAB_COUNT - 1) return "";
  return patternName(model.patternIndex);
}

function px(ctx: OledCtx, x: number, y: number): void {
  ctx.fillRect(x, y, 1, 1);
}

function hline(ctx: OledCtx, x: number, y: number, w: number): void {
  ctx.fillRect(x, y, w, 1);
}

function frame(ctx: OledCtx, x: number, y: number, w: number, h: number): void {
  hline(ctx, x, y, w);
  hline(ctx, x, y + h - 1, w);
  ctx.fillRect(x, y, 1, h);
  ctx.fillRect(x + w - 1, y, 1, h);
}

function blit(ctx: OledCtx, text: string, x: number, top: number, scale = 1): void {
  for (const p of textPixels(text)) {
    ctx.fillRect(x + p.x * scale, top + p.y * scale, scale, scale);
  }
}

function labelledField(
  ctx: OledCtx,
  x: number,
  y: number,
  label: string,
  value: string | null,
  framed: boolean,
  inverted: boolean,
): void {
  if (inverted) {
    ctx.fillRect(x, y, COL_W, ROW_BOX_H);
    ctx.fillStyle = PAPER;
  } else if (framed) {
    frame(ctx, x, y, COL_W, ROW_BOX_H);
  }
  const textX = x + TEXT_INSET;
  const top = y + 1;
  blit(ctx, label, textX, top);
  if (value !== null) {
    blit(ctx, value, textX + textWidth(label) + 4, top);
  }
  if (inverted) {
    ctx.fillStyle = INK;
  }
}

export function drawMainScreenOled(ctx: OledCtx, model: MainScreenModel): void {
  ctx.fillStyle = PAPER;
  ctx.fillRect(0, 0, OLED_W, OLED_H);
  ctx.fillStyle = INK;

  const headline = headlineOf(model);
  if (headline !== "") {
    const w = textWidth(headline) * HEADLINE_SCALE;
    const top = HEADLINE_BOX_Y + Math.floor((HEADLINE_BOX_H - GLYPH_HEIGHT * HEADLINE_SCALE) / 2);
    blit(ctx, headline, Math.round((OLED_W - w) / 2), top, HEADLINE_SCALE);
  }
  if (model.insideTab && model.cursor === 0) {
    frame(ctx, HEADLINE_BOX_X, HEADLINE_BOX_Y, HEADLINE_BOX_W, HEADLINE_BOX_H);
    if (model.fieldOpen) {
      frame(ctx, HEADLINE_BOX_X + 1, HEADLINE_BOX_Y + 1, HEADLINE_BOX_W - 2, HEADLINE_BOX_H - 2);
    }
  }

  const onField = (index: number) => model.insideTab && model.cursor === index;

  if (model.tab === 0) {
    labelledField(ctx, COL_LEFT_X, ROW_A_BOX_Y, "SRC", sourceLabel(model.clockSource),
                  onField(1), onField(1) && model.fieldOpen);
  } else if (model.tab !== TAB_COUNT - 1) {
    labelledField(ctx, COL_LEFT_X, ROW_A_BOX_Y, "LEN", String(model.length),
                  onField(1), onField(1) && model.fieldOpen);
    labelledField(ctx, COL_RIGHT_X, ROW_A_BOX_Y, "SUB", subdivLabel(model.subdiv),
                  onField(2), onField(2) && model.fieldOpen);
    labelledField(ctx, COL_LEFT_X, ROW_B_BOX_Y, "SEP", barLabel(model.barLength),
                  onField(3), onField(3) && model.fieldOpen);
    labelledField(ctx, COL_RIGHT_X, ROW_B_BOX_Y, "EDIT", null, onField(4), false);
  }

  hline(ctx, RULE_X, RULE_Y, RULE_W);

  for (let tab = 0; tab < TAB_COUNT; ++tab) {
    const selected = tab === model.tab;
    if (selected) {
      ctx.fillRect(tabSlotX(tab), TAB_BOX_Y, TAB_SLOT_W, TAB_BOX_H);
      ctx.fillStyle = PAPER;
    }
    const cx = tabCentreX(tab);
    if (tab === 0) {
      frame(ctx, cx - 3, TAB_TOP_Y, 7, 7);
      hline(ctx, cx + 1, TAB_TOP_Y + 2, 2);
      hline(ctx, cx + 1, TAB_TOP_Y + 3, 2);
      px(ctx, cx, TAB_TOP_Y + 2);
    } else if (tab === TAB_COUNT - 1) {
      ctx.fillRect(cx - 2, TAB_TOP_Y + 1, 5, 5);
    } else {
      blit(ctx, String(tab), cx - 2, TAB_TOP_Y);
    }
    if (selected) {
      ctx.fillStyle = INK;
    }
  }
}
