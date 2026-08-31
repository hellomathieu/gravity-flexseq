import { describe, expect, it } from "vitest";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";

import { zoneWithHysteresis, effectiveLengthFor, patternIndexFor } from "../src/domain/LengthCv.js";

/**
 * Vecteurs d'or de la specification CV du lot E3.
 *
 * Le fichier est le MEME que celui du C++ : test/vectors/cv_spec_vectors.tsv.
 * Son chemin est resolu depuis l'emplacement de ce fichier, jamais depuis le
 * repertoire courant, qui est un etat que le test ne controle pas.
 *
 * Les familles A et L sont confrontees a la PRODUCTION, qui les implemente deja.
 * Les familles P et S le sont a un modele de reference LOCAL : le moteur ne les
 * realise pas encore, et les brancher sur lui serait circulaire. Le lot E3.7
 * remplacera ce modele par l'implementation reelle.
 */

const VECTORS = fileURLToPath(
  new URL("../../test/vectors/cv_spec_vectors.tsv", import.meta.url),
);

type Family = "A" | "L" | "P" | "S";

interface Vector {
  family: Family;
  id: string;
  a: number;
  b: number;
  c: number | null;
  expected: number;
}

function parseField(raw: string): number | null {
  if (raw === "-") return null;
  if (!/^[+-]?\d+$/.test(raw)) throw new Error(`nombre malforme: "${raw}"`);
  return Number.parseInt(raw, 10);
}

function loadVectors(): Vector[] {
  const text = readFileSync(VECTORS, "utf8");
  const lines = text.split("\n").filter((line) => line.length > 0);
  if (lines.length === 0) throw new Error("le fichier de vecteurs est vide");
  const out: Vector[] = [];
  for (const line of lines.slice(1)) {
    const cell = line.split("\t");
    if (cell.length !== 6) {
      throw new Error(`la ligne ne porte pas six champs: ${line}`);
    }
    const [familyRaw, id, aRaw, bRaw, cRaw, expectedRaw] = cell as [
      string, string, string, string, string, string,
    ];
    const family = familyRaw as Family;
    if (!["A", "L", "P", "S"].includes(family)) {
      throw new Error(`famille inconnue: ${line}`);
    }
    const a = parseField(aRaw);
    const b = parseField(bRaw);
    const c = parseField(cRaw);
    const expected = parseField(expectedRaw);
    const needsC = family !== "A";
    if (a === null || b === null || expected === null || (needsC && c === null)) {
      throw new Error(`une colonne necessaire porte '-': ${line}`);
    }
    out.push({ family, id, a, b, c, expected });
  }
  if (out.length === 0) throw new Error("le fichier de vecteurs ne porte aucun cas");
  return out;
}

const vectors = loadVectors();
const of = (family: Family) => vectors.filter((v) => v.family === family);

function referenceReadStep(localStep: number, offsetSum: number, length: number): number {
  return (((localStep + offsetSum) % length) + length) % length;
}

describe("Vecteurs CV — chargement", () => {
  it("charge le fichier et y trouve des cas", () => {
    expect(vectors.length).toBeGreaterThanOrEqual(40);
  });

  it("represente les quatre familles", () => {
    for (const family of ["A", "L", "P", "S"] as Family[]) {
      expect(of(family).length).toBeGreaterThan(0);
    }
  });

  it("ne porte aucun identifiant en double", () => {
    const ids = vectors.map((v) => v.id);
    expect(new Set(ids).size).toBe(ids.length);
  });
});

describe("Vecteurs CV — familles implementees en production", () => {
  it("A suit le quantificateur de production", () => {
    for (const v of of("A")) {
      expect(zoneWithHysteresis(v.a, v.b), v.id).toBe(v.expected);
    }
  });

  it("L suit l'ecretage de production", () => {
    for (const v of of("L")) {
      expect(effectiveLengthFor(v.a, v.b + (v.c as number)), v.id).toBe(v.expected);
    }
  });
});

describe("Vecteurs CV — famille P, contre la production", () => {
  it("P suit l'ecretage de production", () => {
    for (const v of of("P")) {
      expect(patternIndexFor(v.a, v.b + (v.c as number)), v.id).toBe(v.expected);
    }
  });
});

describe("Vecteurs CV — familles non encore implementees", () => {
  it("S suit le modele de reference", () => {
    for (const v of of("S")) {
      expect(referenceReadStep(v.a, v.b, v.c as number), v.id).toBe(v.expected);
    }
  });
});

describe("Invariants exhaustifs", () => {
  it("le step lu reste dans la longueur, pour toute entree", () => {
    for (let length = 1; length <= 36; ++length) {
      for (let local = 0; local < length; ++local) {
        for (let sum = -30; sum <= 30; ++sum) {
          const got = referenceReadStep(local, sum, length);
          expect(got).toBeGreaterThanOrEqual(0);
          expect(got).toBeLessThan(length);
        }
      }
    }
  });

  it("une longueur de un lit toujours le premier step", () => {
    for (let sum = -30; sum <= 30; ++sum) {
      expect(referenceReadStep(0, sum, 1)).toBe(0);
    }
  });

  it("un offset nul lit le step local", () => {
    for (let length = 1; length <= 36; ++length) {
      for (let local = 0; local < length; ++local) {
        expect(referenceReadStep(local, 0, length)).toBe(local);
      }
    }
  });

  it("les deux sources commutent sur le pattern", () => {
    for (let base = 0; base <= 15; ++base) {
      for (let one = -15; one <= 15; ++one) {
        for (let two = -15; two <= 15; ++two) {
          expect(patternIndexFor(base, one + two)).toBe(patternIndexFor(base, two + one));
        }
      }
    }
  });

  it("les deux sources commutent sur la longueur", () => {
    for (let base = 1; base <= 36; ++base) {
      for (let one = -15; one <= 15; ++one) {
        for (let two = -15; two <= 15; ++two) {
          expect(effectiveLengthFor(base, one + two)).toBe(effectiveLengthFor(base, two + one));
        }
      }
    }
  });

  it("deux offsets opposes rendent la base", () => {
    for (let base = 1; base <= 36; ++base) {
      expect(effectiveLengthFor(base, 0)).toBe(base);
    }
    for (let base = 0; base <= 15; ++base) {
      for (let amount = 0; amount <= 15; ++amount) {
        expect(patternIndexFor(base, amount - amount)).toBe(base);
      }
    }
  });
});
