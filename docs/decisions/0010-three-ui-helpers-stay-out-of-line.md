# 0010 — Three UI helpers stay out of line, `clampRange` stays inline

- **Status:** accepted
- **Date:** 2026-08-30
- **Supersedes:** —
- **Superseded by:** —

## Context

The firmware uses 27446 of 30720 bytes of Flash, or 89.3 %. The build guard
refuses at 95 %, which is 29184 bytes. The margin is **1738 bytes**. Lot F and
lot E must fit in that margin.

`WORKPLAN.md` held a Flash saving lot, lot S, with two candidates. Decision QB3
of 2026-08-26 deferred the lot until the version 3 path was active. That path is
active since lot B4b.5, so QB3 became due.

**The two candidates are refuted, and each refutation is measured.**

The first candidate was the engine constructor. The PRD already closed it: the
symbol is worth 88 bytes, and every inlining control returns zero or worse under
`-Os` with LTO. The ELF confirms the reason. The constructor has **no symbol**,
because the compiler inlines it.

The second candidate proposed a `PROGMEM` table of bounds in place of a `switch`
in `UiController::adjustFieldValue`. The measurement refutes it for two reasons.

- **The bounds cost nothing.** Each bound appears once, in a clamp, against an
  immediate value. `CLAUDE.md` records the same result for `MAX_LENGTH`, which
  moved for zero bytes.
- **The seven cases are not uniform.** `FIELD_LENGTH` reads one accessor and
  writes another. `FIELD_SUBDIV` and `FIELD_BAR_LENGTH` each convert through a
  table, and each has a fallback. A table of bounds cannot hold that.

**What the bytes are.** The disassembly of `adjustFieldValue` gives 295
instructions in 598 bytes, with **four** external calls. All the helpers are
inlined, and no helper has a symbol. The cost is the repeated expansion of
`clampIndex`, `clampRange`, `wrapIndex` and `oneStep`.

## Decision

**Three helpers of `src/domain/UiController.cpp` carry
`__attribute__((noinline))`:**

- `clampIndex`
- `wrapIndex`
- `oneStep`

**`clampRange` keeps no attribute, and stays inline.**

The compiler then emits one copy of each of the three, and the call sites use a
call. `clampRange` stays expanded inside `clampIndex` and inside the two direct
call sites.

## Consequences

**Measured cost, nine builds of `env:nanoatmega328` on 2026-08-30:**

| Variant | Flash | Delta |
|---|---:|---:|
| reference | 27446 | — |
| `clampRange` | 27434 | −12 |
| `clampIndex` + `clampRange` | 27382 | −64 |
| `clampIndex` + `oneStep` | 27362 | −84 |
| `clampIndex` | 27358 | −88 |
| the four helpers | 27340 | −106 |
| `wrapIndex` + `oneStep` | 27340 | −106 |
| `clampIndex` + `wrapIndex` | 27332 | −114 |
| **`clampIndex` + `wrapIndex` + `oneStep`** | **27320** | **−126** |

The run returned to the reference and gave 27446 bytes again.

**Flash −126 bytes. Static RAM 0 bytes.**

⚠️ **`clampRange` out of line makes the result worse.** Alone it returns 12
bytes. Added to the three, it takes **20 bytes back**. Its body is 20 bytes out
of line, which is less than the cost of the call. Do not add the attribute to it.

⚠️ **Most of the gain is not in the target of lot S.** `adjustFieldValue` goes
from 598 to 502 bytes, so it returns 96. `UiController::handle` goes from 1042 to
926 bytes, so it returns 116. The three new symbols cost 86 bytes: `oneStep` 22,
`clampIndex` 30, and `wrapIndex` 34. The arithmetic of the total is therefore
−96 −116 +86, which is the measured −126 exactly.

**Stack cost: one byte, at worst.** The runtime probe gives 205 bytes before and
after, with 6 of 6 vectors entered. That probe measures the paths the firmware
takes during the run, and it does not prove that it enters the interface path. A
static bound closes that gap. It reads the frame of each function in the
disassembly.

| Function | Reference | Decision |
|---|---:|---:|
| `UiController::handle` | 19 B (16 registers) | 18 B (15 registers) |
| `adjustFieldValue` | 6 B | 6 B |
| deepest helper | — | 2 B |
| **deepest chain** | **25 B** | **26 B** |

`handle` saves one register less, because the inlined logic that used it is gone.
The three helpers are leaves, and they push nothing. Only the return address
remains. The peak therefore stays at or below **206 bytes**, against a reserve of
256.

⚠️ **Do not remove these three attributes.** The rule of this repository forbids
a comment in the code, so the code cannot say why they are there. A reader who
removes them loses 126 bytes of Flash, and the build gives no warning. This ADR
is the only record of that cost.

**The behaviour does not change.** `noinline` has no semantics. The attribute
changes what the compiler emits, and nothing else.

## Alternatives set aside

**The `PROGMEM` table of bounds.** Refuted above, by two measurements.

**The engine constructor.** Closed by the PRD before this decision. The ELF
confirms it: no symbol, because the compiler inlines it.

**The four helpers together.** It returns 106 bytes, so 20 less than the
decision. `clampRange` is the cause.

**`main` and `PagedScreen::renderFrom`.** They are worth 5976 and 2490 bytes, so
each is larger than the whole of lot S. They stay out of this decision, on the
owner's instruction of 2026-08-30. A mixed perimeter would hide what QB3 gave.
They are recorded as a separate opportunity.

## Amendment — 2026-08-31, the stack bound

**The bound of 206 bytes is passed, and lot LCV passes it.** The decision itself
does not change: the three `noinline` attributes stay, and they still return 126
bytes of Flash. This amendment corrects one number, and the reasoning that
produced it stays valid for the firmware it measured.

**Two reproducible measurements settle it**, both taken on 2026-08-31 with
`tools/run-stack-probe.sh`, on the production firmware, with no instrumentation:

| Commit | Firmware RAM | Stack peak | Vectors |
|---|---:|---:|---|
| `7b7e67a`, the commit that records this decision | 1338 B | **205 B** | 6 of 6 |
| `327f9f1`, the head at the time of this amendment | 1357 B | **207 B** | 6 of 6, two identical runs |

**The chronology explains everything, and no historical figure is wrong.** Three
lots follow each other, and each one moves the peak:

```text
lot S     205 B    measured again on 7b7e67a, so this decision was right
lot F     203 B    the three-row grid RETURNS 2 bytes: the rowCY() formula
                   replaces a ternary that used one register more
lot LCV   207 B    +4 bytes: advance() now reaches latestCalibrated() and the
                   quantiser, two call levels deeper than before
```

⚠️ **The figures 203 and 205 never contradicted each other.** They describe two
different firmwares, and the versioned record `tools/memory-baseline` proves it:
the acceptance after lot S carries Flash 27320, and the acceptance after lot F
carries RAM 1317 and Flash 27030, three hours later. A reader who applies the
delta of lot LCV to the peak of lot S gets 209 and concludes wrongly that one of
the two figures is false. The delta of lot LCV applies to lot F.

**What is left of the bound.** The static bound of this decision reads the frame
of each function in the disassembly, and it gave the deepest interface chain at
26 bytes against 25. That reading is unchanged, and it stays valid: it bounds the
interface path, and not the peak of the whole firmware. Lot LCV added depth
somewhere else, on the engine path, which this decision never measured. The
sentence "the peak therefore stays at or below 206 bytes" therefore holds for the
firmware of 2026-08-30, and for that firmware only.

**The reserve does not move.** `RAM_RESERVE` stays at 256 bytes, so the margin at
the current peak is **49 bytes**, and the reserve covers the peak **1.2×**. The
absolute condition holds widely: 207 bytes against 691 bytes of free RAM.
`docs/open-risks.md` line 66 tracks that margin, and its figure of 207 is now
verified.

**A rule this amendment leaves.** A static bound on one call path does not bound
the peak of the firmware. Only the runtime probe does that, and every lot that
adds call depth must run it **before** it acknowledges its footprint.

## References

- PRD §14 (measured footprint, engine constructor set aside)
- `src/domain/UiController.cpp`, the anonymous namespace at the head of the file
- `CLAUDE.md`, memory discipline, for the `MAX_LENGTH` result at zero bytes
- Measurement: `tools/run-build-memory.sh` and `tools/run-stack-probe.sh`,
  2026-08-30
- Measurement of the amendment: `tools/run-stack-probe.sh`, 2026-08-31, on
  `7b7e67a` and on `327f9f1`
- `tools/memory-baseline`, the versioned record, for the chronology of the three
  lots
- `docs/open-risks.md` line 66, which tracks the margin
