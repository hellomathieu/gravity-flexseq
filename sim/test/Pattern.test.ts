import { describe, expect, it } from "vitest";
import { Pattern } from "../src/domain/Pattern.js";

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
    for (const step of boundarySteps) expect(pattern.writeStep(step, true)).toBe(true);
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(pattern.readStep(i)).toBe(boundarySteps.includes(i));
    }
  });

  it("rejects step index 24 without mutation", () => {
    const pattern = new Pattern();
    expect(pattern.readStep(24)).toBeNull();
    expect(pattern.writeStep(23, true)).toBe(true);
    expect(pattern.writeStep(24, false)).toBe(false);
    expect(pattern.readStep(23)).toBe(true);
  });

  it("clear turns all 24 steps off and removes triplets", () => {
    const pattern = new Pattern();
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) pattern.writeStep(i, true);
    pattern.addTriplet(0);

    pattern.clear();

    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(pattern.readStep(i)).toBe(false);
      expect(pattern.isTripletStep(i)).toBe(false);
    }
  });
});

describe("Pattern — triplets (independent of length)", () => {
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

  it("adds triplet at end of the 24-step grid", () => {
    const pattern = new Pattern();
    expect(pattern.addTriplet(21)).toBe(true);
    expect(pattern.isTripletStep(21)).toBe(true);
    expect(pattern.isTripletStep(22)).toBe(true);
    expect(pattern.isTripletStep(23)).toBe(true);
    expect(pattern.isTripletStep(20)).toBe(false);
  });

  it("rejects triplet start 22 and 23 (would exceed the grid)", () => {
    const pattern = new Pattern();
    expect(pattern.addTriplet(22)).toBe(false);
    expect(pattern.addTriplet(23)).toBe(false);
  });

  it("allows any valid start position (no length constraint)", () => {
    const pattern = new Pattern();
    for (const start of [0, 1, 2, 21]) {
      expect(pattern.addTriplet(start)).toBe(true);
      expect(pattern.removeTriplet(start)).toBe(true);
    }
  });

  it("rejects overlapping triplets", () => {
    const pattern = new Pattern();
    expect(pattern.addTriplet(3)).toBe(true);
    expect(pattern.addTriplet(1)).toBe(false);
    expect(pattern.addTriplet(2)).toBe(false);
    expect(pattern.addTriplet(4)).toBe(false);
    expect(pattern.addTriplet(5)).toBe(false);
    expect(pattern.isTripletStart(3)).toBe(true);
  });

  it("allows adjacent triplets without overlap", () => {
    const pattern = new Pattern();
    expect(pattern.addTriplet(0)).toBe(true);
    expect(pattern.addTriplet(3)).toBe(true);
    for (let i = 0; i < 6; ++i) expect(pattern.isTripletStep(i)).toBe(true);
  });

  it("allows maximum eight triplets", () => {
    const pattern = new Pattern();
    for (const start of [0, 3, 6, 9, 12, 15, 18, 21]) {
      expect(pattern.addTriplet(start)).toBe(true);
    }
  });

  it("rejects duplicate triplet", () => {
    const pattern = new Pattern();
    expect(pattern.addTriplet(6)).toBe(true);
    expect(pattern.addTriplet(6)).toBe(false);
    expect(pattern.isTripletStart(6)).toBe(true);
  });

  it("removes triplet", () => {
    const pattern = new Pattern();
    expect(pattern.addTriplet(6)).toBe(true);
    expect(pattern.isTripletStep(7)).toBe(true);
    expect(pattern.removeTriplet(6)).toBe(true);
    expect(pattern.isTripletStart(6)).toBe(false);
    expect(pattern.isTripletStep(6)).toBe(false);
  });

  it("rejects removing a nonexistent triplet", () => {
    const pattern = new Pattern();
    expect(pattern.removeTriplet(6)).toBe(false);
  });

  it("clearTriplets preserves steps", () => {
    const pattern = new Pattern();
    pattern.writeStep(0, true);
    pattern.writeStep(6, true);
    pattern.addTriplet(0);
    pattern.addTriplet(6);

    pattern.clearTriplets();

    expect(pattern.isTripletStart(0)).toBe(false);
    expect(pattern.isTripletStart(6)).toBe(false);
    expect(pattern.readStep(0)).toBe(true);
    expect(pattern.readStep(6)).toBe(true);
  });
});
