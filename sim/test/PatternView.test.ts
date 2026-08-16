import { describe, expect, it } from "vitest";
import { Pattern } from "../src/domain/Pattern.js";
import { viewPattern, toAscii, CELL_SYMBOL } from "../src/sim/PatternView.js";

describe("PatternView — viewPattern", () => {
  it("always projects exactly 24 cells", () => {
    expect(viewPattern(new Pattern(), 16)).toHaveLength(24);
  });

  it("marks positions beyond the channel length as 'beyond'", () => {
    const p = new Pattern();
    const cells = viewPattern(p, 14);

    expect(cells[13]!.kind).toBe("inactive"); // dernier step actif, off
    expect(cells[14]!.kind).toBe("beyond");
    expect(cells[23]!.kind).toBe("beyond");
  });

  it("distinguishes active vs inactive within length", () => {
    const p = new Pattern();
    p.writeStep(0, true);
    p.writeStep(5, true);
    const cells = viewPattern(p, 16);

    expect(cells[0]!.kind).toBe("active");
    expect(cells[1]!.kind).toBe("inactive");
    expect(cells[5]!.kind).toBe("active");
  });

  it("reports beyond-length even if the stored step is active (display rule)", () => {
    const p = new Pattern();
    p.writeStep(20, true); // conserve hors longueur, mais affiche '•'
    const cells = viewPattern(p, 8);

    expect(cells[20]!.kind).toBe("beyond");
  });

  it("flags triplet membership and start", () => {
    const p = new Pattern();
    p.addTriplet(3);
    const cells = viewPattern(p, 24);

    expect(cells[3]!.tripletStart).toBe(true);
    expect(cells[3]!.tripletStep).toBe(true);
    expect(cells[4]!.tripletStep).toBe(true);
    expect(cells[5]!.tripletStep).toBe(true);
    expect(cells[4]!.tripletStart).toBe(false);
    expect(cells[6]!.tripletStep).toBe(false);
  });
});

describe("PatternView — toAscii", () => {
  it("renders 2 rows of 12 with length 16, all off", () => {
    const ascii = toAscii(viewPattern(new Pattern(), 16));
    const lines = ascii.split("\n");

    // marqueur1, cellules1, marqueur2, cellules2
    expect(lines).toHaveLength(4);
    expect(lines[1]).toBe("□".repeat(12)); // steps 0..11 : dans la longueur, off
    expect(lines[3]).toBe("□".repeat(4) + "•".repeat(8)); // 12..15 off, 16..23 beyond
    expect(lines[0]!.trim()).toBe(""); // pas de triolet
    expect(lines[2]!.trim()).toBe("");
  });

  it("draws the triplet marker above a group", () => {
    const p = new Pattern();
    p.addTriplet(0);
    const lines = toAscii(viewPattern(p, 16)).split("\n");

    expect(lines[0]).toBe("‾‾‾" + " ".repeat(9));
    expect(lines[1]!.startsWith(CELL_SYMBOL.inactive)).toBe(true);
  });
});
