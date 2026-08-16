import { describe, expect, it } from "vitest";
import { TsReferenceBackend } from "../src/sim/backend.js";

describe("TsReferenceBackend", () => {
  it("exposes the domain dimensions", () => {
    const b = new TsReferenceBackend();
    expect(b.channelCount).toBe(6);
    expect(b.patternsPerChannel).toBe(16);
  });

  it("toggles a step on and off", () => {
    const b = new TsReferenceBackend();
    expect(b.view(0, 0)[3]!.kind).toBe("inactive");

    b.toggleStep(0, 0, 3);
    expect(b.view(0, 0)[3]!.kind).toBe("active");

    b.toggleStep(0, 0, 3);
    expect(b.view(0, 0)[3]!.kind).toBe("inactive");
  });

  it("sets length and rejects invalid values", () => {
    const b = new TsReferenceBackend();
    expect(b.setLength(0, 0, 8)).toBe(true);
    expect(b.getLength(0, 0)).toBe(8);
    expect(b.setLength(0, 0, 99)).toBe(false);
    expect(b.getLength(0, 0)).toBe(8);
  });

  it("adds then removes a triplet via toggle", () => {
    const b = new TsReferenceBackend();
    b.setLength(0, 0, 24);

    expect(b.toggleTriplet(0, 0, 6)).toBe(true);
    expect(b.view(0, 0)[6]!.tripletStart).toBe(true);

    expect(b.toggleTriplet(0, 0, 6)).toBe(true);
    expect(b.view(0, 0)[6]!.tripletStart).toBe(false);
  });

  it("isolates channels", () => {
    const b = new TsReferenceBackend();
    b.toggleStep(0, 0, 0);
    expect(b.view(0, 0)[0]!.kind).toBe("active");
    expect(b.view(1, 0)[0]!.kind).toBe("inactive");
  });
});
