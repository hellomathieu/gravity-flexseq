import { describe, expect, it } from "vitest";
import { PatternBank } from "../src/domain/PatternBank.js";
import { SequencerEngine, TICKS_PER_SIXTEENTH } from "../src/domain/SequencerEngine.js";
import { TriggerSequencer } from "../src/domain/TriggerSequencer.js";

const STEP = TICKS_PER_SIXTEENTH; // 24

describe("TriggerSequencer", () => {
  it("triggers only on the onset of an active step", () => {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    const trig = new TriggerSequencer(bank, engine);

    engine.setSelectedPattern(0, 0);
    engine.setEffectiveLength(0, 4);
    bank.getPattern(0)!.writeStep(1, true);
    bank.getPattern(0)!.writeStep(3, true);
    engine.start();

    engine.advance(STEP); // onto step 1 (active)
    expect(trig.triggered(0)).toBe(true);

    engine.advance(STEP); // onto step 2 (inactive)
    expect(trig.triggered(0)).toBe(false);

    engine.advance(STEP); // onto step 3 (active)
    expect(trig.triggered(0)).toBe(true);

    engine.advance(STEP); // onto step 0 (inactive)
    expect(trig.triggered(0)).toBe(false);
  });

  it("does not trigger without a step onset", () => {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    const trig = new TriggerSequencer(bank, engine);

    bank.getPattern(0)!.writeStep(1, true);
    engine.setEffectiveLength(0, 4);
    engine.start();

    engine.advance(STEP - 1); // no boundary
    expect(trig.triggered(0)).toBe(false);
  });

  it("fires on multiple channels sharing a pattern", () => {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    const trig = new TriggerSequencer(bank, engine);

    engine.setSelectedPattern(0, 0);
    engine.setSelectedPattern(1, 0);
    engine.setEffectiveLength(0, 4);
    engine.setEffectiveLength(1, 4);
    bank.getPattern(0)!.writeStep(1, true);
    engine.start();

    engine.advance(STEP); // both onto step 1 (active)
    expect(trig.triggered(0)).toBe(true);
    expect(trig.triggered(1)).toBe(true);
  });

  it("keeps channels with different patterns independent", () => {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    const trig = new TriggerSequencer(bank, engine);

    engine.setSelectedPattern(0, 0);
    engine.setSelectedPattern(1, 1);
    engine.setEffectiveLength(0, 4);
    engine.setEffectiveLength(1, 4);
    bank.getPattern(0)!.writeStep(1, true);
    engine.start();

    engine.advance(STEP);
    expect(trig.triggered(0)).toBe(true);
    expect(trig.triggered(1)).toBe(false);
  });
});
