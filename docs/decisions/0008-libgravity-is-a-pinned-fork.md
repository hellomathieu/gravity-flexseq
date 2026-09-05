# 0008 — libGravity is a pinned fork, with a charter

- **Status:** accepted
- **Date:** 2026-08-24
- **Supersedes:** —
- **Superseded by:** —

## Context

`CLAUDE.md` held one rule about the dependency: libGravity is pinned at
`9be88be1f4`, and its anomalies are constraints, never bugs to correct. That
rule made the adapter layer necessary (ADR 0002) and it made the 68
characterization assertions meaningful.

Two defects then showed that the rule was too narrow. Neither can be worked
around from the adapter layer.

**`Clock::SetSource()` attaches a serial handler with the wrong type.**
`NeoHWSerial::isr_t` is `bool (*)(uint8_t, uint8_t)`; libGravity passes
functions that return `void` (`clock.h:29, 89, 115, 159`). PlatformIO's Arduino
AVR builder adds `-fpermissive`, so the type error becomes a warning.
`_rx_complete_irq()` then does `saveToBuffer = _isr(data, status)`, and the
callee sets no return value. Every received MIDI byte is buffered or dropped at
random. Nothing reads that ring, so no behaviour is observable today, but the
read is undefined on each byte.

FlexSeq cannot repair this from outside. In MIDI clock mode the handler that
forwards the clock is `static` inside the class, so replacing it would lose the
MIDI input.

**libGravity declares `uClock` in `depends` while it carries its own copy.**
The copy is in `src/uClock/`, and every include of it is quoted and relative, so
it is the one that compiles. The registry copy is a different version — the
library runs **2.2.1** while `depends` resolves **2.3.0** — and it is never
compiled. A top-level pin in `platformio.ini` does not fix that: it compiles a
second copy, so a second definition of the same ISR joins the link. Measured on
2026-08-24.

The dependency also has no version constraint on U8g2 or RotaryEncoder, so the
largest Flash consumer of the build could move on its own.

Editing `.pio/libdeps` is not an option, and not because a rule forbids it. It
is a build cache. PlatformIO rebuilds it when `lib_deps` changes, on
`pio pkg update`, after a clean, and on a fresh clone. A repair there disappears
in silence and the suite stays green without containing it.

## Decision

libGravity is resolved from **`github.com/hellomathieu/libGravity`**, a fork of
`awonak/libGravity`, pinned by commit in `lib_deps`.

The fork licence stays **MIT**, with Adam Wonak's copyright intact.

### The charter — what the fork may change

- correct what FlexSeq cannot work around from its adapter layer;
- correct a defect **whose repair removes a FlexSeq workaround**;
- move to `PROGMEM` what has no reason to sit in `.data`;
- correct the package metadata;
- add a compilation guard.

### The charter — what the fork must not change

- **the seven audited anomalies.** They are the reason `InputAdapter` exists
  (ADR 0002), and 68 assertions describe them. A repair would invalidate the
  adapter layer and `docs/upstream-defects.md` at the same time;
- a change that stops FlexSeq compiling against upstream, or that changes the
  behaviour of another consumer **without that consumer asking for it**;
- a musical behaviour. uClock stays as it is. ⚠️ **Amended on 2026-09-03**, see
  below: a change that measurement proves equivalent is not a change of
  behaviour. ⚠️ **Amended again on 2026-09-05**, see below: a defect that
  produces a musical behaviour nobody asked for is not an intended behaviour.

⚠️ **The second line said "an API" until 2026-08-25, and that was too broad.**
The owner asked why an improvement offered upstream should be forbidden, and the
answer is that it should not. What the rule protects is that FlexSeq is never
locked into the fork, and an ADDITIVE change does not threaten that: a default
that keeps the previous behaviour leaves every existing consumer untouched, and
FlexSeq's own sources do not depend on it either way. The rule now states the
criterion instead of the word.

### Amendment of 2026-09-03 — the tempo path of uClock

The charter also permits this: **replace the floating-point tempo computation of
uClock with an integer computation, when measurement proves the equivalence.**

**The reason is a defect of the class the charter already covers.** The
floating-point runtime costs **1474 bytes of Flash**, over 23 symbols, and
FlexSeq cannot avoid it from the adapter layer. ⚠️ **This figure said 1082 until
2026-09-03**, and that was an undercount: the pattern that produced it missed the
internal helpers of the float routines — `__fp_cmp`, `__fp_split3`,
`__fp_splitA`, `__fp_round`, `__fp_psc*`, `__fp_szero`, `__fp_zero`,
`__mulohisi3` and `__mulshisi3`. **The measured net gain of the change is 1092
bytes**, the difference being the cost of the integer forms that replace it. Lot S2.1 established the cause on the call graph:

- the public interface of libGravity is **already integer** — `SetTempo(int)`
  and `int Tempo()`. FlexSeq passes no float, so no call site can bypass one;
- `Clock::Init()` calls `uClock.setTempo(DEFAULT_TEMPO)` without a condition.
  The float path runs at every boot;
- the external clock path calls `constrainBpm(freqToBpm(...))` inside uClock.
  A caller cannot reach it.

**The equivalence is measured, not asserted.** The two forms were compared:

- the **internal** path, where the BPM is an integer, gives **0 difference over
  the 400 values** of `[1, 400]`, and 0 over the `[20, 200]` of lot K;
- the **external** path, where uClock estimates a fractional BPM, differs by **at
  most 1 microsecond**. The integer form removes one division of two, so it is
  the more exact of the two forms.

**Six conditions apply to such a change, and all are mandatory:**

1. the interface of libGravity does not change;
2. the BPM clamp of `[1, 400]` stays. ⚠️ Without it the difference reaches
   124992 microseconds on a very slow or very fast external clock;
3. the equivalence is measured again before the change, on every integer BPM of
   the accepted range;
4. the measurement program lives in `tools/`, so that it replays;
5. the change is proved **on the pins** by `run-trigger-probe.sh`. Its CLOCK
   course measures the step to 0.01 %;
6. the gain is measured by a counter-build, symbol by symbol with `avr-nm`.

**Seven more conditions came from the owner on 2026-09-03.** He asked one
question: does the list prevent a regression of what already works, and of the
original features that nobody touched? It did not. The list was about the
computation. These seven are about the system.

7. **A baseline runs before the fork changes.** The whole proof set runs on the
   current HEAD, and the figures are kept. Without a "before", an "after" proves
   nothing;
8. the step duration is measured **on the pins at the bounds** of the product
   range — 20, 120 and 200 BPM — and not at 120 alone;
9. `run-drift-probe.sh` runs **before and after**, for the same duration. A
   change of the timer interval is what that probe measures;
10. **`Clock::Tempo()` returns the same displayed value.** Today it truncates a
    float. The integer form must return the same number, on the internal path
    and for an estimated external tempo;
11. **the external clock path has a proof course before the change.** It had
    none, in simulation or on hardware, when this amendment was written;
12. the stack peak and the static RAM are measured again. Removing the float
    changes the pressure on the registers;
13. **the pin bump is a re-audit**, by the list this ADR already gives: the
    characterization conform with `EXPECTED` unchanged, the four suites at the
    same counts, the resolved tree identical to the archive of the commit, and
    the memory measured.

**And `docs/original-conformity.md` is read again.** No conformity item may
depend on a fractional tempo.

**What stays forbidden is unchanged.** The seven audited anomalies stay. A
change that another consumer did not ask for stays forbidden. A defect that
FlexSeq can work around from its adapter layer stays out of the fork.

⚠️ **A limit of the measurement, named.** The comparison program runs on the
host. A `float` there is IEEE 32 bits, as on the AVR, but this measurement does
not verify the rounding of the AVR division. Condition 5 covers that gap.

### Amendment of 2026-09-05 — a defect that produces an unwanted musical behaviour

The charter also permits this: **repair a defect of the dependency that produces
a musical behaviour nobody asked for.**

**The line this amends is "a musical behaviour. uClock stays as it is."** That
line protects an **intended** behaviour. It was never meant to protect a defect.
The distinction was not written. ⚠️ **The fork repaired such a defect on
2026-09-03, before this amendment existed**, and this section closes that gap.

**The case that produced it.** The external clock froze the module for 2.5 to 60
seconds after the user selected it. The delay grew as the input rate fell.
`handleTimerInt()` ran its external sync branch with no guard on `clock_state`,
so the branch ran before the first pulse. `ext_interval` was still 0, so a
subtraction on a `uint32_t` underflowed. `freqToBpm()` of that value is almost
zero. `constrainBpm()` clamped it to `MIN_BPM`, and the timer programmed an
output tick of 625 milliseconds. One quarter-note step then lasted 60 seconds.

**No consumer can want that behaviour.** The interface promises a clock that
follows its source. The freeze is the absence of that promise, and not a choice
of the author. Entry 12 of `docs/upstream-defects.md` carries the full account.

**Six conditions apply to such a repair, and all are mandatory:**

1. the defect is entered in `docs/upstream-defects.md` **before** the repair,
   with its mechanism read in the source of the dependency;
2. the mechanism is established on the code, and not inferred from the symptom.
   A symptom names a suspect, never a cause;
3. the wrong behaviour is measured before the repair. The repaired behaviour is
   measured after, by the same harness;
4. a proof course guards the repair in the routine pass. Without one, nothing
   detects the return of the defect;
5. the repair stays minimal. It restores the behaviour the interface already
   promises, and it adds no feature;
6. the pin bump is a re-audit, by the list this ADR gives.

**The owner validated these six conditions on 2026-09-05, and kept them without
a change.** The draft of this amendment proposed them. The amendment of
2026-09-03 is different: seven of its thirteen conditions came from the owner.

**What this amendment does NOT permit:**

- a change of an **intended** musical behaviour. The shape of the clock, the
  swing and the ratchets stay as the dependency writes them;
- a repair of one of the seven audited anomalies. They stay, and 68 assertions
  describe them;
- a repair that FlexSeq can work around from its adapter layer. That work belongs
  to `InputAdapter`, and ADR 0002 says so.

**The other forbidden line stays satisfied.** The charter forbids a change that
alters the behaviour of another consumer **without that consumer asking for it**.
A consumer asks for the behaviour the interface promises. It never asks for the
freeze, so the repair gives every consumer what it already expected.

**The measurement of the case, for the record.** The witness is `EXT_TRACE_MS` of
the trigger probe. It reads `OCR1A` and the prescaler of `TCCR1B`, so it reports
the period the timer really applies.

- **before**, at input PPQN 24: 5208 microseconds at 20 ms, **624992 at 380 ms**,
  and 5198 at 2880 ms. The prediction for PPQN 2 was 48 times 625 ms, so 30
  seconds. The measurement gave **30900 ms**, against a start at 900 ms;
- **after**: the tick stays between 5205 and 5208 microseconds for the whole run.
  The four input rates play the step within **0.01 %** of the expected duration;
- **the repair**: the sync branch now waits for `ext_clock_tick` to be non-zero.
  That is the condition under which `ext_interval` carries a measured value.
  **One line, 26 bytes of Flash**, and the RAM does not move.

**The repair also unblocked the test coverage.** The simulated time of the
external courses falls from 183 to 87 seconds. The four input rates and the MIDI
course now run in the routine pass of `run-trigger-probe.sh`.

⚠️ **A limit of the proof, named.** The repair is measured in simulation, on the
simulated pins. The project records **no measurement of it on the module**. The
proof covers the firmware and the dependency. It does not cover the wiring of the
external clock jack.

⚠️ **The repair is not yet offered upstream.** The "Per commit" rule below asks
for that offer, and `docs/upstream-defects.md` entry 12 carries its state.

⚠️ **The repair of 2026-09-03 does not meet all six conditions, and the record
says which.** The conditions govern the repairs that come after this amendment.
For the one that came before it:

- **conditions 2 to 5 are met**, and the record shows it. The mechanism is read
  in the source. The two measurements come from the same harness. The course
  `extclock` runs in the routine pass. The repair is one line;
- **condition 1 is not established.** The entry and the repair belong to the same
  day, and no record fixes their order;
- **condition 6 is met in part.** The gates and the memory are recorded. The
  comparison of the resolved tree to the archive of the commit is not.

### Per commit

One correction per commit. Each commit names the entry of
`docs/upstream-defects.md` that it treats. Each correction is offered upstream,
and the state of the offer goes in that document.

## Consequences

**The existing guard becomes the safety net of the fork, and no tool was
written for it.** `run-libgravity-tests.sh` compares the failing assertions to a
versioned `EXPECTED`, and it fails **in both directions**. A fork commit that
repairs an audited anomaly by accident turns the suite red on its own.

**A pin bump is a re-audit, and a test already forces it.** `test_gravity` pins
the composition of `Gravity::Process()` for exactly this moment.

⚠️ **The list said "the four suites" until 2026-09-03, and there are SIX**, plus
a collection gate. This ADR was written on 2026-08-24, before the adapter
environment and the image check existed. A re-audit that followed the letter
would skip two gates. `platformio.ini` and `tools/run-all-tests.sh` are the
authority; the reference counts are the last accepted ones:

```text
0  the test collection      tools/check-test-collection.py    27 directories, 0 orphan
1  the C++ domain           tools/run-cpp-tests.sh            631 assertions
2  TypeScript               cd sim && npm test                580 tests
2b the TypeScript typing    cd sim && npm run typecheck       clean
3  the input adapter        tools/run-adapter-tests.sh        12 assertions
4  the EEPROM image         tools/run-eeprom-image-check.sh   29 criteria
5  the characterization     tools/run-libgravity-tests.sh     7 red of 68
```

⚠️ **Gate 5 conforms when it is RED**: seven assertions fail by construction,
and the criterion is that `EXPECTED` does not move, **in both directions**.

Two checks stay outside the suites: the **resolved tree identical to the archive
of the commit**, and the **memory measured**.

**The pin lives in one place.** The five AVR environments extend a `[deps]`
section, so a bump is one line. `run-libgravity-tests.sh` reads the commit from
`platformio.ini` instead of holding a copy, and refuses to run when it cannot
find it.

**The fork must disappear.** Every correction accepted upstream brings that
closer. The fork is a working tool, not a divergence to maintain.

**The fork carries THREE changes, and only the first is additive.** ⚠️ **This
paragraph said "one repair" until 2026-09-05.** `LIBGRAVITY_DISPLAY_TYPE`
makes the display transport selectable, with the previous class as the default.
The reason is measured: the SSD1306 is write only, libGravity never reads from
the bus, and the Arduino Wire transport it forces brings a bidirectional,
interrupt driven, slave capable driver. Removing it returned **1678 bytes of
Flash and 216 bytes of RAM**, and the main loop got **faster** — the p90 pass
from 8.80 ms to 6.50 ms, a display band from 4.88 ms to 3.35 ms — because a
polled transfer avoids one interrupt entry and exit per byte.

The choice stays opt-in on purpose. Two drivers cannot share the TWI
peripheral, so a lighter transport enabled by default would break a consumer
that also reads an I2C sensor through Wire.

**The two other changes arrived on 2026-09-03.** `3254fc3` replaces the
floating-point tempo computation of uClock, under the amendment of 2026-09-03.
`c2ec8ab` repairs the freeze of the external clock, under the amendment of
2026-09-05. Neither one is additive, and each one carries its own conditions
above.

**The audited base moved on 2026-08-24**, from `9be88be1f4` to `5c0c34f`, the
fork head. The whole compiled surface of the move is seven added lines in
`src/encoder.h`. The re-audit passed. The move cost RAM +2 and Flash +34, both
attributed: the new `on_long_press` member inside the global `gravity`, and the
new branch of `Encoder::Process()` inlined into `main`.

## Alternatives set aside

**Vendor libGravity inside the FlexSeq repository.** One repository, one clone,
and the audited code becomes versioned. Set aside on two grounds: it puts MIT
code inside a GPLv3 repository, which blurs the provenance of another author's
work, and it makes offering a correction upstream much harder. A pinned fork
gives the same guarantee — the commit is the version.

**A git submodule.** A clone without `--recursive` builds nothing. The project
already has one trap of that shape, since `CLAUDE.md` survives no clone. A
second one is one too many.

**Fork U8g2 as well.** 48 MB and hundreds of drivers, for a two-line repair that
returns 101 bytes of RAM. Its `u8x8_d_ssd1306_128x64_noname.c` declares its
init sequence and its display info without the `U8X8_PROGMEM` macro, which the
library itself defines. That one goes upstream, and the saving waits.

## References
- PRD §1 and §18; `CLAUDE.md`, the section on libGravity integration.
- ADR 0002 for the adapter layer that the audited anomalies justify.
- `docs/upstream-defects.md` for the defect list and the state of each offer.
- `tools/run-libgravity-tests.sh` for the drift guard.
