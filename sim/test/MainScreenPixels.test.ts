import { describe, expect, it } from "vitest";
import { legacyLine, modText, renderMainScreen } from "../src/sim/MainScreenPixels.js";
import { MainParameter, type MainScreenModel } from "../src/domain/MainScreenModel.js";
import { ChannelMode } from "../src/domain/SequencerEngine.js";
import { CvDestination } from "../src/domain/CvDestination.js";
import { STK_L, VELVETSCREEN, textAdvance, textWidth } from "../src/sim/oledFont.js";
import {
  LINE_0_BASELINE_Y,
  LINE_2_BASELINE_Y,
  LINE_LABEL_X,
  MAIN_CENTRE_X,
  MAIN_LABEL_BASELINE_Y,
  MAIN_VALUE_BASELINE_Y,
  RULE_Y,
} from "../src/sim/MainScreenDisplay.js";

/**
 * Le modele que `env:mainscreen` fige, releve dans `src/mainscreen_demo_main.cpp` :
 * onglet 2, curseur sur la troisieme ligne, dans l onglet, mode CLOCK par defaut,
 * `setSubdiv(-4)` donc `x4` en gros et 24 ticks par pas.
 */
const PANEL_MODEL: MainScreenModel = {
  tab: 2,
  insideTab: true,
  cursor: 2,
  fieldOpen: false,
  fieldCount: 3,
  patternIndex: 9,
  length: 20,
  subdiv: -4,
  barLength: 3,
  mode: ChannelMode.CLOCK,
  offset: 0,
  skipChance: 0,
  stepTicks: 24,
  mainParameter: MainParameter.Subdiv,
  cv1Target: 0,
  cv2Target: 0,
  configPage: false,
  tempo: 120,
  clockSource: 0,
};

/**
 * L encre que le PANNEAU recoit, relevee par `tools/run-screen-dump.sh` sur
 * `env:mainscreen`, remise en coordonnees logiques : `y logique = 63 - y panneau`.
 * Ces nombres ne sont pas calcules ici : ils sont lus sur la memoire du panneau.
 */
const PANEL_ROWS: ReadonlyArray<readonly [number, number]> = [
  [3, 20], [4, 19], [5, 23], [6, 23], [7, 30], [8, 7], [9, 6], [10, 7],
  [11, 6], [12, 6], [13, 6], [14, 33], [15, 20], [16, 36], [17, 25], [18, 34],
  [19, 20], [20, 18], [21, 15], [22, 9], [23, 11], [24, 28], [25, 31], [26, 23],
  [27, 29], [28, 16], [29, 16], [30, 19], [36, 23], [37, 18], [38, 21],
  [39, 18], [40, 22], [52, 120], [56, 16], [57, 16], [58, 25], [59, 29],
  [60, 34], [61, 31], [62, 33], [63, 23],
];

const PANEL_INK = 965;

describe("l ecran principal, confronte au PANNEAU (risque 89)", () => {
  it("rend exactement l encre que le panneau recoit", () => {
    expect(renderMainScreen(PANEL_MODEL).count).toBe(PANEL_INK);
  });

  it("rend la meme encre RANGEE PAR RANGEE, et pas seulement le meme total", () => {
    const rows = renderMainScreen(PANEL_MODEL).rows;
    const expected = new Map(PANEL_ROWS);
    for (let y = 0; y < rows.length; ++y) {
      expect(rows[y], `rangee ${y}`).toBe(expected.get(y) ?? 0);
    }
  });

  it("le total des rangees attendues vaut bien l encre attendue", () => {
    expect(PANEL_ROWS.reduce((s, [, n]) => s + n, 0)).toBe(PANEL_INK);
  });
});

describe("les deux largeurs de u8g2, qui ne sont pas la meme chose", () => {
  it("getStrWidth retire l avance du dernier glyphe et ajoute son encre", () => {
    expect(textWidth("MOD:", VELVETSCREEN)).toBe(17);
    expect(textAdvance("MOD:", VELVETSCREEN)).toBe(18);
  });

  it("un texte vide mesure zero des deux facons", () => {
    expect(textWidth("", VELVETSCREEN)).toBe(0);
    expect(textAdvance("", VELVETSCREEN)).toBe(0);
  });

  it("un dernier glyphe sans encre ne subit aucun ajustement", () => {
    expect(textWidth("A ", VELVETSCREEN)).toBe(textAdvance("A ", VELVETSCREEN));
  });

  it("la grande police mesure ses propres largeurs", () => {
    expect(textWidth("x4", STK_L)).toBe(26);
    expect(textAdvance("x4", STK_L)).toBe(28);
  });
});

describe("la convention verticale de u8g2 (ADR 0012)", () => {
  it("l encre d une ligne de base occupe base - h a base - 1", () => {
    const rows = renderMainScreen(PANEL_MODEL).rows;
    expect(rows[LINE_0_BASELINE_Y - 5], "premiere rangee de la ligne 1").toBeGreaterThan(0);
    expect(rows[LINE_0_BASELINE_Y], "la ligne de base elle-meme reste vide").toBe(7);
  });

  it("le gros parametre tient au-dessus de sa ligne de base", () => {
    const { pixels } = renderMainScreen(PANEL_MODEL);
    const inkInRow = (y: number, x0: number, x1: number) => {
      let n = 0;
      for (let x = x0; x <= x1; ++x) if (pixels.has(`${x},${y}`)) n += 1;
      return n;
    };
    const left = MAIN_CENTRE_X - Math.floor(textWidth("x4", STK_L) / 2);
    const right = left + textAdvance("x4", STK_L);
    expect(inkInRow(MAIN_VALUE_BASELINE_Y - 23, left, right), "premiere rangee").toBeGreaterThan(0);
    expect(inkInRow(MAIN_VALUE_BASELINE_Y - 24, left, right), "rien au-dessus").toBe(0);
    expect(inkInRow(MAIN_VALUE_BASELINE_Y, left, right), "ni sur la ligne de base").toBe(0);
  });

  it("le pave du curseur fait h + 2 rangees, de base - h - 1 a base + 1", () => {
    const { pixels } = renderMainScreen(PANEL_MODEL);
    const boxX = LINE_LABEL_X - 1;
    const boxRight = boxX + textWidth("MOD:", VELVETSCREEN) + 1;
    for (let y = LINE_2_BASELINE_Y - 6; y <= LINE_2_BASELINE_Y; ++y) {
      expect(pixels.has(`${boxX},${y}`), `bord gauche du pave en ${y}`).toBe(true);
      expect(pixels.has(`${boxRight},${y}`), `bord droit du pave en ${y}`).toBe(true);
    }
    expect(pixels.has(`${boxX},${LINE_2_BASELINE_Y - 7}`), "rien au-dessus").toBe(false);
    expect(pixels.has(`${boxX},${LINE_2_BASELINE_Y + 1}`), "rien en dessous").toBe(false);
  });

  it("l etiquette du parametre principal et le filet ne se touchent pas", () => {
    const rows = renderMainScreen(PANEL_MODEL).rows;
    expect(rows[MAIN_LABEL_BASELINE_Y - 1]).toBeGreaterThan(0);
    for (let y = MAIN_LABEL_BASELINE_Y; y < RULE_Y; ++y) {
      expect(rows[y], `rangee ${y} entre l etiquette et le filet`).toBe(0);
    }
    expect(rows[RULE_Y]).toBe(120);
  });
});

/**
 * Le meme releve, pour un onglet de canal en SEQ. Lu sur `env:mainscreen`
 * compile avec `-DFLEXSEQ_DEMO_MODE_SEQ=1`, meme methode et meme modele : seuls
 * le mode et le parametre principal changent.
 */
const SEQ_PANEL_ROWS: ReadonlyArray<readonly [number, number]> = [
  [3, 20], [4, 14], [5, 35], [6, 32], [7, 41], [8, 15], [9, 12], [10, 12],
  [11, 9], [12, 9], [13, 10], [14, 26], [15, 22], [16, 27], [17, 20],
  [18, 22], [19, 11], [20, 10], [21, 9], [22, 9], [23, 9], [24, 39],
  [25, 40], [26, 43], [27, 39], [28, 18], [29, 19], [30, 29], [36, 20],
  [37, 12], [38, 19], [39, 11], [40, 13], [52, 120], [56, 16], [57, 16],
  [58, 25], [59, 29], [60, 34], [61, 31], [62, 33], [63, 23],
];

const SEQ_PANEL_INK = 1003;

describe("l onglet d un canal en SEQ", () => {
  const seq: MainScreenModel = {
    ...PANEL_MODEL,
    mode: ChannelMode.SEQ,
    mainParameter: MainParameter.Pattern,
  };

  it("rend exactement l encre que le panneau recoit", () => {
    expect(renderMainScreen(seq).count).toBe(SEQ_PANEL_INK);
  });

  it("rend la meme encre RANGEE PAR RANGEE", () => {
    const rows = renderMainScreen(seq).rows;
    const expected = new Map(SEQ_PANEL_ROWS);
    for (let y = 0; y < rows.length; ++y) {
      expect(rows[y], `rangee ${y}`).toBe(expected.get(y) ?? 0);
    }
  });

  it("le total des rangees attendues vaut bien l encre attendue", () => {
    expect(SEQ_PANEL_ROWS.reduce((s, [, n]) => s + n, 0)).toBe(SEQ_PANEL_INK);
  });

  it("porte MODE, EDIT et CONFIG, les trois lignes de l original", () => {
    expect(legacyLine(seq, 0)).toEqual(["MODE:", "SEQ"]);
    expect(legacyLine(seq, 1)).toEqual(["EDIT", ""]);
    expect(legacyLine(seq, 2)).toEqual(["CONFIG", ""]);
  });

  it("EDIT et CONFIG sont des entrees : elles ne portent aucune valeur", () => {
    expect(legacyLine(seq, 1)[1]).toBe("");
    expect(legacyLine(seq, 2)[1]).toBe("");
  });
});

describe("la ligne MOD nomme le routage des deux entrees", () => {
  const avec = (a: number, b: number): MainScreenModel =>
    ({ ...PANEL_MODEL, mode: ChannelMode.SEQ, configPage: true, cv1Target: a, cv2Target: b });

  it("la position nomme l entree, CV1 avant CV2", () => {
    expect(modText(avec(CvDestination.PATTERN, CvDestination.LENGTH))).toBe("P/L");
    expect(modText(avec(CvDestination.LENGTH, CvDestination.PATTERN))).toBe("L/P");
  });

  it("une entree libre s ecrit avec un tiret", () => {
    expect(modText(avec(CvDestination.RESET, CvDestination.NONE))).toBe("R/-");
    expect(modText(avec(CvDestination.NONE, CvDestination.STEP))).toBe("-/S");
  });

  it("aucun routage se lit OFF", () => {
    expect(modText(avec(CvDestination.NONE, CvDestination.NONE))).toBe("OFF");
  });

  it("le nommage accepte un routage que le cycle ne produit pas", () => {
    expect(modText(avec(CvDestination.PATTERN, CvDestination.PATTERN))).toBe("P/P");
  });

  it("la ligne 3 de la page CONFIG porte ce nom", () => {
    expect(legacyLine(avec(CvDestination.STEP, CvDestination.RESET), 2)).toEqual(["MOD:", "S/R"]);
  });
});

/**
 * Le meme releve, pour la page CONFIG PATTERN d'un canal en SEQ dont CV1 va au
 * PATTERN et CV2 a la LENGTH, curseur sur MOD. Lu sur `env:mainscreen` compile
 * avec `-DFLEXSEQ_DEMO_MOD=1`.
 */
const CONFIG_PANEL_ROWS: ReadonlyArray<readonly [number, number]> = [
  [3, 19], [4, 13], [5, 36], [6, 34], [7, 42], [8, 15], [9, 12], [10, 12],
  [11, 9], [12, 9], [13, 10], [14, 31], [15, 30], [16, 37], [17, 28],
  [18, 29], [19, 11], [20, 10], [21, 9], [22, 9], [23, 9], [24, 29],
  [25, 40], [26, 38], [27, 40], [28, 15], [29, 18], [30, 19], [36, 20],
  [37, 12], [38, 19], [39, 11], [40, 13], [52, 120], [56, 16], [57, 16],
  [58, 25], [59, 29], [60, 34], [61, 31], [62, 33], [63, 23],
];

const CONFIG_PANEL_INK = 1015;

describe("la page CONFIG PATTERN, confrontee au PANNEAU", () => {
  const config: MainScreenModel = {
    ...PANEL_MODEL,
    mode: ChannelMode.SEQ,
    mainParameter: MainParameter.Pattern,
    configPage: true,
    cv1Target: CvDestination.PATTERN,
    cv2Target: CvDestination.LENGTH,
  };

  it("rend exactement l encre que le panneau recoit", () => {
    expect(renderMainScreen(config).count).toBe(CONFIG_PANEL_INK);
  });

  it("rend la meme encre RANGEE PAR RANGEE", () => {
    const rows = renderMainScreen(config).rows;
    const expected = new Map(CONFIG_PANEL_ROWS);
    for (let y = 0; y < rows.length; ++y) {
      expect(rows[y], `rangee ${y}`).toBe(expected.get(y) ?? 0);
    }
  });

  it("le total des rangees attendues vaut bien l encre attendue", () => {
    expect(CONFIG_PANEL_ROWS.reduce((s, [, n]) => s + n, 0)).toBe(CONFIG_PANEL_INK);
  });
});
