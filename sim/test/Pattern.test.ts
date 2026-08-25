import { describe, expect, it } from "vitest";
import {
  Pattern,
  RATCHET_NONE,
  RATCHET_2,
  RATCHET_3,
  RATCHET_4,
  RATCHET_6,
  RATCHET_TRIPLET,
  ratchetTriggers,
  ratchetSpan,
} from "../src/domain/Pattern.js";

/**
 * Pattern = contenu partage (24 steps + triolets), sans longueur.
 * La longueur est un etat par channel (voir SequencerEngine).
 */
describe("Pattern — steps", () => {
  it("defaults to all steps off", () => {
    const pattern = new Pattern();
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(pattern.readStep(i)).toBe(false);
    }
  });

  it("holds thirty-two steps", () => {
    expect(Pattern.DEFAULT_TOTAL_STEPS).toBe(32);
  });

  it("writes and reads every step", () => {
    const pattern = new Pattern();
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(pattern.writeStep(i, i % 2 === 0)).toBe(true);
    }
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(pattern.readStep(i)).toBe(i % 2 === 0);
    }
  });

  it("covers bit boundaries 0,7,8,15,16,23", () => {
    const pattern = new Pattern();
    const boundarySteps = [0, 7, 8, 15, 16, 23];
    for (const step of boundarySteps) expect(pattern.writeStep(step, true)).toBe(true);
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(pattern.readStep(i)).toBe(boundarySteps.includes(i));
    }
  });

  it("rejects step index 32 without mutation", () => {
    const pattern = new Pattern();
    expect(pattern.readStep(32)).toBeNull();
    expect(pattern.writeStep(31, true)).toBe(true);
    expect(pattern.writeStep(32, false)).toBe(false);
    expect(pattern.readStep(31)).toBe(true);
  });

  it("writes and reads the steps above 23", () => {
    const pattern = new Pattern();
    for (const step of [24, 27, 30, 31]) {
      expect(pattern.writeStep(step, true), `step ${step}`).toBe(true);
    }
    for (let i = 24; i < 32; ++i) {
      expect(pattern.readStep(i), `step ${i}`).toBe(
        i === 24 || i === 27 || i === 30 || i === 31,
      );
    }
    expect(pattern.readStep(23)).toBe(false);
  });

  it("clear turns all 24 steps off and removes ratchets", () => {
    const pattern = new Pattern();
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) pattern.writeStep(i, true);
    pattern.setRatchet(0, RATCHET_3);

    pattern.clear();

    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(pattern.readStep(i)).toBe(false);
      expect(pattern.getRatchet(i)).toBe(RATCHET_NONE);
    }
  });
});

describe("Pattern — ratchets (un code par step)", () => {
  it("n'a aucun ratchet par defaut", () => {
    const p = new Pattern();
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(p.getRatchet(i)).toBe(RATCHET_NONE);
    }
  });

  it("ecrit et relit un ratchet par step", () => {
    const p = new Pattern();
    expect(p.setRatchet(0, RATCHET_2)).toBe(true);
    expect(p.setRatchet(1, RATCHET_3)).toBe(true);
    expect(p.setRatchet(31, RATCHET_TRIPLET)).toBe(true);
    expect(p.getRatchet(0)).toBe(RATCHET_2);
    expect(p.getRatchet(1)).toBe(RATCHET_3);
    expect(p.getRatchet(31)).toBe(RATCHET_TRIPLET);
    expect(p.getRatchet(2)).toBe(RATCHET_NONE);
  });

  it("accepte un ratchet sur les steps au-dela de 23", () => {
    const p = new Pattern();
    expect(p.setRatchet(24, RATCHET_6)).toBe(true);
    expect(p.setRatchet(31, RATCHET_4)).toBe(true);
    expect(p.getRatchet(24)).toBe(RATCHET_6);
    expect(p.getRatchet(31)).toBe(RATCHET_4);
    expect(p.getRatchet(25)).toBe(RATCHET_NONE);
  });

  it("accepte un ratchet sur N'IMPORTE quel step (plus de contrainte de groupe)", () => {
    const p = new Pattern();
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(p.setRatchet(i, RATCHET_TRIPLET), `step ${i}`).toBe(true);
    }
    expect(p.getRatchet(29)).toBe(RATCHET_TRIPLET);
    expect(p.getRatchet(31)).toBe(RATCHET_TRIPLET);
  });

  it("rejette un code ou un index invalide", () => {
    const p = new Pattern();
    expect(p.setRatchet(0, 1)).toBe(false);
    expect(p.setRatchet(0, 5)).toBe(false); // non representable a 96 PPQN
    expect(p.setRatchet(32, RATCHET_2)).toBe(false);
    expect(p.getRatchet(0)).toBe(RATCHET_NONE);
    expect(p.getRatchet(32)).toBe(RATCHET_NONE);
  });

  it("expose le nombre de declenchements et la duree occupee", () => {
    expect(ratchetTriggers(RATCHET_NONE)).toBe(1);
    expect(ratchetTriggers(RATCHET_2)).toBe(2);
    expect(ratchetTriggers(RATCHET_4)).toBe(4);
    expect(ratchetTriggers(RATCHET_6)).toBe(6);
    // Le triolet declenche 3 fois mais sur DEUX unites de temps.
    expect(ratchetTriggers(RATCHET_TRIPLET)).toBe(3);
    expect(ratchetSpan(RATCHET_TRIPLET)).toBe(2);
    expect(ratchetSpan(RATCHET_4)).toBe(1);
  });

  it("clear() remet steps et ratchets a zero", () => {
    const p = new Pattern();
    p.writeStep(3, true);
    p.setRatchet(3, RATCHET_4);
    p.clear();
    expect(p.readStep(3)).toBe(false);
    expect(p.getRatchet(3)).toBe(RATCHET_NONE);
  });

  it("les ratchets survivent a l'edition des steps", () => {
    const p = new Pattern();
    p.setRatchet(5, RATCHET_3);
    p.writeStep(5, true);
    p.writeStep(5, false);
    expect(p.getRatchet(5)).toBe(RATCHET_3);
  });
});

describe("setLowStepMask", () => {
  it("ecrit les seize premiers steps et laisse les autres", () => {
    const pattern = new Pattern();
    pattern.writeStep(20, true);
    pattern.setLowStepMask(0x9111);
    const on: number[] = [];
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      if (pattern.readStep(i) === true) on.push(i);
    }
    expect(on).toEqual([0, 4, 8, 12, 15, 20]);
  });
});
