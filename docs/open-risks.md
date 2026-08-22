# Open risks and watch items

**Last review: 2026-08-22.** Ten open lines, nineteen closed or accepted.

## What this document is, and is not

It is a **tracking index**, not a source of truth. Every line points to where the
fact is established — Notion PRD, an ADR, `CLAUDE.md`, or the code — and copies
only what it strictly needs to be understandable on its own. The project rule is
strict on this: a fact lives in **one** source, elsewhere it is referenced
(`.claude/rules/knowledge-persistence.md`).

It therefore carries **no decision**. A decision goes to the PRD when it is a
product decision, to an ADR when it is architectural.

To be re-read at every *knowledge checkpoint*: a line closes, gets reworded, or
disappears. A risk that stays written without moving across several reviews is
either closed without anyone noting it, or accepted without anyone saying so.

## What is still open

Eleven lines, and each one states **what it is waiting for and from whom**. A line
that waits for nothing from nobody no longer belongs here: it is in the table
below.

| # | Subject | Severity | What is left, and from whom |
|---|---|---|---|
| 1 | **The PRODUCTION firmware has never run on the module.** `env:bringup` has, and every line it exercises now answers, `EDGE` included | medium | flash `main.cpp` once the UI and the transport are wired (line 11b). What stays unverified is the production render and its timing on real hardware rather than under simavr |
| 6 | **5.6 % of CPU** paid by the ADC ISR even with no channel routing CV | low | **deferred to §10.2 by decision (2026-08-21)**. In isolation the conditioning would be inapplicable: no channel routes CV, so conditioning would amount to switching CV off |
| 11b | **No control is wired in `main.cpp`**: only `clock.AttachIntHandler()` is called — no EXT, no source, no tempo, no start/stop; buttons and encoder are `Process()`-ed with no callback attached | medium | **designed on 2026-08-22** (PRD §12.1, §8.1, §11.1 and ADR 0002); what is left is the implementation. Not a defect but the state of progress. Consequence: a flash today would validate the hardware chain, not the features |
| 14 | **Three audited libGravity anomalies are latent and land exactly on the next task**: `Button` losing a release inside the debounce window, `Encoder`'s false first movement, `Clock::SetSource` ignoring the `SOURCE_LAST` sentinel. Reachability table in **PRD §18** | medium | absorb all three in `InputAdapter` (**ADR 0002**) **while wiring the UI (§12.1) and the transport (§8.1)**. None is reachable today -- no callback attached, no clock source selected (line 11b). **`Button` is now bounded by measurement (2026-08-22)**: the anomaly needs a contact shorter than `DEBOUNCE_MS = 10`, which a finger does not reach. Deliberate presses give 10/10; fast taps lose 30 to 55 %, but that is `env:bringup` blinding its own loop, not the anomaly. So for a button pressed by hand it is practically unreachable, and the owner **decided on 2026-08-22 that the adapter does nothing about it**, conditional on there being no impact -- the analysis satisfying that condition, and the test that would falsify it, are in ADR 0002 |
| 15 | **`DigitalOutput::Init()` does not force the output OFF**, and the audited test proves it | low | nothing today: harmless at cold boot, for the reason established in **PRD §18** (AVR reset leaves `PORT` at 0, and `gravity` is a global in `.bss`). What is left is to handle it **if a software reboot is ever added** -- that is when the pin could stay HIGH while the firmware believes it is off |
| 20 | **libGravity's encoder has no debounce, and the dependency says so**: `_rotate_change()` carries the literal comment `// Validation (TODO: add debounce check)`. Observed on the module: two fast detents produce **no event at all**, because a bounced detent gives +1 then -1 and the position returns to where it was | medium | absorb it in `InputAdapter` (**ADR 0002**), together with line 14, **while wiring the UI (PRD §12.1)**. Not a loss of counts: the library *accelerates* fast rotation (x3 under 16 ms, x2 under 32 ms), so a real fast turn would be amplified, never erased. What is seen is cancellation |
| 21 | **Two persisted preferences of the original firmware are hardcoded or absent in FlexSeq**: `rotateScreen` (FlexSeq fixes `U8G2_R2`) and `reverseEnc` (never set, and libGravity's default is the opposite of the original's -- clockwise decrements on the module) | low | expose both in the settings tab (**PRD §4.1**, §12.1), and call `SetReverseDirection(true)` in `InputAdapter` so the default matches the original. Not a defect of either library: encoder direction depends on the wiring of the two pins, which is why both expose the setting |
| 22 | **The re-arm threshold assumes the source falls back below +0.5 V.** A source that idles higher latches the gate for good: one reset, never another. Measured on the module: a Gravity output, switched off, presents about **+1.5 V** to a CV input (+153 in `Read()` units, against -27 with the cable out), so seen from the jack it is high-or-floating rather than high-or-low | low | **no decision taken.** Raising the threshold is an arbitration, and it needs measurements of several sources, not this one case. Decide it while implementing the CV destinations (PRD §10.2). The resting noise is measured at **-26 ± 3** on both channels, so the +1 V arm threshold keeps 128 counts of margin -- the arm side is not in question |
| 24 | **1536 bytes of Flash may be available beyond what PlatformIO assumes, and the question is no longer academic — the wired firmware measures 91.9 % against a 90 % guard (2026-08-22).** PlatformIO declares 30720 (32768 − 2048) while the bootloader measures 512 bytes | low, and it is an opportunity rather than a risk | read the `BOOTSZ` fuse, which needs an ISP programmer -- the bootloader returns `0x0` for fuses. What is measured is the *content* of the top 512 bytes, not the space `BOOTSZ` reserves, so the figure stays an inference. **30720 remains the safe assumption and is not touched.** Would matter if the Flash budget tightens at PRD §12.1 |
| 12 | **The trigger pulse measures 8.8 ms** instead of the 5 ms configured: the auto-off sits at the end of `loop()`, so 5 ms rounded up to the next pass | low | **nothing today** — 1.8 % of a step at 120 BPM in `/1`. To revisit once fast SUBDIV exists: at `x4` (125 ms/step) it is 7 % of the step, and 12 % on the worst pass. The threshold is written down; it no longer has to be rediscovered |

## What is closed or accepted

These lines **wait for nothing any more**. They stay written so that nobody
reopens them without knowing what has already been established or costed.

| # | Subject | State | By what |
|---|---|---|---|
| 23 | **Restoring the original firmware, the project's escape hatch** | **closed 2026-08-22, on the module** | run end to end. Four blocking criteria green, the decisive one being the readback: 27648 bytes identical to the backup over `0x0000`-`0x6BFF`, and the bootloader's 512 bytes untouched. Then the part no tool covers, confirmed by the owner with his eyes: the original **boots**, the **eight patterns are there**, the **tempo reads 120**, and the **channel settings are intact**. The tempo is what proves the EEPROM survived -- `-D` means no chip erase, so nothing below address 384 was ever at risk. The trim to `0x0000`-`0x6BFF` did its job: optiboot was never sent its own pages. `tools/run-original-restore.sh`, and PRD §2 for the geometry it relies on |
| 2 | The OLED's physical mounting decides whether the rotated image lands upright | **closed 2026-08-21, on the module** | flashed and read with the eye: the text is upright. The panel is mounted upside down and `U8G2_R2` compensates, exactly as the firmware assumes. No simulator could settle this one -- only the hardware |
| 17 | The module's EEPROM is not backed up, real CV calibration out of reach | **closed 2026-08-21** | `tools/run-eeprom-dump.sh`, three blocking criteria, backup verified and reproducible byte for byte. The calibration is now recoverable from it. What the defaults cost is measured: CV reads **-39 / -40** at rest instead of 0, a systematic offset consistent across both channels |
| 18 | The main loop froze after 10 to 20 seconds, while every interrupt kept running | **closed 2026-08-22** | it was the **power**. The module was running on USB alone, with the analog front end unpowered. On rack power it ran past **87 s** with no interruption. The diagnosis came from a side channel: the MIDI clock held exactly 48 bytes/s for 60 s -- 24 PPQN at 120 BPM -- so the chip and uClock were never at fault, only the loop |
| 19 | No physical control had been observed working | **closed 2026-08-22, on the module** | every one of them answers. Encoder, its switch, SHIFT and PLAY: pin state, short-press counter and clock toggle all behave. Two real findings came out of it and moved to lines 20 and 21. Two apparent faults were neither: the callbacks fire on **release** by design (PRD §5.5), and a press beyond `LONG_PRESS_DURATION_MS = 750` fires the **separate** long-press callback, which `env:bringup` did not attach -- deterministic, not erratic. Both long handlers were then attached and the behaviour is **measured, no longer deduced**: on either button a short press adds to the short counter alone, a long press to the long counter alone. The audited `Button` anomaly stays untested, since it needs a release falling inside the debounce window and ordinary presses are far too slow to reach it (line 14) |
| 1d | `EDGE` -- CvGate's edge detection had never seen a real signal | **closed 2026-08-22, on the module** | verified in both regimes. On an external clock: one edge per pulse, and the rate follows the tempo through a 4:1 change, which no internal artefact could do. On a held level: the input swung 512 down to 153 and back, dozens of times, and produced **zero** edges -- front and not level, proven on hardware. Cold start reads 0 and stays there, so there is no false edge at initialisation either |
| 1c | The six output paths had never been exercised on hardware | **closed 2026-08-22** | the panel carries **one LED per output**, so no patch cable is needed -- a claim I had got wrong from libGravity's pin map alone. The cycle runs 1 to 6, then PULSE, then an idle step, and every LED lights in turn |
| 0 | Module revision unconfirmed | **closed 2026-08-21** | the owner confirmed a **SHIFT button** on the panel, hence **Rev 2+**: libGravity's pin map is the right one. Rev 1 defines `SHIFT_BTN_PIN 100`, that is, no SHIFT button at all |
| 1b | No binary exercised the `pattern → onset → pulse` path | **measured 2026-08-21** | `tools/run-trigger-probe.sh` injects the content into simulated RAM and watches the 7 pins of the production binary: 6/6 outputs, 11/11 gaps, jitter 1.00 ms worst (0.2 % of a step). Closed **on the simulation side**; the hardware remains line 1 |
| 3 | "The heap is corrupted during a simavr run" | **resolved 2026-08-21** | it was a one-byte out-of-bounds read in simavr's UART logger. The four harnesses disarm `AVR_UART_FLAG_STDIO`: 2 SIGSEGV in 5 → 0 in 5, 3 ASan reports in 3 → 0 in 3 |
| 4 | RAM and Flash grow with every feature | **accepted and sized 2026-08-21** | not a risk but a permanent constraint under an active guard — **and what remains to be built fits**, see the sizing below |
| 5 | The stack margin shrinks as the measurement gets more complete | **accepted and sized 2026-08-21** | 159 B peak against 361 B of margin, six ISR families proved entered. One named gap only, and its obligation is written below |
| 7 | 15.3 ms peak on the full refresh, 1 frame in 16.0 | **design property** | ADR 0001. Both ways of getting rid of it are costed below; do not reopen without a better number |
| 8 | Mixed state during a transfer, ~4.6 ms | **arithmetically impossible to fix** | the only remedy is a full 1024 B buffer, against 264 B available. See below |
| 9 | Wokwi unverified: `"rotate"` and the wiring | **accepted 2026-08-21** (decision) | off the critical path since `run-screen-dump.sh` validates the render. `env:wokwi` stays useful: it is that probe's target, under simavr, unrelated to Wokwi |
| 10 | `decode-velvetscreen.py` requires the neighbouring `GravityFW` clone | **acceptable** | single-use tool; `--src`, `$GRAVITY_FW_INO`, and an explicit error carrying the clone URL |
| 11 | `CLAUDE.md` survives no `git clone` | **closed by decision** (2026-08-19) | the owner's explicit decision, deliberate non-versioning |
| 13 | The MIDI expander's `PULSE` stays silent | **observation, not a defect** | `main.cpp` does not drive `gravity.pulse`: the expander is not in the path yet (PRD §16) |
| 16 | `wokwi_main.cpp` was the last active caller of `gravity.Process()`, the function with the uninitialized loop index | **closed 2026-08-21** | commit `9dda448` calls the pieces instead, exactly as `main.cpp` does. No active caller remains; `run-screen-dump.sh` stays green (24/24 steps, rotation 180), so the render harness no longer runs undefined behaviour. The two calls in `test_gravity.cpp` stay -- they characterize the pinned dependency |

### The memory budget, costed once — lines 4 and 5

These two lines said "monitored" without ever saying **how much is left and for
what**. It has been costed since 2026-08-21, and the costing lives in **PRD §15**
— its normative source — not here. What matters for closing these lines:

- **264 B of RAM available** for new static data (520 B free minus the 256 B
  stack reserve), against **~52 B estimated** for everything left to build — UI,
  transport, persistence, CV destinations, RECORDING. Margin of about **5×**.
  Persistence costs almost nothing because the bank is **already** in RAM: the
  EEPROM write reads it in place, with no copy.
- **6244 B of Flash** before the guard refuses at 90 %. The full UI is the only
  genuinely expensive item to come, on the order of 2 to 4 kB.
- **The trigger is explicit**, there is nothing left to judge case by case:
  failure beyond +16 B of RAM or +512 B of Flash unacknowledged, ceilings at
  256 B free or 90 % of Flash. `--accept` is never done without looking at the
  per-symbol diagnostic.
- **The one gap in the stack measurement, and its obligation.** The probe measures
  what the firmware **executes during the run**. The EEPROM write is not in it
  because it does not exist — but it will not be in it **automatically** the day
  it does either: the run will have to **provoke** it. That is the one thing not
  to forget in §11.

### The two properties of the spread render, and the price of removing them — lines 7 and 8

**Line 7, the 15.3 ms peak.** Two ways to remove it, both evaluated:

- *rasterize the title across two passes*, accumulating in the 128 B buffer U8g2
  already owns — clearing and sending nothing in between. Cost: 0 B of RAM, but a
  partial-draw state machine that **breaks the indivisible-cycle invariant** ADR
  0001 rests on, and a band showing its old content one pass longer. Rejected:
  that is a lot of fragility for a peak concerning one frame in sixteen and
  already inside its budget;
- *shorten the title, or reduce the font*. Rejected by the owner, who wants
  explicit titles.

The peak therefore stays, and it is a choice: a deliberate safety net against a
defect in our own dirty-band logic, which repairs itself within a few frames.

**Line 8, the ~4.6 ms mixed state.** The only remedy is double buffering, hence
**1024 B** — against 264 B available. This is not a trade-off, it is an
arithmetic impossibility. The line is closed for that reason, and not because
anyone decided to live with it.

## Method rules born from these subjects

**A declared field is not a feature.** Twice on 2026-08-22, reading the original
firmware's `channel` struct led to a wrong conclusion about its behaviour: two
`CVxTarget` fields read as two offered routings, when the interface enforces
mutual exclusion; and two `CVxRange` fields read as a depth setting, when they
are never read at all. Both times the real code was **simpler** than the struct
suggested, and both times the correction came from reading the interaction and
generation code rather than the declaration. Read what the code *does* with a
field before deciding what the field *means*.


Each cost a real mistake, and each holds beyond the subject that produced it.

**Measure where, not only how much.** A bimodal distribution says *how many*
passes are slow, never *which ones*. I inferred from "one pass in seven is long"
that it was the row of 12 steps; it was the title. The cost-by-position
measurement established it in a single run (PRD §14).

**A green test proves nothing until it has been red.** Every important assertion
in this repository has been verified by mutation — removing the model freeze,
inverting the band conversion, lowering the reserve. Two of them passed for the
wrong reason before that check.

**A tool must not assume what it measures.** Four occurrences, all on the same
probe. It grouped bands by eight and announced 504 ms frames the day there were
only seven; groupings are now done by the **protocol** — U8g2 control byte,
SSD1306 page addressing — and not by thresholds. On 2026-08-21, three more,
found while verifying a single figure that would not reproduce:

- it divided **the whole** first half of the run by the number of conversions to
  derive an ISR rate, when conversions only start after `setup()`: the rate
  depended on the duration of the measurement (31.5 µs at 4 s, 27.2 at 16 s).
  Measured over an inner window: 26.0 µs at all three durations;
- it **labelled** its maximum "periodic full refresh" without any full frame
  having fallen inside the measured regime;
- its frame duration mixed the two regimes, a bimodal distribution at roughly
  equal weight: the median flipped from one mode to the other with the run length.

Three symptoms of one cause: **a quantity that moves when the duration of the
measurement moves is not a quantity.** It is the cheapest test to put a tool
through, and it had never been done.

A fifth the same day, on another probe: `trigger_probe` **assumed the playhead's
phase** — that the first pulse observed would be the pattern's first active step.
Since `transport.start()` runs in `setup()`, the engine is already turning when
the content arrives, and the probe declared a perfectly correct firmware
"off-grid". What holds without knowing the phase is the **sequence of gaps**, up
to a rotation — and that is also the only claim with a musical meaning.

**A lost output sends you looking for the defect elsewhere.** Redirected `stdout`
is block-buffered: a report vanished on the crash, which sent me looking for a
loading defect where the problem was in the allocator.

**A side channel tells you which half is dead.** The bring-up screen froze and
four inputs read zero, which looks like four faults. One measurement separated
them: the firmware emits a MIDI clock from an interrupt, so counting its bytes
from the host proved the chip, the ISRs and the tempo were all intact and only
the loop was stuck. Look for an output the suspected component does **not**
drive -- it costs nothing and it halves the search.

**A frozen display makes every input look broken.** Before reading any input on
hardware, prove the screen is refreshing. The bring-up firmware's own liveness
indicators are the `OUT` digit (moves every 800 ms) and the tick counter; without
that check, an hour goes into inputs that were never at fault.

**A diagnostic that blinds itself measures its own blindness.** `env:bringup`
redraws a whole frame in one call, which blocks its loop, so it polls the buttons
only part of the time. Fast taps went missing and I read that as the audited
`Button` anomaly biting at human speed. It was not: a threshold defect loses all
or nothing, while a blind window loses more the faster you tap -- and the loss
grew with the tapping rate, 30 % then 55 %. Deliberate presses gave 10 out of 10.
Before reading any input on a diagnostic, know how often it looks. And do not
turn the loss rate into a frame duration: counting taps by hand, at a speed
nobody calibrated, gives an order of magnitude consistent with the code's ~100 ms
estimate and nothing better. The honest number would come from
`run-blocking-probe.sh` pointed at `env:bringup`, which has never been done.

**A software header is not a measurement of the device.** PRD §2 gave the upload path as
57600 baud, read off PlatformIO's board manifest and never off the module. The
board is optiboot at 115200, so the first backup attempt died on `not in sync`.
The manifest was true of the manifest and false of this unit. Cost: nothing, but
only because the attempt came before the first flash and not during it. The same
mistake, twice in two days: I read libGravity's pin map, saw the outputs were
jacks, and told the owner his panel had no indicator LEDs. It has one per output,
and they made the whole output chain verifiable without a patch cable. A header
describes what the firmware drives, never what the board shows.

**A plausible artifact is not a verified one.** The same session produced an
EEPROM dump that looked entirely credible: 1024 bytes, 1.2 % of `0xFF`, no error
from avrdude. It was byte for byte the first kilobyte of the flash, because
optiboot does not read EEPROM and reports nothing when asked. A check on the
shape would have passed it. Only a check on the **content** caught it: 1024/1024
identical to the flash. So the criterion for a backup is never its size or its
entropy -- it is that the bytes are the ones you asked for.
