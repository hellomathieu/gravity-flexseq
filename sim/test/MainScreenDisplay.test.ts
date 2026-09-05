import { describe, expect, it } from "vitest";
import {
  COL_LEFT_X,
  COL_RIGHT_X,
  HEADLINE_BOX_H,
  HEADLINE_BOX_Y,
  RULE_X,
  RULE_Y,
  ROW_A_BOX_Y,
  ROW_B_BOX_Y,
  ROW_BOX_H,
  TAB_BOX_Y,
  TAB_COUNT,
  TAB_SLOT_W,
  drawMainScreenOled,
  headlineOf,
  patternName,
  sourceLabel,
  subdivLabel,
  barLabel,
  tabCentreX,
  tabSlotX,
  MainParameter,
  type MainScreenModel,
} from "../src/sim/MainScreenDisplay.js";
import { OLED_H, OLED_W, type OledCtx } from "../src/sim/OledDisplay.js";
import { CHANNEL_TAB_FIELDS } from "../src/domain/UiController.js";
import { ChannelMode } from "../src/domain/SequencerEngine.js";

function recorder() {
  const painted = new Set<string>();
  let style = "#111";
  const ctx: OledCtx = {
    get fillStyle() {
      return style;
    },
    set fillStyle(v: string) {
      style = v;
    },
    fillRect(x: number, y: number, w: number, h: number) {
      for (let dy = 0; dy < h; ++dy) {
        for (let dx = 0; dx < w; ++dx) {
          const px = x + dx;
          const py = y + dy;
          if (px < 0 || py < 0 || px >= OLED_W || py >= OLED_H) continue;
          if (style === "#111") painted.add(`${px},${py}`);
          else painted.delete(`${px},${py}`);
        }
      }
    },
  } as OledCtx;
  return { ctx, painted };
}

function channelTab(tab = 1): MainScreenModel {
  return {
    tab,
    insideTab: false,
    cursor: 0,
    fieldOpen: false,
    fieldCount: CHANNEL_TAB_FIELDS,
    patternIndex: 0,
    length: 16,
    subdiv: 1,
    barLength: 4,
    mode: ChannelMode.SEQ,
    offset: 0,
    skipChance: 0,
    stepTicks: 96,
    mainParameter: MainParameter.Pattern,
    cv1Target: 0,
    cv2Target: 0,
    configPage: false,
    tempo: 120,
    clockSource: 0,
  };
}

const inkInRows = (painted: Set<string>, y0: number, y1: number) => {
  let n = 0;
  for (const key of painted) {
    const y = Number(key.split(",")[1]);
    if (y >= y0 && y <= y1) ++n;
  }
  return n;
};

describe("MainScreenDisplay — labels, mirrored from the C++ renderer", () => {
  it("names the sixteen patterns exactly as the firmware does", () => {
    expect(patternName(0)).toBe("A1");
    expect(patternName(7)).toBe("A8");
    expect(patternName(8)).toBe("B1");
    expect(patternName(15)).toBe("B8");
    expect(patternName(-1)).toBe("--");
    const all = new Set(Array.from({ length: 16 }, (_, i) => patternName(i)));
    expect(all.size).toBe(16);
  });

  it("shows SUBDIV the Gravity way", () => {
    expect(subdivLabel(1)).toBe("/1");
    expect(subdivLabel(128)).toBe("/128");
    expect(subdivLabel(-2)).toBe("x2");
    expect(subdivLabel(-24)).toBe("x24");
  });

  it("shows a separation of none as a dash", () => {
    expect(barLabel(0)).toBe("-");
    expect(barLabel(6)).toBe("6");
  });

  it("gives the six clock sources distinct labels", () => {
    const labels = new Set(Array.from({ length: 6 }, (_, i) => sourceLabel(i)));
    expect(labels.size).toBe(6);
    expect(sourceLabel(0)).toBe("INT");
    expect(sourceLabel(5)).toBe("MIDI");
  });

  it("makes the headline the pattern name, the tempo, or nothing", () => {
    expect(headlineOf(channelTab(1))).toBe("A1");
    expect(headlineOf({ ...channelTab(0), tempo: 240 })).toBe("240");
    expect(headlineOf(channelTab(TAB_COUNT - 1))).toBe("");
  });
});

describe("MainScreenDisplay — geometry", () => {
  it("has eight evenly spaced tab slots", () => {
    expect(TAB_SLOT_W).toBe(16);
    expect(tabCentreX(0)).toBe(8);
    expect(tabCentreX(7)).toBe(120);
    for (let tab = 1; tab < TAB_COUNT; ++tab) {
      expect(tabCentreX(tab) - tabCentreX(tab - 1)).toBe(16);
    }
  });

  it("inverts the selected tab and only that one", () => {
    const { ctx, painted } = recorder();
    drawMainScreenOled(ctx, channelTab(3));
    expect(painted.has(`${tabSlotX(3)},${TAB_BOX_Y}`)).toBe(true);
    expect(painted.has(`${tabSlotX(3) + TAB_SLOT_W - 1},${TAB_BOX_Y}`)).toBe(true);
    expect(painted.has(`${tabSlotX(4) + 1},${TAB_BOX_Y}`)).toBe(false);
  });

  it("draws the rule and keeps the two field rows above it", () => {
    const { ctx, painted } = recorder();
    drawMainScreenOled(ctx, channelTab());
    expect(painted.has(`${RULE_X},${RULE_Y}`)).toBe(true);
    expect(ROW_A_BOX_Y + ROW_BOX_H).toBeLessThanOrEqual(ROW_B_BOX_Y);
    expect(ROW_B_BOX_Y + ROW_BOX_H).toBeLessThanOrEqual(RULE_Y);
    expect(HEADLINE_BOX_Y + HEADLINE_BOX_H).toBeLessThanOrEqual(ROW_A_BOX_Y);
  });

  it("puts the two columns side by side without overlapping", () => {
    expect(COL_LEFT_X).toBeLessThan(COL_RIGHT_X);
    const { ctx, painted } = recorder();
    drawMainScreenOled(ctx, channelTab());
    expect(inkInRows(painted, ROW_A_BOX_Y, ROW_A_BOX_Y + ROW_BOX_H - 1)).toBeGreaterThan(0);
    expect(inkInRows(painted, ROW_B_BOX_Y, ROW_B_BOX_Y + ROW_BOX_H - 1)).toBeGreaterThan(0);
  });

  it("draws the headline in the same single font as everything else", () => {
    const r = recorder();
    drawMainScreenOled(r.ctx, channelTab());
    expect(inkInRows(r.painted, HEADLINE_BOX_Y, HEADLINE_BOX_Y + HEADLINE_BOX_H - 1))
      .toBeGreaterThan(0);
  });

  it("leaves room below the two rows for the CV fields of PRD 10.2", () => {
    const r = recorder();
    drawMainScreenOled(r.ctx, channelTab());
    const top = ROW_B_BOX_Y + ROW_BOX_H;
    expect(RULE_Y - top).toBeGreaterThanOrEqual(2 * ROW_BOX_H);
    expect(inkInRows(r.painted, top, RULE_Y - 1)).toBe(0);
  });

  it("shows nothing in the content area of the deferred settings tab", () => {
    const { ctx, painted } = recorder();
    drawMainScreenOled(ctx, channelTab(TAB_COUNT - 1));
    expect(inkInRows(painted, HEADLINE_BOX_Y, ROW_B_BOX_Y + ROW_BOX_H - 1)).toBe(0);
    expect(painted.has(`${RULE_X},${RULE_Y}`)).toBe(true);
    expect(inkInRows(painted, TAB_BOX_Y, OLED_H - 1)).toBeGreaterThan(0);
  });

  it("draws no cursor while on the tab bar", () => {
    const bar = recorder();
    drawMainScreenOled(bar.ctx, { ...channelTab(), insideTab: false, cursor: 1 });
    const inside = recorder();
    drawMainScreenOled(inside.ctx, { ...channelTab(), insideTab: true, cursor: 1 });
    const rows = (r: { painted: Set<string> }) =>
      inkInRows(r.painted, ROW_A_BOX_Y, ROW_A_BOX_Y + ROW_BOX_H - 1);
    expect(rows(inside)).toBeGreaterThan(rows(bar));
  });

  it("inks an open field more than a merely framed one", () => {
    const framed = recorder();
    drawMainScreenOled(framed.ctx, { ...channelTab(), insideTab: true, cursor: 1 });
    const open = recorder();
    drawMainScreenOled(open.ctx, {
      ...channelTab(),
      insideTab: true,
      cursor: 1,
      fieldOpen: true,
    });
    const rows = (r: { painted: Set<string> }) =>
      inkInRows(r.painted, ROW_A_BOX_Y, ROW_A_BOX_Y + ROW_BOX_H - 1);
    expect(rows(open)).toBeGreaterThan(rows(framed));
  });
});
