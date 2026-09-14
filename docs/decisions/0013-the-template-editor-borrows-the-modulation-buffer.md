# 0013 — The template editor borrows the modulation buffer of channel 1

- **Status:** superseded
- **Date:** 2026-09-13
- **Supersedes:** —
- **Superseded by:** `PRD.md` §5.0, amendment of 2026-09-14 (third of the day)

> ⚠️ **SUPERSEDED ON 2026-09-14, and the subject of this decision is REMOVED.**
> The owner removed the `PATTERNS` tab and its template editor, for the Flash
> budget: a counter-build measured **896 bytes**, and those bytes buy lots 22,
> K, 17a and 14. This ADR answered "where does the template under edit live";
> after the removal nothing is under edit, so the question has no subject.
>
> ⚠️ **THE CODE STILL CARRIES THE EDITOR at the time of this note.** The decision
> is taken and `PRD.md` records it; the removal is the work that follows. Read
> this ADR as the record of why the editor borrowed the buffer, never as a
> description of the firmware.
>
> **What the removal does NOT touch:** ADR 0006 and the template / instance
> model, the modulation buffer itself, and its round-robin service. They stay,
> and ADR 0011 stays with them.

## Context

Lot 16E step 4 gives the `PATTERNS` tab its content. PRD §5.0 point 9 puts a
template editor there, with an audition through channel 1.

The template under edit must be reachable by four readers:

- the renderer, which draws the grid;
- `UiController`, which toggles a step, sets a ratchet, and clears;
- the engine, which plays the template on channel 1;
- the deferred EEPROM write, which publishes the 24 bytes of the record.

The editor must not write the instance of channel 1. The acceptance criterion of
lot E says that an edit of a template affects no channel before a reload.

**Two facts of the repository bear on the choice, and both are measured.**

Lot E3.6.3 measured the placement of a pattern buffer. The same 138 bytes held
as members of `SequencerEngine` cost **+318 bytes** of Flash. Held outside, with
one pointer inside the engine, they cost **+24**. The engine object must not
grow.

The modulation buffer already holds what the editor needs.
`ModulatedPatternState` carries one `Pattern` and one length byte per channel.
`SequencerEngine::patternForChannel()` returns that buffer, through a **writable**
overload, as soon as `loaded[channel]` carries an index
(`src/domain/SequencerEngine.cpp:209`).

`serviceOneModulationTemplateLoad()` releases every channel that is not routed
to `PATTERN` and in `SEQ`. It does so on every pass of `loop()`, before any other
work, and that order is a contract (`include/flexseq/Persistence.h:504`).

## Decision

**The template editor borrows the modulation buffer of channel 1.**

One flag says that the editor holds the buffer. The service of the pattern
modulation respects that flag in **both** of its loops: it releases nothing, and
it elects nothing, for a channel the editor holds.

The deferred template write reads its bytes from the buffer while the editor
holds it, and from the instance otherwise. The `SAVE` path of PRD §12.9 keeps the
instance as its source, and the two paths must not merge.

## Consequences

- **The engine gains no read path, and no field.** The audition works through the
  accessor that already exists. This is the whole reason for the decision.
- **The editor writes no instance.** The acceptance criterion of lot E holds by
  construction, not by a guard.
- **RAM cost: 4 bytes**, all of them in `ModulatedPatternState`, so outside the
  engine: the template index the editor holds, which doubles as the flag; the mode
  and the base length of channel 1, which the editor restores when it closes; and
  one flag that says something changed. ⚠️ **The Flash cost is not measured
  yet.** The lot measures it sub-step by sub-step.
- **The CV pattern modulation of channel 1 is suspended while the editor is
  open.** The user hears the template under edit, which is the purpose of the
  audition.
- **The buffer is released only after the deferred write ends.** The write pulls
  each byte at the moment it writes it, one per pass, so the source must stay
  valid for about 82 ms. Channel 1 therefore plays the template for that delay
  after the editor closes.
- **ADR 0011 applies without change.** The editor writes the buffer, so the editor
  invalidates the timing cache of channel 1, in the block that publishes the
  load. The inventory of writers of ADR 0011 gains one entry.

## Alternatives set aside

- **A dedicated buffer outside the engine, with a pointer inside.** It is the
  form of `ModulatedPatternState`, and it costs about 26 bytes of RAM more, a
  third source in `patternForChannel()`, and three consumers to teach. Set aside
  on the memory budget: 1997 bytes of Flash stay before the guard, against 1840 to
  3630 estimated for the work that remains, of which 500 to 900 belong to this lot
  (`WORKPLAN.md`, section `RM.16`).
- **A buffer inside the engine.** Refuted by the measurement of lot E3.6.3, above.
- **The instance of channel 1 as the edit buffer.** It costs nothing, and it
  destroys the pattern channel 1 plays. It breaks the acceptance criterion of lot
  E.

## References

- PRD §5.0 points 9 and 10; PRD §12.9, the `SAVE` flow.
- `include/flexseq/SequencerEngine.h`, `ModulatedPatternState`,
  `patternForChannel()`.
- `include/flexseq/Persistence.h`, `serviceOneModulationTemplateLoad()`,
  `loadTemplateIntoModulationBuffer()`, `PersistenceScheduler::advance()`.
- ADR 0006, patterns are templates in EEPROM; ADR 0011, the writer of the
  modulation buffer invalidates the timing cache.
- `WORKPLAN.md`, section `16E.2`.
