import { describe, expect, it } from "vitest";
import {
  DEFAULT_REVERSAL_WINDOW_MS as W,
  EncoderFilter,
  MAX_DELTA,
} from "../src/domain/EncoderFilter.js";

function armed(f: EncoderFilter, atMs = 0): void {
  f.filter(1, atMs);
}

describe("EncoderFilter — the false first movement", () => {
  it("swallows the first reported movement, of either sign", () => {
    const up = new EncoderFilter();
    expect(up.sawFirstMovement).toBe(false);
    expect(up.filter(1, 0)).toBe(0);
    expect(up.sawFirstMovement).toBe(true);
    expect(up.filter(1, 1000)).toBe(1);

    const down = new EncoderFilter();
    expect(down.filter(-3, 0)).toBe(0);
    expect(down.filter(-3, 1000)).toBe(-3);
  });

  it("re-arms the guard on reset", () => {
    const f = new EncoderFilter();
    armed(f);
    expect(f.filter(1, 100)).toBe(1);
    f.reset();
    expect(f.filter(1, 200)).toBe(0);
  });
});

describe("EncoderFilter — the detent bounce", () => {
  it("swallows a reversal inside the window and keeps one outside it", () => {
    const f = new EncoderFilter();
    armed(f);
    expect(f.filter(1, 100)).toBe(1);
    expect(f.filter(-1, 105)).toBe(0);
    expect(f.filter(-1, 100 + W)).toBe(-1);
  });

  it("treats the window boundary as exclusive", () => {
    const inside = new EncoderFilter();
    armed(inside);
    inside.filter(1, 100);
    expect(inside.filter(-1, 100 + W - 1)).toBe(0);

    const boundary = new EncoderFilter();
    armed(boundary);
    boundary.filter(1, 100);
    expect(boundary.filter(-1, 100 + W)).toBe(-1);
  });

  // Le cas observe sur le module : deux crans rapides ne donnaient RIEN.
  it("turns the two-fast-detents case into two events instead of none", () => {
    const f = new EncoderFilter();
    armed(f);
    const reported = [1, -1, 1, -1];
    const at = [100, 103, 106, 109];
    let total = 0;
    for (let i = 0; i < reported.length; ++i) total += f.filter(reported[i]!, at[i]!);
    expect(total).toBe(2);
  });

  it("never swallows a same-direction burst", () => {
    const f = new EncoderFilter();
    armed(f);
    let total = 0;
    for (let i = 0; i < 10; ++i) total += f.filter(1, 100 + i);
    expect(total).toBe(10);
  });

  it("measures the window from the last accepted detent, not from a bounce", () => {
    const f = new EncoderFilter();
    armed(f);
    f.filter(1, 100);
    f.filter(-1, 102);
    expect(f.filter(-1, 100 + W)).toBe(-1);
  });

  it("does not let a zero delta move the window", () => {
    const f = new EncoderFilter();
    armed(f);
    f.filter(1, 100);
    f.filter(0, 100 + W - 2);
    expect(f.filter(-1, 100 + W)).toBe(-1);
  });
});

describe("EncoderFilter — what passes through unchanged", () => {
  it("preserves the dependency's acceleration", () => {
    const f = new EncoderFilter();
    armed(f);
    expect(f.filter(3, 100)).toBe(3);
    expect(f.filter(-2, 1000)).toBe(-2);
  });

  it("judges a reversal on the sign, not the magnitude", () => {
    const f = new EncoderFilter();
    armed(f);
    f.filter(3, 100);
    expect(f.filter(-3, 102)).toBe(0);
  });

  it("clamps an absurd delta to the UI range", () => {
    const f = new EncoderFilter();
    armed(f);
    expect(f.filter(30000, 100)).toBe(MAX_DELTA);
    expect(f.filter(-30000, 1000)).toBe(-MAX_DELTA);
  });
});

describe("EncoderFilter — the threshold is adjustable because it is unmeasured", () => {
  it("honours the window it was given, and a later change of it", () => {
    const f = new EncoderFilter(40);
    expect(f.reversalWindowMs).toBe(40);
    armed(f);
    f.filter(1, 100);
    expect(f.filter(-1, 130)).toBe(0);
    f.setReversalWindowMs(5);
    expect(f.filter(-1, 140)).toBe(-1);
  });

  it("lets everything through with a window of zero", () => {
    const f = new EncoderFilter(0);
    armed(f);
    f.filter(1, 100);
    expect(f.filter(-1, 100)).toBe(-1);
  });

  it("exposes the last interval, which is what the measurement needs", () => {
    const f = new EncoderFilter();
    armed(f);
    f.filter(1, 100);
    expect(f.lastIntervalMs).toBe(100);
    f.filter(1, 137);
    expect(f.lastIntervalMs).toBe(37);
    f.filter(-1, 140);
    expect(f.lastIntervalMs).toBe(3);
  });
});
