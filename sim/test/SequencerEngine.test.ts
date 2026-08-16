import { describe, expect, it } from "vitest";
import {
  SequencerEngine,
  TICKS_PER_SIXTEENTH,
  DEFAULT_LENGTH,
  CHANNEL_COUNT,
} from "../src/domain/SequencerEngine.js";

const STEP = TICKS_PER_SIXTEENTH; // 24 ticks = 1/16 (provisoire)

describe("SequencerEngine — transport & masterPhase", () => {
  it("starts stopped at phase 0 with default per-channel state", () => {
    const e = new SequencerEngine();
    expect(e.masterPhase).toBe(0);
    expect(e.isRunning).toBe(false);
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) {
      expect(e.getEffectiveLength(ch)).toBe(DEFAULT_LENGTH);
      expect(e.effectiveStep(ch)).toBe(0);
    }
  });

  it("does not advance while stopped", () => {
    const e = new SequencerEngine();
    e.advance(STEP);
    expect(e.masterPhase).toBe(0);
  });

  it("advances by ticks only while running", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(); // +1 tick default
    expect(e.masterPhase).toBe(1);
    e.advance(STEP);
    expect(e.masterPhase).toBe(1 + STEP);
  });

  it("stop() preserves the phase; advance after stop is a no-op", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(STEP * 3);
    e.stop();
    expect(e.masterPhase).toBe(STEP * 3);
    e.advance(STEP);
    expect(e.masterPhase).toBe(STEP * 3);
  });

  it("reset() zeroes the phase without changing running state", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(STEP * 5);
    e.reset();
    expect(e.masterPhase).toBe(0);
    expect(e.isRunning).toBe(true);
  });

  it("ignores negative or non-integer tick advances", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(-4);
    e.advance(1.5);
    expect(e.masterPhase).toBe(0);
  });
});

describe("SequencerEngine — effectiveStep derivation", () => {
  it("derives the step as floor(phase / ticksPerStep) % effectiveLength", () => {
    const e = new SequencerEngine();
    e.start();
    expect(e.effectiveStep(0)).toBe(0);

    e.advance(STEP); // one full step
    expect(e.effectiveStep(0)).toBe(1);

    e.advance(STEP - 1); // still within step 1 until the boundary
    expect(e.effectiveStep(0)).toBe(1);

    e.advance(1); // crosses to step 2
    expect(e.effectiveStep(0)).toBe(2);
  });

  it("wraps effectiveStep at effectiveLength", () => {
    const e = new SequencerEngine();
    e.start();
    e.setEffectiveLength(0, 16);
    e.advance(STEP * 16); // one full loop of 16 steps
    expect(e.effectiveStep(0)).toBe(0);
    e.advance(STEP); // step 1 again
    expect(e.effectiveStep(0)).toBe(1);
  });

  it("keeps masterPhase untouched when LENGTH changes", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(STEP * 10);
    const before = e.masterPhase;

    expect(e.setEffectiveLength(0, 4)).toBe(true);
    expect(e.masterPhase).toBe(before);

    expect(e.setEffectiveLength(0, 24)).toBe(true);
    expect(e.masterPhase).toBe(before);
  });

  it("does NOT jump the playhead when LENGTH shrinks but stays within range", () => {
    // Regression : anciennement effectiveStep = absStep % length sautait meme
    // quand localStep < newLength. Desormais (phase locale lissee) : conserve.
    const e = new SequencerEngine();
    e.start();
    e.advance(STEP * 5); // localStep = 5, length 16
    expect(e.effectiveStep(0)).toBe(5);

    expect(e.setEffectiveLength(0, 11)).toBe(true);
    expect(e.effectiveStep(0)).toBe(5); // pas de saut (5 < 11)

    expect(e.setEffectiveLength(0, 8)).toBe(true);
    expect(e.effectiveStep(0)).toBe(5); // pas de saut (5 < 8)
  });

  it("folds the playhead into range only when LENGTH drops at/below it", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(STEP * 13); // localStep = 13
    expect(e.setEffectiveLength(0, 11)).toBe(true);
    expect(e.effectiveStep(0)).toBe(13 % 11); // 2, replie car hors bornes
  });

  it("keeps the current step when LENGTH grows", () => {
    const e = new SequencerEngine();
    e.start();
    e.setEffectiveLength(0, 8);
    e.advance(STEP * 3); // localStep = 3
    expect(e.setEffectiveLength(0, 24)).toBe(true);
    expect(e.effectiveStep(0)).toBe(3); // inchange
    e.advance(STEP); // continue sans saut
    expect(e.effectiveStep(0)).toBe(4);
  });

  it("global reset realigns all channels to step 0", () => {
    const e = new SequencerEngine();
    e.start();
    e.setEffectiveLength(1, 3);
    e.advance(STEP * 7);
    expect(e.effectiveStep(0)).toBeGreaterThan(0);
    e.reset();
    expect(e.effectiveStep(0)).toBe(0);
    expect(e.effectiveStep(1)).toBe(0);
    expect(e.masterPhase).toBe(0);
  });

  it("rejects invalid effectiveLength without mutation", () => {
    const e = new SequencerEngine();
    expect(e.setEffectiveLength(0, 12)).toBe(true);
    expect(e.setEffectiveLength(0, 0)).toBe(false);
    expect(e.setEffectiveLength(0, 25)).toBe(false);
    expect(e.getEffectiveLength(0)).toBe(12);
  });

  it("isolates execution state between channels (same master phase)", () => {
    const e = new SequencerEngine();
    e.start();
    e.setEffectiveLength(0, 16);
    e.setEffectiveLength(1, 3);
    e.advance(STEP * 4);

    expect(e.effectiveStep(0)).toBe(4 % 16); // 4
    expect(e.effectiveStep(1)).toBe(4 % 3); // 1
    expect(e.masterPhase).toBe(STEP * 4);
  });

  it("supports different per-channel ticksPerStep from one master phase", () => {
    const e = new SequencerEngine();
    e.start();
    e.setTicksPerStep(1, STEP * 2); // channel 1 advances half as fast
    e.advance(STEP * 4);

    expect(e.effectiveStep(0)).toBe(4); // 96/24 = 4 steps
    expect(e.effectiveStep(1)).toBe(2); // 96/48 = 2 steps
  });

  it("rejects invalid channel indices and ticksPerStep", () => {
    const e = new SequencerEngine();
    expect(e.effectiveStep(6)).toBe(-1);
    expect(e.setEffectiveLength(6, 8)).toBe(false);
    expect(e.setTicksPerStep(0, 0)).toBe(false);
  });
});

describe("SequencerEngine — per-channel selected pattern", () => {
  it("defaults every channel to pattern 0", () => {
    const e = new SequencerEngine();
    for (let ch = 0; ch < 6; ++ch) expect(e.getSelectedPattern(ch)).toBe(0);
  });

  it("sets and reads a channel's selected pattern independently", () => {
    const e = new SequencerEngine();
    expect(e.setSelectedPattern(0, 3)).toBe(true);
    expect(e.setSelectedPattern(1, 10)).toBe(true);
    expect(e.getSelectedPattern(0)).toBe(3);
    expect(e.getSelectedPattern(1)).toBe(10);
    expect(e.getSelectedPattern(2)).toBe(0);
  });

  it("rejects out-of-range pattern indices and channels", () => {
    const e = new SequencerEngine();
    expect(e.setSelectedPattern(0, -1)).toBe(false);
    expect(e.setSelectedPattern(0, 16)).toBe(false);
    expect(e.setSelectedPattern(6, 0)).toBe(false);
    expect(e.getSelectedPattern(6)).toBe(-1);
  });
});

describe("SequencerEngine — hasStepped (onset)", () => {
  it("reports boundary crossings for the last advance()", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(STEP - 1);
    expect(e.hasStepped(0)).toBe(false);
    e.advance(1); // crosses the first boundary
    expect(e.hasStepped(0)).toBe(true);
    e.advance(1); // within the step
    expect(e.hasStepped(0)).toBe(false);
  });

  it("is false while stopped and for an invalid channel", () => {
    const e = new SequencerEngine();
    e.advance(STEP);
    expect(e.hasStepped(0)).toBe(false);
    expect(e.hasStepped(6)).toBe(false);
  });

  it("is per-channel with different ticksPerStep", () => {
    const e = new SequencerEngine();
    e.start();
    e.setTicksPerStep(1, STEP * 2);
    e.advance(STEP);
    expect(e.hasStepped(0)).toBe(true);
    expect(e.hasStepped(1)).toBe(false);
    e.advance(STEP);
    expect(e.hasStepped(1)).toBe(true);
  });
});
