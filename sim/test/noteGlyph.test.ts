import { describe, expect, it } from "vitest";
import {
  subdivGlyph,
  makeNoteBitmap,
  GLYPH_W,
  GLYPH_H,
  type NoteType,
} from "../src/domain/noteGlyph.js";
import { SUBDIVS } from "../src/domain/subdiv.js";

describe("noteGlyph — SUBDIV → note mapping", () => {
  it("maps the 7 pure power-of-2 notes", () => {
    expect(subdivGlyph(4)).toEqual({ note: "ronde", dotted: false, triplet: false });
    expect(subdivGlyph(2)).toEqual({ note: "blanche", dotted: false, triplet: false });
    expect(subdivGlyph(1)).toEqual({ note: "noire", dotted: false, triplet: false });
    expect(subdivGlyph(-2)).toEqual({ note: "croche", dotted: false, triplet: false });
    expect(subdivGlyph(-4)).toEqual({ note: "double", dotted: false, triplet: false });
    expect(subdivGlyph(-8)).toEqual({ note: "triple", dotted: false, triplet: false });
    expect(subdivGlyph(-16)).toEqual({ note: "quadruple", dotted: false, triplet: false });
  });

  it("maps dotted notes: /3 = dotted half, /6 = dotted whole", () => {
    expect(subdivGlyph(3)).toEqual({ note: "blanche", dotted: true, triplet: false });
    expect(subdivGlyph(6)).toEqual({ note: "ronde", dotted: true, triplet: false });
  });

  it("maps triplets: x3, x6, x12, x24 carry a '3'", () => {
    expect(subdivGlyph(-3)).toEqual({ note: "croche", dotted: false, triplet: true });
    expect(subdivGlyph(-6)).toEqual({ note: "double", dotted: false, triplet: true });
    expect(subdivGlyph(-12)).toEqual({ note: "triple", dotted: false, triplet: true });
    expect(subdivGlyph(-24)).toEqual({ note: "quadruple", dotted: false, triplet: true });
  });

  it("returns null (text fallback) for non-note values", () => {
    for (const v of [5, 7, 8, 9, 10, 11, 12, 16, 24, 32, 64, 128]) {
      expect(subdivGlyph(v), `subdiv ${v}`).toBeNull();
    }
  });

  it("covers exactly 13 of the 25 SUBDIV values with a glyph", () => {
    const withGlyph = SUBDIVS.filter((v) => subdivGlyph(v) !== null);
    expect(SUBDIVS.length).toBe(25);
    expect(withGlyph.length).toBe(13);
  });
});

describe("noteGlyph — bitmaps", () => {
  const count = (g: number[][]) => g.flat().filter((p) => p === 1).length;
  const col4 = (g: number[][]) => g.some((row) => row[4] === 1);

  it("produces a GLYPH_H × GLYPH_W matrix", () => {
    const g = makeNoteBitmap("noire");
    expect(g).toHaveLength(GLYPH_H);
    expect(g[0]).toHaveLength(GLYPH_W);
  });

  it("ronde has no stem; blanche and noire do", () => {
    expect(col4(makeNoteBitmap("ronde"))).toBe(false);
    expect(col4(makeNoteBitmap("blanche"))).toBe(true);
    expect(col4(makeNoteBitmap("noire"))).toBe(true);
  });

  it("filled notehead has more ink than an open one", () => {
    expect(count(makeNoteBitmap("noire"))).toBeGreaterThan(count(makeNoteBitmap("blanche")));
  });

  it("more flags means more ink (croche < double < triple < quadruple)", () => {
    const notes: NoteType[] = ["croche", "double", "triple", "quadruple"];
    const inks = notes.map((n) => count(makeNoteBitmap(n)));
    for (let i = 1; i < inks.length; ++i) expect(inks[i]!).toBeGreaterThan(inks[i - 1]!);
  });

  it("the augmentation dot adds ink to a dotted note", () => {
    expect(count(makeNoteBitmap("blanche", true))).toBeGreaterThan(
      count(makeNoteBitmap("blanche", false)),
    );
  });
});
