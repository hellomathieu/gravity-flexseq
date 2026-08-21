# Open risks and watch items

**Last review: 2026-08-21.** Eight open lines, twelve closed or accepted.

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

Eight lines, and each one states **what it is waiting for and from whom**. A line
that waits for nothing from nobody no longer belongs here: it is in the table
below.

| # | Subject | Severity | What is left, and from whom |
|---|---|---|---|
| 1 | **Nothing has ever run on the module.** Everything is simulation and native tests | **dominant** | the first flash. **Deferred by the owner's decision (2026-08-21)**: the module is available, the wait is deliberate. `env:bringup` makes that flash diagnosable line by line, not less risky |
| 2 | **The OLED's physical mounting** determines whether the rotated image lands upright | low (was medium) | nothing to do — lifts together with line 1. **Corroborated meanwhile**: the original firmware's Rev 2+ config resolves to `U8G2_R2`, exactly what FlexSeq does (mind the inverted logic of the `rotateScreen` flag: `false` → R2). It is also a menu option persisted in EEPROM, so a module whose rotation had been flipped would show FlexSeq upside down — harmless and reversible |
| 6 | **5.6 % of CPU** paid by the ADC ISR even with no channel routing CV | low | **deferred to §10.2 by decision (2026-08-21)**. In isolation the conditioning would be inapplicable: no channel routes CV, so conditioning would amount to switching CV off |
| 11b | **No control is wired in `main.cpp`**: only `clock.AttachIntHandler()` is called — no EXT, no source, no tempo, no start/stop; buttons and encoder are `Process()`-ed with no callback attached | medium | **this is the next piece of work**: wire the UI (§12) and the transport (§8). Not a defect but the state of progress. Consequence: a flash today would validate the hardware chain, not the features |
| 14 | **Three audited libGravity anomalies are latent and land exactly on the next task**: `Button` losing a release inside the debounce window, `Encoder`'s false first movement, `Clock::SetSource` ignoring the `SOURCE_LAST` sentinel. Reachability table in **PRD §18** | medium | absorb all three in the adapter layer **while wiring the UI (§12) and the transport (§8)**. None is reachable today -- no callback attached, no clock source selected (line 11b) |
| 15 | **`DigitalOutput::Init()` does not force the output OFF**, and the audited test proves it | low | nothing today: harmless at cold boot, for the reason established in **PRD §18** (AVR reset leaves `PORT` at 0, and `gravity` is a global in `.bss`). What is left is to handle it **if a software reboot is ever added** -- that is when the pin could stay HIGH while the firmware believes it is off |
| 17 | **The module's EEPROM is not backed up, and its real CV calibration is out of reach.** libGravity has no notion of EEPROM, so `CvSampler` runs on the defaults `CALIBRATED_LOW = -566` / `CALIBRATED_HIGH = 512`, not on the calibration stored by the original firmware | low | flash `env:eepromdump` and capture the dump. Does **not** block line 1: FlexSeq writes no EEPROM byte, and a bootloader upload does not touch it. Becomes a real precondition at PRD §11, and the source of the real calibration for §10 |
| 12 | **The trigger pulse measures 8.8 ms** instead of the 5 ms configured: the auto-off sits at the end of `loop()`, so 5 ms rounded up to the next pass | low | **nothing today** — 1.8 % of a step at 120 BPM in `/1`. To revisit once fast SUBDIV exists: at `x4` (125 ms/step) it is 7 % of the step, and 12 % on the worst pass. The threshold is written down; it no longer has to be rediscovered |

## What is closed or accepted

These lines **wait for nothing any more**. They stay written so that nobody
reopens them without knowing what has already been established or costed.

| # | Subject | State | By what |
|---|---|---|---|
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

**A manifest is not a measurement of the device.** PRD §2 gave the upload path as
57600 baud, read off PlatformIO's board manifest and never off the module. The
board is optiboot at 115200, so the first backup attempt died on `not in sync`.
The manifest was true of the manifest and false of this unit. Cost: nothing, but
only because the attempt came before the first flash and not during it.

**A plausible artifact is not a verified one.** The same session produced an
EEPROM dump that looked entirely credible: 1024 bytes, 1.2 % of `0xFF`, no error
from avrdude. It was byte for byte the first kilobyte of the flash, because
optiboot does not read EEPROM and reports nothing when asked. A check on the
shape would have passed it. Only a check on the **content** caught it: 1024/1024
identical to the flash. So the criterion for a backup is never its size or its
entropy -- it is that the bytes are the ones you asked for.
