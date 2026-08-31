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
checks, against another bound. ⚠️ **The amendment of 2026-08-30 changes the two
bounds to the same value. It does not change this rule.**

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
to lot SF3, which raises `MAX_LENGTH` to 36. See the amendment of 2026-08-30.

## Amendment — 2026-08-30 (lot SF3)

Lot SF3 moves `MAX_LENGTH` from 24 to 36. The two validation domains now hold the
same value:

```text
setBaseLength()             [1, 24]  ->  [1, 36]
setBaseLengthFromStorage()  [1, 36]  ->  [1, 36]
```

**The two entry points stay, and both bounds stay.** The distinction is no longer
a distinction of value. It is a distinction of responsibility:

- `setBaseLength()` is the user path. Its bound is the interface cap;
- `setBaseLengthFromStorage()` is the codec path. Its bound is the pattern
  capacity, through `MAX_STORED_LENGTH`.

Do not merge the two functions, and do not delete `MAX_STORED_LENGTH`. Two facts
make the convergence temporary. The persistence format must carry `baseLength`
without the interface cap. And `LENGTH_CV_OFFSET` becomes variable with the
Length CV. A merge would repair nothing and would remove a boundary the project
needs again.

⚠️ **The convergence costs five mutants, and the loss is recorded here rather
than absorbed.** Two properties disappear together. `baseLength` and
`effectiveLength` hold the same value at all times, because `LENGTH_CV_OFFSET`
is 0. And the two entry points carry the same bound. No test separates either
pair, so five mutants become **equivalent mutants**:

- `cpp: the channel record restores the base through the manual entry point`;
- `cpp: loadTemplate restores the length through the manual entry point`;
- `ts: loadTemplate restores the length through the manual entry point`;
- `cpp: saveTemplate serialises the effective length`;
- `ts: saveTemplate serialises the effective length`.

The five leave the denominator. Lot SF3 adds one TypeScript mutant on
`MAX_LENGTH`, which closes a gap the C++ side never had. The denominator
therefore goes from 230 to **226**.

⚠️ **Three of the five were found by the probe, not by the audit.** The audit of
lot SF3a read the mutants that name `MAX_LENGTH`, and it found the mutants that
name a constant. It did not find the mutants that name a **function**. The sweep
that finds the whole class searches the probe for `setBaseLengthFromStorage` and
for `getBaseLength`, never for the bound. This is the same failure the method
rules of `docs/open-risks.md` already describe: a search on a name does not find
a dependence on an effect.

⚠️ **Two tests lose their discriminating power at the same time, and they were
renamed rather than left to claim a proof they no longer carry.**
`test_save_template_writes_the_base_length_not_the_effective_one` and its
TypeScript twin now read `test_save_template_writes_the_channel_length_into_the_record`
and `ecrit la longueur du canal dans l'enregistrement`. They still assert that
`saveTemplate()` writes the channel length at the length offset. They no longer
assert **which of the two fields** it reads. That proof returns with the Length
CV, and not before.

⚠️ **The screen no longer shows a length the channel cannot play.** The paragraph
under Consequences said the screen would show 24 while the base held 36. Lot SF3
ends that state: the base, the effective length and the grid all stop at 36.

**The single-writer limit is unchanged.** `refreshEffectiveLength()` stays the
only writer of `effectiveLength`, and that property stays unprovable by
behaviour while `LENGTH_CV_OFFSET` is 0.

## Amendment — 2026-08-30 (lot LCV.4a)

`LENGTH_CV_OFFSET` stops being a constant. Lot LCV gives each channel its own
offset, driven by the Length CV, so the value varies per channel and over time.

**The argument that made it a constant is spent.** This decision recorded that a
field would cost RAM for a value that cannot vary. The value now varies. The
constant becomes a per-channel state, and the RAM it costs buys a feature instead
of nothing.

**Three invariants survive, and they are why this amendment is short.**

- `refreshEffectiveLength()` stays the **single writer** of `effectiveLength`. The
  offset changes where the input comes from, never who writes the result;
- `baseLength` stays the persisted value. The CV never writes it, and
  `saveTemplate()` keeps reading the base;
- the derivation keeps its shape, `clamp(baseLength + offset, MIN_LENGTH,
  MAX_LENGTH)`. Only `offset` moves from an immediate to a field.

**The single-writer property becomes falsifiable.** The amendment of lot SF3
states the condition: while the offset is 0, a direct write to `effectiveLength`
produces the value the derivation produces, so that mutant is equivalent. A
varying offset separates the two, and the property can be tested.

⚠️ **TWO of the five equivalent mutants of lot SF3 come back, not five, and the
measurement of lot LCV.3c is what says so.** This paragraph announced five, for
one reason. The five depend on **two different properties**, and only one of them
changes with the Length CV.

- `cpp` and `ts: saveTemplate serialises the effective length` depend on
  `baseLength` differing from `effectiveLength`. The Length CV makes that
  reachable, so **these two come back**;
- `cpp: the channel record restores the base through the manual entry point` and
  the two `loadTemplate` ones swap `setBaseLengthFromStorage()` for
  `setBaseLength()`. They depend on the two entry points having **different
  bounds**. Lot SF3 made those bounds equal, both `[1, 36]`, and lot LCV did not
  undo it: the two functions still have the same body and the same domain. **These
  three stay equivalent.**

⚠️ **And the two that came back did not come back on their own.** Restored, they
survived — not because they were equivalent, but because **no test exercised
`saveTemplate()` under an active modulation**. That is a coverage gap, not an
equivalence: an equivalent mutant can never be red, this one only needed the
test. Two tests, one per language, now assert that a base of 18 modulated to 28
still stores 18. They are the first tests of this decision's own rule — SAVE
writes the base — and they were **impossible to write before the seam existed**.

The denominator is **232, measured** on 2026-08-31 with a full pass at 232/232.
⚠️ **This line said 2026-08-30 until 2026-08-31.** The two mutants that make the
denominator 232 were restored by commit `0de899c`, dated 2026-08-31, so the pass
that counts them cannot precede it.

**Out of scope of this amendment.** The mapping from CV to offset, which lot
LCV.2 decides. The routing, which PRD §10.2 holds. The RECORDING freeze, which
PRD §5.5 holds since 2026-08-30.

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

- PRD §5.2 (the base is persisted), §10 (the clamp formula), §10.2 (the CV
  routing), §5.5 (LENGTH is frozen during a RECORDING), §11.1 (a template
  length runs from 1 to 36, `MAX_LENGTH` is 24 until lot SF3)
- ADR 0006 (templates in EEPROM, instances in RAM), ADR 0007 (`sizeof(Pattern)`)
- `include/flexseq/SequencerEngine.h`, `src/domain/SequencerEngine.cpp`
- Measurement: `tools/run-build-memory.sh`, 2026-08-29
