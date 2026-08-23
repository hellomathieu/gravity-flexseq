import { describe, expect, it } from "vitest";
import { PatternBank } from "../src/domain/PatternBank.js";
import { SequencerEngine, PPQN, ChannelMode, CHANNEL_COUNT } from "../src/domain/SequencerEngine.js";
import { Prng } from "../src/domain/Prng.js";
import { TriggerSequencer } from "../src/domain/TriggerSequencer.js";

const STEP = PPQN; // 96 = default ticksPerStep (/1)

describe("TriggerSequencer", () => {
  it("triggers only on the onset of an active step", () => {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    const trig = new TriggerSequencer(bank, engine);
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) engine.setChannelMode(ch, ChannelMode.SEQ);

    engine.setSelectedPattern(0, 0);
    engine.setEffectiveLength(0, 4);
    bank.getPattern(0)!.writeStep(1, true);
    bank.getPattern(0)!.writeStep(3, true);
    engine.start();

    engine.advance(STEP); // onto step 1 (active)

    trig.update();
    expect(trig.triggered(0)).toBe(true);

    engine.advance(STEP); // onto step 2 (inactive)

    trig.update();
    expect(trig.triggered(0)).toBe(false);

    engine.advance(STEP); // onto step 3 (active)

    trig.update();
    expect(trig.triggered(0)).toBe(true);

    engine.advance(STEP); // onto step 0 (inactive)

    trig.update();
    expect(trig.triggered(0)).toBe(false);
  });

  it("does not trigger without a step onset", () => {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    const trig = new TriggerSequencer(bank, engine);
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) engine.setChannelMode(ch, ChannelMode.SEQ);

    bank.getPattern(0)!.writeStep(1, true);
    engine.setEffectiveLength(0, 4);
    engine.start();

    engine.advance(STEP - 1); // no boundary

    trig.update();
    expect(trig.triggered(0)).toBe(false);
  });

  it("fires on multiple channels sharing a pattern", () => {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    const trig = new TriggerSequencer(bank, engine);
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) engine.setChannelMode(ch, ChannelMode.SEQ);

    engine.setSelectedPattern(0, 0);
    engine.setSelectedPattern(1, 0);
    engine.setEffectiveLength(0, 4);
    engine.setEffectiveLength(1, 4);
    bank.getPattern(0)!.writeStep(1, true);
    engine.start();

    engine.advance(STEP); // both onto step 1 (active)

    trig.update();
    expect(trig.triggered(0)).toBe(true);
    expect(trig.triggered(1)).toBe(true);
  });

  it("keeps channels with different patterns independent", () => {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    const trig = new TriggerSequencer(bank, engine);
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) engine.setChannelMode(ch, ChannelMode.SEQ);

    engine.setSelectedPattern(0, 0);
    engine.setSelectedPattern(1, 1);
    engine.setEffectiveLength(0, 4);
    engine.setEffectiveLength(1, 4);
    bank.getPattern(0)!.writeStep(1, true);
    engine.start();

    engine.advance(STEP);

    trig.update();
    expect(trig.triggered(0)).toBe(true);
    expect(trig.triggered(1)).toBe(false);
  });
});

describe("TriggerSequencer — modes (PRD 4.2)", () => {
  function countTriggers(
    trig: TriggerSequencer,
    engine: SequencerEngine,
    channel: number,
    steps: number,
  ): number {
    let kept = 0;
    for (let i = 0; i < steps; ++i) {
      engine.advance(STEP);
      trig.update();
      kept += trig.triggerCount(channel);
    }
    return kept;
  }

  function rig(mode: ChannelMode, skipChance = 0) {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    const trig = new TriggerSequencer(bank, engine);
    engine.setChannelMode(0, mode);
    engine.setSkipChance(0, skipChance);
    engine.start();
    return { bank, engine, trig };
  }

  it("CLOCK declenche a chaque step, pattern vide ou non", () => {
    const { engine, trig } = rig(ChannelMode.CLOCK);
    engine.setEffectiveLength(0, 4);
    expect(countTriggers(trig, engine, 0, 8)).toBe(8);
  });

  it("RANDOM ne saute jamais a 0", () => {
    const { engine, trig } = rig(ChannelMode.RANDOM, 0);
    expect(countTriggers(trig, engine, 0, 64)).toBe(64);
  });

  it("RANDOM saute toujours a 10", () => {
    const { engine, trig } = rig(ChannelMode.RANDOM, 10);
    expect(countTriggers(trig, engine, 0, 64)).toBe(0);
  });

  it("RANDOM a 5 en garde a peu pres la moitie", () => {
    const { engine, trig } = rig(ChannelMode.RANDOM, 5);
    const kept = countTriggers(trig, engine, 0, 1000);
    expect(kept).toBeGreaterThan(400);
    expect(kept).toBeLessThan(600);
  });

  it("RANDOM est reproductible d'une execution a l'autre", () => {
    const a = rig(ChannelMode.RANDOM, 5);
    const b = rig(ChannelMode.RANDOM, 5);
    expect(countTriggers(a.trig, a.engine, 0, 200)).toBe(
      countTriggers(b.trig, b.engine, 0, 200),
    );
  });

  it("une graine differente donne une execution differente", () => {
    const a = rig(ChannelMode.RANDOM, 5);
    const b = rig(ChannelMode.RANDOM, 5);
    b.trig.seed(0x1234);

    let differed = false;
    for (let i = 0; i < 200 && !differed; ++i) {
      a.engine.advance(STEP);
      b.engine.advance(STEP);
      a.trig.update();
      b.trig.update();
      differed = a.trig.triggered(0) !== b.trig.triggered(0);
    }
    expect(differed).toBe(true);
  });

  it("le tirage n'est consomme que sur un step reellement franchi", () => {
    const idle = rig(ChannelMode.RANDOM, 5);
    const dense = rig(ChannelMode.RANDOM, 5);

    for (let i = 0; i < 50; ++i) {
      idle.engine.advance(STEP / 2);
      idle.trig.update();
      expect(idle.trig.triggerCount(0)).toBe(0);
      idle.engine.advance(STEP / 2);
      idle.trig.update();

      dense.engine.advance(STEP);
      dense.trig.update();
      expect(idle.trig.triggerCount(0)).toBe(dense.trig.triggerCount(0));
    }
  });

  it("les comptes ne bougent pas tant qu'update n'est pas rappele", () => {
    const { engine, trig } = rig(ChannelMode.CLOCK);
    engine.advance(STEP);
    expect(trig.triggerCount(0)).toBe(0);
    trig.update();
    expect(trig.triggerCount(0)).toBe(1);
    expect(trig.triggerCount(0)).toBe(1);
  });

  it("un channel hors bornes ne declenche jamais", () => {
    const { engine, trig } = rig(ChannelMode.CLOCK);
    engine.advance(STEP);
    trig.update();
    expect(trig.triggerCount(CHANNEL_COUNT)).toBe(0);
  });
});

describe("Prng", () => {
  it("ne se fige jamais et ne rend jamais zero", () => {
    const prng = new Prng();
    let previous = prng.next();
    for (let i = 0; i < 2000; ++i) {
      const value = prng.next();
      expect(value).not.toBe(0);
      expect(value).not.toBe(previous);
      previous = value;
    }
  });

  it("couvre toute la plage de tirage", () => {
    const prng = new Prng();
    const hit = new Set<number>();
    for (let i = 0; i < 500; ++i) {
      const value = prng.below(10);
      expect(value).toBeLessThan(10);
      hit.add(value);
    }
    expect(hit.size).toBe(10);
  });

  it("donne la meme suite que le C++ pour la graine par defaut", () => {
    const prng = new Prng();
    const golden = [54031, 61861, 5940, 65394, 5969];
    expect(golden.map(() => prng.next())).toEqual(golden);
  });
});
