# 0004 — A rate change lands on the beat

- **Status:** accepted
- **Date:** 2026-08-23
- **Supersedes:** —
- **Superseded by:** —

## Context

The owner saw two channels drift apart on the module. Both played at `/1`. One of
them went through other rates and came back to `/1`, and it no longer fell with
the other. The original firmware does not do this.

**Measured on the TypeScript reference model**, after a round trip and a return to
`/1`:

| Path | Offset |
|---|---|
| `/1` to `/2` to `/1` | 0 tick |
| `/1` to `x2` to `/1` | 48 ticks |
| `/1` to `x4` to `/1` | 48 ticks |
| `/1` to `x3` to `/1` | 64 ticks |

A tick is 1/96 of a quarter note. At 120 BPM, 48 ticks are 250 ms and 64 ticks are
333 ms. The offset is audible, and permanent.

**The cause.** A division lasts a multiple of 96 ticks, so its step onsets stay on
the quarter-note grid. A multiplication lasts `96 / n` ticks and puts onsets
between the beats. FlexSeq applied a rate change **at once**, mid-step, kept the
accumulator, and folded it only when it was above the new step length. The phase
was never re-derived from `masterPhase`, so an off-grid phase survived for ever.

**What the original does**, and the audit found it in two places that answer each
other:

- `Interactions.ino:141` and `:275` apply the new rate at once **only when the
  transport is stopped**: `if (!isPlaying) { calculateCycles(); }`;
- `Gravity.ino:454` carries the author's own comment, "switching modes on the beat
  and resetting channel clock", and calls `calculateCycles()` under
  `if (pulseCount == 0)`. The original defines `PPQN 24` (`Gravity.ino:13`), so
  `pulseCount` counts 0 to 23 per quarter note and that test **is** the beat.
  FlexSeq runs at 96 PPQN, so its own beat is `masterPhase % 96 == 0`.

Every rate divides or multiplies the quarter note, so a channel counter is 0, or
a whole number of beats, at every beat. The change therefore lands on an aligned value.

The original is aligned to **1 tick**, not exactly: when it leaves a division
mid-cycle, `Gravity.ino:491` resets the counter on the pulse *after* the beat.
That is 5.2 ms at 120 BPM, which is why the owner sees no problem there.

## Decision

A rate change **waits for the next beat**. `setSubdiv()` and `setTicksPerStep()`
both go through `scheduleTicks()`:

- transport stopped, or `masterPhase` already on a beat, the change applies at
  once;
- otherwise the rate waits in `pendingTicks`, and `advance()` applies it at the
  next beat it crosses.

The **selection** is visible at once: `subdiv` is written immediately, so the
screen and the EEPROM carry the user's choice without waiting. This is the
original's split between `channels[i].subDiv` and `playingModes[i]`.

A global `reset()` applies a pending rate instead of dropping it.

## Consequences

FlexSeq goes one step beyond the original, on purpose, in two ways.

**The alignment is exact.** When the rate is applied, the phase is re-derived from
the beat: `acc = beatTick % stepTicks`, pre-adjusted for the ticks of the current
pass. The original's residual tick disappears.

**The correction survives a drained burst.** `advance()` can receive several ticks
at once when the loop was blocked, so a beat can be crossed without being seen
exactly. The re-derivation keeps the result right, which a plain "reset the
counter on the beat" would not.

**The cost is accepted.** The rate takes effect up to one beat later, which is
500 ms at 120 BPM. That is the original's behaviour, and PRD §1 keeps it.

`pendingTicks` costs **12 bytes of RAM**, 2 per channel. Flash grew by 278 bytes.
A first version used two 32-bit modulos and cost 350 bytes; a beat counter
(`beatTick_`, one byte) replaced them.

**The alignment is on the beat, not on the global origin.** Two channels at the
same division whose rates were set at different beats can still differ by a whole
beat. The original does not realign a division either, so this is faithful. It
also costs far less: aligning to the origin needs `masterPhase % stepTicks`, a
32-bit modulo.

## Alternatives set aside

**Re-derive the phase and apply at once.** One line, no RAM, and the alignment is
exact too. Set aside because the current step is then cut short, sometimes very
short, and because the original deliberately defers.

**Leave it, and realign with PLAY.** Measured: a stop and a start does clear the
offset. Set aside as a workaround for a defect, not a behaviour.

## What the measurement also cleared

Five other paths were measured before anything was asserted, and **none** shifts a
channel: a LENGTH edit, selecting another pattern, a mode change, a ratchet edit
on the current step, and a `setTicksPerStep` round trip.

**The triplet does not shift a channel either**, against a first reading that said
it might. A triplet step lasts exactly twice `ticksPerStep` on all 25 rates, so it
stays on the channel's own sub-grid, which divides 96.

## References

- PRD §1, and the product decision on when a rate change takes effect.
- Original firmware: `Gravity.ino:454`, `:491`, `:500`; `Interactions.ino:141`, `:275`.
- `src/domain/SequencerEngine.cpp`, `sim/src/domain/SequencerEngine.ts`.
- `test/test_subdiv_phase/`, `sim/test/SubdivPhase.test.ts`: 14 assertions each side.
- `tools/run-mutation-probe.py`: the `cpp-subdiv` and `ts-subdiv` mutants.
