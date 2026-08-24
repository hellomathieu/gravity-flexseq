# 0003 — FlexSeq does not undo libGravity's encoder acceleration

- **Status:** accepted
- **Date:** 2026-08-23
- **Supersedes:** —
- **Superseded by:** —

## Context

The pinned libGravity accelerates the encoder. `Encoder::_rotate_change()`
(`encoder.h:88`) returns the movement accumulated since the last poll, then
multiplies it:

```cpp
change = position - previous_pos_;
if (ms < 16)      change *= 3;
else if (ms < 32) change *= 2;
```

`ms` is `encoder_.getMillisBetweenRotations()`, the interval between the last two
detents. `Encoder::Process()` calls `on_rotate(change)` one time only. FlexSeq
collapses every magnitude to 1 with `oneStep()`.

Read on its own, that code says detents are lost: any detent after the first, in
the same poll, disappears. The owner saw a fast turn lag on the module on
2026-08-23, and the reading looked confirmed.

**The measurement says otherwise.** A diagnostic firmware (`env:encoderprobe`)
counted, on the module, every value that libGravity delivered and the duration of
every main-loop pass:

| Measure | Result |
|---|---|
| `\|change\|` = 1 | 24 callbacks |
| `\|change\|` = 2, 3, or 4 and more | **0** |
| passes below 8 ms | saturated the counter (65535) |
| passes from 8 to 16 ms | 2226 |
| passes from 16 to 32 ms | 43 |
| passes of 32 ms and more | 0 |
| longest pass | 19 ms |

The acceleration therefore **never fires by hand**. A factor of 3 needs two
detents less than 16 ms apart, which is more than 62 detents per second; a factor
of 2 needs more than 31 per second. A hand on a Eurorack encoder does not reach
those rates, and the histogram shows it: every callback carried a magnitude of 1.

Detents do not accumulate between two polls either. The longest pass is 19 ms, so
two detents in one pass would need a rate no hand produces.

## Decision

FlexSeq **does not try to recover a detent count** from the value libGravity
delivers. `oneStep()` stays: one detent is one movement, everywhere except the
tempo.

**Amended on 2026-08-23 by lot 19: the tempo no longer keeps the raw delta.** The
owner decided that the acceleration disappears everywhere, the tempo included
(PRD §16), so `oneStep()` now applies to every field.

The change is **inoffensive**, and the measurement above is why: `|change|` was 1
on all 24 callbacks, so the raw delta and one detent were already the same value.
Removing the exception changes nothing observable on this hardware; it removes a
branch. The cost the owner accepted is a range crossed detent by detent: 270
detents from 30 to 300 BPM.

The ratchet ring also moves one choice per gesture (owner's decision,
2026-08-23). It holds 6 choices and skips the ones the channel rate refuses.

## Consequences

`oneStep()` is cheap insurance rather than a workaround. It costs one comparison,
and it protects a property the owner measured on 2026-08-22: the ×3 acceleration
made the navigation skip selections. That the acceleration does not fire today
does not make the guard useless — a faster poll, or a different encoder, would
bring it back.

**An attempt in the other direction was implemented and removed the same day.**
It replaced `oneStep()` with `max(1, |change| / 3)`, on the reasoning that two
detents in one pass are less than one pass apart, and that a pass stays below
16 ms. Both halves of that reasoning failed against the module: the pass reaches
19 ms, and no `|change|` above 1 ever arrived, so the rule was **inert** — it
returned exactly what `oneStep()` returned. It is recorded here so that the same
reasoning is not tried again from the code alone.

**The lag the owner saw is not explained by this path.** Whether libGravity loses
detents upstream, in its quadrature decoding or its interrupt, is open and tracked
in `docs/open-risks.md`, line 30. libGravity carries no debounce, and its own code
says so.

## Alternatives set aside

**Read `getMillisBetweenRotations()` to know the factor.** FlexSeq cannot: the
`RotaryEncoder encoder_` member is private in libGravity's `Encoder`
(`encoder.h:86`), only `change` is protected, and `libGravity.cpp` already owns
the two PCINT vectors. The dependency is pinned at commit `9be88be1f4` and must
not be patched.

**Deduce the factor from the poll period.** This is the attempt described above.
The premise was measured false.

## References

- PRD §16, the audit decisions of 2026-08-23.
- `docs/open-risks.md`, line 30.
- `docs/upstream-defects.md`, libGravity entry 9.
- `src/domain/UiController.cpp`, `sim/src/domain/UiController.ts`.
- `env:encoderprobe`, `src/hal/EncoderProbe.cpp`.
