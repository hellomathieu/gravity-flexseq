import { describe, expect, it } from "vitest";
import { SUBDIVS, subdivToTicks, DEFAULT_SUBDIV } from "../src/domain/subdiv.js";

describe("subdiv — SUBDIV -> ticksPerStep (libGravity 96 PPQN)", () => {
  it("maps unity (1) to a quarter note (96 ticks)", () => {
    expect(subdivToTicks(1)).toBe(96);
  });

  it("divides (positive) => slower: 96 x v", () => {
    expect(subdivToTicks(2)).toBe(192);
    expect(subdivToTicks(4)).toBe(384);
    expect(subdivToTicks(128)).toBe(12288);
  });

  it("multiplies (negative) => faster: 96 / |v|", () => {
    expect(subdivToTicks(-2)).toBe(48); // 1/8
    expect(subdivToTicks(-4)).toBe(24); // 1/16
    expect(subdivToTicks(-8)).toBe(12); // 1/32
    expect(subdivToTicks(-24)).toBe(4);
  });

  it("default SUBDIV is /1 (quarter = 96 ticks)", () => {
    expect(subdivToTicks(DEFAULT_SUBDIV)).toBe(96);
  });

  it("matches libGravity CLOCK_MOD_PULSES for every official value", () => {
    // CLOCK_MOD (official) -> CLOCK_MOD_PULSES.
    const expected: Record<number, number> = {
      128: 12288, 64: 6144, 32: 3072, 24: 2304, 16: 1536, 12: 1152, 11: 1056,
      10: 960, 9: 864, 8: 768, 7: 672, 6: 576, 5: 480, 4: 384, 3: 288, 2: 192,
      1: 96,
      [-2]: 48, [-3]: 32, [-4]: 24, [-6]: 16, [-8]: 12, [-12]: 8, [-16]: 6, [-24]: 4,
    };
    for (const [v, ticks] of Object.entries(expected)) {
      expect(subdivToTicks(Number(v)), `subdiv ${v}`).toBe(ticks);
    }
  });

  it("every exposed SUBDIV yields an integer tick count >= 1", () => {
    for (const v of SUBDIVS) {
      const t = subdivToTicks(v);
      expect(t, `subdiv ${v}`).toBeGreaterThanOrEqual(1);
      expect(Number.isInteger(t)).toBe(true);
    }
  });

  it("rejects invalid values (0, non-integer)", () => {
    expect(subdivToTicks(0)).toBe(0);
    expect(subdivToTicks(1.5)).toBe(0);
  });
});
