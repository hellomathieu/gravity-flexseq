import { describe, expect, it } from "vitest";
import {
  GRID_STEPS,
  HEADER_LINE_W,
  HEADER_LINE_Y,
  GRID_BOTTOM_Y,
  colX,
  rowCY,
  renderPatternScreen,
  type PatternScreenPixelModel,
} from "../src/sim/PatternScreenPixels.js";
import {
  RATCHET_2,
  RATCHET_3,
  RATCHET_4,
  RATCHET_6,
  RATCHET_NONE,
  RATCHET_TRIPLET,
} from "../src/domain/Pattern.js";

/**
 * Le modele que `env:wokwi` fige, releve dans `src/wokwi_main.cpp` : dix steps
 * actifs, cinq ratchets dont un triolet, longueur 20 donc les steps 20 a 35 en
 * simple point, curseur sur le step 5, separation de mesure en 3.
 *
 * Le playhead est pose sur le step 0, qui est ACTIF : son pixel central est donc
 * EFFACE, ce qui retire un pixel. C est ainsi que le panneau a ete capture.
 */
function panelModel(playhead = 0): PatternScreenPixelModel {
  const steps = new Array<boolean>(GRID_STEPS).fill(false);
  for (const i of [0, 2, 5, 6, 7, 8, 13, 15, 16, 19]) steps[i] = true;
  const ratchets = new Array<number>(GRID_STEPS).fill(RATCHET_NONE);
  ratchets[2] = RATCHET_2;
  ratchets[6] = RATCHET_6;
  ratchets[8] = RATCHET_3;
  ratchets[15] = RATCHET_TRIPLET;
  ratchets[16] = RATCHET_4;
  return {
    title: "EDIT PATTERN A1",
    steps,
    ratchets,
    length: 20,
    cursor: 5,
    playhead,
    barLength: 3,
  };
}

/**
 * L encre que le PANNEAU recoit, relevee par `tools/run-screen-dump.sh` sur
 * `env:wokwi`, remise en coordonnees logiques : `y logique = 63 - y panneau`.
 */
const PANEL_ROWS: ReadonlyArray<readonly [number, number]> = [
  [2, 34], [3, 21], [4, 32], [5, 19], [6, 27], [10, 120], [12, 3], [13, 3],
  [14, 12], [15, 5], [16, 41], [17, 47], [18, 46], [19, 47], [20, 41],
  [21, 5], [22, 12], [23, 12], [24, 6], [25, 9], [26, 4], [27, 9], [30, 3],
  [31, 3], [32, 3], [33, 3], [34, 25], [35, 29], [36, 33], [37, 31], [38, 29],
  [39, 3], [40, 3], [41, 5], [42, 5], [43, 3], [44, 1], [45, 1], [48, 3],
  [49, 3], [50, 3], [51, 3], [52, 3], [53, 3], [54, 15], [55, 3], [56, 3],
  [57, 3], [58, 3], [59, 3], [60, 3],
];

const PANEL_INK = 786;

describe("l ecran EDIT, confronte au PANNEAU (risque 89)", () => {
  it("rend exactement l encre que le panneau recoit", () => {
    expect(renderPatternScreen(panelModel()).count).toBe(PANEL_INK);
  });

  it("rend la meme encre RANGEE PAR RANGEE", () => {
    const rows = renderPatternScreen(panelModel()).rows;
    const expected = new Map(PANEL_ROWS);
    for (let y = 0; y < rows.length; ++y) {
      expect(rows[y], `rangee ${y}`).toBe(expected.get(y) ?? 0);
    }
  });

  it("le total des rangees attendues vaut l encre attendue", () => {
    expect(PANEL_ROWS.reduce((s, [, n]) => s + n, 0)).toBe(PANEL_INK);
  });

  it("le playhead sur un step actif EFFACE un pixel, sur un step vide il en ajoute", () => {
    const none = renderPatternScreen(panelModel(-1)).count;
    expect(renderPatternScreen(panelModel(0)).count).toBe(none - 1);
    expect(renderPatternScreen(panelModel(1)).count).toBe(none + 1);
  });
});

describe("la grille des 36 steps", () => {
  it("les 36 steps sont a leur place, trois rangees de douze", () => {
    const { pixels } = renderPatternScreen(panelModel(-1));
    const centres = [18, 36, 54];
    for (let i = 0; i < GRID_STEPS; ++i) {
      expect(rowCY(i), `rangee du step ${i}`).toBe(centres[Math.floor(i / 12)]);
      const cx = colX(i);
      let ink = 0;
      for (let dx = -2; dx <= 2; ++dx) {
        for (let dy = -2; dy <= 2; ++dy) {
          if (pixels.has(`${cx + dx},${rowCY(i) + dy}`)) ink += 1;
        }
      }
      expect(ink, `le step ${i} porte de l encre`).toBeGreaterThan(0);
    }
  });

  it("un step au-dela de la longueur porte UN SEUL pixel", () => {
    const { pixels } = renderPatternScreen(panelModel(-1));
    for (const i of [20, 25, 35]) {
      let ink = 0;
      for (let dx = -2; dx <= 2; ++dx) {
        for (let dy = -2; dy <= 2; ++dy) {
          if (pixels.has(`${colX(i) + dx},${rowCY(i) + dy}`)) ink += 1;
        }
      }
      expect(ink, `le step ${i}, au-dela de la longueur`).toBe(1);
    }
  });

  it("la grille finit sur la derniere rangee de l ecran", () => {
    expect(GRID_BOTTOM_Y).toBe(63);
  });

  it("le filet de l en-tete est complet", () => {
    expect(renderPatternScreen(panelModel(-1)).rows[HEADER_LINE_Y]).toBe(HEADER_LINE_W);
  });
});
