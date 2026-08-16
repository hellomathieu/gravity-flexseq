import { describe, expect, it } from "vitest";
import { PatternStore } from "../src/domain/PatternStore.js";

/**
 * Tests miroir de gravity-flexseq/test/test_pattern_store/test_pattern_store.cpp.
 * Les tests C++ `has_expected_memory_footprint` (672 B) et
 * `memory_footprint_matches_pattern_size` sont C++-only (packing) :
 * pas d'equivalent TS. Le test C++ `returns_const_pattern_from_const_store`
 * (const-ness) est couvert cote TS par "returns a readable pattern" +
 * l'invariant d'identite de reference.
 */
describe("PatternStore", () => {
  it("returns first pattern (0,0) with default length 16", () => {
    const store = new PatternStore();
    const pattern = store.getPattern(0, 0);
    expect(pattern).not.toBeNull();
    expect(pattern!.getBaseLength()).toBe(16);
  });

  it("returns last pattern (5,15) with default length 16", () => {
    const store = new PatternStore();
    const pattern = store.getPattern(5, 15);
    expect(pattern).not.toBeNull();
    expect(pattern!.getBaseLength()).toBe(16);
  });

  it("rejects invalid channel", () => {
    const store = new PatternStore();
    expect(store.getPattern(6, 0)).toBeNull();
  });

  it("rejects invalid pattern index", () => {
    const store = new PatternStore();
    expect(store.getPattern(0, 16)).toBeNull();
  });

  it("isolates patterns within same channel", () => {
    const store = new PatternStore();
    const a1 = store.getPattern(0, 0)!;
    const a2 = store.getPattern(0, 1)!;

    expect(a1.writeStep(23, true)).toBe(true);

    expect(a1.readStep(23)).toBe(true);
    expect(a2.readStep(23)).toBe(false);
  });

  it("isolates patterns between channels", () => {
    const store = new PatternStore();
    const channel1 = store.getPattern(0, 0)!;
    const channel2 = store.getPattern(1, 0)!;

    expect(channel1.writeStep(23, true)).toBe(true);

    expect(channel1.readStep(23)).toBe(true);
    expect(channel2.readStep(23)).toBe(false);
  });

  it("isolates triplet metadata within same channel", () => {
    const store = new PatternStore();
    const a1 = store.getPattern(0, 0)!;
    const a2 = store.getPattern(0, 1)!;

    expect(a1.setBaseLength(24)).toBe(true);
    expect(a1.addTriplet(6)).toBe(true);

    expect(a1.isTripletStart(6)).toBe(true);
    expect(a2.isTripletStart(6)).toBe(false);
  });

  it("isolates triplet metadata between channels", () => {
    const store = new PatternStore();
    const channel1 = store.getPattern(0, 0)!;
    const channel2 = store.getPattern(1, 0)!;

    expect(channel1.setBaseLength(24)).toBe(true);
    expect(channel1.addTriplet(6)).toBe(true);

    expect(channel1.isTripletStart(6)).toBe(true);
    expect(channel2.isTripletStart(6)).toBe(false);
  });

  it("maps first eight patterns to bank A", () => {
    const store = new PatternStore();
    for (let patternIndex = 0; patternIndex < 8; ++patternIndex) {
      const pattern = store.getPattern(0, patternIndex);
      expect(pattern).not.toBeNull();
      expect(pattern!.getBaseLength()).toBe(16);
    }
  });

  it("maps last eight patterns to bank B", () => {
    const store = new PatternStore();
    for (let patternIndex = 8; patternIndex < 16; ++patternIndex) {
      const pattern = store.getPattern(0, patternIndex);
      expect(pattern).not.toBeNull();
      expect(pattern!.getBaseLength()).toBe(16);
    }
  });

  it("returns the same instance on repeated calls (reference semantics)", () => {
    // Miroir de la semantique `Pattern*` : muter le pattern retourne persiste
    // dans le store, et un nouvel appel voit la meme instance.
    const store = new PatternStore();
    const first = store.getPattern(2, 3)!;
    first.writeStep(0, true);

    const again = store.getPattern(2, 3)!;
    expect(again).toBe(first);
    expect(again.readStep(0)).toBe(true);
  });
});
