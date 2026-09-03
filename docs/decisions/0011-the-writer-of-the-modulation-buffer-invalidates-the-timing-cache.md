# 0011 — The writer of the modulation buffer invalidates the timing cache

- **Status:** accepted
- **Date:** 2026-09-03
- **Supersedes:** —
- **Superseded by:** —

## Context

A channel in `SEQ` with a CV source routed to `PATTERN` plays a modulation
buffer, not its instance. `serviceOneModulationTemplateLoad()` fills that buffer
from an EEPROM template, one channel per pass of `loop()`, and publishes the
index it loaded in `loaded[channel]`.

The engine caches two values per channel at each step boundary:
`stepTicks` and `triggers`. Both derive from the ratchet of the current step,
read once by `refreshStepTiming()`. The content of the step, active or not, is
read live by `TriggerSequencer` at each onset it emits.

The two reads go through the same pointer. The loader rewrites the buffer in
place, so the content changes under the reader while the cache keeps the ratchet
of the previous template. PRD §10.3 property P35 requires that the content and
the ratchet of a decided trigger come from the same pattern.

**The violation is measured on the pins.** The trigger probe course
`cvpattern` uses two templates: OLD carries sixteen active steps and a TRIPLET on
each, NEW carries the same steps and no ratchet. When the derived index moves
from OLD to NEW, the production firmware emits **3 onsets** in the step of the
change: the TRIPLET cached from OLD applies to the content of NEW. The contract
requires 1.

Two forms were proven on the same fixture, each injected then restored:

| | B2 | B6 |
|---|---|---|
| site | `main.cpp` calls `refreshTiming(channel)` on the channel the service returns | the service calls `refreshTiming(channel)` after `loaded[channel] = wanted` |
| onsets at the change | 1 | 1 |
| native proof | none: `main.cpp` has no native test | `test_persistence`, 1.1 s |
| RAM | +0 | +0 |
| Flash | +14 B, 28610 | +14 B, 28610 |

The two binaries have the same size to the byte: LTO inlines the same call at
another site. The memory budget does not separate them.

## Decision

**The writer of the modulation buffer invalidates the timing cache, in the same
block that publishes the load.**

```cpp
int8_t serviceOneModulationTemplateLoad(Storage& storage, SequencerEngine& engine,
                                        ModulatedPatternState& state);
```

After a successful load, and only then:

```cpp
state.loaded[channel] = wanted;
engine.refreshTiming(channel);
```

The refused path publishes nothing, so it invalidates nothing: the loader
validates the length byte before it writes one byte of the buffer.

The dependency direction stays service to engine. The engine gains no field and
no call, and it still does not know `Storage`. The service loses the `const` on
the engine reference.

## Consequences

- P35 holds for the writer identified in lot STEP: the publication and the
  invalidation sit in one block, with no line between them. The proof is native,
  `test_persistence`, and physical, the `cvpattern` course.
- `refreshTiming(channel)` keeps `resetSubOnset = false`: the onsets already
  emitted in the step stay emitted, and the count is clamped when the new
  ratchet offers fewer. This is the rule of PRD §6.3 for an edit during play.
- The rule generalises: **no pattern data may become observable with a timing
  cache from another provenance.** Every writer of a pattern that a channel plays
  must invalidate the cache of that channel. The known writers today:
  `UiController` on a ratchet edit and on a clear, which already call
  `refreshTiming()`; `resetToDefaults()` of both image classes, which calls
  `refreshTiming()`; `load()` at boot, whose instance bytes reach a stopped
  engine that `start()` refreshes on every channel; `loadTemplate()`, which
  writes the content and then goes through `setSelectedPattern()`, which
  refreshes. Lot STEP verifies that inventory before the implementation.
- Cost accepted: RAM +0, Flash +14 B, measured on the spike. The `const`
  guarantee on the engine reference is lost, locally and explicitly.

## Alternatives set aside

- **B2, the orchestration in `main.cpp` refreshes the channel the service
  returns.** Same behaviour, same size. Set aside because the guarantee would
  live outside the component that knows the publication, one function return
  away from it, and because `main.cpp` has no native test: its only proof is the
  trigger probe.
- **B1, invalidate `stepTicks` in the engine.** Rejected: `stepTicks` is a
  temporal state engaged inside `advance()`.
- **B4, a flag raised by the engine.** Rejected: the event already exists as the
  return value of the service.
- **B5, self-invalidation by comparing the played index at each tick.**
  Rejected: it invalidates nothing on the case that matters, content of NEW with
  the ratchet of OLD stays possible, and it puts a comparison on the per-tick
  path.

## References

- PRD §10.3, property P35; PRD §6.3, edit during play.
- `include/flexseq/Persistence.h`, `serviceOneModulationTemplateLoad()`,
  `loadTemplateIntoModulationBuffer()`.
- `src/domain/SequencerEngine.cpp`, `refreshStepTiming()`, `refreshTiming()`.
- `include/flexseq/TriggerSequencer.h`, `decide()`, `activeStep()`.
- `tools/run-trigger-probe.sh`, courses `patold`, `patnew`, `cvpattern`;
  `tools/eeprom-image.cpp`, `--template` and `--selected`.
- ADR 0006, templates in EEPROM; ADR 0007, one nibble per ratchet.
