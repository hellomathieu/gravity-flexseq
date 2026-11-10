import { describe, expect, it } from "vitest";
import {
  stepCenters,
  drawOled,
  GRID_BOTTOM_Y,
  OLED_W,
  type OledCtx,
} from "../src/sim/OledDisplay.js";
import { RATCHET_NONE, RATCHET_2, RATCHET_3, RATCHET_TRIPLET } from "../src/domain/Pattern.js";

/**
 * Mock ctx modelisant vraiment le 1-bit : l'encre ajoute le pixel, la couleur
 * papier l'EFFACE (c'est ainsi que le step joue creuse le centre du glyphe).
 */
function recorder() {
  const painted = new Set<string>();
  const ctx: OledCtx = {
    fillStyle: "",
    fillRect(x: number, y: number, w: number, h: number) {
      const ink = ctx.fillStyle === "#111";
      for (let dx = 0; dx < w; ++dx) {
        for (let dy = 0; dy < h; ++dy) {
          const key = `${x + dx},${y + dy}`;
          if (ink) painted.add(key);
          else painted.delete(key);
        }
      }
    },
  };
  return { ctx, painted };
}

type Kind = "active" | "inactive" | "beyond";
const cell = (index: number, kind: Kind, ratchet = RATCHET_NONE) => ({ index, kind, ratchet });

const C = stepCenters();

describe("OledDisplay — geometry (sketch.ino)", () => {
  it("has 36 centers on 3 rows of 12", () => {
    expect(C).toHaveLength(36);
    for (let i = 0; i < 12; ++i) expect(C[i]!.y).toBe(C[0]!.y);
    for (let i = 12; i < 24; ++i) expect(C[i]!.y).toBe(C[12]!.y);
    for (let i = 24; i < 36; ++i) expect(C[i]!.y).toBe(C[24]!.y);
    expect(C[12]!.y).toBeGreaterThan(C[0]!.y);
    expect(C[24]!.y).toBeGreaterThan(C[12]!.y);
  });

  // 18, 36 et 54 en toutes lettres : une assertion qui lirait ROW_CY suivrait la
  // constante et ne prouverait rien de sa valeur.
  it("places the three row centres at 18, 36 and 54", () => {
    expect(C[0]!.y).toBe(18);
    expect(C[12]!.y).toBe(36);
    expect(C[24]!.y).toBe(54);
    expect(C[11]!.y).toBe(18);
    expect(C[23]!.y).toBe(36);
    expect(C[35]!.y).toBe(54);
  });

  it("ends the grid on the last pixel row", () => {
    expect(GRID_BOTTOM_Y).toBe(63);
  });

  it("uses a 10 px pitch, centered, inside 128 px", () => {
    expect(C[1]!.x - C[0]!.x).toBe(10);
    expect(C[0]!.x).toBeGreaterThanOrEqual(0);
    expect(C[11]!.x).toBeLessThanOrEqual(OLED_W);
    // the same column grid on both rows
    for (let col = 0; col < 12; ++col) expect(C[col]!.x).toBe(C[col + 12]!.x);
  });
});

describe("OledDisplay — step glyphs", () => {
  const has = (p: Set<string>, x: number, y: number) => p.has(`${x},${y}`);

  it("draws nothing below the grid", () => {
    const { ctx, painted } = recorder();
    drawOled(ctx, { title: "", ticksPerStep: 96, cells: [], cursor: -1, playhead: -1 });
    for (const key of painted) {
      expect(Number(key.split(",")[1])).toBeLessThanOrEqual(GRID_BOTTOM_Y);
    }
  });

  it("draws a filled disc for an active step", () => {
    const { ctx, painted } = recorder();
    drawOled(ctx, { title: "", ticksPerStep: 96, cells: [cell(0, "active")], cursor: -1, playhead: -1 });
    const { x, y } = C[0]!;
    expect(has(painted, x, y)).toBe(true); // centre plein
    expect(has(painted, x - 2, y)).toBe(true); // 5 px de large
    expect(has(painted, x + 2, y)).toBe(true);
  });

  it("draws a hollow ring for an inactive step", () => {
    const { ctx, painted } = recorder();
    drawOled(ctx, { title: "", ticksPerStep: 96, cells: [cell(0, "inactive")], cursor: -1, playhead: -1 });
    const { x, y } = C[0]!;
    expect(has(painted, x, y)).toBe(false); // centre vide
    expect(has(painted, x - 2, y)).toBe(true); // bords presents
    expect(has(painted, x + 2, y)).toBe(true);
  });

  it("draws a single dot beyond LENGTH", () => {
    const { ctx, painted } = recorder();
    drawOled(ctx, { title: "", ticksPerStep: 96, cells: [cell(0, "beyond")], cursor: -1, playhead: -1 });
    const { x, y } = C[0]!;
    expect(has(painted, x, y)).toBe(true);
    expect(has(painted, x - 2, y)).toBe(false); // rien d'autre
    expect(has(painted, x + 2, y)).toBe(false);
  });

  it("draws a triangle for a TRIPLET step instead of a disc", () => {
    const tri = recorder();
    drawOled(tri.ctx, {
      title: "", ticksPerStep: 96,
      cells: [cell(0, "active", RATCHET_TRIPLET)],
      cursor: -1,
      playhead: -1,
    });
    const { x, y } = C[0]!;
    // apex : un seul pixel en haut, base large en bas
    expect(tri.painted.has(`${x},${y - 2}`)).toBe(true);
    expect(tri.painted.has(`${x - 2},${y - 2}`)).toBe(false);
    expect(tri.painted.has(`${x - 2},${y + 2}`)).toBe(true);
  });

  it("draws a hollow triangle for an inactive TRIPLET step", () => {
    const tri = recorder();
    drawOled(tri.ctx, {
      title: "", ticksPerStep: 96,
      cells: [cell(0, "inactive", RATCHET_TRIPLET)],
      cursor: -1,
      playhead: -1,
    });
    const { x, y } = C[0]!;
    const rows = ["..#..", ".#.#.", ".#.#.", "#...#", "#####"];
    for (let r = 0; r < 5; ++r) {
      for (let c = 0; c < 5; ++c) {
        expect(tri.painted.has(`${x - 2 + c},${y - 2 + r}`), `row ${r} col ${c}`).toBe(rows[r]![c] === "#");
      }
    }
  });

  const digitInk = (p: Set<string>) => {
    let n = 0;
    const { x, y } = C[0]!;
    for (let r = 0; r < 5; ++r) for (let c = 0; c < 3; ++c) if (p.has(`${x - 1 + c},${y + 5 + r}`)) n++;
    return n;
  };

  it("draws no ratchet digit under an inactive step", () => {
    const r = recorder();
    drawOled(r.ctx, { title: "", ticksPerStep: 96, cells: [cell(0, "inactive", RATCHET_2)], cursor: -1, playhead: -1 });
    expect(digitInk(r.painted)).toBe(0);
    const on = recorder();
    drawOled(on.ctx, { title: "", ticksPerStep: 96, cells: [cell(0, "active", RATCHET_2)], cursor: -1, playhead: -1 });
    expect(digitInk(on.painted)).toBe(11);
  });

  it("draws no ratchet digit when the rate of the channel refuses the ratchet", () => {
    const refused = recorder();
    drawOled(refused.ctx, { title: "", ticksPerStep: 4, cells: [cell(0, "active", RATCHET_3)], cursor: -1, playhead: -1 });
    expect(digitInk(refused.painted)).toBe(0);
    const accepted = recorder();
    drawOled(accepted.ctx, { title: "", ticksPerStep: 6, cells: [cell(0, "active", RATCHET_3)], cursor: -1, playhead: -1 });
    expect(digitInk(accepted.painted)).toBe(11);
  });

  it("prints the ratchet digit under the step (but not for the triplet)", () => {
    const withDigit = recorder();
    drawOled(withDigit.ctx, {
      title: "", ticksPerStep: 96,
      cells: [cell(0, "active", RATCHET_3)],
      cursor: -1,
      playhead: -1,
    });
    const plain = recorder();
    drawOled(plain.ctx, { title: "", ticksPerStep: 96, cells: [cell(0, "active")], cursor: -1, playhead: -1 });

    const below = (p: Set<string>) => {
      let n = 0;
      const { x, y } = C[0]!;
      for (let dx = -3; dx <= 3; ++dx)
        for (let dy = 4; dy <= 10; ++dy) if (p.has(`${x + dx},${y + dy}`)) n++;
      return n;
    };
    expect(below(withDigit.painted)).toBeGreaterThan(below(plain.painted));
  });
});

describe("OledDisplay — measure separation (graphical only)", () => {
  const gutter = (k: number) => C[k]!.x - 5;
  const hasBar = (p: Set<string>, k: number) =>
    p.has(`${gutter(k)},${C[k]!.y}`) && p.has(`${gutter(k)},${C[k]!.y - 4}`);

  it("draws a bar every N steps, inside each row", () => {
    const { ctx, painted } = recorder();
    drawOled(ctx, { title: "", ticksPerStep: 96, cells: [], cursor: -1, playhead: -1, barLength: 4 });
    expect(hasBar(painted, 4)).toBe(true);
    expect(hasBar(painted, 8)).toBe(true);
    expect(hasBar(painted, 16)).toBe(true);
    expect(hasBar(painted, 20)).toBe(true);
  });

  it("never draws a bar at a row edge", () => {
    const { ctx, painted } = recorder();
    drawOled(ctx, { title: "", ticksPerStep: 96, cells: [], cursor: -1, playhead: -1, barLength: 4 });
    expect(hasBar(painted, 12)).toBe(false); // debut de la 2e ligne
  });

  it("draws no bar when the separation is none", () => {
    const { ctx, painted } = recorder();
    drawOled(ctx, { title: "", ticksPerStep: 96, cells: [], cursor: -1, playhead: -1, barLength: 0 });
    for (let k = 1; k < 24; ++k) expect(hasBar(painted, k), `k=${k}`).toBe(false);
  });

  it("separations of 2, 3 and 6 all land inside the rows", () => {
    for (const n of [2, 3, 6]) {
      const { ctx, painted } = recorder();
      drawOled(ctx, { title: "", ticksPerStep: 96, cells: [], cursor: -1, playhead: -1, barLength: n });
      expect(hasBar(painted, n), `bar ${n}`).toBe(true);
      expect(hasBar(painted, 12), `bar ${n} at row wrap`).toBe(false);
    }
  });
});

describe("OledDisplay — cursor & playhead", () => {
  it("frames the edited step with a 9x9 box", () => {
    const { ctx, painted } = recorder();
    drawOled(ctx, { title: "", ticksPerStep: 96, cells: [], cursor: 5, playhead: -1 });
    const { x, y } = C[5]!;
    expect(painted.has(`${x - 4},${y - 4}`)).toBe(true);
    expect(painted.has(`${x + 4},${y - 4}`)).toBe(true);
    expect(painted.has(`${x - 4},${y + 4}`)).toBe(true);
  });

  it("marks the played step by clearing the centre of an active glyph", () => {
    const lit = recorder();
    drawOled(lit.ctx, { title: "", ticksPerStep: 96, cells: [cell(3, "active")], cursor: -1, playhead: 3 });
    const { x, y } = C[3]!;
    // le pixel central est efface (dessine en couleur papier)
    expect(lit.painted.has(`${x},${y}`)).toBe(false);
    expect(lit.painted.has(`${x - 2},${y}`)).toBe(true); // le reste du disque demeure
  });

  it("marks the played step by inking the centre of an inactive glyph", () => {
    const { ctx, painted } = recorder();
    drawOled(ctx, { title: "", ticksPerStep: 96, cells: [cell(3, "inactive")], cursor: -1, playhead: 3 });
    const { x, y } = C[3]!;
    expect(painted.has(`${x},${y}`)).toBe(true);
  });
});
