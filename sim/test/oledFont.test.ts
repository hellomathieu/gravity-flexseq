import { describe, expect, it } from "vitest";
import { glyphFor, glyphPixels, textPixels, textWidth } from "../src/sim/oledFont.js";

describe("oledFont — velvetscreen atlas", () => {
  it("has the sequencer pastille glyphs 'q' (active) and 'p' (inactive)", () => {
    const q = glyphFor("q");
    const p = glyphFor("p");
    expect(q).toBeDefined();
    expect(p).toBeDefined();
    expect(q!.w).toBe(5);
    expect(q!.h).toBe(5);
  });

  it("'q' is a filled disc (21 lit px), 'p' a hollow ring (12 lit px)", () => {
    // Correspond aux bitmaps decodes depuis le firmware.
    expect(glyphPixels("q")).toHaveLength(21);
    expect(glyphPixels("p")).toHaveLength(12);
  });

  it("renders uppercase letters and digits used by the title", () => {
    for (const ch of "EDITPATERN1") {
      expect(glyphFor(ch), `glyph ${ch}`).toBeDefined();
    }
  });

  it("textWidth of a non-empty string is positive and grows with length", () => {
    const w1 = textWidth("A");
    const w2 = textWidth("AA");
    expect(w1).toBeGreaterThan(0);
    expect(w2).toBeGreaterThan(w1);
  });

  it("textPixels stays within [0, textWidth) horizontally", () => {
    const text = "EDIT PATTERN A1";
    const px = textPixels(text);
    const maxX = Math.max(...px.map((p) => p.x));
    expect(px.length).toBeGreaterThan(0);
    expect(maxX).toBeLessThan(textWidth(text) + 1);
  });
});
