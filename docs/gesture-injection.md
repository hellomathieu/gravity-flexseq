# Gesture injection — the contract between the recipes and the pins

Tracking document, **never normative**. It fixes how a gesture produced by
`GestureDriver` is replayed on the real firmware, and how a failure is
classified. Decisions belong to the PRD or to an ADR.

Written 2026-08-25, phase P2.0. The firmware is **not modified**, and the
measured binary is the production one, LTO included.

## The four gestures, and nothing else

`GestureDriver.events` carries one of four shapes. Each has exactly one physical
translation.

| Gesture | Pins | Sequence |
|---|---|---|
| `Rotate(±1)` | encoder A (PC3, `A3`) and B (PD4, `D4`) | one full quadrature cycle from the latch |
| `Press` | encoder switch (PC0, `A0`) | low for 60 ms |
| `LongPress` | encoder switch | low for 900 ms |
| `ShiftRotate(±1)` | SHIFT (PB4, `D12`) plus the encoder | SHIFT low, rotate, SHIFT released |

`PlayPress` reuses the mechanism already written for `drift_probe.c`: PORTD5 low
for 60 ms.

## The constants, in four groups that are never mixed

A millisecond is an **injection** unit. It says how long the harness drives a
pin, or how long it lets the firmware run. It is **never** a criterion about the
engine: see "The units" below.

**Group 1 — injection durations.** How long a pin is physically held down.

| Constant | Value | Source |
|---|---|---|
| `NO_OP_PRESS_MS` | 5 ms | below the debounce, so the firmware must ignore it |
| `PRESS_MS` | 60 ms | above the debounce, far below the long press |
| `LONG_PRESS_MS` | 900 ms | above the 750 ms threshold, with margin |

**Group 2 — settling delays.** Simulated time given to the firmware, with no pin
driven, so that the main loop polls and renders.

| Constant | Value | Why |
|---|---|---|
| `BUTTON_MARGIN_MS` | 15 ms | around each button edge, so the polling loop sees the level before and after it |
| `DETENT_SETTLE_MS` | 50 ms | after **each detent**, because the events are dispatched from `Process()`. It is also above the 32 ms acceleration window of libGravity, so no detent is multiplied |
| `FRAME_SETTLE_MS` | 250 ms | after a gesture, so the frame that the gesture triggers is fully sent |

⚠️ **This table said "settle between gestures 50 ms" until 2026-08-25.** The
50 ms sit between two **detents**; the delay after a gesture is
`FRAME_SETTLE_MS`, and it is five times larger.

**Group 3 — encoder timing.**

| Constant | Value | Source |
|---|---|---|
| `EDGE_SPACING_MS` | 1 ms | the encoder carries **no debounce**; the pin-change interrupt only needs to be serviced |
| `EDGES_PER_DETENT` | 4 | one full quadrature cycle back to (HIGH, HIGH) |

**Group 4 — thresholds read from the firmware, never chosen here.**

| Constant | Value | Source |
|---|---|---|
| `BUTTON_DEBOUNCE_MS` | 10 ms | `button.h:16` |
| `SHIFT_LONG_PRESS_MS` | 750 ms | `button.h:17`, and it fires **on release** (`button.h:79-84`) |
| `LOOP_POLL_RESERVE_MS` | 20 ms | one main-loop pass, above the 13.87 ms worst pass measured by `run-blocking-probe.sh` |

## A SHIFT burst carries at most twelve detents, and the harness enforces it

`Button` samples the release **at poll time**, not at the physical edge, and it
declares a long press when `millis() - last_press_ >= 750`. So the duration the
firmware observes is the injected one plus up to one main-loop pass. That is why
`LOOP_POLL_RESERVE_MS` exists.

The injected hold of a burst of `n` detents is exact, and derived:

```
DETENT_MS       = EDGES_PER_DETENT * EDGE_SPACING_MS + DETENT_SETTLE_MS = 54
SHIFT_HOLD_MS(n) = 2 * BUTTON_MARGIN_MS + n * DETENT_MS                 = 30 + 54n
```

The measurement confirms the model rather than assuming it: `n = 3` gives
30 + 162 = **192 ms**, which is the value read on the module's simulated binary.

The ceiling and the burst size follow:

```
SHIFT_HOLD_CEILING_MS = 750 - 20                                   = 730
SHIFT_BURST_DETENTS   = (730 - 1 - 30) / 54                        = 12
```

| n | injected hold | plus one loop pass | verdict |
|---:|---:|---:|---|
| 12 | **678 ms** | 698 ms | accepted, 52 ms of margin |
| 13 | 732 ms | 752 ms | refused: the observed hold can cross 750 ms |
| 20 | 1110 ms | 1130 ms | refused by a wide margin |

**13 is the arithmetic ceiling on the injected duration alone; 12 is the
retained ceiling**, because the criterion that matters is what the firmware
observes, and a 4 ms margin against the polling quantisation is not a margin.

⚠️ **This document said "bursts of at most 20 detents" until 2026-08-25, and the
harness never split at all.** `SHIFT_BURST_DETENTS = 20` was defined and printed
in the report, and `shiftRotate()` injected every detent under a single hold. A
burst of 20 lasts 1110 ms, so the value published as the safety rule was the one
value that broke it. No gesture ever reached that size, so nothing failed; the
contract was simply not enforced.

**What enforces it now**, in three places that fail independently:

- five `static_assert` in `gesture_probe.cpp`. They fix the value at compile
  time, prove it is the **largest** one that fits, and refuse the old 20;
- `shiftBurst()` refuses a burst **before injecting it** and exits with code 4.
  The guard is not a check after the fact: nothing is driven on the pin;
- `shiftRotate()` splits a long request into bursts of at most
  `SHIFT_BURST_DETENTS`, and **releases SHIFT between them**, with a gap of
  `SHIFT_BURST_GAP_MS` = 45 ms, above the debounce plus one polling pass.

⚠️ **45 ms is not the rest between two bursts.** `shiftBurst()` already ends with
`BUTTON_MARGIN_MS` + `FRAME_SETTLE_MS`, so the real rest is **310 ms**. Read the
figure from the sum, never from `SHIFT_BURST_GAP_MS` alone.

A release under 750 ms fires `EVENT_SHIFT_PRESS`, which `UiController::handle()`
**discards on its first line**. Splitting therefore adds no side effect. What it
must never do is inject a detent while SHIFT is high, because that becomes
`EVENT_ROTATE` and navigates instead of adjusting; the splitter drives detents
inside the hold only.

⚠️ **The splitter meets that condition on the pin, and the condition is not
sufficient.** The campaign of 2026-08-25 measured two distinct defects of this
harness. Neither is a firmware defect: the bank stays at 0 diff and the five
untouched outputs stay at 96 / 1536 in every course.

**Defect 1 -- a burst above six detents is not reliable on LENGTH.** A single
burst applies exactly its detents up to **6**, and becomes erratic from **7**:
3, 5, 7, 5, 1 and 4 applied for 7 to 12 requested. Slowing 6 detents to a 654 ms
hold keeps them exact; accelerating 8 detents to a 222 ms hold keeps them wrong,
with the same deficit. So the factor is the **detent count**, not the hold, not
the rhythm. ⚠️ The limit is **not universal**: on SUBDIV, bursts of 7 (`rig`) and
8 (R11) apply in full. The contradiction is open -- `docs/open-risks.md` line 44.

**Defect 2 -- the field cursor moves.** After a burst the selection frame can
move by exactly one field. Every detent is driven inside the hold, so the stray
event is not an injection. The rest between bursts is **not** the cause, and
neither entry nor release margin explains it. **Undetermined.**

**The working rule until a splitter is designed:** keep a burst to **6 detents
or fewer**. Every recipe validated in P2.6 respects it on LENGTH.

The suppression flag (`rotatedWhileShiftHeld`) stays a safety net, never the
plan — and the counter below is what proves it was not used.

## What the thirteen recipes ask of the splitter

Counted on the reference model, not estimated: the number of **consecutive**
`ShiftRotate` events each recipe of `gestureRecipes.test.ts` produces. Measured
2026-08-25.

| Recipe | SHIFT detents | Bursts of 12 | Splitting needed |
|---|---:|---|---|
| 1, 2, 3 navigation | 0 | — | no |
| 4 LENGTH plays a new period | 3 | 3 | no |
| **5 LENGTH clamps at the maximum** | **30** | **12 + 12 + 6** | **yes** |
| 6 SUBDIV changes the step duration | 1 | 1 | no |
| **7 SUBDIV stays within the 25 legal rates** | **50** | **12 + 12 + 12 + 12 + 2** | **yes** |
| 8 a step toggles, and no other | 0 | — | no |
| 9 a ratchet lands on an active step | 4 | 4 | no |
| 10 an inactive step refuses the ratchet | 0 | — | no |
| 11 the refused codes are skipped | 8, then 2 | 8 / 2 | no |
| 12 the triplet goes on and comes off | 10 in one run | 10 | no |
| 13 channel, LENGTH, back | 3 | 3 | no |

**Two recipes out of thirteen need more than one burst**, and both are the
saturation cases: recipe 5 walks LENGTH to its clamp, recipe 7 walks SUBDIV to
the end of its 25 rates. Recipe 12 is the one to watch: its two `setRatchet`
calls are contiguous in the event log, so a translator that groups consecutive
`ShiftRotate` events into one hold sees **10** detents — under the ceiling, but
only by two.

**Recipe 11 reaches `x24` in a single burst, and its refusal costs zero
detents.** Its `setSubdiv(-8)` is **eight positions in the list of 25 rates**,
not the value −8: measured, it lands on `ticksPerStep = 4`, which is 96 / 24,
that is `x24`. Eight detents give an injected hold of 30 + 8 × 54 = **462 ms**,
below the 678 ms of a full burst and far below the threshold, so the recipe needs
no splitting and its `SHIFT` hold stays valid.

`GestureDriver.setRatchet()` then tests `ratchetFitsStep()` **before** rotating,
so a code the rate refuses returns `applied: false` without emitting a single
event: at 4 ticks per step, `RATCHET_6` costs zero detents and the triplet costs
two. Whether the **firmware** skips the refused codes the same way is **not
established here**. The model says so; P2.6 is what will read it off the
module's binary.

## The units, and the rule that governs them

Four units, and they are never interchanged.

| Unit | Definition |
|---|---|
| **cycle** | 1/16 000 000 s of simulated time, exact |
| **tick** | one entry into `TIMER1_COMPA`, the engine's 96 PPQN grid, **counted**, never derived from a duration |
| **step** | `ticksPerStep` ticks, from the channel's SUBDIV |
| **onset** | one rising edge on an output pin |

**The rule**: milliseconds define the **physical injection** durations — the
5 ms, 60 ms, 900 ms and 750 ms of the table above — and they may appear in a
human report. **They are never a criterion of temporal validation.** Every
temporal criterion about the engine is expressed in **ticks**, and a tick is
counted from `TIMER1_COMPA`.

## The outputs, and which channel each one carries

Established by measurement in the harness, not assumed from navigation: the first
run of P2.5 edited channel 1 while measuring channel 0, and the mismatch is what
fixed the mapping.

| Output | Pin | Channel | Reached from the tab bar by |
|---|---|---|---|
| `OUT1` | PD7 | **channel 0** | **tab 1** |
| `OUT2` | PB0 | channel 1 | tab 2 |
| `OUT3` | PB2 | channel 2 | tab 3 |
| `OUT4` | PD6 | channel 3 | tab 4 |
| `OUT5` | PB1 | channel 4 | tab 5 |
| `OUT6` | PB3 | channel 5 | tab 6 |

Tab 0 is the clock tab and carries no channel. P2.5 relies on this mapping, and
P2.6 must keep it: a recipe that targets channel k must be verified on `OUT(k+1)`,
the five others serving as the non-contagion witnesses.

## The encoder rests at both pins HIGH

`Encoder` uses `RotaryEncoder::LatchMode::FOUR3` (`encoder.h:30`), and the pins
are `INPUT_PULLUP`. The rest state is therefore (HIGH, HIGH), and one detent is a
full cycle back to it. **Which cycle direction the library reads as positive is
measured in P2.1, never assumed** — libGravity's default direction is the
opposite of the original firmware's, and the adapter absorbs it.

## The direction, MEASURED on 2026-08-25

| Physical sequence | Effect on the tab bar |
|---|---|
| **A first** — A low, B low, A high, B high | **+1** |
| **B first** — B low, A low, B high, A high | **−1** |

Six detents, each moving the selection by exactly one, each followed by 1248
bytes of display traffic.

⚠️ **The first detent after boot is swallowed.** It moves nothing and produces
**zero** bytes on the bus, so the interface never saw an event — the encoder
layer absorbed it. This matches the audited anomaly of libGravity: the encoder
has an unsafe initial state. The driver therefore **primes** with one detent
after boot and checks that it produced nothing, before any recipe runs.

## The presses, MEASURED on 2026-08-25

The screen is read as a whole: a hash of the panel memory is the signature of the
interface state, so no layout assumption is needed.

| Hold | Screen signature | I2C traffic | Reading |
|---|---|---|---|
| **5 ms** | unchanged | **0 bytes** | the debounce bites: the firmware never saw it |
| **60 ms** | **changed** | 1248 bytes | short press taken |
| **900 ms** | **back to the starting signature, byte for byte** | 1248 bytes | long press, exactly one level up |

The release is part of every gesture, because the long press fires on release.
The 900 ms case is the strongest of the three: coming back to the **same panel
bytes** proves both that the long press was seen and that it climbed exactly one
level, without needing to read any internal state.

## SHIFT plus rotation, MEASURED on 2026-08-25

Run inside the EDIT screen, on step 0 of A1, which the factory mask makes active.

| Observation | Value |
|---|---|
| ratchet nibble of step 0 | **00 → 04**, three codes further |
| SHIFT held | **192 ms**, under the 750 ms threshold |
| I2C traffic | 3120 bytes |
| pattern steps | **unchanged**, mask still `0x9111` |

The last line is the parasite witness, and it is brutal on purpose: in EDIT,
`SHIFT` plus a long press **clears the pattern**. A pattern that still carries its
factory mask therefore proves that the release produced no long press.

## Verification B, MEASURED on 2026-08-25

Two runs of the same binary: the first without an EEPROM image, so the factory
control gates verification A; the second with an image in `SEQ` mode, steps
0, 3, 4, 9, 15, `SUBDIV x4`, which is the only way to reach `SEQ` — **the channel
mode is not editable in the interface**.

Everything below is counted in **ticks**, never converted from microseconds.

| Observation | Before | After | Meaning |
|---|---|---|---|
| OUT1 period | **384** | **456** | 16 steps → 19 steps of 24 ticks: LENGTH applied |
| OUT2 to OUT6 period | 384 | **384** | no contagion: LENGTH is per channel |
| OUT1 step | **24** | **32** | `x4` → `x3`: SUBDIV applied |

**The deferral of ADR 0004 is observed, not assumed**: the SUBDIV gesture lands at
tick **4643**, the next beat boundary is **4704**, and the first step of 32 ticks
is seen at **5308** — after the boundary, never before. The harness records the
three ticks before interpreting them.

⚠️ **The first run of this phase edited the wrong channel**, and the non-contagion
witness is what showed it: `OUT2` moved to 456 while `OUT1` stayed at 384. The
navigation carried one rotation too many, so the gesture landed on channel 1. The
witness turned a silent mistake into a visible one.

## Three witnesses, and three classes of failure

A fourth witness, for the SHIFT long press specifically, is described below:
the behavioural witnesses cannot see it.

A verification that cannot separate these three says nothing about the firmware.

| Class | What happened | Witness |
|---|---|---|
| **1. gesture not injected** | the firmware never saw it | **no I2C traffic** after the gesture. Any state change bumps `revision()`, so the screen redraws; a silent bus means the interface never reacted |
| **2. instrument wrong** | the reading is meaningless | **control read**: the factory patterns are read back from RAM before any gesture and compared with the masks the domain defines. A mismatch means the address or the arithmetic is wrong |
| **3. firmware at fault** | traffic seen, instrument sane, effect wrong | the only case where an anomaly may be attributed to the firmware |

**Rule of verdict**: a recipe whose verification does not separate the three is
declared `INVALID`. Never `PASS`, never `FAIL`.

### The three verdicts, in full

**PASS** — all five hold:

- the expected gesture produced the expected traffic;
- the instrument control is sane;
- the observed value or effect is **exactly** the expected one;
- the observation window is long enough to make the result decidable;
- no `INVALID` condition applies.

**FAIL** — traffic present, instrument sane, and the observed effect differs from
the expected one. This is the only shape under which an anomaly may be attributed
to the firmware.

**INVALID** — the gap cannot be attributed to the firmware: no traffic after a
gesture (class 1), a failed bank control (class 2), an observation window too
short to decide, or a SHIFT hold measured above 750 ms.

## Every criterion carries its class, and the class decides the verdict

Written 2026-08-25. Until then the probe called every red `bad`, so a gesture
that was never injected read exactly like a firmware defect. The three classes
above are now **carried by the code**, not only by this document: `ok`, `bad`
and `inval` are three distinct helpers, and `inval` also raises a separate flag.

| Class | Helper | Meaning | Can it ever become `FAIL`? |
|---|---|---|---|
| 1 | `inval` | no traffic — the interface never saw the gesture | never |
| 2 | `inval` | the instrument or a precondition is unsound | never |
| 3 | `bad` | traffic present, instrument sound, effect wrong | this is the only path |

### The witnesses, and what each one gates

A class-3 criterion is evaluated **only** when every witness it depends on is
green. Otherwise it is `INVALID` — never a second, derived defect.

| Witness | Class | Gates |
|---|---|---|
| six detents: sample count | 2 | the detent traffic and displacement criteria |
| six detents: traffic | 1 | the displacement criterion |
| `patternBank` unchanged by rotations alone | 2 | every criterion of P2.3, verification A and FRACT |
| factory control byte for byte | 2 | every criterion of P2.3, verification A, FRACT and phase B |
| **entry into EDIT** | 2 | P2.3, verification A, FRACT |
| SHIFT traffic | 1 | the ratchet and parasite criteria of P2.3 |
| SHIFT hold below 750 ms | 2 | the same two |
| **the ratchet effect of P2.3** | 3 | verification A, whose four gestures start from the code P2.3 left |
| traffic of the four verification-A gestures | 1 | the four criteria of verification A |
| FRACT traffic, window purity, burst measurement, splitting | 1 and 2 | the two FRACT effect criteria |
| phase-B initial state, phase-B traffic, phase-B bursts | 1 and 2 | LENGTH, non-contagion, SUBDIV, ADR 0004 |

⚠️ **A wrong upstream effect makes the downstream expectations meaningless, not
wrong.** Verification A drives the ratchet on from the code P2.3 left behind, so
if the P2.3 criterion is red the four A criteria are `INVALID`. One root defect,
one `bad`, never a cascade. This dependency was **found by the counter-evidence,
not by reading the code**: with the P2.3 gesture skipped, `triolet pose` read 03
instead of 07 and produced a firmware defect whose real cause was class 1.

### Entry into EDIT, and why the tab band is the witness

The EDIT screen is rendered by `PatternScreen`, which draws **no tab bar**; every
other level is rendered by `MainScreen`, which draws eight tab boxes across the
full width. The bottom band therefore carries a measured, threshold-free
signature: **8 slots with ink before the gesture, fewer than 8 after**. Measured
2026-08-25: 8 then **4**, the four being the left-aligned footer of the EDIT
screen (`FOOTER_X = 4`). The criterion also requires traffic on the navigation
gesture and a screen signature distinct from both the tab bar and the tab level.

It proves the interface left the tab-bar rendering for the pattern rendering. It
does not prove *which* channel is being edited; that stays the business of the
output mapping.

### Four traffic deltas that did not exist

The four gestures of verification A — triplet on, triplet off, step toggled,
step toggled back — published no traffic figure, so a gesture never injected and
a wrong effect produced the same line. The probe now measures a delta for each
one on the production binary. Measured 2026-08-25: **2340, 4680, 780 and 1248
bytes**. A zero on any of them is class 1.

### Sample floors, each derived from the harness

A criterion must never be green on nothing. Each floor comes from a count the
harness itself fixes, not from a round number.

| Criterion | Floor | Where the number comes from |
|---|---|---|
| six detents | **exactly 6** | the harness injects three A-first then three B-first |
| bursts, structure phase | **at least 5** | five `shiftRotate` calls, each yielding at least one burst |
| bursts, temporal phase | **at least 2** | two `shiftRotate` calls |
| non-contagion | **exactly 5 outputs** | OUT2 to OUT6 |
| press signatures | **all four present** | the reference plus the three holds |
| ADR 0004 deferral | both ticks present | a missing tick decides nothing |

⚠️ **`salves de la phase B` was green on zero samples** until 2026-08-25: its
test was `OVER2 == 0` with no count, so an empty log passed. Its twin in the
FRACT block already required `NSALVES > 0`.

### The counter-evidence for the three classes

`SELFTEST=1` re-runs the whole probe three times, through causes rather than
faked measurements:

| Lever | Cause reproduced | Required result | Measured 2026-08-25 |
|---|---|---|---|
| none | everything sound | `PASS`, exit **0** | PASS, 0, 0 defect, 0 undecidable |
| `EXPECT_RATCHET_APRES=05` | the expectation changes, the instrument does not | `FAIL`, exit **1**, no *witness* INVALID | FAIL, 1, 1 defect at the root, 4 downstream |
| `SKIP_EDIT=1` | the EDIT precondition is not established | `INVALID`, exit **5**, zero defect | INVALID, 5, 0 defect, 9 undecidable |
| `SKIP_SHIFT=1` | the SHIFT gesture is not injected | `INVALID`, exit **5**, zero defect | INVALID, 5, 0 defect, 7 undecidable |

Each assertion checks the **exit status and the verdict word**, not just a
string in the output.

The second lever moves the **expectation**, never the instrument: that is what
makes it the only path able to feed a `FAIL`.

⚠️ **The fourth lever is the decisive one.** With the SHIFT gesture skipped, the
downstream effect is genuinely wrong — `triolet pose` reads **03 instead of
07** — while its own traffic, bank and factory witnesses are all green. Only the
chaining through `P23_OK` keeps it from becoming a firmware defect. The
assertion checks exactly that: `⛔ triolet pose` present, `❌ triolet pose`
absent.

## The fourth witness: a parasitic long press that nothing else can see

Added 2026-08-25, and it exists because the obvious witness is **not enough**.

In EDIT, `SHIFT` plus a long press clears the pattern, so "the steps are intact"
looks like proof that no long press fired. It is not. `onShiftLongPress()`
returns early when `rotatedWhileShiftHeld` is set, and a SHIFT hold that carries
a rotation always sets it. The long press therefore fires, is absorbed, and
leaves the pattern untouched.

**Measured, on a deliberately broken harness**: with the split and the guard
removed, a 26-detent request became one hold of **1434 ms**, which produced
**two** long presses. The pattern came back **intact** and the whole bank came
back **identical to the factory one**. Every behavioural witness said PASS on a
gesture that was physically invalid.

What separates the two cases is the firmware's own counter, `suppressedLong`,
read from RAM by its address from `avr-nm`. It is a `uint16_t` scalar, so
reading it needs no layout guarantee.

⚠️ **The counter counts long presses that were ABSORBED, not long presses.**
`onShiftLongPress()` only increments when `rotatedWhileShiftHeld` is set; a long
press with no rotation goes straight through to `clearPattern()` and increments
nothing. **Both witnesses are therefore required, and neither is redundant:**

| Witness | What it rules out |
|---|---|
| the pattern's step masks are intact | no long press went **through** to the controller |
| the counter is unchanged | no long press was **absorbed** by the safety net |

⚠️ The counter is **shared** with the encoder's own suppression
(`onEncoderLongPress`). The harness therefore counts the transitions of the
encoder switch across the measurement window and requires **zero**: with the
switch never driven low, `onShiftLongPress` is the only possible writer. Without
that count, a non-zero delta would have two possible causes.

### What the harness checks before it trusts the symbol

The symbol is resolved with `avr-nm -S --defined-only`, and every step below is
a blocking one. The order is not free: the type decides whether the address is a
data-space address at all, so it is checked **before** any conversion.

| Step | Rule | Why |
|---|---|---|
| 1 | the name predicate matches **exactly one** symbol | `head -1` on several matches is an arbitrary choice. The predicate is anchored on the Itanium length prefix, `14suppressedLongE$`, so `suppressedLongPresses` cannot match |
| 2 | the type is `b`, `B`, `d` or `D` | `.bss` or `.data`. Anything else — `t`, `T`, `r`, `R`, `U`, `C` — is not a RAM object |
| 3 | the size column is **present** and equals **2** | `avr-nm -S` **omits the size column when `st_size` is 0**, so a sized row has four fields and an unsized one three. A parser that reads `$2` as the size reads the *type* on an unsized symbol |
| 4 | the VMA is at or above `0x800000`, and the RAM address is `VMA − 0x800000` | the semantic conversion, not `& 0xFFFF`. A mask silently turns a flash symbol into a plausible RAM address; the subtraction fails loudly |
| 5 | `addr > avr->ioend` and `addr + 1 <= avr->ramend` | the bounds come from simavr's own core definition, never from literals. The `+ 1` is what makes a **2-byte** object fit, not just its first byte |
| 6 | the counter is read as `readable` plus a value, never as a sentinel | a `-1` meaning "unreadable" and a delta of `-1` are different facts and must not share an encoding |
| 7 | the delta is **not negative** | the counter is monotonic; a negative delta can only be a wrong symbol or a torn read |
| 8 | the encoder switch did not move during the window | otherwise the counter has two possible writers |

**Every one of the eight failures is `INVALID`, never `FAIL`.** The cause is the
instrument, and nothing can be concluded about the firmware. Measured 2026-08-25
under `SELFTEST=1`: the nine negative cases — zero symbol, several symbols,
size 1, type `t`, address below `ioend`, a 2-byte object straddling `ramend`, a
null address, a wrong address end to end, and a negative delta end to end — all
produce `INVALID`, 9 out of 9.

⚠️ **The retention of the symbol is an accident of the optimiser, not a
property.** `flexseq::input::suppressedLongPresses()`, the accessor, is **absent
from the binary**: `--gc-sections` removed it for want of a caller. The variable
survives only because two live functions increment it, and the whole image
contains exactly **eight** references to `0x0220`, all of them inside those two
increments. Nothing reads it. Nothing in C++ or in the build guarantees that a
non-`volatile`, internal-linkage object with no reads is preserved. Observed
stable over three builds, two of them from clean: same `firmware.hex` md5, same
address. If it ever disappears, step 1 turns the witness `INVALID`.

## The exit codes of `run-gesture-probe.sh`

### The global verdict, and the two counters it reads

The script counts, it does not flag. `bad()` increments `BAD_COUNT` and nothing
else; `inval()` increments `INVAL_COUNT` and nothing else. The two are disjoint,
which is what makes a three-valued verdict possible at all — before 2026-08-25
`inval()` also raised the failure flag, so the two classes were one state.

```
BAD_COUNT   > 0                        -> FAIL
BAD_COUNT  == 0 and INVAL_COUNT > 0    -> INVALID
BAD_COUNT  == 0 and INVAL_COUNT == 0   -> PASS
```

**`FAIL` wins over `INVALID`, and that is deliberate.** A defect at the root
makes the criteria downstream of it undecidable, so a real `FAIL` normally
arrives with several `INVALID` beside it. Measured: a wrong expectation on the
P2.3 ratchet gives **1 defect and 4 undecidable criteria**, because the four
verification-A gestures start from the code P2.3 leaves. Ranking `INVALID`
first would hide the one thing that was decided.

This is sound only because of the gating described above: a `bad` can fire
**only** when its own witnesses are green. The `INVALID` that survive next to a
`FAIL` are therefore never its prerequisites — they are its consequences. The
counter-evidence proves the property rather than assuming it: with a witness
broken, the run produces **zero** defect.

### Two families of exit code, and they must not be confused

| Code | Family | Meaning |
|---|---|---|
| **0** | global verdict | `PASS` |
| **1** | global verdict | `FAIL` — at least one firmware criterion failed |
| **5** | global verdict | `INVALID` — no firmware criterion failed, at least one is undecidable |
| 2 | instrumentation error | a bad argument, or a mutation pattern absent from the source |
| 3 | instrumentation error | the bank control failed: class 2, the run stops at once |
| 4 | instrumentation error | a SHIFT burst was refused **before injection** |
| 127 | instrumentation error | a tool is missing |

Codes 2, 3, 4 and 127 stop the run **before** any verdict line: there is nothing
to weigh. Their messages carry the word `INVALID` so the vocabulary stays one
vocabulary, but their codes are unchanged — a caller that already distinguishes
them keeps working.

`SELFTEST=1` is **not** covered by this grammar. It judges the instrument, not
the firmware, and keeps a binary 0/1. Its own failure flag is a separate
variable, and the SELFTEST block exits in both branches, so it can never reach
the global counters. One assertion checks that at runtime: `BAD_COUNT` and
`INVAL_COUNT` must come out of the block exactly as they went in.

`SELFTEST=1` runs no gesture. It replays three mutations of the splitting and
nine negative cases of the fourth witness, and requires each one to be detected.

The mutations: no splitting at all (exit 4), a ceiling forced back to 20 detents
(the static assertions refuse to compile), and neither guard nor splitting (a
1434 ms hold and a non-zero counter). Measured 2026-08-25: 3/3 detected.

The negative cases of the witness are driven by three overrides that touch
nothing in the firmware: `SUPPRESSED_SYMBOL` aims the resolver at another
symbol, `SUPPRESSED_ADDR_FORCE` forces the address, `SUPPRESSED_BIAS` biases the
second reading of the counter. Four cases are resolved before any simulation;
three run the `symbolcheck` phase, which validates the address and returns
immediately; two re-run the whole probe and check that it exits non-zero with an
`INVALID` classification. Measured 2026-08-25: 9/9 detected.

## Verification A, MEASURED on 2026-08-25

The control is not a list of expected numbers copied into the harness. The
harness **links the domain itself** — all of `src/domain/` — builds a
`PatternBank` natively, and compares it **byte for byte** with the 320 bytes read
from the simulated RAM.

**The expectation is produced by the firmware's own boot rule**, not by a rewrite
of it (`src/main.cpp:170-176`):

```c
if (!persistence.load(eeprom, persistentImage)) {
    loadFactoryPatterns(patternBank);
}
```

The harness replays exactly that. `PersistenceScheduler::load()` is a template
over a `Storage` that exposes `read(address)`, so the harness supplies a ten-line
adapter over the EEPROM image bytes it hands to the machine, and falls back to
`loadFactoryPatterns()` when the reader refuses the header — which is also the
no-image case, simavr returning `0xFF`. **One control covers both**, and it
reports which source it used: `controle_source` reads `usine` or `image`.

⚠️ **Until 2026-08-25 the control was BYPASSED whenever an image was preloaded**
(`temporal || memcmp(...)`), because the image replaces the bank and the factory
comparison could only fail. Verification B therefore ran with no class-2 control
at all. It now verifies the state **actually injected**. Counter-evidence:
`IMAGE_MUTATE=<offset>` flips one byte of the copy given to the machine while the
expectation keeps the original image — measured, the control turns red and the
run exits **3**.

⚠️ **The exit codes 3 and 4 were never propagated by the script.** It used
`if ! "$BIN" …; then RC=$?`, where `$?` is the status of the negation, so `RC`
was always 0 and the run died with the default code 1. Found by the counter-evidence
above, which demanded a 3 and got a 1.

| Step | Observation |
|---|---|
| control at boot | the RAM bank is **identical, byte for byte**, to the one the domain builds |
| triplet applied | ratchet nibble of step 0 = **07** |
| triplet removed | nibble back to **00**, and the whole bank matches the factory one again |
| step toggled | mask `0x9111` → **`0x9113`**: step 1 alone changed |
| step toggled back | mask `0x9111`, and the **entire** bank is the factory one again |

If the control fails, the harness **stops with exit code 3** and no verdict is
rendered about the firmware. That is class 2: the instrument is wrong, and
nothing else can be concluded.

## What is read, and why it is legitimate

`patternBank` only. Its layout is guaranteed by a `static_assert` that ships with
the firmware: `sizeof(Pattern) == 20`, four bytes of steps then sixteen bytes of
ratchet nibbles, and `PatternBank` is `Pattern[16]`. The address comes from
`avr-nm`. **Nothing else is read**: LENGTH and SUBDIV are verified on the output
pins, because their storage carries no such guarantee.

`-g` was measured and set aside: it leaves the `.hex` byte-identical but produces
no usable type information under LTO.
