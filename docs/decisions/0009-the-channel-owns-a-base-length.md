# 0009 — The channel owns a base length, the effective length derives from it

- **Status:** accepted
- **Date:** 2026-08-29
- **Supersedes:** —
- **Superseded by:** —

## Context

PRD §5.2 states that the length set on screen and persisted is the **base**, and
that a CV modulation never overwrites it. PRD §10 gives the formula:
`effectiveLength = clamp(base + f(cv), 1, MAX_LENGTH)`.

The code held **one** length per channel, `ChannelState::effectiveLength`, and
the persistence wrote that field. The Length CV does not exist yet, so `f(cv)`
is absent and the two values coincide. The contradiction was therefore
**latent**: a `saveTemplate()` that read `effectiveLength` would persist a
temporary CV modulation as a permanent length the day the CV arrives.

Lot B4b.6 builds the template-to-instance flow, so it needed the answer before
writing `saveTemplate()`.

⚠️ **A historical document places `baseLength` inside `Pattern`.** That model is
superseded. `Pattern` owns **no** length: PRD §5.0 point 3 and ADR 0006 put the
length in the EEPROM template record and in the channel state, and
`static_assert(sizeof(Pattern) == 23)` leaves no room for one. Do not re-derive
the old model from that document.

## Decision

**`ChannelState` carries two distinct values.**

- `baseLength` is the persisted value. The user edits it, `LOAD` restores it,
  and `SAVE` writes it.
- `effectiveLength` is **derived**, and is what the engine plays:
  `effectiveLength = clamp(baseLength + LENGTH_CV_OFFSET, MIN_LENGTH, MAX_LENGTH)`.

`LENGTH_CV_OFFSET` is a named constant equal to **0** at this stage. It is not a
field: a field would cost RAM for a value that cannot vary yet, and the constant
gives the future CV lot a single place to change.

**`SAVE` persists `baseLength`, never `effectiveLength`.**

**`LOAD` preserves the template length, 25 to 36 included.** The clamp applies to
`effectiveLength` alone, so a template of 36 loaded under `MAX_LENGTH = 24` gives
`baseLength = 36` and `effectiveLength = 24`. A load-then-save round trip
therefore returns 36, and destroys nothing.

**The channel record must accept the same range.** Its length byte carries
`baseLength`, from 1 to 36. This changes the accepted values, not the layout:
same byte, same offset, same version 3 image.

**`effectiveLength` has a single structural writer**, the derivation. No other
site assigns it.

**The playhead rule does not change.** The fold stays keyed on the effective
length, and its existing assertions must pass unmodified.

## Consequences

**Two validation domains, by entry point, and that is deliberate.** A manual edit
accepts `[MIN_LENGTH, MAX_LENGTH]`, so `[1, 24]` today. A load from storage
accepts `[MIN_LENGTH, MAX_TEMPLATE_LENGTH]`, so `[1, 36]`. The second entry point
is named for where the value comes from, never for the absence of a check: it
checks, against another bound.

**Measured cost of the field: RAM +6 bytes, Flash −2 bytes** (2026-08-29, commit
`ee6ed26`). The 6 bytes are attributed rather than deduced: the whole gap sits in
the `engine` symbol, 267 to 273 bytes, one byte per channel with no alignment
padding. The Flash figure has no established cause and none is invented.

⚠️ **The single-writer property is NOT provable by behaviour today, and this
limit must not be presented as a guard.** With `LENGTH_CV_OFFSET` at 0, a direct
write to `effectiveLength` produces exactly the value the derivation produces, so
a mutant that bypasses the derivation is an **equivalent mutant**. What is
falsifiable is a derivation that never runs: the value then goes stale and the
suite turns red. The property becomes falsifiable the day `f(cv)` stops being
zero.

**Out of scope, and recorded so it is not discovered later.** The Length CV
itself. And the screen, which will show 24 while the base holds 36: that belongs
to the interface lot that raises `MAX_LENGTH` to 36.

## Alternatives set aside

**Write the contract, keep the code.** `saveTemplate()` would read
`effectiveLength`, and a document would say it is the base. Free, and it rests on
a future lot remembering. This project already carries that failure class — see
line 51 of `docs/open-risks.md`.

**Clamp the base on load.** `baseLength = min(templateLength, MAX_LENGTH)` needs
no second validation domain and no change to the channel record. It was refused
because a load-then-save round trip would silently turn a template of 36 into a
template of 24. It recreates the very trap this decision closes.

**Defer to the CV lot.** The position `WORKPLAN.md` held until 2026-08-29. It was
refused for the same reason: it becomes wrong in silence.

## References

- PRD §5.2 (the base is persisted), §10 (the clamp formula), §11.1 (a template
  length runs from 1 to 36, `MAX_LENGTH` is 24 until lot F)
- ADR 0006 (templates in EEPROM, instances in RAM), ADR 0007 (`sizeof(Pattern)`)
- `include/flexseq/SequencerEngine.h`, `src/domain/SequencerEngine.cpp`
- Measurement: `tools/run-build-memory.sh`, 2026-08-29
