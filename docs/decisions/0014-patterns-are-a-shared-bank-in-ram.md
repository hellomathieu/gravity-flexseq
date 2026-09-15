# 0014 — Patterns are a shared bank in RAM

- **Status:** accepted
- **Date:** 2026-09-15
- **Supersedes:** ADR 0006, ADR 0011
- **Superseded by:** —

## Context

ADR 0006 made the sixteen patterns **templates in EEPROM**, and gave each
channel an **instance in RAM**, a private copy that the channel edits. The
decision bought a per-channel variant of a pattern, and it saved RAM at a time
when the pattern held 32 steps and 321 bytes were free.

Three facts changed since 2026-08-23.

**The Flash is the binding resource, and the model costs Flash.** The template
zone, the six instances, the modulation buffer with its round-robin service,
the deferred template write, the load primitives and the confirmation question
`SURE` all exist because a channel plays a copy. On 2026-09-14 the owner
removed the `PATTERNS` tab for the Flash budget, and the sixteen templates
became read only: the user authored six patterns instead of sixteen.

**The original shares its patterns.** The Sitka firmware holds sixteen shared
arrays, `seqA1..seqB8`, and one selector per channel, `channel.seqPattern`
(`Gravity.ino:71`). PRD §1 keeps the features of the original, and PRD §5.1
recorded that sharing as a validated decision before ADR 0006 replaced it.

**A prototype measured the shared model on 2026-09-15**, on a throwaway
branch, without the read-only marker of this decision:

```text
Flash   28992 -> 27868   RETURNS 1124 bytes
RAM      1513 ->  1582   COSTS     69 bytes, against 151 of allowed growth
stack     200 ->   200   unchanged, 6/6 watched vectors
loop     p90 6.62 -> 6.63 ms   budget 12 ms
image     588 ->   434 bytes
```

The RAM estimate that preceded it, +92 bytes, was wrong on two terms: the
modulation buffer weighs 152 bytes on the ELF, not 138, and the allowed growth
is 151, not 140. **Read the weight of an object on the ELF, never from the sum
of the fields you remember.**

## Decision

**The sixteen patterns live in one `PatternBank` in RAM, and the EEPROM
persists that bank.** A channel holds a selector, `selectedPattern`, and it
plays the pattern the selector names. To edit a pattern edits it for every
channel that plays it. There is no template, no instance, no copy, and no
confirmation before a load.

The engine holds **one pointer** to the bank, the form that `bank_` had before
lot B4b.7 and that `modulated_` had after it. The engine object does not grow:
lot E3.6.3 measured that 138 bytes held as members cost +318 bytes of Flash,
and the same bytes held outside with one pointer cost +24.

**Two reads per channel, and one difference between them.**

```text
selected(channel) = bank[selectedPattern]          what the PATTERN field names
played(channel)   = bank[patternCvIndex(channel)]  what the engine plays, and what EDIT shows
```

`patternCvIndex()` is `clamp(selectedPattern + Σ zones, 0, 15)`, derived at
the step boundary. Without a `PATTERN` routing the two reads are the same
pattern. The `PATTERN` modulation is therefore **a change of index**: it reads
no EEPROM and fills no buffer.

**The length stays with the channel.** ADR 0009 does not move. A bank record
carries the content alone, 23 bytes.

**`A1` to `A8` are editable.** The rule `index < 8` disappears. The factory
content is seeded when the persisted image is invalid, and only then.

**The `EDIT` screen shows the played pattern, and it is read only while a
source of the channel is routed to `PATTERN`.** The title reads `CV PATTERN`
with the name of the played pattern, the step cursor is not drawn, and the
three content gestures do nothing. `SEP` stays adjustable. The rule follows the
routing and never the zone.

**The persisted format moves to version 4**, 434 bytes, and the whole image is
scanned. There is no migration from version 3. PRD §11.1 is the normative
source of the layout.

## Consequences

- **Flash returns about 1100 bytes, RAM costs about 70.** The prototype gives
  the order of size; the lot measures the delivered figure, step by step, on
  `tools/memory-baseline`.
- **The per-channel variant of a pattern disappears.** Two channels on `B4`
  hear the same edit. The user authors sixteen patterns, as the original does,
  instead of six.
- **The patterns edited on the module are lost at the first boot of the new
  firmware**, as every change of version has been. The settings of the
  original firmware, below the address 320, are not touched.
- **ADR 0011 loses its subject.** No writer fills a buffer under the reader
  any more. Its general rule survives in this ADR: **no pattern data may become
  observable with a timing cache from another provenance.** The writers of a
  played pattern are the `EDIT` gestures, which call `refreshTiming()`, the load
  gesture, which refreshes the channel it changes, and `load()` at boot, whose
  bytes reach a stopped engine that `start()` refreshes. A change of the derived
  index happens at the step boundary, where `refreshStepTiming()` reads the
  ratchet of the new pattern with its content: the cache and the content come
  from the same boundary.
- **`SAVE` has no subject any more**, PRD §12.9: there is no copy to publish.
- **The read-only guard is a routing predicate**, `isRoutedToPattern()`, which
  already exists and moves from the persistence header into the domain.
- **A gesture that loads is instant**: it writes one byte of state and refreshes
  the timing. The deferred load request and the `Storage` dependency of the
  load path disappear.
- **The tooling follows the format**: `tools/eeprom-image.cpp` loses
  `--template`, `--instance` and `--per-channel` and gains a bank option; the
  probes that preloaded templates or watched `loaded[]` are re-targeted.

## Alternatives set aside

- **Keep the template and instance model and wire `SAVE`.** Measured on
  2026-09-14 at +158 bytes, against 1420 available for four lots estimated at
  586 to 906 bytes, in a category that overran its estimates by 2.7 to 12
  times. It also keeps the divergence from the original at sixteen slots that
  nobody can write.
- **The length in the bank record.** It would make the length a property of the
  pattern and reverse ADR 0009, and the Length CV would need a base that the
  channel no longer owns. Not measured. The owner chose the channel on
  2026-09-15.
- **The `EDIT` screen edits the selected pattern while the channel plays the
  derived one.** What the user sees would not be what sounds. The owner chose
  the played pattern, read only, on 2026-09-15.
- **A read-only rule that follows the zone, not the routing.** It would appear
  and disappear with a voltage, in the middle of an edit.
- **A factory reset gesture in this lot.** The gesture budget is full, and a
  field costs 70 bytes at least. Lot 14 carries the factory reset.

## References

- PRD §5.0, amendment of 2026-09-15; §5.1; §10.2, the `PATTERN` destination;
  §11.1, version 4; §12.9.
- ADR 0006, superseded; ADR 0009, unchanged; ADR 0011, superseded; ADR 0013,
  already superseded.
- `include/flexseq/PatternBank.h`, `include/flexseq/SequencerEngine.h`,
  `include/flexseq/Persistence.h`.
- `WORKPLAN.md`, section `LOT PARTAGE`, for the measurement of the prototype
  and the delivery steps.
- The original firmware, `Gravity.ino:71` and `:383-398`, for the shared
  arrays and the selector.
