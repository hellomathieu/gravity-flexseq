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

## References

- PRD §14 (measured footprint, engine constructor set aside)
- `src/domain/UiController.cpp`, the anonymous namespace at the head of the file
- `CLAUDE.md`, memory discipline, for the `MAX_LENGTH` result at zero bytes
- Measurement: `tools/run-build-memory.sh` and `tools/run-stack-probe.sh`,
  2026-08-30
