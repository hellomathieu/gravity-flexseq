# The external clock — the measurement of lot XCLK

Tracking document, **never normative**. It records how the external clock is
measured, what the measurement establishes, and what it cannot say. Decisions
belong to `PRD.md` or to an ADR.

Written 2026-09-03, when lot XCLK closed. It exists because the rationale of a
measurement harness may not live in a comment: the rule of
`.claude/rules/github-conventions.md` forbids comments in code files, and
`CLAUDE.md` survives no clone. `docs/timing-drift.md` and
`docs/gesture-injection.md` are the precedent for this form.

## Why the lot exists

**No harness of the project drove an external clock, and neither did the
hardware.** The debt sits in the development plan under lot 5, "the external
clock makes the engine advance — never wired". So the path had no baseline: the
absence of a regression could not be shown, because nothing said what the
correct behaviour was.

## The chain, and what starts the transport

```text
EXT_PIN is 2, so PD2. libGravity attaches a RISING interrupt to it.
onExternalEdge() counts the pulse and calls Clock::Tick().
transport::apply() starts the transport on the FIRST pulse, because PLAY is
inert outside the internal clock. That follows the original, Gravity.ino:321-322.
```

A square wave gives one edge per period whatever the idle level of the pin,
because the harness drives both levels itself.

**The MIDI source takes the same chain by another transport.** The byte `0xF8`
arrives on the UART, `0xFA` starts and `0xFC` stops. libGravity forces the input
to 24 PPQN for that source, as the standard does.

## The settling window is DERIVED, never assumed

uClock smooths the external interval with a PLL, `PLL_X = 220`:

```text
ext_interval = (ext_interval * 220 + 36 * last) >> 8
```

So the residual after `n` pulses is `(220 / 256)^n`:

```text
5,0 % after 20 pulses · 1,0 % after 31 · 0,1 % after 46
```

`EXT_DISCARD` carries that number, and it defaults to **31**. It counts
**pulses**, because the PLL converges per pulse.

⚠️ **`EXT_MEASURE_STEPS` counts STEPS, not pulses.** An input pulse is
`96 / ppqn` output ticks, so a step of `/1` is `ppqn` pulses: at input PPQN 24,
forty pulses make 1.7 steps, and no criterion on a train holds on that.

## The freeze of defect 12 must be absorbed

`docs/upstream-defects.md` entry 12 carries the defect. The window therefore
opens after it, and the harness derives it:

```text
a tick at MIN_BPM = 60000000 / 96 / 1  = 625000 us
the recovery waits mod_clock_ref ticks = 96 / input_ppqn of them
```

```text
input PPQN   freeze     duration the course needs
        24   2500 ms                       15 s
         4  15000 ms                       31 s
         2  30000 ms                       50 s
         1  60000 ms                       87 s
```

## The five criteria, all derived

A step of `/1` is 96 output ticks and an input pulse is `96 / ppqn` of them, so
the **expected step is the injected period times the input PPQN**.

```text
C1  the mean step over the window   budget = the PLL residual plus one tick in 96
C2  a cyclic rotation of the motif  a criterion without a phase
C3  the instant oscillation         bound = PHASE_FACTOR, 16 >> 8, so 6.25 %
C4  a usable window                 INVALID, never FAIL
C5  the freeze is over              read on the timer witness
```

⚠️ **C5 exists apart for a measured reason.** The budget of C1 widens with the
uncertainty it carries: with `EXT_DISCARD=0` the PLL residual is 1, so the budget
becomes 101 % and C1 passes on any measurement at all. That is correct for a
derived budget, and it is why a separate guard reads the conditions.

## The timer witness

`EXT_TRACE_MS` makes the harness read **OCR1A and the prescaler of TCCR1B**, so
it reports the period the timer really applies:

```text
period = (OCR1A + 1) * prescaler / F_CPU
```

It depends on no layout of a class, and it reads the hardware rather than what
the library believes. **It found defect 12**: a collapse to 624992 us at 380 ms,
then a recovery at 2880 ms at input PPQN 24.

## The levers, and each one was seen red

```text
EXT_EXPECT_PERIOD_US   the expectation is decoupled from the injection
EXT_PIN_FORCE          the injection moves to another pin of port D
EXT_FREEZE_MS=0        the window opens inside the freeze
EXT_C3_BOUND_PCT       the bound of C3 is tightened
MIDI_SEND_START=0      the 0xFA byte is withheld
MUTATE=<step>          a step enters the image and not the expectation
```

⚠️ **Each course sets the OTHER transport to zero explicitly.** Without that, a
variable inherited from the caller enabled both injections at once, and two
clocks drove the module in silence: 13 edges where 7 were expected, gaps of 0
steps, C1 at 11.76 %. The method rule lives in `docs/open-risks.md`.

## What the measurement establishes

**The engine follows the external source.** Injecting a period of 16667 us, so
150 BPM at input PPQN 24, gives a measured step of **400.00 ms** instead of 500.
On the internal clock at 120 BPM the step would be 500 ms and C1 would be red by
25 %. That holds on the pin and on the MIDI transport.

```text
course        needed   freeze     measured / expected step   error   C3
extclock 24     15 s   2500 ms     500.01 / 499.99 ms        0.00 %  0.08 %
extclock  4     31 s  15000 ms     499.99 / 500.00 ms        0.00 %  0.06 %
extclock  2     50 s  30000 ms     499.99 / 500.00 ms        0.00 %  0.10 %
extclock  1     87 s  60000 ms     497.18 / 500.00 ms        0.56 %  0.97 %
midiclock       15 s   2500 ms     499.94 / 499.99 ms        0.01 %  0.07 %
```

⚠️ **Input PPQN 1 is the least precise, and that is expected.** One pulse every
500 ms gives the PLL much less to work with. The derived budget of 1.95 % covers
it, so the criterion holds without being widened for the occasion.

## What it does NOT establish

- **the real module is not tested.** simavr is not an analogue clock, and the
  debt of lot 5 stays open;
- **only input PPQN 24 is guarded by the routine pass** — `docs/open-risks.md`
  line 82. The other three rates and the MIDI course run on demand;
- nothing on the **jitter** of a real external source, nor on the electrical
  cleanliness of the signal;
- defect 12 is **not repaired**, by a decision of the owner.

## Two behaviours the courses observed

Both are in `docs/original-conformity.md`, and neither is decided:

- the transport starts **without the MIDI Start byte**, on the first clock byte;
- under the **MIDI** source, a pulse on pin PD2 drives the clock as well.
