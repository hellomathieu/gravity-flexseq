# 0012 — The screen geometry lives in a vector file that both languages read

- **Status:** accepted
- **Date:** 2026-09-04
- **Supersedes:** —
- **Superseded by:** —

## Context

A screen geometry exists twice in this project. `MainScreen.h` and
`PatternScreen.h` hold it for the AVR firmware, and `MainScreenDisplay.ts` and
`PatternView.ts` hold it again for the TypeScript preview. The two are
independent by an earlier decision: the preview draws **its own** view, and the
pixel proof stays `tools/run-screen-dump.sh`, which reads the framebuffer the
panel really receives.

That independence is affordable while the two views show different things. It
stops being affordable at lot 16, where both carry the same figures: the tab
slot of 12 px, the transport indicator at `x=121`, the column of three fields.

**Two divergences are already measured, and they show the cost of the copy.**

The preview adds **one pixel of tracking per character** where the firmware
lets the font decide the advance. On `SUBDIVISION`, eleven characters, that is
ten pixels: **47 px in the firmware against 57 in the preview, for a box of
55**. The same label fits on one side and overflows on the other.

The space character of the decoded font atlas carries an advance of **minus
six**. `sim/src/sim/oledFont.ts` knows it and forces three pixels. Any second
reader of that atlas has to know it too, or every space pulls the next word six
pixels backwards.

A method rule of `docs/open-risks.md`, born from risk 68, already states the
class of the problem: **a tool reads a constant of the domain, it never keeps a
copy of it.** Two copies of one geometry diverge.

## Decision

**The geometry lives in a vector file that both languages read.** The file
follows the precedent of `test/vectors/cv_spec_vectors.tsv`: golden cases, a
`family` column that alone decides how a line is read, and an identifier that is
never parsed.

The C++ side **confronts its layout `static_assert` to that file**. The
TypeScript side **sets its view from it**. Neither holds a second copy.

**The mockups of the channel tab move into `sim/` and read the same file.** They
exist today outside the repository and they carry their own copy of the geometry,
which is exactly what this decision forbids.

**Seven counter-proofs are due**, as for the CV vector file: an absent file, an
empty file, zero cases, a malformed line, an unknown family, a false literal, a
duplicated identifier. ⚠️ **A suite that loads zero vectors MUST fail.**

## Consequences

A geometry divergence becomes a **red test** instead of a difference nobody
sees. The two languages stop being able to drift apart in silence, and lot 16
no longer starts by copying figures.

**What this decision does NOT change.** The preview still draws its own view,
and a green preview proves the **design**, never the pixels of the module. The
pixel proof stays `tools/run-screen-dump.sh`. This decision removes a duplicated
geometry; it does not mirror the renderers, and mirroring them is refused —
that would duplicate code with no domain semantics, and it would have to be
proved twice.

**The cost.** A versioned file, a reader in each language, and the seven
counter-proofs. The mockups become maintained code instead of a throwaway file.

**The mechanism is designed with lot 11**, which is the first lot that needs it.
The exact line format and the families are not fixed by this ADR: it fixes that
one file feeds both languages, and that the file is the only source.

### The baseline convention of u8g2, which both languages must share

Added 2026-09-04. u8g2 places the ink of a glyph **entirely above the baseline**:
a glyph of height `h` drawn at the baseline `b` occupies the rows `b - h` to
`b - 1`, and the row `b` itself stays empty. A selection box that leaves one
pixel of air on each side therefore starts at `b - h - 1` and is `h + 2` tall.

This is not a detail of style. A box centred on `b` instead of on the ink is off
by two pixels on one side and one on the other, which is exactly the error the
owner saw in the mockups on 2026-09-03. The TypeScript view must draw from the
same convention, or the two languages will disagree on every box while both
reading the same geometry file.

### A capacity guard is not a layout relation — amended 2026-09-04

`run-geometry-coverage.py` requires the vector file to name every constant a
layout `static_assert` reads. Lot 11 added six guards of another kind: they bound
the **capacity of a scratch buffer**, `sizeof(LBL_SUBDIVISION) <= LABEL_SCRATCH`
and the arithmetic bound of `offset/ticksPerStep`.

The tool skips them, and the difference is of **subject** and not of form. A
layout relation is an inequality with slack, so its value can move while the
relation stays true and nothing would see it — that is why the vector file
exists. A capacity is bounded against a real datum: the guard bites as soon as a
label outgrows its buffer, verified by a counter-proof at 10 against 14.

The rule is mechanical: an expression naming an identifier that ends in
`_SCRATCH` is a capacity guard. **The report says how many it skipped** rather
than staying silent, and a counter-proof confirms the exclusion did not blind
it: an invented constant read by a new layout `static_assert` is still reported
and still exits 1.

## Alternatives set aside

**Mirror the C++ renderers in TypeScript.** Refused. It duplicates code that
carries no domain semantics, and each copy would need its own proof.

**Leave the copy and watch it by hand.** Refused by the measurement above: two
divergences already exist, and neither was noticed until this file was written.

## References

- `test/vectors/cv_spec_vectors.tsv` and `test/vectors/README` — the precedent
- `sim/src/sim/oledFont.ts` — the space advance of minus six, and the tracking
- `include/flexseq/MainScreen.h`, `include/flexseq/PatternScreen.h` — the C++ geometry
- `docs/open-risks.md` — the method rule born from risk 68
- `tools/run-screen-dump.sh` — the pixel proof, which this decision does not replace
- PRD §13 — the roles of TypeScript, of the AVR C++ and of the simulation
