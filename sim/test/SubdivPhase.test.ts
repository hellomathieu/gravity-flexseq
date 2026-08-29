import { describe, expect, it } from "vitest";
import { SequencerEngine, PPQN } from "../src/domain/SequencerEngine.js";
import { Transport } from "../src/domain/Transport.js";
import { SUBDIVS, subdivToTicks } from "../src/domain/subdiv.js";
import { PatternBank } from "../src/domain/PatternBank.js";
import { RATCHET_TRIPLET, RATCHET_NONE } from "../src/domain/Pattern.js";
import { ChannelMode } from "../src/domain/SequencerEngine.js";

function rig() {
  const engine = new SequencerEngine();
  const transport = new Transport(engine);
  return { engine, transport };
}

/** Avance tick par tick et rend les instants d'onset de step des deux channels. */
function onsets(engine: SequencerEngine, from: number, ticks: number) {
  const a: number[] = [];
  const b: number[] = [];
  for (let t = from + 1; t <= from + ticks; ++t) {
    engine.advance(1);
    if (engine.hasStepped(0)) a.push(t);
    if (engine.hasStepped(1)) b.push(t);
  }
  return { a, b };
}

describe("SUBDIV — la phase reste sur la grille du maitre", () => {
  it("a rate change waits for the next beat while the transport runs", () => {
    const { engine, transport } = rig();
    transport.start();
    for (let t = 0; t < 50; ++t) engine.advance(1);

    expect(engine.setSubdiv(0, -2)).toBe(true);
    expect(engine.getSubdiv(0)).toBe(-2);
    expect(engine.getTicksPerStep(0)).toBe(96);

    for (let t = 50; t < 95; ++t) engine.advance(1);
    expect(engine.getTicksPerStep(0)).toBe(96);

    engine.advance(1);
    expect(engine.getTicksPerStep(0)).toBe(48);
  });

  it("a rate change applies at once while the transport is stopped", () => {
    const { engine } = rig();
    expect(engine.setSubdiv(0, -2)).toBe(true);
    expect(engine.getTicksPerStep(0)).toBe(48);
  });

  it("a rate change applies at once when the phase is already on a beat", () => {
    const { engine, transport } = rig();
    transport.start();
    expect(engine.setSubdiv(0, -2)).toBe(true);
    expect(engine.getTicksPerStep(0)).toBe(48);
    for (let t = 0; t < 96; ++t) engine.advance(1);
    expect(engine.setSubdiv(0, 1)).toBe(true);
    expect(engine.getTicksPerStep(0)).toBe(96);
  });

  it("the last selection before the beat is the one that applies", () => {
    const { engine, transport } = rig();
    transport.start();
    for (let t = 0; t < 40; ++t) engine.advance(1);
    engine.setSubdiv(0, -2);
    engine.setSubdiv(0, -4);
    expect(engine.getTicksPerStep(0)).toBe(96);
    for (let t = 40; t < 96; ++t) engine.advance(1);
    expect(engine.getTicksPerStep(0)).toBe(24);
    expect(engine.getSubdiv(0)).toBe(-4);
  });

  it("two channels at the same rate agree after a round trip through x3", () => {
    const { engine, transport } = rig();
    transport.start();
    for (let t = 0; t < 150; ++t) engine.advance(1);
    engine.setSubdiv(0, -3);
    for (let t = 150; t < 260; ++t) engine.advance(1);
    engine.setSubdiv(0, 1);
    const r = onsets(engine, 260, 400);
    const a = r.a.filter((t) => t > 400);
    const b = r.b.filter((t) => t > 400);
    expect(a.length).toBeGreaterThan(0);
    expect(a[0]! - b[0]!).toBe(0);
    for (const t of a) expect(t % 96).toBe(0);
  });

  it("every multiplication returns to the grid", () => {
    for (const sub of SUBDIVS.filter((s) => s < 0)) {
      const { engine, transport } = rig();
      transport.start();
      for (let t = 0; t < 137; ++t) engine.advance(1);
      engine.setSubdiv(0, sub);
      for (let t = 137; t < 611; ++t) engine.advance(1);
      engine.setSubdiv(0, 1);
      const r = onsets(engine, 611, 400);
      const a = r.a.filter((t) => t > 800);
      const b = r.b.filter((t) => t > 800);
      expect(a.length, `SUBDIV ${sub}`).toBeGreaterThan(0);
      expect(a[0]! - b[0]!, `SUBDIV ${sub}`).toBe(0);
    }
  });

  it("every division returns to the grid", () => {
    for (const sub of SUBDIVS.filter((s) => s > 1 && subdivToTicks(s) <= 8 * PPQN)) {
      const { engine, transport } = rig();
      transport.start();
      for (let t = 0; t < 137; ++t) engine.advance(1);
      engine.setSubdiv(0, sub);
      for (let t = 137; t < 1571; ++t) engine.advance(1);
      engine.setSubdiv(0, 1);
      const r = onsets(engine, 1571, 500);
      const a = r.a.filter((t) => t > 1700);
      const b = r.b.filter((t) => t > 1700);
      expect(a.length, `SUBDIV ${sub}`).toBeGreaterThan(0);
      expect(a[0]! - b[0]!, `SUBDIV ${sub}`).toBe(0);
    }
  });

  it("a global reset applies the pending rate instead of dropping it", () => {
    const { engine, transport } = rig();
    transport.start();
    for (let t = 0; t < 40; ++t) engine.advance(1);
    expect(engine.setSubdiv(0, -2)).toBe(true);
    expect(engine.getTicksPerStep(0)).toBe(96);

    engine.reset();
    expect(engine.getTicksPerStep(0)).toBe(48);
    expect(engine.getSubdiv(0)).toBe(-2);

    const r = onsets(engine, 0, 200);
    expect(r.a.length).toBeGreaterThan(0);
    for (const t of r.a) expect(t % 48).toBe(0);
  });

  it("a drained burst of ticks does not lose the pending rate", () => {
    const { engine, transport } = rig();
    transport.start();
    engine.advance(90);
    engine.setSubdiv(0, -2);
    expect(engine.getTicksPerStep(0)).toBe(96);
    engine.advance(9);
    expect(engine.getTicksPerStep(0)).toBe(48);
    const r = onsets(engine, 99, 300);
    const a = r.a.filter((t) => t > 150);
    for (const t of a) expect(t % 48).toBe(0);
  });
});

describe("SUBDIV — aucune autre edition ne decale un channel", () => {
  /** Les deux channels tournent a la meme cadence ; l'edition ne touche que le 0. */
  function driftAfter(act: (engine: SequencerEngine, bank: PatternBank) => void): number {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    engine.setPatternBank(bank);
    for (const ch of [0, 1]) engine.setChannelMode(ch, ChannelMode.SEQ);
    engine.setSelectedPattern(0, 0);
    engine.setSelectedPattern(1, 1);
    const transport = new Transport(engine);
    transport.start();
    for (let t = 0; t < 137; ++t) engine.advance(1);
    act(engine, bank);
    const a: number[] = [];
    const b: number[] = [];
    for (let t = 138; t <= 900; ++t) {
      engine.advance(1);
      if (engine.hasStepped(0)) a.push(t);
      if (engine.hasStepped(1)) b.push(t);
    }
    const t0 = a.filter((t) => t > 600)[0]!;
    const t1 = b.filter((t) => t > 600)[0]!;
    return t0 - t1;
  }

  it("a LENGTH edit does not shift the channel", () => {
    expect(driftAfter((e) => e.setBaseLength(0, 8))).toBe(0);
    expect(driftAfter((e) => e.setBaseLength(0, 1))).toBe(0);
  });

  it("selecting another pattern does not shift the channel", () => {
    expect(driftAfter((e) => e.setSelectedPattern(0, 5))).toBe(0);
  });

  it("a mode change does not shift the channel", () => {
    expect(driftAfter((e) => e.setChannelMode(0, ChannelMode.CLOCK))).toBe(0);
  });

  it("editing the ratchet of the current step does not shift the channel", () => {
    expect(driftAfter((e, bank) => {
      bank.getPattern(0)!.setRatchet(e.effectiveStep(0), RATCHET_TRIPLET);
      e.refreshTiming(0);
    })).toBe(0);
    expect(driftAfter((e, bank) => {
      bank.getPattern(0)!.setRatchet(e.effectiveStep(0), RATCHET_NONE);
      e.refreshTiming(0);
    })).toBe(0);
  });

  it("a setTicksPerStep round trip returns to the grid", () => {
    for (const mid of [24, 32, 48, 192, 384]) {
      const engine = new SequencerEngine();
      const transport = new Transport(engine);
      transport.start();
      for (let t = 0; t < 137; ++t) engine.advance(1);
      engine.setTicksPerStep(0, mid);
      for (let t = 137; t < 611; ++t) engine.advance(1);
      engine.setTicksPerStep(0, 96);
      const r = onsets(engine, 611, 789);
      const a = r.a.filter((t) => t > 900)[0]!;
      const b = r.b.filter((t) => t > 900)[0]!;
      expect(a - b, `mid=${mid}`).toBe(0);
    }
  });
});
