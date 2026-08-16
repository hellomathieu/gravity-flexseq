import { describe, expect, it } from "vitest";
import { stepCenters, OLED_W } from "../src/sim/OledDisplay.js";

describe("OledDisplay — stepCenters", () => {
  it("produces exactly 24 centers", () => {
    expect(stepCenters()).toHaveLength(24);
  });

  it("splits into two rows of 12 at distinct y", () => {
    const c = stepCenters();
    const row0Y = c[0]!.y;
    const row1Y = c[12]!.y;
    expect(row1Y).toBeGreaterThan(row0Y);
    for (let i = 0; i < 12; ++i) expect(c[i]!.y).toBe(row0Y);
    for (let i = 12; i < 24; ++i) expect(c[i]!.y).toBe(row1Y);
  });

  it("keeps all centers within the 128px width and left-to-right", () => {
    const c = stepCenters();
    for (const p of c) {
      expect(p.x).toBeGreaterThanOrEqual(0);
      expect(p.x).toBeLessThanOrEqual(OLED_W);
    }
    // colonnes croissantes sur chaque ligne
    for (let i = 1; i < 12; ++i) expect(c[i]!.x).toBeGreaterThan(c[i - 1]!.x);
    // meme grille de colonnes sur les deux lignes
    for (let col = 0; col < 12; ++col) expect(c[col]!.x).toBe(c[col + 12]!.x);
  });
});
