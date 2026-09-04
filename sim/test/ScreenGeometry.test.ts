import { describe, expect, it } from "vitest";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";

import { OLED_H, OLED_W } from "../src/sim/OledDisplay.js";
import * as md from "../src/sim/MainScreenDisplay.js";
import { GRID_STEPS, ROW_WIDTH } from "../src/sim/PatternView.js";
import {
  GLYPH_HEIGHT,
  STK_L,
  STK_L_HEIGHT,
  VELVETSCREEN,
  type Font,
} from "../src/sim/oledFont.js";

/**
 * Geometrie d'ecran partagee — ADR 0012.
 *
 * Le fichier est le MEME que celui du C++ :
 * test/vectors/screen_geometry_vectors.tsv. Son chemin est resolu depuis
 * l'emplacement de ce fichier, jamais depuis le repertoire courant, qui est un
 * etat que le test ne controle pas.
 *
 * La colonne `owners` dit quels langages possedent le fait. Ce lecteur
 * confronte les lignes `both` et `ts`, et il REFUSE une paire (famille, index)
 * qu'il ne connait pas : une ligne sautee en silence serait un trou.
 */

const VECTORS = fileURLToPath(
  new URL("../../test/vectors/screen_geometry_vectors.tsv", import.meta.url),
);

type Family = "M" | "E" | "F";
type Owners = "both" | "cpp" | "ts";

interface Vector {
  family: Family;
  id: string;
  index: number;
  owners: Owners;
  cppName: string;
  expected: number;
}

function wholeNumber(raw: string, quoi: string): number {
  if (raw === "-" || raw.length === 0) {
    throw new Error(`${quoi} est une colonne necessaire : "${raw}"`);
  }
  if (!/^[+-]?\d+$/.test(raw)) throw new Error(`${quoi} n'est pas un entier : "${raw}"`);
  return Number.parseInt(raw, 10);
}

function loadVectors(): Vector[] {
  const text = readFileSync(VECTORS, "utf8");
  const lines = text.split("\n").filter((l) => l.length > 0);
  if (lines.length === 0) throw new Error("le fichier de geometrie est vide");
  const out: Vector[] = [];
  const seen = new Set<string>();
  for (const line of lines.slice(1)) {
    const cell = line.split("\t");
    if (cell.length !== 6) throw new Error(`la ligne ne porte pas six champs : ${line}`);
    const [family, id, index, owners, cppName, expected] = cell;
    // Le controle de longueur ci-dessus ne reduit pas les types : il faut le
    // dire au compilateur, sinon `tsc` refuse et vitest ne verrait rien.
    if (
      family === undefined || id === undefined || index === undefined
      || owners === undefined || cppName === undefined || expected === undefined
    ) {
      throw new Error(`un champ manque : ${line}`);
    }
    if (family !== "M" && family !== "E" && family !== "F") throw new Error(`famille inconnue : ${line}`);
    if (id.length === 0 || id === "-") throw new Error(`l'identifiant est necessaire : ${line}`);
    if (seen.has(id)) throw new Error(`identifiant duplique : ${line}`);
    seen.add(id);
    if (owners !== "both" && owners !== "cpp" && owners !== "ts") {
      throw new Error(`owners n'est ni both, ni cpp, ni ts : ${line}`);
    }
    out.push({
      family,
      id,
      index: wholeNumber(index, "l'index"),
      owners,
      cppName,
      expected: wholeNumber(expected, "la valeur attendue"),
    });
  }
  if (out.length === 0) throw new Error("le fichier de geometrie ne porte aucun cas");
  return out;
}

/** La production, et rien d'autre. `undefined` = le lecteur ne connait pas. */
function production(family: Family, index: number): number | undefined {
  if (family === "M") {
    const table: Record<number, number> = {
      0: OLED_W, 1: OLED_H, 2: md.TAB_COUNT, 3: md.TAB_SLOT_W,
      4: md.TAB_BASELINE_Y, 5: md.TAB_TOP_Y, 6: md.TAB_BOX_Y, 7: md.TAB_BOX_H,
      8: md.RULE_Y, 9: md.RULE_X, 10: md.RULE_W,
      11: md.HEADLINE_BOX_X, 12: md.HEADLINE_BOX_Y, 13: md.HEADLINE_BOX_W,
      14: md.HEADLINE_BOX_H, 15: md.HEADLINE_BASELINE_Y,
      16: md.ROW_A_BOX_Y, 17: md.ROW_B_BOX_Y, 18: md.ROW_BOX_H,
      19: md.COL_LEFT_X, 20: md.COL_RIGHT_X, 21: md.COL_W, 22: md.TEXT_INSET,
      23: md.GLYPH_SIZE, 24: md.ROW_A_BASELINE_Y, 25: md.ROW_B_BASELINE_Y,
      26: md.LINE_LABEL_X, 27: md.LINE_VALUE_X,
      28: md.LINE_0_BASELINE_Y, 29: md.LINE_1_BASELINE_Y, 30: md.LINE_2_BASELINE_Y,
      31: md.MAIN_CENTRE_X, 32: md.MAIN_BOX_W,
      33: md.MAIN_VALUE_BASELINE_Y, 34: md.MAIN_LABEL_BASELINE_Y,
      35: md.LINE_SPACING_Y,
    };
    return table[index];
  }
  if (family === "F") {
    const fonts: Record<number, number> = {
      0: GLYPH_HEIGHT,
      1: STK_L_HEIGHT,
      2: Object.keys(VELVETSCREEN).length,
      3: Object.keys(STK_L).length,
      4: maxWidthOf(VELVETSCREEN),
      5: maxWidthOf(STK_L),
    };
    return fonts[index];
  }
  const table: Record<number, number> = { 0: ROW_WIDTH, 2: GRID_STEPS };
  return table[index];
}

function maxWidthOf(font: Font): number {
  let w = 0;
  for (const g of Object.values(font)) if (g.w > w) w = g.w;
  return w;
}

const vectors = loadVectors();

describe("geometrie d'ecran partagee (ADR 0012)", () => {
  it("charge des vecteurs, et une suite qui en charge zero doit echouer", () => {
    expect(vectors.length).toBeGreaterThan(0);
  });

  it("chaque ligne que TypeScript possede vaut la constante de production", () => {
    let confrontees = 0;
    for (const v of vectors) {
      if (v.owners === "cpp") continue;
      const actual = production(v.family, v.index);
      expect(
        actual,
        `${v.id} : famille ${v.family} index ${v.index} — le lecteur ne connait `
          + `aucune constante, et une ligne sautee est un trou`,
      ).not.toBeUndefined();
      expect(actual, `${v.id} (famille ${v.family} index ${v.index})`).toBe(v.expected);
      ++confrontees;
    }
    expect(confrontees, "aucune ligne confrontee : le garde serait vide").toBeGreaterThan(0);
  });

  it("le fichier nomme chaque constante que le lecteur connait", () => {
    for (const family of ["M", "E", "F"] as Family[]) {
      for (let index = 0; index < 64; ++index) {
        if (production(family, index) === undefined) continue;
        const nommee = vectors.some((v) => v.family === family && v.index === index);
        expect(
          nommee,
          `famille ${family} index ${index} existe chez le lecteur et le fichier `
            + `ne le nomme pas : cette constante n'est pas gardee`,
        ).toBe(true);
      }
    }
  });
});
