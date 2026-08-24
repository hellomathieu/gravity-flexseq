import { describe, expect, it } from "vitest";
import { PatternBank, PATTERN_COUNT } from "../src/domain/PatternBank.js";
import { Pattern, RATCHET_3 } from "../src/domain/Pattern.js";
import { FACTORY_PATTERN_COUNT, loadFactoryPatterns } from "../src/domain/FactoryPatterns.js";

// Les huit patterns d'usine de l'original, lus dans Gravity.ino:83-90 et ecrits
// ici en listes de steps LITTERALES. Comparer a la table de production ferait
// bouger l'attente avec la valeur.
const EXPECTED: ReadonlyArray<readonly [string, readonly number[]]> = [
  ["A1", [0, 4, 8, 12, 15]],
  ["A2", [4, 11]],
  ["A3", [0, 3, 6, 9, 12]],
  ["A4", [2, 3, 6, 7, 10, 11, 14, 15]],
  ["A5", [1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15]],
  ["A6", [2, 4, 6, 10, 12, 14]],
  ["A7", [0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14]],
  ["A8", [0, 1, 4, 5, 8, 9, 10, 12, 13, 15]],
];

function activeSteps(bank: PatternBank, index: number): number[] {
  const pattern = bank.getPattern(index)!;
  const out: number[] = [];
  for (let step = 0; step < Pattern.DEFAULT_TOTAL_STEPS; ++step) {
    if (pattern.readStep(step) === true) out.push(step);
  }
  return out;
}

describe("les patterns d usine", () => {
  it("portent le contenu de l original", () => {
    const bank = new PatternBank();
    loadFactoryPatterns(bank);
    EXPECTED.forEach(([name, steps], index) => {
      expect(activeSteps(bank, index), name).toEqual([...steps]);
    });
  });

  it("laissent la banque B vide", () => {
    const bank = new PatternBank();
    loadFactoryPatterns(bank);
    for (let index = FACTORY_PATTERN_COUNT; index < PATTERN_COUNT; ++index) {
      expect(activeSteps(bank, index)).toEqual([]);
    }
  });

  it("ne portent aucun ratchet", () => {
    const bank = new PatternBank();
    loadFactoryPatterns(bank);
    for (let index = 0; index < PATTERN_COUNT; ++index) {
      const pattern = bank.getPattern(index)!;
      for (let step = 0; step < Pattern.DEFAULT_TOTAL_STEPS; ++step) {
        expect(pattern.getRatchet(step)).toBe(0);
      }
    }
  });

  it("laissent eteints les steps que l original n avait pas", () => {
    const bank = new PatternBank();
    loadFactoryPatterns(bank);
    for (let index = 0; index < FACTORY_PATTERN_COUNT; ++index) {
      const beyond = activeSteps(bank, index).filter((s) => s >= 16);
      expect(beyond).toEqual([]);
    }
  });

  it("se rechargent sans rien changer", () => {
    const bank = new PatternBank();
    loadFactoryPatterns(bank);
    loadFactoryPatterns(bank);
    EXPECTED.forEach(([name, steps], index) => {
      expect(activeSteps(bank, index), name).toEqual([...steps]);
    });
  });

  it("effacent une edition au rechargement", () => {
    const bank = new PatternBank();
    loadFactoryPatterns(bank);
    const first = bank.getPattern(0)!;
    first.writeStep(1, true);
    first.setRatchet(0, RATCHET_3);
    loadFactoryPatterns(bank);
    expect(activeSteps(bank, 0)).toEqual([0, 4, 8, 12, 15]);
    expect(first.getRatchet(0)).toBe(0);
  });
});
