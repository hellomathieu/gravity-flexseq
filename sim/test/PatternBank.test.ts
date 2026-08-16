import { describe, expect, it } from "vitest";
import { PatternBank, PATTERN_COUNT } from "../src/domain/PatternBank.js";

describe("PatternBank — 16 shared patterns", () => {
  it("exposes 16 patterns", () => {
    const bank = new PatternBank();
    for (let i = 0; i < PATTERN_COUNT; ++i) {
      expect(bank.getPattern(i)).not.toBeNull();
    }
  });

  it("rejects out-of-range indices", () => {
    const bank = new PatternBank();
    expect(bank.getPattern(-1)).toBeNull();
    expect(bank.getPattern(16)).toBeNull();
    expect(bank.getPattern(1.5)).toBeNull();
  });

  it("returns the SAME instance on repeated calls (shared reference)", () => {
    const bank = new PatternBank();
    const a = bank.getPattern(3)!;
    a.writeStep(0, true);
    const again = bank.getPattern(3)!;
    expect(again).toBe(a);
    expect(again.readStep(0)).toBe(true);
  });

  it("keeps distinct patterns independent", () => {
    const bank = new PatternBank();
    bank.getPattern(0)!.writeStep(5, true);
    expect(bank.getPattern(1)!.readStep(5)).toBe(false);
  });
});
