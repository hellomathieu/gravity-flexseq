import { describe, it, expect } from "vitest";
import { SequencerEngine, ChannelMode } from "../src/domain/SequencerEngine";
import { Transport } from "../src/domain/Transport";

const STEP = 96;

describe("le transport — D79", () => {
  it("start() arme et le premier tick sonne le step 0", () => {
    const e = new SequencerEngine();
    const t = new Transport(e);
    for (let ch = 0; ch < 6; ++ch) e.setChannelMode(ch, ChannelMode.SEQ);
    t.resume();
    t.tick(5 * STEP);
    t.start();
    expect(e.masterPhase).toBe(0);
    t.tick(1);
    for (let ch = 0; ch < 6; ++ch) {
      expect(e.onsetCount(ch)).toBe(1);
      expect(e.effectiveStep(ch)).toBe(0);
    }
  });

  it("resume() n'arme pas", () => {
    const e = new SequencerEngine();
    const t = new Transport(e);
    e.setChannelMode(0, ChannelMode.SEQ);
    t.start();
    t.tick(1);
    t.stop();
    t.resume();
    t.tick(1);
    expect(e.onsetCount(0)).toBe(0);
  });
});
