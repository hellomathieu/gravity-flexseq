# Cumulative timing drift — the measurement campaign

Tracking document, **never normative**. It records how the drift campaign
measures, what it measured, and what it cannot say. Decisions belong to the PRD
or to an ADR.

Started 2026-08-25. This page covers the instrumentation and the **short
validation campaign** only. The long campaigns, the delay test and the overload
sweep come later, and this page grows with them.

## The question

Does the sequencer lose its place in the grid as time passes? The jitter is
known — 1.00 to 1.23 ms, measured by `run-trigger-probe.sh` — but a jitter that
never accumulates and a slow drift look the same over 20 seconds.

The campaign therefore separates **four** phenomena, and never mixes them:

| | What it is | Where it is measured |
|---|---|---|
| **jitter** | a single onset leaves late | `timing_error_us` |
| **cumulative drift** | the error grows with time | the slope of `grid_error_ticks` |
| **timer frequency error** | the tick period is not the ideal one | the ISR period against the simulated cycles |
| **onset loss** | an onset is owed and never emitted | expected against emitted |

## Three clocks, and they do not measure the same thing

1. **simulated cycles**, 16 MHz exact — the ground truth;
2. **entry into `TIMER1_COMPA`** — the engine's tick grid. uClock calls
   `onOutputPPQNCallback` exactly once per interrupt (`uClock.cpp:349-352`), and
   `main.cpp:71` does `++pendingTicks` in it, so one interrupt is one tick;
3. **the rising edge on an output pin** — the actual emission.

The firmware is **not instrumented**. The mode, the pattern, the rate and the
tempo arrive through a preloaded EEPROM image built by `tools/eeprom-image.cpp`,
which links the domain code itself.

## The anchor is the MIDI Start byte, and it has to be

The interrupt **runs from boot**: `Clock::Init()` starts uClock (`clock.h:67`)
while the engine boots stopped. Counting interrupts from the first cycle would
count ticks the engine never received.

`uClock::start()` calls its start callback before it goes STARTED
(`uClock.cpp:134-136`), and libGravity writes `0xFA` on the serial port
(`clock.h:179`). That byte is observable from outside, so it is the origin. The
harness re-anchors on **every** `0xFA` and clears the recorded edges, so a
restart cannot mix two grids.

⚠️ **The module emits three MIDI Start and two MIDI Stop before the user ever
touches PLAY.** Observed on 2026-08-25, on the production binary:

```
START a 1.332 ms     Clock::Init() demarre uClock
STOP  a 351.403 ms   le premier apply() voit le moteur a l arret
START a 351.759 ms   Clock::SetSource() finit par `if (was_playing) uClock.start()`
STOP  a 352.119 ms   le passage suivant remet l horloge a l arret
START a 660.430 ms   PLAY
```

This is a real observation about downstream gear, not a drift. It is recorded in
`docs/open-risks.md`.

## Two errors, and they never replace one another

```
grid_error_ticks = tick d emission - tick attendu     entier, >= 0
timing_error_us  = instant du front - instant du tick attendu
```

`grid_error_ticks` answers "does the engine keep its place in the discrete
grid". `timing_error_us` answers "how late is this pulse". A 6 ms delay means
nothing without knowing whether a tick lasts 5 ms or 500 ms, and a one-tick
offset means nothing without knowing that the loop drained two ticks in one
pass.

**A non-zero `grid_error_ticks` is not a drift.** The loop drains ticks in
batches, pays one onset per pass per channel (`main.cpp:253-257`), and a pulse
lasts 5 ms before the output re-arms. At 120 BPM a tick lasts 5.208 ms and a
pass on the EDIT screen is 6.50 ms at p90, so a value of 1 or 2 is the normal
regime there.

Drift is the **slope**, and it is judged by three estimators that must agree:
ordinary least squares with its standard error, Theil-Sen (robust, because the
delays are one-sided — an onset can only be late), and the end-to-end change.
The conclusion is never drawn from `first == last`.

## The oracle is arithmetic, and it truncates like the engine

The expected tick of each onset is computed in the harness, not borrowed from
the domain: an expectation produced by the code under test would confirm itself.

```
ticksPerStep = 96 * v        si SUBDIV v > 0
ticksPerStep = 96 / |v|      si SUBDIV v < 0
span         = 2 pour le triolet, 1 sinon
stepTicks    = ticksPerStep * span
sous-onset k = (stepTicks * k) / triggers      division ENTIERE, uint32
```

The multiplication comes **first** and the division is integer, exactly as
`SequencerEngine.cpp:38-45`. The guard is the same too: a ratchet whose slot
would be shorter than `MIN_SLOT_TICKS = 2` falls back to a single trigger.

**Uneven gaps are a result, never a defect.** At `x12` a step lasts 8 ticks, so
a ratchet 3 lands on 0, 2, 5 — gaps of 2, 3, 3. The error stays under one tick
per sub-onset and **restarts from zero at every step**, because the engine's
accumulator keeps the exact remainder (`SequencerEngine.cpp:167`). An analyser
comparing against a floating-point position would report up to one tick of
"drift" per sub-onset, which is why the rounding rule is asserted by its own
unit tests.

## The first campaign — 1 minute, 2026-08-25

Command: `DURATION=60 SAVE=1 ./tools/run-drift-probe.sh`

| | |
|---|---|
| simulated duration | 60 s (13 s of wall clock, build and harness included) |
| tempo | 120 BPM |
| rate | SUBDIV `/1`, 96 ticks per step |
| pattern | steps 0, 3, 4, 9, 15 of 16, mode SEQ, six channels |
| ticks | 11 394 |
| onsets expected | 37 per line, **222** over six lines |
| onsets emitted | **222** |
| onsets dropped | **0** |
| onsets unexpected | **0** |
| `grid_error_ticks` | min **0**, max **0**, mean 0, median 0 — the only value met is 0 |
| `timing_error_us` | min **609**, max **862**, mean 741, median 739, standard deviation 60 |
| OLS slope | 0.000e+0 tick/tick, standard error 0.000e+0 |
| Theil-Sen slope | 0.000e+0 |
| cumulative drift | **0.000 tick** over the run |
| ISR period | **5208.000000 µs** against 5208.333333 ideal, **−64.0 ppm** |

Coherence, all green: timestamps strictly increasing, MIDI Clock every 4 ticks
without exception over the whole run, a single anchor with no restart after it.

**The timer term is confirmed by arithmetic, not only measured.**
`uClock::bpmToMicroSeconds()` returns a `uint32_t`, so 5208.33 µs truncates to
5208; `setTimer()` then writes `OCR1A = 10415` with a prescaler of 8, which is
5208.0 µs exactly. The prediction was −64 ppm and the measurement is −64.0 ppm.
The sequencer therefore runs **fast** by about 0.23 s per hour at 120 BPM, and
that belongs to the timer, never to the engine.

## What this campaign does NOT establish

- **The real crystal.** simavr models a perfect 16 MHz. Measuring the module's
  own oscillator needs hardware and an external reference, over hours.
- **Long-run behaviour.** One minute is 11 394 ticks. The 5-minute and 15-minute
  campaigns are what will bound a rare defect.
- **The EDIT screen.** This run starts on the main screen, where a pass costs
  about 0.2 ms, which is why every `grid_error_ticks` is 0. The screen that
  renders continuously is a separate condition, still to be measured.
- **Overload.** No rate here comes close to the emission ceiling.
- **The original firmware.** It has never been run under this harness.

## The instrument is tested before it is trusted

`sim/src/analysis/driftEstimator.ts` carries the statistics, and
`sim/test/driftEstimator.test.ts` carries 18 assertions on them: a null series, a
one-sided jitter, a known linear drift, a recovered slope, a missing onset, a
duplicated onset, a punctual delay followed by a return to the grid, a delay
never recovered, the rounding table, and the onset budget.

Ten mutations of the analyser were injected and **all ten were detected**: a
slope estimator that always answers zero, a verdict that calls a bounded jitter a
drift, a verdict that falls back to first-against-last, a missing-onset detector
removed, an unexpected-onset detector removed, a sub-onset that rounds instead of
truncating, a sub-onset that divides before multiplying, a Theil-Sen that answers
the mean, an onset budget that forgets the pending onsets, and a standard
deviation without its square root.
