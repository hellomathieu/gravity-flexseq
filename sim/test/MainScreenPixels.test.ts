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
  TAB_CLOCK,
  TAB_PATTERNS,
  ROW_A_BOX_Y,
  ROW_B_BOX_Y,
  TAB_SETTINGS,
  TAB_WIDE_GLYPH_W,
  TAB_COUNT,
  TAB_SLOT_W,
  TRANSPORT_PLAY_X,
  TRANSPORT_STOP_X,
  TRANSPORT_STOP_W,
  tabCentreX,
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
  slotEmpty: false,
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
  running: true,
};

/**
 * L encre que le PANNEAU recoit, relevee par `tools/run-screen-dump.sh` sur
 * `env:mainscreen`, remise en coordonnees logiques : `y logique = 63 - y panneau`.
 * Ces nombres ne sont pas calcules ici : ils sont lus sur la memoire du panneau.
 */
const PANEL_ROWS: ReadonlyArray<readonly [number, number]> = [
  [3, 20], [4, 19], [5, 23], [6, 23], [7, 30], [8, 7], [9, 6], [10, 7],
  [11, 6], [12, 6], [13, 6], [14, 33], [15, 20], [16, 36], [17, 25],
  [18, 34], [19, 20], [20, 18], [21, 15], [22, 9], [23, 11], [24, 28],
  [25, 31], [26, 23], [27, 29], [28, 16], [29, 16], [30, 19], [36, 23],
  [37, 18], [38, 21], [39, 18], [40, 22], [52, 120], [56, 12], [57, 12],
  [58, 29], [59, 30], [60, 30], [61, 28], [62, 28], [63, 12],
];

const PANEL_INK = 939;

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

describe("les glyphes de la barre, en pixels", () => {
  const on = (px: Set<string>, x: number, y: number) => px.has(`${x},${y}`);

  it("les trois glyphes partagent la bande des chiffres, 58 a 62", () => {
    const px = renderMainScreen(PANEL_MODEL).pixels;
    for (const tab of [TAB_CLOCK, TAB_PATTERNS, TAB_SETTINGS]) {
      const x0 = tabCentreX(tab) - 6;
      let top = -1;
      let bottom = -1;
      for (let y = 56; y <= 63; ++y) {
        for (let dx = 0; dx < 12; ++dx) {
          if (on(px, x0 + dx, y)) {
            if (top < 0) top = y;
            bottom = y;
          }
        }
      }
      expect(top, `haut du creneau ${tab}`).toBe(58);
      expect(bottom, `bas du creneau ${tab}`).toBe(62);
    }
  });

  it("le glyphe de PATTERNS est deux rangees de trois points d un pixel", () => {
    const px = renderMainScreen(PANEL_MODEL).pixels;
    const cx = tabCentreX(TAB_PATTERNS);
    for (const x of [cx - 3, cx, cx + 3]) {
      expect(on(px, x, 58)).toBe(true);
      expect(on(px, x, 62)).toBe(true);
      expect(on(px, x, 59)).toBe(false);
      expect(on(px, x, 60)).toBe(false);
      expect(on(px, x, 61)).toBe(false);
    }
    expect(on(px, cx - 2, 58)).toBe(false);
    expect(on(px, cx - 1, 58)).toBe(false);
  });

  it("le glyphe des reglages est deux curseurs de sept pixels", () => {
    expect(TAB_WIDE_GLYPH_W).toBe(7);
    const px = renderMainScreen(PANEL_MODEL).pixels;
    const cx = tabCentreX(TAB_SETTINGS);
    const x = cx - 3;
    for (let dx = 0; dx < 7; ++dx) {
      expect(on(px, x + dx, 59), `glissiere 1 en ${x + dx}`).toBe(true);
      expect(on(px, x + dx, 61), `glissiere 2 en ${x + dx}`).toBe(true);
      expect(on(px, x + dx, 60), `vide en ${x + dx}`).toBe(false);
    }
    expect(on(px, cx, 58)).toBe(true);
    expect(on(px, x + 1, 62)).toBe(true);
    expect(on(px, x, 58)).toBe(false);
    expect(on(px, x, 62)).toBe(false);
    expect(on(px, x + 7, 59)).toBe(false);
  });
});

describe("l indicateur de transport, hors de la navigation", () => {
  const on = (px: Set<string>, x: number, y: number) => px.has(`${x},${y}`);
  const inkRight = (px: Set<string>) => {
    let n = 0;
    for (let x = 118; x < 128; ++x) for (let y = 56; y <= 63; ++y) if (on(px, x, y)) ++n;
    return n;
  };

  it("montre Play a x = 122 quand le transport tourne", () => {
    expect(TRANSPORT_PLAY_X).toBe(122);
    const px = renderMainScreen({ ...PANEL_MODEL, running: true }).pixels;
    expect(inkRight(px)).toBe(9);
    for (const [x, y] of [[122, 58], [122, 59], [123, 59], [122, 60], [123, 60],
                          [124, 60], [122, 61], [123, 61], [122, 62]]) {
      expect(on(px, x!, y!), `${x},${y}`).toBe(true);
    }
    expect(on(px, 121, 60)).toBe(false);
  });

  it("montre Stop a x = 121 quand le transport est arrete", () => {
    expect(TRANSPORT_STOP_X).toBe(121);
    expect(TRANSPORT_STOP_W).toBe(5);
    const px = renderMainScreen({ ...PANEL_MODEL, running: false }).pixels;
    expect(inkRight(px)).toBe(25);
    for (let x = 121; x < 126; ++x) {
      for (let y = 58; y <= 62; ++y) expect(on(px, x, y), `${x},${y}`).toBe(true);
    }
    expect(on(px, 126, 60)).toBe(false);
  });

  it("n est pas dessine hors horloge interne", () => {
    for (const clockSource of [1, 2, 3, 4, 5]) {
      for (const running of [true, false]) {
        const px = renderMainScreen({ ...PANEL_MODEL, clockSource, running }).pixels;
        expect(inkRight(px), `source ${clockSource}`).toBe(0);
      }
    }
  });

  it("reste hors des neuf creneaux de la barre", () => {
    expect(TAB_SLOT_W * TAB_COUNT).toBe(108);
    expect(TRANSPORT_STOP_X).toBeGreaterThanOrEqual(108);
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
  [37, 12], [38, 19], [39, 11], [40, 13], [52, 120], [56, 12], [57, 12],
  [58, 29], [59, 30], [60, 30], [61, 28], [62, 28], [63, 12],
];

const SEQ_PANEL_INK = 977;

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
  [37, 12], [38, 19], [39, 11], [40, 13], [52, 120], [56, 12], [57, 12],
  [58, 29], [59, 30], [60, 30], [61, 28], [62, 28], [63, 12],
];

const CONFIG_PANEL_INK = 989;

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

/**
 * L encre que le PANNEAU recoit pour l onglet PATTERNS, relevee le 2026-09-13 par
 * `PLATFORMIO_BUILD_FLAGS="-DFLEXSEQ_DEMO_TAB_PATTERNS=1" ASCII=1 ENVNAME=mainscreen
 * ./tools/run-screen-dump.sh`, remise en coordonnees logiques. Ces nombres sont LUS
 * sur la memoire du panneau, jamais calcules ici.
 */
const PATTERNS_PANEL_ROWS: ReadonlyArray<readonly [number, number]> = [
  [2, 17], [3, 6], [4, 12], [5, 25], [6, 31], [7, 29], [8, 32], [9, 12],
  [10, 12], [11, 9], [12, 10], [13, 11], [14, 15], [15, 15], [16, 17],
  [17, 13], [18, 11], [19, 9], [20, 9], [21, 9], [22, 9], [23, 12],
  [24, 15], [25, 22], [26, 20], [27, 17], [36, 25], [37, 14], [38, 21],
  [39, 13], [40, 18], [52, 120], [56, 12], [57, 12], [58, 29], [59, 32],
  [60, 34], [61, 30], [62, 30], [63, 12]
];

const PATTERNS_PANEL_INK = 801;

describe("l onglet PATTERNS — lot 16E etape 4b", () => {
  const patternsTab = (slotEmpty: boolean): MainScreenModel => ({
    ...PANEL_MODEL,
    tab: TAB_PATTERNS,
    insideTab: true,
    // Une seule ligne selectionnable, EDIT, et le curseur y est.
    cursor: 0,
    fieldCount: 1,
    slotEmpty,
    // Ce que mainScreenModelOf produit sur cet onglet : le pattern que l ecran
    // NOMME est l emplacement parcouru, et il est le parametre principal.
    patternIndex: 10,
    mainParameter: MainParameter.Pattern,
  });

  const inkInRows = (rows: number[], y0: number, y1: number): number => {
    let n = 0;
    for (let y = y0; y <= y1; y += 1) n += rows[y] ?? 0;
    return n;
  };

  // La mise en page est celle d un canal en SEQ : une grande valeur a gauche,
  // son etiquette dessous, et deux lignes a droite. La troisieme reste vide.
  it("encre la grande valeur et les deux premieres lignes", () => {
    const rows = renderMainScreen(patternsTab(true)).rows;
    expect(inkInRows(rows, MAIN_VALUE_BASELINE_Y - 10, MAIN_VALUE_BASELINE_Y))
      .toBeGreaterThan(0);
    expect(inkInRows(rows, MAIN_LABEL_BASELINE_Y - 4, MAIN_LABEL_BASELINE_Y))
      .toBeGreaterThan(0);
    expect(inkInRows(rows, LINE_0_BASELINE_Y - 4, LINE_0_BASELINE_Y))
      .toBeGreaterThan(0);
    // ⚠️ La troisieme ligne se mesure COLONNE par colonne : la grande valeur
    // occupe les memes RANGEES a gauche, donc un compte par rangee y verrait
    // toujours de l encre et ne prouverait rien.
    const { pixels } = renderMainScreen(patternsTab(true));
    let onLine2 = 0;
    for (const key of pixels) {
      const parts = key.split(",");
      const x = Number(parts[0]);
      const y = Number(parts[1]);
      if (x >= LINE_LABEL_X && y > LINE_2_BASELINE_Y - 6 && y <= LINE_2_BASELINE_Y) {
        onLine2 += 1;
      }
    }
    expect(onLine2, "la troisieme ligne reste vide").toBe(0);
  });

  it("rend exactement ce que le panneau a recu, rangee par rangee", () => {
    const model = patternsTab(true);
    const rows = renderMainScreen(model).rows;
    const expected = new Map<number, number>(
      PATTERNS_PANEL_ROWS.map(([y, n]) => [y, n]),
    );
    const divergent: string[] = [];
    for (let y = 0; y < 64; y += 1) {
      const mine = rows[y] ?? 0;
      const theirs = expected.get(y) ?? 0;
      if (mine !== theirs) divergent.push(`y=${y} miroir=${mine} panneau=${theirs}`);
    }
    expect(divergent.join(" | ")).toBe("");
    expect(renderMainScreen(model).count).toBe(PATTERNS_PANEL_INK);
  });

  it("distingue un emplacement vide d un emplacement occupe", () => {
    const free = renderMainScreen(patternsTab(true)).count;
    const used = renderMainScreen(patternsTab(false)).count;
    expect(free).not.toBe(used);
  });
});
