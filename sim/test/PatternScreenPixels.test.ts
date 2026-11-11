import { describe, expect, it } from "vitest";
import { textWidth } from "../src/sim/oledFont.js";
import {
  GRID_STEPS,
  HEADER_LINE_W,
  HEADER_LINE_Y,
  GRID_BOTTOM_Y,
  colX,
  rowCY,
  renderPatternScreen,
  COUNTER_MAX_W,
  COUNTER_PAD,
  COUNTER_X,
  SEP_FRAME_PAD,
  SEP_LABEL_X,
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
 * actifs, sept ratchets dont un triolet actif, un triolet inactif au step 10 et
 * un ratchet 4 inactif au step 3, longueur 20 donc les steps 20 a 35 en
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
  ratchets[10] = RATCHET_TRIPLET;
  ratchets[3] = RATCHET_4;
  ratchets[13] = 8;
  ratchets[17] = 10;
  ratchets[19] = 9;
  ratchets[1] = 1;
  ratchets[4] = 12;
  ratchets[9] = 14;
  ratchets[11] = 15;
  return {
    title: "EDIT PAT. A1",
    steps,
    ratchets,
    length: 20,
    cursor: 5,
    playhead,
    barLength: 3,
    sepSelected: false,
    sepOpen: false,
    counterStep: 6,
    baseLength: 20,
    lenSelected: false,
    lenOpen: false,
    ticksPerStep: 96,
  };
}

/**
 * L encre que le PANNEAU recoit, relevee par `tools/run-screen-dump.sh` sur
 * `env:wokwi`, remise en coordonnees logiques : `y logique = 63 - y panneau`.
 */
const PANEL_ROWS: ReadonlyArray<readonly [number, number]> = [
  [2, 42], [3, 25], [4, 40], [5, 23], [6, 39], [10, 120], [14, 3], [15, 3],
  [16, 12], [17, 5], [18, 39], [19, 47], [20, 46], [21, 45], [22, 40],
  [23, 5], [24, 12], [25, 21], [26, 10], [27, 16], [28, 8], [29, 16], [31, 3],
  [32, 3], [33, 3], [34, 3], [35, 23], [36, 23], [37, 27], [38, 25],
  [39, 27], [40, 3], [41, 3], [42, 11], [43, 7], [44, 9], [45, 3], [46, 7],
  [48, 3], [49, 3], [50, 3], [51, 3], [52, 3], [53, 3], [54, 15], [55, 3],
  [56, 3], [57, 3], [58, 3], [59, 3], [60, 3],
];

const PANEL_INK = 848;

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

describe("le rendu d un step, PRD 12.1", () => {
  const FILLED_TRIANGLE = ["..#..", ".###.", ".###.", "#####", "#####"];
  const HOLLOW_TRIANGLE = ["..#..", ".#.#.", ".#.#.", "#...#", "#####"];
  const RING = [".###.", "#...#", "#...#", "#...#", ".###."];
  const DISC = [".###.", "#####", "#####", "#####", ".###."];

  function one(active: boolean, ratchet: number, ticksPerStep = 96): Set<string> {
    const steps = new Array<boolean>(GRID_STEPS).fill(false);
    steps[4] = active;
    const ratchets = new Array<number>(GRID_STEPS).fill(RATCHET_NONE);
    ratchets[4] = ratchet;
    return renderPatternScreen({
      title: null,
      steps,
      ratchets,
      length: 24,
      cursor: -1,
      playhead: -1,
      barLength: 0,
      sepSelected: false,
      sepOpen: false,
      counterStep: 0,
      baseLength: 0,
      lenSelected: false,
      lenOpen: false,
      ticksPerStep,
    }).pixels;
  }

  function glyphIs(px: Set<string>, rows: string[]): boolean {
    const cx = colX(4);
    const cy = rowCY(4);
    for (let r = 0; r < 5; ++r) {
      for (let c = 0; c < 5; ++c) {
        if (px.has(`${cx - 2 + c},${cy - 2 + r}`) !== (rows[r]![c] === "#")) return false;
      }
    }
    return true;
  }

  function digitInk(px: Set<string>): number {
    const cx = colX(4);
    const y0 = rowCY(4) + 5;
    let n = 0;
    for (let r = 0; r < 5; ++r) for (let c = 0; c < 3; ++c) if (px.has(`${cx - 1 + c},${y0 + r}`)) ++n;
    return n;
  }

  it("un triolet actif est un triangle plein", () => {
    expect(glyphIs(one(true, RATCHET_TRIPLET), FILLED_TRIANGLE)).toBe(true);
  });

  it("un triolet inactif est un triangle vide, sans chiffre", () => {
    const px = one(false, RATCHET_TRIPLET);
    expect(glyphIs(px, HOLLOW_TRIANGLE)).toBe(true);
    expect(digitInk(px)).toBe(0);
  });

  it("un triolet inactif se dessine a toutes les cadences", () => {
    expect(glyphIs(one(false, RATCHET_TRIPLET, 4), HOLLOW_TRIANGLE)).toBe(true);
  });

  const RIGHT_HALF_DISC = ["...#.", "...##", "...##", "...##", "...#."];

  it("un contretemps actif est la moitie droite d un disque, sans chiffre", () => {
    const px = one(true, 8);
    expect(glyphIs(px, RIGHT_HALF_DISC)).toBe(true);
    expect(digitInk(px)).toBe(0);
  });

  it("un contretemps inactif est un anneau, sans chiffre", () => {
    const px = one(false, 8);
    expect(glyphIs(px, RING)).toBe(true);
    expect(digitInk(px)).toBe(0);
  });

  it("un pas en pause est un anneau avec son chiffre, actif ou non", () => {
    const active = one(true, 9);
    expect(glyphIs(active, RING), "une pause active est un anneau vide").toBe(true);
    expect(digitInk(active), "avec le chiffre 2 dessous").toBe(11);
    const inactive = one(false, 11);
    expect(glyphIs(inactive, RING)).toBe(true);
    expect(digitInk(inactive), "avec le chiffre 4 dessous").toBe(9);
  });

  function digitIs(px: Set<string>, rows: string[]): boolean {
    const cx = colX(4);
    const y0 = rowCY(4) + 5;
    for (let r = 0; r < 5; ++r) {
      for (let c = 0; c < 3; ++c) {
        if (px.has(`${cx - 1 + c},${y0 + r}`) !== (rows[r]![c] === "#")) return false;
      }
    }
    return true;
  }

  it("les pauses longues montrent leur propre chiffre", () => {
    const digits: Record<number, string[]> = {
      12: ["###", "#..", "###", "..#", "###"],
      13: ["###", "#..", "###", "#.#", "###"],
      14: ["###", "..#", "..#", "..#", "..#"],
      15: ["###", "#.#", "###", "#.#", "###"],
    };
    for (const code of [12, 13, 14, 15]) {
      const px = one(code === 13, code);
      expect(glyphIs(px, RING), `anneau pour le code ${code}`).toBe(true);
      expect(digitIs(px, digits[code]!), `chiffre du code ${code}`).toBe(true);
    }
  });

  it("un demi-silence est le demi-anneau haut, actif ou non, sans chiffre", () => {
    const TOP_HALF_RING = [".###.", "#...#", "#...#", ".....", "....."];
    for (const active of [true, false]) {
      const px = one(active, 1, 4);
      expect(glyphIs(px, TOP_HALF_RING)).toBe(true);
      expect(digitInk(px)).toBe(0);
    }
  });

  it("le chiffre d une pause se dessine a la cadence la plus rapide", () => {
    const px = one(true, 10, 4);
    expect(glyphIs(px, RING)).toBe(true);
    expect(digitInk(px)).toBe(11);
  });

  it("le demi-disque se dessine a la cadence la plus rapide", () => {
    expect(glyphIs(one(true, 8, 4), RIGHT_HALF_DISC)).toBe(true);
  });

  it("un step inactif avec un autre ratchet est un anneau, sans chiffre", () => {
    const px = one(false, RATCHET_2);
    expect(glyphIs(px, RING)).toBe(true);
    expect(digitInk(px)).toBe(0);
  });

  it("le chiffre revient quand le step devient actif", () => {
    const px = one(true, RATCHET_2);
    expect(glyphIs(px, DISC)).toBe(true);
    expect(digitInk(px)).toBe(11);
  });

  it("le chiffre n est pas dessine quand la cadence refuse le ratchet", () => {
    const px = one(true, RATCHET_3, 4);
    expect(glyphIs(px, DISC)).toBe(true);
    expect(digitInk(px)).toBe(0);
  });

  it("le chiffre est dessine a la premiere cadence qui accepte le ratchet", () => {
    expect(digitInk(one(true, RATCHET_3, 6))).toBe(11);
  });

  it("un ratchet 2 garde son chiffre a la cadence la plus rapide", () => {
    expect(digitInk(one(true, RATCHET_2, 4))).toBe(11);
  });
});

describe("le compteur de l en-tete, avec la vraie police", () => {
  const on = (px: Set<string>, x: number, y: number) => px.has(`${x},${y}`);
  const stepW = textWidth("6/");
  const lengthW = textWidth("20");

  it("ne dessine aucun pave sans selection", () => {
    const { pixels } = renderPatternScreen(panelModel());
    expect(on(pixels, COUNTER_X - COUNTER_PAD, 1)).toBe(false);
    expect(on(pixels, COUNTER_X - COUNTER_PAD, 7)).toBe(false);
  });

  it("une longueur selectionnee inverse le compteur entier", () => {
    const { pixels } = renderPatternScreen({ ...panelModel(), lenSelected: true });
    const right = COUNTER_X + stepW + lengthW + COUNTER_PAD - 1;
    expect(on(pixels, COUNTER_X - COUNTER_PAD, 1), "coin haut gauche").toBe(true);
    expect(on(pixels, COUNTER_X - COUNTER_PAD, 7), "coin bas gauche").toBe(true);
    expect(on(pixels, right, 1), "coin haut droit").toBe(true);
    expect(on(pixels, COUNTER_X - COUNTER_PAD - 1, 4), "rien a gauche").toBe(false);
    expect(on(pixels, right + 1, 4), "rien a droite").toBe(false);
  });

  it("une longueur ouverte inverse le denominateur seul", () => {
    const { pixels } = renderPatternScreen({ ...panelModel(), lenSelected: true, lenOpen: true });
    const left = COUNTER_X + stepW - COUNTER_PAD;
    expect(on(pixels, left, 1), "le pave commence a la longueur").toBe(true);
    expect(on(pixels, left + lengthW + 2 * COUNTER_PAD - 1, 7), "et la couvre").toBe(true);
    expect(on(pixels, COUNTER_X - COUNTER_PAD, 1), "le numero de pas reste en clair").toBe(false);
  });

  it("un compteur sans pas dessine un tiret a la place du numero", () => {
    const dash = renderPatternScreen({ ...panelModel(), counterStep: 0 });
    const six = renderPatternScreen({ ...panelModel(), counterStep: 6 });
    let dashInk = 0;
    let sixInk = 0;
    for (let x = COUNTER_X; x < COUNTER_X + 4; ++x) {
      for (let y = 2; y <= 6; ++y) {
        if (dash.pixels.has(`${x},${y}`)) dashInk += 1;
        if (six.pixels.has(`${x},${y}`)) sixInk += 1;
      }
    }
    expect(dashInk, "un tiret est une seule rangee de pixels").toBeLessThanOrEqual(4);
    expect(dashInk).toBeGreaterThan(0);
    expect(sixInk).toBeGreaterThan(dashInk);
  });

  it("un modele sans longueur ne dessine aucun compteur", () => {
    const withCounter = renderPatternScreen(panelModel()).count;
    const without = renderPatternScreen({ ...panelModel(), baseLength: 0 }).count;
    expect(without).toBeLessThan(withCounter);
  });

  it("le pave du compteur le plus large degage le titre et SEP", () => {
    const { pixels } = renderPatternScreen({
      ...panelModel(), counterStep: 36, baseLength: 36, lenSelected: true,
    });
    for (let x = COUNTER_X + COUNTER_MAX_W + COUNTER_PAD; x < SEP_LABEL_X - 1; ++x) {
      expect(on(pixels, x, 1), `colonne ${x}`).toBe(false);
    }
    for (let x = 0; x < 128; ++x) {
      expect(on(pixels, x, 0), "le pave deborde vers le haut").toBe(false);
      expect(on(pixels, x, 8), "le pave deborde dans la bande 1").toBe(false);
    }
  });
});

describe("la valeur de SEP en edition, avec la vraie police", () => {
  const on = (px: Set<string>, x: number, y: number) => px.has(`${x},${y}`);

  function opened(barLength: number): Set<string> {
    return renderPatternScreen({
      ...panelModel(),
      barLength,
      sepSelected: true,
      sepOpen: true,
    }).pixels;
  }

  // La valeur ouverte est un PAVE PLEIN, et non un cadre : le cadre laissait
  // 2 pixels autour d un chiffre de 5, et le proprietaire l a lu illisible.
  it("remplit le pave sur toute sa hauteur", () => {
    expect(SEP_FRAME_PAD).toBe(2);
    expect(SEP_VALUE_X).toBe(120);
    const px = opened(3);
    expect(on(px, 118, 1), "bord gauche du pave").toBe(true);
    expect(on(px, 125, 1), "bord droit du pave").toBe(true);
    expect(on(px, 118, 7), "le pave descend jusqu en bas a gauche").toBe(true);
    expect(on(px, 125, 7), "le pave descend jusqu en bas a droite").toBe(true);
    for (let y = 2; y <= 6; ++y) {
      expect(on(px, 119, y), `pave plein a gauche en y=${y}`).toBe(true);
      expect(on(px, 124, y), `pave plein a droite en y=${y}`).toBe(true);
    }
  });

  it("remplit le pave pour les quatre valeurs que SEP peut prendre", () => {
    for (const bar of [2, 3, 4, 6]) {
      const px = opened(bar);
      for (let y = 2; y <= 6; ++y) {
        expect(on(px, 119, y), `SEP ${bar}, pave plein a gauche en y=${y}`).toBe(true);
        expect(on(px, 124, y), `SEP ${bar}, pave plein a droite en y=${y}`).toBe(true);
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

