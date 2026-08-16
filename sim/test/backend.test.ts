import { describe, expect, it } from "vitest";
import { TsReferenceBackend } from "../src/sim/backend.js";

describe("TsReferenceBackend — dimensions & edition", () => {
  it("exposes 6 channels and a shared bank of 16 patterns", () => {
    const b = new TsReferenceBackend();
    expect(b.channelCount).toBe(6);
    expect(b.patternCount).toBe(16);
  });

  it("toggles a step on and off (on the channel's selected pattern)", () => {
    const b = new TsReferenceBackend();
    expect(b.view(0)[3]!.kind).toBe("inactive");
    b.toggleStep(0, 3);
    expect(b.view(0)[3]!.kind).toBe("active");
    b.toggleStep(0, 3);
    expect(b.view(0)[3]!.kind).toBe("inactive");
  });

  it("length is per-channel", () => {
    const b = new TsReferenceBackend();
    expect(b.setLength(0, 8)).toBe(true);
    expect(b.getLength(0)).toBe(8);
    expect(b.getLength(1)).toBe(16); // inchange
    expect(b.setLength(0, 99)).toBe(false);
    expect(b.getLength(0)).toBe(8);
  });

  it("adds then removes a triplet via toggle", () => {
    const b = new TsReferenceBackend();
    expect(b.toggleTriplet(0, 6)).toBe(true);
    expect(b.view(0)[6]!.tripletStart).toBe(true);
    expect(b.toggleTriplet(0, 6)).toBe(true);
    expect(b.view(0)[6]!.tripletStart).toBe(false);
  });
});

describe("TsReferenceBackend — shared patterns", () => {
  it("shares content when two channels select the same pattern", () => {
    const b = new TsReferenceBackend();
    b.setSelectedPattern(0, 0); // CH1 -> A1
    b.setSelectedPattern(1, 0); // CH2 -> A1 (meme pattern)

    b.toggleStep(0, 5); // edite A1 depuis CH1
    expect(b.view(1)[5]!.kind).toBe("active"); // visible sur CH2
  });

  it("keeps channels independent when they select different patterns", () => {
    const b = new TsReferenceBackend();
    b.setSelectedPattern(0, 0); // A1
    b.setSelectedPattern(1, 1); // A2

    b.toggleStep(0, 5);
    expect(b.view(0)[5]!.kind).toBe("active");
    expect(b.view(1)[5]!.kind).toBe("inactive");
  });

  it("shared content, but length stays per-channel", () => {
    const b = new TsReferenceBackend();
    b.setSelectedPattern(0, 0);
    b.setSelectedPattern(1, 0); // meme pattern A1
    b.setLength(0, 16);
    b.setLength(1, 8);

    b.toggleStep(0, 3);
    expect(b.view(1)[3]!.kind).toBe("active"); // contenu partage
    expect(b.getLength(0)).toBe(16);
    expect(b.getLength(1)).toBe(8); // longueur distincte
    expect(b.view(1)[10]!.kind).toBe("beyond"); // au-dela des 8 steps de CH2
    expect(b.view(0)[10]!.kind).toBe("inactive"); // dans les 16 de CH1
  });

  it("get/set selected pattern per channel", () => {
    const b = new TsReferenceBackend();
    b.setSelectedPattern(2, 9);
    expect(b.getSelectedPattern(2)).toBe(9);
    expect(b.getSelectedPattern(0)).toBe(0);
  });
});

describe("TsReferenceBackend — transport", () => {
  it("advances the master phase only while playing", () => {
    const b = new TsReferenceBackend();
    b.advanceTicks(24);
    expect(b.masterPhase()).toBe(0);
    b.play();
    b.advanceTicks(24);
    expect(b.masterPhase()).toBe(24);
  });

  it("pause preserves phase; reset zeroes it", () => {
    const b = new TsReferenceBackend();
    b.play();
    b.advanceTicks(48);
    b.pause();
    expect(b.isPlaying()).toBe(false);
    expect(b.masterPhase()).toBe(48);
    b.resetPhase();
    expect(b.masterPhase()).toBe(0);
  });

  it("effectiveStep follows the per-channel length", () => {
    const b = new TsReferenceBackend();
    b.setLength(0, 4);
    b.play();
    b.advanceTicks(24 * 5); // 5 steps at 1/16
    expect(b.effectiveStep(0)).toBe(5 % 4); // 1
  });
});
