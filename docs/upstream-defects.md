# Upstream defects

Defects found in the two Sitka Instruments sources FlexSeq depends on or refers
to. This file exists to make an upstream contribution possible: each entry names
the defect, where it is proven, and how large the fix looks.

It is **not** a source of truth for FlexSeq's own design. What FlexSeq does about
each defect belongs to the PRD (§18 for the reachability audit) or to
`docs/open-risks.md` (lines 14, 20, 21). Here we describe the dependency, not our
workaround.

## Scope and honesty

**`libGravity` is audited; the original firmware is not.** The libGravity list
below was audited against commit `9be88be1f4`, entries 1 to 9 are each reproduced
by a test in `env:native_libgravity`, and the pin has moved to `5c0c34f` since. The original firmware has only ever been read as
a behavioural reference, so its single entry is what happened to surface, not the
result of a sweep. Do not read the short list as a clean bill of health.

## libGravity, pinned at `5c0c34f`

**The pin moved on 2026-08-24**, from `9be88be1f4` to `5c0c34f`, the head of the
project fork. The whole compiled surface of the move is seven added lines in
`src/encoder.h`. The re-audit passed: the characterization suite stays conform
with `EXPECTED` unchanged, `test_encoder` keeps its 19 assertions and its 2
anomalies, and `test_gravity` its 6.

**A fork now exists**, and ADR 0008 carries its charter. Entries 1 to 9 below are
the **audited anomalies**, and the charter forbids repairing them: they are the
reason `InputAdapter` exists and 68 assertions describe them. Entries 10 and
above are what the fork may repair.

Verified 2026-08-20 for entries 1 to 9: none was fixed upstream three commits
after `9be88be1f4`. Re-read on `5c0c34f` for two of them on 2026-08-24: the
encoder still carries `// Validation (TODO: add debounce check).` at
`encoder.h:101`, and the serial handler is still mistyped.

| # | Defect | Proven by | Fix looks like |
|---|---|---|---|
| 1 | **The compiler warns about this one at every full build**, `-Wsign-compare` on `analog_input.h`, which nobody had connected to the defect until the warning reference was generated on 2026-08-24. `AnalogInput::IsRisingEdge()` never reports a negative-to-positive crossing. `old_read_` is `uint16_t` while `read_` is `int16_t`, so a negative previous value becomes a large positive and "it was already high" is true whenever it was in fact negative. That is the most ordinary case on a bipolar input | `test_analog_input`, 2 assertions | one type |
| 2 | `Button` loses a release that falls inside the debounce window | `test_button`, 1 assertion | moderate — the state machine has to remember the release |
| 3 | `Encoder` starts in an unsafe state: `previous_pos_` is not initialised, so the first poll can report a movement that never happened. ⚠️ **Measured on the production binary on 2026-08-25**: the anomaly shows up as a **lost** first detent rather than a false one. The very first detent after power-up moves nothing and produces **zero** display traffic, so the interface never receives an event; every detent after it moves exactly one step. Any harness that drives the encoder must therefore prime with one detent and check that it produced nothing | `test_encoder`, 2 assertions; `run-gesture-probe.sh` | one initialisation |
| 4 | `Encoder` has no debounce at all, and the code says so: `_rotate_change()` carries `// Validation (TODO: add debounce check)`. ⚠️ **The consequence written here until 2026-08-23 was REFUTED on the module**: two fast detents do NOT cancel each other. Measured, they give exactly −2, and fifteen seconds of fast rotation give −917 with zero reversals. What is real is a bounce **at a turning point**, the fastest measured reversal being 2 ms | hardware, `docs/open-risks.md` line 20 | an enhancement, not a one-liner |
| 5 | `DigitalOutput::Init()` does not force the pin OFF, and does not turn off an output that is already active | `test_digital_output`, 2 assertions | one write |
| 6 | `Gravity::Process()` has an uninitialised loop index — `for (int i; i < OUTPUT_COUNT; i++)` — which is undefined behaviour | read; `test_gravity` pins the function's composition | one initialisation |
| 7 | `Clock::SetSource()` does not handle the `SOURCE_LAST` sentinel, which surfaces as a compiler warning rather than wrong behaviour | read | one guard |
| 8 | Packaging: a consumer must declare `NeoHWSerial @ 1.6.9` explicitly; the library does not pull it | build | manifest |
| 9 | `Encoder` accelerates, hides the factor, and the acceleration is **unreachable by hand**. `_rotate_change()` returns the movement accumulated since the last poll, multiplied by 3 below 16 ms and by 2 below 32 ms, and `Process()` passes it to `on_rotate()` in **one** call. The factor comes from `encoder_.getMillisBetweenRotations()`, and `encoder_` is **private**, so a consumer cannot recover the true detent count from the value it receives. Measured on the module 2026-08-23: the factor never fired -- 24 callbacks, all of magnitude 1. A factor of 3 needs more than 62 detents per second | `env:encoderprobe` on the module; `docs/open-risks.md` line 30; ADR 0003 | an accessor, or a switch that turns the acceleration off |
| 10 | **`Clock::SetSource()` attaches a serial handler with the WRONG TYPE, and `-fpermissive` accepts it.** `NeoHWSerial::isr_t` is `bool (*)(uint8_t, uint8_t)` (`NeoHWSerial.h:213`); libGravity passes functions that return `void` (`clock.h:29, 89, 115, 159`). PlatformIO's Arduino AVR builder adds `-fpermissive`, so the type error becomes a warning. At run time `_rx_complete_irq()` does `saveToBuffer = _isr(data, status)` (`NeoHWSerial.h:157`) and the callee sets no return value, so the flag reads an undefined register: **every received MIDI byte is buffered or dropped at random**. Nothing in FlexSeq reads that ring, so no behaviour is observable, but the read is undefined on each byte | the compiler, quoted below | four lines: both handlers return `bool` and return `false` |
| 11 | **uClock is declared in `depends` while libGravity carries its own copy, in a DIFFERENT version.** The copy lives in `src/uClock/` and every include of it is quoted and relative (`clock.h:18`, `uClock.cpp:34-35`), so it is the one that compiles — measured in the build tree. Its header says **2.2.1**; `depends=uClock` resolves **2.3.0** from the registry, which is downloaded and never compiled. So the manifest advertises a version that never runs, and the running version is declared nowhere | read and measured, 2026-08-24 | one line: remove `uClock` from `depends` |
| 12 | **The external clock freezes the module for 2.5 to 60 seconds after it is selected, and the delay grows as the input PPQN falls.** Found on 2026-09-03 by lot XCLK, the first course of the project to drive an external clock. `handleTimerInt()` runs its external sync branch with NO guard on `clock_state`, so it runs before the first pulse arrives. `ext_interval` is still 0 and `ext_clock_us` is still 0, so `counter -= phase_mult(sync_interval)` takes a subtraction on a `uint32_t` that holds 0. The result is a very large number, `freqToBpm()` of it is almost 0, `constrainBpm()` clamps it to `MIN_BPM`, which is 1, and `setTimerTempo(1)` programs an output tick of 625 ms. One step of a quarter note then lasts 60 s. **The recovery waits for the next sync boundary**, which is `mod_clock_ref` ticks away, so `96 / input_ppqn` times 625 ms | **the trigger probe, course `extclock`, with the timer witness `EXT_TRACE_MS`.** The witness reads OCR1A and the prescaler of TCCR1B, so it reports the period the timer really applies. At input PPQN 24: 5208 us at 20 ms, **624992 us at 380 ms**, and 5198 us at 2880 ms. The prediction for PPQN 2 was 48 times 625 ms, so 30 s: **measured 30900 ms**, against a start at 900 ms. Both figures come from one mechanism | **REPAIRED in the fork on 2026-09-03**, pin `c2ec8ab`, by a decision of the owner: it is a defect and it produced unwanted musical behaviour. The sync branch now waits for `ext_clock_tick` to be non-zero, which is exactly the condition under which `ext_interval` carries a measured value. **One line, 26 bytes of Flash.** Measured at the slowest input rate, where the freeze lasted 60 s: the tick stays between 5205 and 5208 us for the whole run instead of collapsing to 624992, and the four input rates play the step within 0.01 % of the expected duration. ⚠️ **The repair also unblocked the test coverage**: the four rates and the MIDI course now run in the routine pass, because the durations fall from 183 to 87 seconds of simulation. **Not yet offered upstream** |

Entry 10's warning, quoted exactly:

```
clock.h:89:48: warning: invalid conversion from 'void (*)(uint8_t, uint8_t)'
to 'NeoHWSerial::isr_t {aka bool (*)(unsigned char, unsigned char)}' [-fpermissive]
```

**A top-level pin cannot fix entry 11, and trying made things worse.** Measured
on 2026-08-24: `midilab/uClock@2.3.0` in `lib_deps` pinned nothing — uClock stayed
the transitive dependency with the bare `uClock` constraint — and it added a
**second** `uClock.cpp.o` to the link, so a second definition of the same ISR.
The link survives because the registry archive member is never pulled. The line
was removed. A top-level `lib_deps` entry is compiled; a `depends` entry is only
resolved and put on the include path.

Six of the first nine are one to three lines.

**Encoder direction is not on this list, deliberately.** It depends on how the
two pins are wired, and both projects expose the setting —
`Encoder::SetReverseDirection(bool)` here, `reverseEnc` in the original. Their
defaults are simply opposite. See PRD §4.1.

## Not a defect, but a cost worth naming once

**916 bytes of Flash go into 32-bit float arithmetic, and all of it comes from
uClock.** Measured with `avr-objdump` on 2026-08-24: the only callers of
`__divsf3`, `__mulsf3`, `__floatunsisf`, `__fixunssfsi`, `__cmpsf2` and `__gesf2`
are `setTimer(unsigned long)`, `uClockClass::setTempo(float)` and `__vector_11`,
which is uClock's timer ISR. So the float maths also runs **inside an
interrupt**, and it is part of that ISR's 974 bytes.

The figure is 1072 bytes of helpers minus 156 bytes of integer helpers. It is
3.2 % of the Flash the project has.

⚠️ **BOTH FIGURES ABOVE ARE UNDERCOUNTS, and the count is settled on
2026-09-03 by lot S2.1-c.** `avr-nm` reads **1474 bytes** over **23 symbols** on
the firmware that carries the float path. The 916 of 2026-08-24 and the 1082 that
this section carried until today both missed the internal helpers of the float
routines: `__fp_cmp`, `__fp_split3`, `__fp_splitA`, `__fp_round`, `__fp_psc*`,
`__fp_szero`, `__fp_zero`, `__mulohisi3` and `__mulshisi3`.

**The change was then measured on a counter-build**: Flash **28764 to 27672**, so
**-1092 bytes**, and RAM **+2**. The 382 bytes between 1474 and 1092 are the cost
of the integer forms. The integer division helpers were already linked, so they
add nothing. Attribution: the float helpers go to **zero**, `setTimer` returns
140 bytes, `main` returns 12, and **the timer ISR GROWS by 44**.

⚠️ **And the decision this section called for HAS been taken.** ADR 0008 was
amended on 2026-09-03: the fork may replace the computation when measurement
proves the equivalence, under thirteen conditions. The sentence below stays
because it states the constraint correctly; it is no longer the last word.

**It is irrecoverable, and that is the point of writing it down.** uClock is
vendored inside a pinned dependency, and ADR 0008 forbids the fork from touching
a musical behaviour. Anyone hunting for Flash will find these symbols at the top
of the list; this line saves them the search. The only lever would be rewriting
uClock's tempo arithmetic in integers, inside an ISR, where musical accuracy is
what is at stake. That needs an explicit decision, not an optimisation pass.

## libGravity's own reference firmware (`firmware/Gravity/`)

**A third source, read on 2026-08-25.** libGravity ships reference applications
next to the library, under **MIT** rather than GPLv3. They implement swing, a
duty cycle, a mute, CV destinations and a multi-slot state manager, so FlexSeq
reads them before writing lots G, I, J, 13 and E. The catalogue of what is worth
reusing is in `WORKPLAN.md`; what follows is what must not be.

Nothing here is compiled into FlexSeq. These files are examples, and the build
tree confirms it: only `libGravity.cpp` and `src/uClock/uClock.cpp` are linked.

| # | Defect | Established by | Fix looks like |
|---|---|---|---|
| A | **`applyCvMod()` assigns the wrong field four times.** `channel.h:215-218` writes `base_clock_mod_index` into the cv-modulated probability, duty cycle, offset and swing. The four destinations therefore receive a clock division index. It only shows when no CV destination is active, which is the branch that resets the cached values | read, 2026-08-25 | four lines |
| B | **`solidTick()` and `hollowTick()` are identical**, both `drawBox(56, 4, 4, 4)` (`display.h:227-228`). The hollow marker does not exist, so the swing indicator cannot tell a triplet division from a straight one — which is the whole point of having two | read, 2026-08-25 | one line, `drawFrame` instead of `drawBox` |
| C | **`displaySaveSlot()` returns nothing** when the slot is above `MAX_SAVE_SLOTS` (`display.h:248-255`). A non-void function that falls off its end is undefined behaviour | read, 2026-08-25 | one line |

**These are not on the fork's repair list.** The charter of ADR 0008 covers what
FlexSeq compiles, and it compiles none of this. They are recorded so that nobody
copies them, and so that an upstream contribution can name them if one is ever
made.

## Original firmware (`GravityFW`)

| # | Defect | Established by |
|---|---|---|
| 4 | **`CV2Calibration` is dead in the reading path: both inputs are conditioned by `CV1Calibration`.** The variable exists, is saved to EEPROM and is loaded back (`Gravity.ino:53,596,653`), but the code that turns an ADC reading into a value compares input 2 against `CV1Calibration` (`Interactions.ino:432-438`). So calibrating CV2 changes a stored byte and nothing else, and CV2's zero point is CV1's | read, 2026-08-23, during the conformity audit |
| 3 | **The encoder acceleration is present and commented out** (`Interactions.ino:100-104`), so the original accelerates nothing. Its only filter drops a reversal under 200 ms. This is not a defect of the original; it is recorded here because FlexSeq inherited an acceleration from libGravity that the original never had, and cancels it | read, 2026-08-23 |
| 2 | `channel::CV1Range` and `channel::CV2Range` are **dead fields**: declared in the struct, never read, never written, never displayed. The per-channel CV amplitude is hard-coded instead (`map(randMod, 0, 1023, -5, +5)`). They are nonetheless persisted, so 2 of every channel's 9 EEPROM bytes carry nothing -- 12 bytes across the six channels | read, 2026-08-22 |
| 1 | `saveState()` rewrites the whole state — more than 300 bytes — on every recording keystroke, so an EEPROM write of ~3.4 ms lands in the middle of a musical event | read; PRD §11 records the consequence for FlexSeq's own persistence |

## Before proposing anything upstream

Three things to settle first, none of them technical.

**Where.** Neither project is on GitHub. Both live on `git.sitkainstruments.com`,
so the contribution mechanism has to be checked before a patch is written.

**Our tests would invert.** `env:native_libgravity` documents these defects: 7 of
its 68 assertions fail *by construction*, and the runner fails on drift **in
either direction**. An upstream fix would therefore turn our suite red, and the
`EXPECTED` set plus `test/README` must be updated in the same move. That is the
intended behaviour, not an obstacle — but it is work.

**Nothing reaches us on its own.** FlexSeq pins a commit of its own fork by
decision (ADR 0008). An accepted upstream fix changes nothing here until the pin
is deliberately moved, and moving it requires re-auditing `Gravity::Process()`
(PRD §18) — which `test_gravity` turns into a failure rather than an oversight.
