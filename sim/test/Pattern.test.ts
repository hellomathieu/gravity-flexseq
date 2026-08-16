import { describe, expect, it } from "vitest";
import { Pattern } from "../src/domain/Pattern.js";

/**
 * Tests miroir de gravity-flexseq/test/test_pattern/test_pattern.cpp.
 * Chaque `it(...)` correspond a un `RUN_TEST(...)` cote Unity C++.
 * Le test C++ `has_expected_memory_footprint` (sizeof==7) n'a pas
 * d'equivalent TS : c'est une contrainte de packing C++/AVR, verifiee
 * cote firmware uniquement (voir Pattern.ts).
 */
describe("Pattern — steps & length", () => {
  it("defaults to length 16 and all steps off", () => {
    const pattern = new Pattern();
    expect(pattern.getBaseLength()).toBe(16);
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(pattern.readStep(i)).toBe(false);
    }
  });

  it("accepts every valid base length", () => {
    const pattern = new Pattern();
    for (let length = Pattern.MIN_PATTERN_LENGTH; length <= Pattern.MAX_PATTERN_LENGTH; ++length) {
      expect(pattern.setBaseLength(length)).toBe(true);
      expect(pattern.getBaseLength()).toBe(length);
    }
  });

  it("rejects invalid base lengths without mutation", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(12)).toBe(true);

    expect(pattern.setBaseLength(0)).toBe(false);
    expect(pattern.getBaseLength()).toBe(12);

    expect(pattern.setBaseLength(25)).toBe(false);
    expect(pattern.getBaseLength()).toBe(12);

    expect(pattern.setBaseLength(255)).toBe(false);
    expect(pattern.getBaseLength()).toBe(12);
  });

  it("writes and reads all 24 steps", () => {
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
    for (const step of boundarySteps) {
      expect(pattern.writeStep(step, true)).toBe(true);
    }
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(pattern.readStep(i)).toBe(boundarySteps.includes(i));
    }
  });

  it("rejects step index 24 without mutation", () => {
    const pattern = new Pattern();

    // Hors bornes en lecture : null (equivalent du retour false C++).
    expect(pattern.readStep(24)).toBeNull();

    expect(pattern.writeStep(23, true)).toBe(true);
    expect(pattern.writeStep(24, false)).toBe(false);

    // Le step 23 n'a pas ete affecte par l'ecriture rejetee.
    expect(pattern.readStep(23)).toBe(true);
  });

  it("changing length preserves steps outside length", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(8)).toBe(true);

    expect(pattern.writeStep(8, true)).toBe(true);
    expect(pattern.writeStep(23, true)).toBe(true);

    expect(pattern.setBaseLength(4)).toBe(true);
    expect(pattern.setBaseLength(24)).toBe(true);

    expect(pattern.readStep(8)).toBe(true);
    expect(pattern.readStep(23)).toBe(true);
  });

  it("clear turns all 24 steps off and preserves length", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(12)).toBe(true);

    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(pattern.writeStep(i, true)).toBe(true);
    }

    pattern.clear();

    expect(pattern.getBaseLength()).toBe(12);
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(pattern.readStep(i)).toBe(false);
      expect(pattern.isTripletStep(i)).toBe(false);
    }
  });
});

describe("Pattern — triplets", () => {
  it("has no triplets by default", () => {
    const pattern = new Pattern();
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(pattern.isTripletStart(i)).toBe(false);
      expect(pattern.isTripletStep(i)).toBe(false);
    }
  });

  it("adds triplet at beginning", () => {
    const pattern = new Pattern();
    expect(pattern.addTriplet(0)).toBe(true);

    expect(pattern.isTripletStart(0)).toBe(true);
    expect(pattern.isTripletStep(0)).toBe(true);
    expect(pattern.isTripletStep(1)).toBe(true);
    expect(pattern.isTripletStep(2)).toBe(true);
    expect(pattern.isTripletStep(3)).toBe(false);
  });

  it("adds triplet at end of 24 steps", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(24)).toBe(true);
    expect(pattern.addTriplet(21)).toBe(true);

    expect(pattern.isTripletStart(21)).toBe(true);
    expect(pattern.isTripletStep(21)).toBe(true);
    expect(pattern.isTripletStep(22)).toBe(true);
    expect(pattern.isTripletStep(23)).toBe(true);
    expect(pattern.isTripletStep(20)).toBe(false);
  });

  it("rejects triplet start 22 and 23", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(24)).toBe(true);
    expect(pattern.addTriplet(22)).toBe(false);
    expect(pattern.addTriplet(23)).toBe(false);
  });

  it("rejects triplet outside base length", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(12)).toBe(true);
    expect(pattern.addTriplet(9)).toBe(true);
    expect(pattern.addTriplet(10)).toBe(false);
  });

  it("allows triplet start at any valid position", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(24)).toBe(true);

    expect(pattern.addTriplet(0)).toBe(true);
    expect(pattern.removeTriplet(0)).toBe(true);

    expect(pattern.addTriplet(1)).toBe(true);
    expect(pattern.removeTriplet(1)).toBe(true);

    expect(pattern.addTriplet(2)).toBe(true);
    expect(pattern.removeTriplet(2)).toBe(true);

    expect(pattern.addTriplet(21)).toBe(true);
  });

  it("rejects overlapping triplets", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(24)).toBe(true);
    expect(pattern.addTriplet(3)).toBe(true);

    expect(pattern.addTriplet(1)).toBe(false);
    expect(pattern.addTriplet(2)).toBe(false);
    expect(pattern.addTriplet(4)).toBe(false);
    expect(pattern.addTriplet(5)).toBe(false);

    expect(pattern.isTripletStart(3)).toBe(true);
  });

  it("allows adjacent triplets without overlap", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(24)).toBe(true);
    expect(pattern.addTriplet(0)).toBe(true);
    expect(pattern.addTriplet(3)).toBe(true);

    expect(pattern.isTripletStep(0)).toBe(true);
    expect(pattern.isTripletStep(1)).toBe(true);
    expect(pattern.isTripletStep(2)).toBe(true);
    expect(pattern.isTripletStep(3)).toBe(true);
    expect(pattern.isTripletStep(4)).toBe(true);
    expect(pattern.isTripletStep(5)).toBe(true);
  });

  it("allows maximum eight triplets", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(24)).toBe(true);
    for (const start of [0, 3, 6, 9, 12, 15, 18, 21]) {
      expect(pattern.addTriplet(start)).toBe(true);
    }
  });

  it("rejects duplicate triplet", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(24)).toBe(true);
    expect(pattern.addTriplet(6)).toBe(true);
    expect(pattern.addTriplet(6)).toBe(false);
    expect(pattern.isTripletStart(6)).toBe(true);
  });

  it("removes triplet", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(24)).toBe(true);
    expect(pattern.addTriplet(6)).toBe(true);

    expect(pattern.isTripletStep(6)).toBe(true);
    expect(pattern.isTripletStep(7)).toBe(true);
    expect(pattern.isTripletStep(8)).toBe(true);

    expect(pattern.removeTriplet(6)).toBe(true);

    expect(pattern.isTripletStart(6)).toBe(false);
    expect(pattern.isTripletStep(6)).toBe(false);
    expect(pattern.isTripletStep(7)).toBe(false);
    expect(pattern.isTripletStep(8)).toBe(false);
  });

  it("rejects removing nonexistent triplet", () => {
    const pattern = new Pattern();
    expect(pattern.removeTriplet(6)).toBe(false);
  });

  it("clearTriplets preserves steps and length", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(12)).toBe(true);

    expect(pattern.writeStep(0, true)).toBe(true);
    expect(pattern.writeStep(6, true)).toBe(true);

    expect(pattern.addTriplet(0)).toBe(true);
    expect(pattern.addTriplet(6)).toBe(true);

    pattern.clearTriplets();

    expect(pattern.isTripletStart(0)).toBe(false);
    expect(pattern.isTripletStart(6)).toBe(false);

    expect(pattern.readStep(0)).toBe(true);
    expect(pattern.readStep(6)).toBe(true);
    expect(pattern.getBaseLength()).toBe(12);
  });

  it("clear removes triplets", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(24)).toBe(true);
    expect(pattern.addTriplet(0)).toBe(true);
    expect(pattern.addTriplet(6)).toBe(true);

    pattern.clear();

    expect(pattern.isTripletStart(0)).toBe(false);
    expect(pattern.isTripletStart(6)).toBe(false);
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(pattern.isTripletStep(i)).toBe(false);
    }
  });

  it("triplet metadata survives length reduction", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(16)).toBe(true);
    expect(pattern.addTriplet(13)).toBe(true);

    expect(pattern.isTripletStart(13)).toBe(true);
    expect(pattern.isTripletStep(13)).toBe(true);
    expect(pattern.isTripletStep(14)).toBe(true);
    expect(pattern.isTripletStep(15)).toBe(true);

    expect(pattern.setBaseLength(12)).toBe(true);

    expect(pattern.isTripletStart(13)).toBe(true);
  });

  it("triplet can be reactivated after length increase", () => {
    const pattern = new Pattern();
    expect(pattern.setBaseLength(16)).toBe(true);
    expect(pattern.addTriplet(13)).toBe(true);

    expect(pattern.setBaseLength(12)).toBe(true);
    expect(pattern.isTripletStart(13)).toBe(true);

    expect(pattern.setBaseLength(16)).toBe(true);

    expect(pattern.isTripletStart(13)).toBe(true);
    expect(pattern.isTripletStep(13)).toBe(true);
    expect(pattern.isTripletStep(14)).toBe(true);
    expect(pattern.isTripletStep(15)).toBe(true);
  });
});
