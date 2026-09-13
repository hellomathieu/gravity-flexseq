import { describe, expect, it } from "vitest";
import {
  GRID_STEPS,
  HEADER_LINE_W,
  HEADER_LINE_Y,
  GRID_BOTTOM_Y,
  colX,
  rowCY,
  renderPatternScreen,
  SEP_FRAME_PAD,
  SEP_LABEL_X,
  LEN_LABEL_X,
  TITLE_BASELINE_Y,
  SEP_VALUE_X,
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
    sepSelected: false,
    sepOpen: false,
  };
}

/**
 * L encre que le PANNEAU recoit, relevee par `tools/run-screen-dump.sh` sur
 * `env:wokwi`, remise en coordonnees logiques : `y logique = 63 - y panneau`.
 */
const PANEL_ROWS: ReadonlyArray<readonly [number, number]> = [
  [2, 47], [3, 27], [4, 43], [5, 24], [6, 38], [10, 120], [14, 3], [15, 3],
  [16, 12], [17, 5], [18, 41], [19, 47], [20, 46], [21, 47], [22, 41],
  [23, 5], [24, 12], [25, 12], [26, 6], [27, 9], [28, 4], [29, 9], [31, 3],
  [32, 3], [33, 3], [34, 3], [35, 25], [36, 29], [37, 33], [38, 31],
  [39, 29], [40, 3], [41, 3], [42, 5], [43, 5], [44, 3], [45, 1], [46, 1],
  [48, 3], [49, 3], [50, 3], [51, 3], [52, 3], [53, 3], [54, 15], [55, 3],
  [56, 3], [57, 3], [58, 3], [59, 3], [60, 3],
];

const PANEL_INK = 832;

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
    const centres = [20, 37, 54];
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

describe("le cadre de SEP en edition, avec la vraie police", () => {
  const on = (px: Set<string>, x: number, y: number) => px.has(`${x},${y}`);

  function opened(barLength: number): Set<string> {
    return renderPatternScreen({
      ...panelModel(),
      barLength,
      sepSelected: true,
      sepOpen: true,
    }).pixels;
  }

  it("degage la valeur d un pixel de chaque cote", () => {
    expect(SEP_FRAME_PAD).toBe(2);
    expect(SEP_VALUE_X).toBe(120);
    const px = opened(3);
    expect(on(px, 118, 1), "colonne gauche du cadre").toBe(true);
    expect(on(px, 125, 1), "colonne droite du cadre").toBe(true);
    expect(on(px, 118, 7), "le cadre ferme en bas a gauche").toBe(true);
    expect(on(px, 125, 7), "le cadre ferme en bas a droite").toBe(true);
    for (let y = 2; y <= 6; ++y) {
      expect(on(px, 119, y), `degagement gauche en y=${y}`).toBe(false);
      expect(on(px, 124, y), `degagement droite en y=${y}`).toBe(false);
    }
  });

  it("degage les trois valeurs que SEP peut prendre", () => {
    for (const bar of [2, 3, 4, 6]) {
      const px = opened(bar);
      for (let y = 2; y <= 6; ++y) {
        expect(on(px, 119, y), `SEP ${bar}, degagement gauche en y=${y}`).toBe(false);
        expect(on(px, 124, y), `SEP ${bar}, degagement droite en y=${y}`).toBe(false);
      }
    }
  });

  it("ne sort ni de la bande 0 ni de l ecran", () => {
    const px = opened(3);
    for (let x = 118; x < 128; ++x) {
      expect(on(px, x, 0), `debordement vers le haut en x=${x}`).toBe(false);
      expect(on(px, x, 8), `debordement en bande 1 en x=${x}`).toBe(false);
    }
  });
});

/**
 * L encre que le PANNEAU recoit pour l EDITEUR DE TEMPLATES, relevee le
 * 2026-09-13 par `PLATFORMIO_BUILD_FLAGS="-DFLEXSEQ_DEMO_TEMPLATE_EDITOR=1"
 * ASCII=1 ./tools/run-screen-dump.sh`, remise en coordonnees logiques. Meme
 * contenu que l image de reference, titre et en-tete changes.
 */
const TEMPLATE_PANEL_ROWS: ReadonlyArray<readonly [number, number]> = [
  [2, 40], [3, 25], [4, 38], [5, 22], [6, 41], [10, 120], [14, 3], [15, 3],
  [16, 12], [17, 5], [18, 41], [19, 47], [20, 46], [21, 47], [22, 41],
  [23, 5], [24, 12], [25, 12], [26, 6], [27, 9], [28, 4], [29, 9], [31, 3],
  [32, 3], [33, 3], [34, 3], [35, 25], [36, 29], [37, 33], [38, 31],
  [39, 29], [40, 3], [41, 3], [42, 5], [43, 5], [44, 3], [45, 1], [46, 1],
  [48, 3], [49, 3], [50, 3], [51, 3], [52, 3], [53, 3], [54, 15], [55, 3],
  [56, 3], [57, 3], [58, 3], [59, 3], [60, 3]
];

const TEMPLATE_PANEL_INK = 819;

describe("l en-tete de l editeur de templates — lot 16E etape 4d", () => {
  // Le titre est retire : l en-tete ne porte alors que l etiquette et sa valeur,
  // donc le premier pixel encre de la bande DIT ou l etiquette commence.
  const headerModel = (templateEditor: boolean): PatternScreenPixelModel => ({
    ...panelModel(-1),
    title: null,
    cursor: -1,
    length: 16,
    templateEditor,
  });

  const firstInkX = (model: PatternScreenPixelModel): number => {
    const { pixels } = renderPatternScreen(model);
    let min = 128;
    for (const key of pixels) {
      const parts = key.split(",");
      const x = Number(parts[0]);
      const y = Number(parts[1]);
      if (y <= TITLE_BASELINE_Y && x < min) min = x;
    }
    return min;
  };

  it("place l etiquette LEN a sa propre position, plus a gauche que SEP", () => {
    expect(firstInkX(headerModel(true))).toBe(LEN_LABEL_X);
    expect(firstInkX(headerModel(false))).toBe(SEP_LABEL_X);
    expect(LEN_LABEL_X).toBeLessThan(SEP_LABEL_X);
  });

  it("rend exactement ce que le panneau a recu, rangee par rangee", () => {
    const model: PatternScreenPixelModel = {
      // playhead 0 comme le releve de reference : la capture du panneau a lieu
      // avec le playhead sur un pas ACTIF, ou son marqueur est absorbe.
      ...panelModel(0),
      title: "TEMPLATE B3",
      templateEditor: true,
    };
    const rows = renderPatternScreen(model).rows;
    const expected = new Map<number, number>(
      TEMPLATE_PANEL_ROWS.map(([y, n]) => [y, n]),
    );
    const divergent: string[] = [];
    for (let y = 0; y < 64; y += 1) {
      const mine = rows[y] ?? 0;
      const theirs = expected.get(y) ?? 0;
      if (mine !== theirs) divergent.push(`y=${y} miroir=${mine} panneau=${theirs}`);
    }
    expect(divergent.join(" | ")).toBe("");
    expect(renderPatternScreen(model).count).toBe(TEMPLATE_PANEL_INK);
  });

  it("ne rend pas la meme chose qu un en-tete de canal", () => {
    expect(renderPatternScreen(headerModel(true)).count)
      .not.toBe(renderPatternScreen(headerModel(false)).count);
  });
});
