# 0006 — Patterns are templates in EEPROM, instances in RAM

- **Status:** accepted
- **Date:** 2026-08-23
- **Supersedes:** —
- **Superseded by:** —

## Context

PRD §5.1 held one **shared bank of 16 patterns, resident in RAM**. Every channel
pointed into it, so editing a pattern changed what every channel referencing it
played. That mirrored the original firmware on `main`, which holds sixteen shared
arrays.

The owner asked for a different model on 2026-08-23: a **separate pattern-editing
space**, and a channel that loads a pattern as a **template** and then edits its
own copy, without touching the template or any other channel.

**The measured facts that make the choice cheap.**

| | Per pattern | Resident in RAM |
|---|---|---|
| today, 24 steps | 15 B | 16 patterns, **240 B** |
| 32 steps, resident bank | 20 B | 16 patterns, 320 B |
| **32 steps, template model** | 20 B | **6 instances, 120 B** |

Free RAM is 321 B, the measured peak stack is 206 B, and the guard demands 256 B
free. The template model therefore **returns about 120 B** where a resident
32-step bank would cost 80 B more.

⚠️ **The table above reads "today" as of this ADR's date, 2026-08-23. Lot A
changed the first two lines on 2026-08-25**: the pattern is 20 B, and the bank is
the middle line — 16 residents, 320 B — because lot B has not moved the templates
to EEPROM yet. The measured cost of that step was RAM +85 B, not +80: `PagedScreen`
holds a copy of the pattern by value in its frozen frame, and it grew with it.
Free RAM is now 534 B. The third line stays the target, and the arithmetic that
justified the decision does not change.

## Decision

The sixteen patterns are **templates**, and they live in **EEPROM only**. They are
never played from there.

A channel in `SEQ` holds its **own pattern instance in RAM**. Loading a template
copies it into the instance and takes **the LENGTH stored in the template**.

⚠️ **This sentence said "derives the LENGTH from the last non-empty step" until
2026-08-25, and that was refuted by the factory patterns.** The eight patterns
of the original all play sixteen steps, but four of them — A2, A3, A6 and A7 —
have their last active step before index 15. Deriving would give them 12, 13, 15
and 15, so **four of the eight would play a length the original never had**, and
A2 would drift by four steps every cycle.

The information is genuinely absent: a pattern with content up to step 11 and a
length of 16 carries the same bits as the same pattern with a length of 12.
Trailing silence stores nothing, so no function can recover it.

The template record therefore holds the length: **21 bytes**, 20 of content plus
one. `sizeof(Pattern)` stays at 20 — the length is a fact of STORAGE, not of
content, exactly as it already is for the channel, whose EEPROM record has
carried its `effectiveLength` since format version 2. Deriving while storing the
other was the asymmetry that hid the defect.

Rounding up to a multiple of eight would recover all eight factory patterns, and
it must still be refused: it is a fit to eight samples that all happen to be
sixteen steps long, and it would make a length of 12, 20 or 28 unreachable,
which removes the `LEN` field the PRD adds in §1.

The derivation survives for **one** case: a slot that is empty and receives
content for the first time. PRD §5.0 point 3 is the normative source. Editing the instance touches neither the template nor any other channel.
Reloading the template overwrites the instance.

`A1` to `A8` refuse edition. The rule is the index, `index < 8`, so the refusal
costs no storage.

## Consequences

**RAM falls, EEPROM grows.** Both figures must be measured after the change, not
assumed. The expectation is about 120 B returned in RAM.

EEPROM must hold the templates **and** the instances: a power cycle would
otherwise lose the user's work.

| | Bytes |
|---|---|
| 16 templates | 336 |
| 6 channel instances | 120 |
| the rest of the state | about 50 |
| **from address 384** | **up to about 890, of 1024** |

⚠️ **The template line was 320 bytes until 2026-08-25**, when the length moved
into the record: 16 x 21 rather than 16 x 20.

FlexSeq still writes nothing below address 384, so the original's own settings
survive. That rule does not change.

**Timing.** Loading a template is an EEPROM **read**, so it is fast. Saving a
template is a **write**, 3.4 ms per byte, and it must go through the existing
persistence scheduler, which writes one byte on a pass with no onset. A save of a
whole template is therefore spread over many passes, and the interface must not
assume it is finished at once.

**No sharing at play time.** This is the **inverse** of the original's behaviour,
and it is a deliberate divergence. PRD §1 keeps the original's features; this one
is deliberately replaced, and PRD §5.0 says so.

**A gesture budget that is already full.** Saving and loading need gestures, and
the eight plus the ninth are taken. The PATTERNS tab carries them, in its own
context, which is what makes them possible without a tenth gesture.

## Amendment — 2026-08-26

The 36-step foundation (commit `58e238a`) changed the pattern representation
from 20 to 23 bytes:

- 5 bytes of step data;
- 18 bytes of ratchets.

The template record therefore grows from 21 to 24 bytes, because it stores the
pattern content plus its length.

The EEPROM allocation becomes:

| | Bytes |
|---|---:|
| 16 template records | 384 |
| 6 channel instances | 138 |
| the rest of the state | 64 |
| **total, from address 384** | **586, up to address 969** |

"The rest of the state" is 54 bytes of channel records, 3 global bytes, 6
preference bytes and the version byte.

The decision that EEPROM holds both the templates and the instances does not
change.

**The free space is 53 bytes**, from address 970 to address 1022. Address 1023
carries the original firmware's `memCode`, and FlexSeq never writes it. The
`static_assert` in `include/flexseq/Persistence.h` states that rule.

PRD §11.1 must describe the instance area in addition to the template area. It
does not describe it today.

## Alternatives set aside

**Keep the resident shared bank.** At 32 steps it costs 320 B of RAM, 200 B more
than the template model, and it forbids a per-channel variant of a pattern, which
is the whole point of the owner's request.

**A private bank of 16 per channel.** Already set aside in the PRD, at about
560 B of RAM.

## References
- PRD §5.0 and §5.1; §11.1 for the EEPROM format.
- ADR 0007 for the ratchet storage, which sets the pattern size.
- `docs/original-1.2-dev-features.md` for the branch that carries the per-pattern
  length.
- `WORKPLAN.md`, lots A and B.
