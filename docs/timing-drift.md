# Cumulative timing drift — the measurement campaign

Tracking document, **never normative**. It records how the drift campaign
measures, what it measured, and what it cannot say. Decisions belong to the PRD
or to an ADR.

Started 2026-08-25. This page covers the instrumentation, the campaigns of 1, 5
and 15 minutes, the EDIT screen condition and the artificial delay. The overload
sweep comes later, and this page grows with it.

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

> ⚠️ **The figures above predate decision D79 (2026-09-02).** A global reset now
> arms the entry onset of step 0, and the first tick after the Start emits it.
> The expected model of `drift_probe.c` carries that onset at tick 1, so the
> reference count is **38 per line, 228 over six lines** since then. The
> re-derivation was measured in both states: before the model change the run
> read 228 fronts against 222 expected, one "unexpected" per channel; after it,
> 228 against 228, zero unexpected, and the armed onset pairs as late by at
> most 2 ticks — the loop-pass granularity, reported and not hidden.
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

## The longer campaigns — 2026-08-25

Same conditions as the first one, on the main screen unless stated.

| | 5 minutes | 15 minutes | 5 minutes on EDIT |
|---|---|---|---|
| ticks | 57 477 | **172 684** | 57 536 |
| onsets expected / emitted | 1122 / 1122 | **3372 / 3372** | 1122 / 1122 |
| dropped | 0 | 0 | 0 |
| `grid_error_ticks` | only 0 | only 0 | only 0 |
| `timing_error_us` median | 753 | 756 | 734 |
| cumulative drift | 0.000 tick | **0.000 tick** | 0.000 tick |
| ISR period | −64.0 ppm | −64.0 ppm | −64.0 ppm |

The fifteen-minute run resolves one tick over 900 s, which is **5.8 ppm**, eleven
times finer than the timer term. Nothing appears.

**The EDIT screen did not produce the regime it was chosen for, and the cause is
structural.** The loop pays the onsets at the top of the pass, before it renders,
and a frame only starts when the playhead moves. A frame is eight bands over
eight passes, about 27 ms, while a step at 120 BPM in SUBDIV `/1` lasts 500 ms.
The render is therefore phase locked to the onset and finishes long before the
next one. Rendering can only delay an onset when two onsets fall closer together
than a frame.

## The artificial delay — 2026-08-25

The question is not whether a trigger can be late. It is whether a punctual
delay of the main loop leaves a **permanent** offset in the grid.

### Two mechanisms were rejected by measurement

**A burst of received MIDI bytes.** simavr delivers them at the port speed,
31250 baud, so the interrupt tops out near 3 kHz and steals one or two percent
of the CPU. It cannot starve the loop, and simavr floods its log with
"RX buffer overrun" for every byte beyond the FIFO.

**A burst of edges on the external clock input.** `INT0` is vector 1 and
`TIMER1_COMPA` is vector 11, so the burst runs **before the tick**. The measured
interrupt period went to **+396 ppm** instead of −64: the harness was moving the
very reference it measures against.

### The retained mechanism

The harness raises the **ADC prescaler** to its fastest setting for the length of
the window, then restores it. The ADC vector is 21, therefore **below** the tick,
which keeps being served on time. Nothing else is touched, and the firmware is
not modified.

Restoring matters as much as boosting. A first version wrote the whole saved
register back and the conversion chain died: the ADC ran 186 765 times over the
run instead of 449 574, and the loop became faster than nominal **after** the
window. The restore now rewrites the prescaler bits only, and the totals confirm
it: 449 947 with the delay against 449 574 without, a difference of 373, which is
the surplus of the window itself.

### Two proofs, and they need two placements

| Proof | Measurement |
|---|---|
| the onset is late | emitted **8139 µs** after its tick, against **757 µs** median outside the window |
| the loop misses its end of pass | a pulse caught by the window stays high **8.867 ms** against **4.699 ms** median |

At 500 ms between onsets, a window that covers an onset's tick cannot also
overlap a pulse that is already high. The first proof therefore comes from a
window placed on the tick, the second from the same 10 ms window placed 0.9 ms
after the rising edge.

### The campaign

60 s simulated, 120 BPM, SUBDIV `/1`, main screen, six channels, one 10 ms delay
at 5158.4 ms.

| | before | during | after |
|---|---|---|---|
| onsets | 12 | **6**, one per channel | **204** |
| `grid_error_ticks` | 0 | **1** everywhere | **0** everywhere |
| `timing_error_us` median | 707 | 7687 (7241 to 8139) | 758 |

222 onsets expected, 222 emitted, **0 dropped**, 0 unexpected. The debt reached
one onset per channel, deduced rather than read. Recovery took **one onset**: the
next one, 500 ms later, is already back on the grid. Final error 0, persistent
offset 0, Theil-Sen slope 0.

**Verdict: recovery.** A punctual delay of the loop moves one onset by one tick
and nothing else. The grid is not displaced.

## The greedy matching broke, and the fix is to stop matching first — 2026-08-25

The overload sweep reported **1441 dropped onsets** on one configuration. The raw
edge count said **12**. The analyser was wrong, and the way it was wrong matters.

### Why it broke

The old rule read:

```
if the edge is before the expectation minus a tolerance  -> unexpected
else if the edge is at or after the NEXT expectation     -> the onset is missing
else                                                     -> matched
```

The second line assumes **the delay is smaller than the interval between two
onsets**. Under overload that assumption is false by construction: the debt pays
one onset per main-loop pass, so six sub-onsets packed into 4.17 ms come out
spread over tens of milliseconds. The edge of onset *i* lands past the
expectation of *i+1*, the matcher calls *i* missing, pairs the edge with *i+1*,
and **the shift propagates to the end of the run**. Hence the signature:
`dropped` and `unexpected` almost equal — 1441 against 1429 — and negative grid
errors once the shift inverted.

### What is impossible, and it is not a matter of algorithm

`TriggerSequencer` holds the debt in **`owed_[ch]`, a counter**, not a queue of
identities. When three onsets are owed and three pulses come out, nothing in the
system says which is which — the information does not exist, on the pin or in
memory. When the clamp at `MAX_OWED = 6` bites, the surplus disappears without a
trace.

No matcher can therefore attribute an edge to a given onset once the delay
exceeds the interval. The answer is not a better matcher: it is a measurement
that does not need matching.

### The method

Two cumulative series per channel, one arithmetic and one observed:

```
E(t) = onsets expected up to t
O(t) = edges observed up to t
D(t) = E(t) - O(t)     the outstanding debt
```

`D(t)` needs no identity. Its **peak** is the largest backlog, its **floor** is
what was lost for good, and its shape separates the four cases: a bump that
returns to zero is a delay, a floor that rises is a loss, a positive slope is an
accumulating debt, and none of them is a drift of the grid.

Pairing survives, but it now declares its limits. `reconcileTimeline()` pairs in
order, counts `matched`, `late`, `missing`, `extra`, and — the new one —
**`ambiguous`**. A single loss is attributed to a position only when exactly one
position is feasible under "an onset cannot be emitted before its tick"; when
several are, the analyser returns `ambiguous` with the candidate range rather
than choosing silently. Above one loss it does not attribute at all.

### The reference campaign, replayed — 300 BPM, x8 + R6, 20 s

| | old analyser | new analyser |
|---|---:|---:|
| expected | 8742 | 8742 |
| edges observed | 8730 | 8730 |
| dropped / missing | **1441** | **12** |
| unexpected / extra | **1429** | **0** |
| ambiguous | not measured | **12** |
| grid error | −1 to 4, negative values | not published, pairing ambiguous |
| max delay | not measured | **65 ticks**, 135 ms |
| max debt | not measured | **8** |

**The 12 are real losses, and the deficit says where.** The floor of `D(t)` is
**2 per channel from tick 1000 to the end of the run**, and it never returns to
zero. The peak of 8 happens at **tick 50**, in the startup burst, which is also
the only window where the backlog exceeds the firmware's clamp of 6 — ticks 46 to
60. Eight owed against a cap of six leaves two, which is exactly the floor that
follows. Nothing is lost afterwards: the floor stays at 2 for the remaining
9000 ticks.

The horizon explains none of it: a truncation at the end would leave a transient
deficit, not a floor carried across the whole run.

**No ceiling is concluded from this.** Whether those losses come from the pulse
width, from the loop rate, from the simulated ADC or from a combination is a
separate question, and this campaign was run to make the measurement trustworthy,
not to answer it.

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
