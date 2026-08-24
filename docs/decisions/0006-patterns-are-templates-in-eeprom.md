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

## Decision

The sixteen patterns are **templates**, and they live in **EEPROM only**. They are
never played from there.

A channel in `SEQ` holds its **own pattern instance in RAM**. Loading a template
copies it into the instance and **derives the LENGTH** from the last non-empty
step. Editing the instance touches neither the template nor any other channel.
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
| 16 templates | 320 |
| 6 channel instances | 120 |
| the rest of the state | about 50 |
| **from address 384** | **up to about 874, of 1024** |

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
