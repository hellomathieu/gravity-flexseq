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

## The timings, and where each number comes from

| Constant | Value | Source |
|---|---|---|
| debounce | 10 ms | `button.h:16` |
| long press | 750 ms | `button.h:17`, and it fires **on release** (`button.h:79-84`) |
| short press | 60 ms | above the debounce, far below the long press |
| long press hold | 900 ms | above the threshold, with margin |
| quadrature edge spacing | 1 ms | the encoder carries **no debounce**; the pin-change interrupt only needs to be serviced |
| settle between gestures | 50 ms | the events are dispatched from `Process()`, so the loop must run |
| SHIFT hold ceiling | 750 ms | above it the release becomes a long press, and `SHIFT` + long press **clears the pattern** |

**A long `shiftRotate` is therefore split into bursts of at most 20 detents**,
SHIFT being released between bursts. The suppression flag
(`rotatedWhileShiftHeld`) is a safety net, never the plan.

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

## Verification A, MEASURED on 2026-08-25

The control is not a list of expected numbers copied into the harness. The
harness **links the domain itself** — `FactoryPatterns.cpp`, `Pattern.cpp`,
`PatternBank.cpp` — builds a `PatternBank` natively, seeds it with
`loadFactoryPatterns()`, and compares it **byte for byte** with the 320 bytes read
from the simulated RAM.

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
