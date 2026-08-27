import { describe, expect, it } from "vitest";
import { PatternBank, PATTERN_COUNT } from "../src/domain/PatternBank.js";
import { Pattern, RATCHET_3 } from "../src/domain/Pattern.js";
import {
  FACTORY_MASK_BYTES,
  FACTORY_PATTERN_COUNT,
  FACTORY_STEP_COUNT,
  factoryStepMask,
  loadFactoryPatterns,
} from "../src/domain/FactoryPatterns.js";
import { v3 } from "../src/domain/Persistence.js";

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

function expectedMask(index: number): number {
  const entry = EXPECTED[index];
  if (!entry) return 0;
  return entry[1].reduce((mask, step) => mask | (1 << step), 0);
}

function expectedRecordByte(index: number, offset: number): number {
  if (index < 0 || index >= PATTERN_COUNT) return 0;
  if (offset < 0 || offset >= 24) return 0;
  if (offset === 23) return 16;
  if (offset >= 2) return 0;
  return (expectedMask(index) >> (offset * 8)) & 0xff;
}

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

describe("le masque d usine", () => {
  it("reproduit les steps de l original", () => {
    for (let index = 0; index < FACTORY_PATTERN_COUNT; ++index) {
      expect(factoryStepMask(index), EXPECTED[index]![0]).toBe(expectedMask(index));
    }
  });

  it("est nul pour les emplacements B", () => {
    for (let index = FACTORY_PATTERN_COUNT; index < PATTERN_COUNT; ++index) {
      expect(factoryStepMask(index)).toBe(0);
    }
  });

  it("refuse un index hors bornes", () => {
    expect(factoryStepMask(16)).toBe(0);
    expect(factoryStepMask(64)).toBe(0);
    expect(factoryStepMask(255)).toBe(0);
    expect(factoryStepMask(-1)).toBe(0);
  });

  it("garde ses constantes", () => {
    expect(FACTORY_PATTERN_COUNT).toBe(8);
    expect(FACTORY_STEP_COUNT).toBe(16);
    expect(FACTORY_MASK_BYTES).toBe(2);
    expect(v3.FACTORY_TEMPLATE_LENGTH).toBe(16);
  });
});

describe("le record d usine", () => {
  it("couvre les 384 octets de la zone des templates", () => {
    const expected: number[] = [];
    const produced: number[] = [];
    for (let index = 0; index < PATTERN_COUNT; ++index) {
      for (let offset = 0; offset < 24; ++offset) {
        expected.push(expectedRecordByte(index, offset));
        produced.push(v3.factoryTemplateByte(index, offset));
      }
    }
    expect(produced).toHaveLength(384);
    expect(PATTERN_COUNT * v3.TEMPLATE_RECORD).toBe(384);
    expect(produced).toEqual(expected);
  });

  it("serialise le masque en petit-boutiste", () => {
    for (let index = 0; index < FACTORY_PATTERN_COUNT; ++index) {
      const mask = expectedMask(index);
      expect(v3.factoryTemplateByte(index, 0)).toBe(mask & 0xff);
      expect(v3.factoryTemplateByte(index, 1)).toBe((mask >> 8) & 0xff);
    }
    expect(v3.factoryTemplateByte(0, 0)).toBe(0x11);
    expect(v3.factoryTemplateByte(0, 1)).toBe(0x91);
  });

  it("ne porte aucun step au dela de 15", () => {
    for (let index = 0; index < PATTERN_COUNT; ++index) {
      for (let offset = 2; offset < 5; ++offset) {
        expect(v3.factoryTemplateByte(index, offset)).toBe(0);
      }
    }
  });

  it("ne porte aucun ratchet", () => {
    for (let index = 0; index < PATTERN_COUNT; ++index) {
      for (let offset = 5; offset < 23; ++offset) {
        expect(v3.factoryTemplateByte(index, offset)).toBe(0);
      }
    }
  });

  it("declare seize steps pour les seize templates", () => {
    for (let index = 0; index < PATTERN_COUNT; ++index) {
      expect(v3.factoryTemplateByte(index, 23)).toBe(16);
    }
  });

  it("laisse les emplacements B sans contenu", () => {
    for (let index = FACTORY_PATTERN_COUNT; index < PATTERN_COUNT; ++index) {
      for (let offset = 0; offset < 23; ++offset) {
        expect(v3.factoryTemplateByte(index, offset)).toBe(0);
      }
      expect(v3.factoryTemplateByte(index, 23)).toBe(16);
    }
  });

  it("refuse un offset hors bornes", () => {
    for (let index = 0; index < PATTERN_COUNT; ++index) {
      expect(v3.factoryTemplateByte(index, 24)).toBe(0);
      expect(v3.factoryTemplateByte(index, 255)).toBe(0);
      expect(v3.factoryTemplateByte(index, -1)).toBe(0);
    }
  });

  it("refuse un index hors bornes", () => {
    for (let offset = 0; offset < 24; ++offset) {
      expect(v3.factoryTemplateByte(16, offset)).toBe(0);
      expect(v3.factoryTemplateByte(255, offset)).toBe(0);
      expect(v3.factoryTemplateByte(-1, offset)).toBe(0);
    }
  });
});
