# Open risks and watch items

**Last review: 2026-08-24.** Fifteen open lines, twenty-seven closed or accepted.

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

Fifteen lines, and each one states **what it is waiting for and from whom**. A line
that waits for nothing from nobody no longer belongs here: it is in the table
below.

| # | Subject | Severity | What is left, and from whom |
|---|---|---|---|
| 1 | **The production firmware HAS run on the module since 2026-08-23**, so this line is no longer about the flash. Two things stay unverified on real hardware: the **timing** of the production render, still measured only under simavr, and the **fast presses in EDIT** that would refute the analysis of the audited `Button` anomaly (ADR 0002). ⚠️ **HALF OF THE FIRST ITEM CLOSED ON 2026-08-25, AND THE OTHER HALF DID NOT.** The render timing was measured through a PROXY, `env:wokwi`, because no production binary redrew. `FLEXSEQ_START_IN_EDIT=1` now puts the production firmware on the EDIT screen **and starts the transport**, so the real binary is measured: **p90 6.50 ms** against a 12 ms budget, median 5.01, one display band 3.36, routine frame 23.5, full refresh 45.9, worst pass 13.87 on the periodic full frame. What stays open is that this is still **simulation**. ⚠️ **The commit message of `5398e64` claimed the flag opens this measurement, and T4 weakened that claim three commits later** by making the module boot stopped: an EDIT screen with a still playhead almost never redraws. The flag now starts the transport, which is what makes the claim true again. **The method rule this leaves:** a behaviour change elsewhere can empty a measurement of its meaning without touching the tool, and the tool will not say so unless it is asked to. `run-blocking-probe.sh` had a floor of `bands >= 8`, one single frame, which the 14 samples of the main screen passed; the floor is now **16 frames**, because a full refresh comes one frame in sixteen and the worst pass belongs only to it. And a run with no sample said its output was *unreadable*, which sent the reader to the wrong place: it now says the binary does not redraw, and names the flag | medium | try fast presses **inside EDIT** -- on the main screen the loop is far faster and the test would refute nothing |
| 6 | **5.6 % of CPU** paid by the ADC ISR even with no channel routing CV | low | **deferred to §10.2 by decision (2026-08-21)**. In isolation the conditioning would be inapplicable: no channel routes CV, so conditioning would amount to switching CV off |
| 14 | **Three audited libGravity anomalies are latent and land exactly on the next task**: `Button` losing a release inside the debounce window, `Encoder`'s false first movement, `Clock::SetSource` ignoring the `SOURCE_LAST` sentinel. Reachability table in **PRD §18** | medium | absorb all three in `InputAdapter` (**ADR 0002**) **while wiring the UI (§12.1) and the transport (§8.1)**. None is reachable today -- no callback attached, no clock source selected (line 11b). **`Button` is now bounded by measurement (2026-08-22)**: the anomaly needs a contact shorter than `DEBOUNCE_MS = 10`, which a finger does not reach. Deliberate presses give 10/10; fast taps lose 30 to 55 %, but that is `env:bringup` blinding its own loop, not the anomaly. So for a button pressed by hand it is practically unreachable, and the owner **decided on 2026-08-22 that the adapter does nothing about it**, conditional on there being no impact -- the analysis satisfying that condition, and the test that would falsify it, are in ADR 0002 |
| 15 | **`DigitalOutput::Init()` does not force the output OFF**, and the audited test proves it | low | nothing today: harmless at cold boot, for the reason established in **PRD §18** (AVR reset leaves `PORT` at 0, and `gravity` is a global in `.bss`). What is left is to handle it **if a software reboot is ever added** -- that is when the pin could stay HIGH while the firmware believes it is off |
| 22 | **The re-arm threshold assumes the source falls back below +0.5 V.** A source that idles higher latches the gate for good: one reset, never another. Measured on the module: a Gravity output, switched off, presents about **+1.5 V** to a CV input (+153 in `Read()` units, against -27 with the cable out), so seen from the jack it is high-or-floating rather than high-or-low | low | **no decision taken.** Raising the threshold is an arbitration, and it needs measurements of several sources, not this one case. Decide it while implementing the CV destinations (PRD §10.2). The resting noise is measured at **-26 ± 3** on both channels, so the +1 V arm threshold keeps 128 counts of margin -- the arm side is not in question |
| 24 | **1536 bytes of Flash may be available beyond what PlatformIO assumes, and the question is no longer academic — the wired firmware measures 91.9 % against a 90 % guard (2026-08-22).** PlatformIO declares 30720 (32768 − 2048) while the bootloader measures 512 bytes | low, and it is an opportunity rather than a risk | read the `BOOTSZ` fuse, which needs an ISP programmer -- the bootloader returns `0x0` for fuses. What is measured is the *content* of the top 512 bytes, not the space `BOOTSZ` reserves, so the figure stays an inference. **30720 remains the safe assumption and is not touched.** Would matter if the Flash budget tightens at PRD §12.1 |
| 26 | **FlexSeq drops features and pages of the original firmware, and no rule caught it.** The owner restated the project rule on 2026-08-23: **keep every original feature and every original page; only the SEQ mode evolves.** Three omissions are already measured -- the three channel modes (found 2026-08-22, PRD §4.2), the fields of the BPM tab (line 27), and the navigation (line 28). The class of defect is the same each time: a page was rebuilt from a design instead of from the original | **high** | **an inventory, original screen by original screen and field by field**, is the only thing that closes this. Lot 15 of `WORKPLAN.md`. Each gap then goes to a lot or becomes an explicit decision. Until the inventory exists, no one can say how many omissions are left |
| 27 | **The BPM tab exposes ONE field where the original has four.** The original draws `MODE` (INT / EXT / MIDI), `MOD` (CV1 / CV2), `RANGE` and `PPQN`, and the number of fields changes with the mode (`UI.ino`, `displayTab == 0`). FlexSeq draws `SRC` alone, with five values that fuse the source and the PPQN. `RANGE` -- the tempo modulation depth, 1 to 5, shown x10 -- is in PRD §10.1 but has no field | **high** | lot 16 of `WORKPLAN.md`. The fusion of source and PPQN is a divergence that was never decided: the inventory of line 26 must state whether FlexSeq keeps it or restores the original split |
| 28 | **The navigation lacks two elements of the original, and one glyph is misleading.** The original tab bar carries 7 tabs plus a **separate status glyph** at x=121: `t` when stopped, `r` when playing, drawn only when the clock is internal. FlexSeq has 8 NAVIGABLE tabs, and the eighth draws `drawBox(cx - 2, cy - 2, 5, 5)` -- a filled 5x5 square. The owner read it as a Play/Stop indicator on the module, which is what a filled square looks like | **high** | lot 16. Target agreed 2026-08-23: BPM, channels 1 to 6, **global config with a gear icon**, then the **Play/Stop indicator at the far right, not navigable**. That is 8 tabs plus one indicator |
| 29 | **One font for everything, so no parameter reads as the main one.** The original uses its `velvetscreen` font and draws the main parameter large. FlexSeq calls `setFont(u8g2_font_5x7_tr)` once in `main.cpp` and never changes it, so the pattern name and the tempo have the size of a label. `logisoso26` was removed to save 808 B of Flash, and the ten custom glyphs meant to replace it are **designed, not implemented** (PRD §12.1, ~500 B estimated against 2646) | medium | **the arbitration is decided (2026-08-23, owner): FlexSeq keeps the design of the original pages, with the glyphs and the fonts already there; a font too heavy for the Flash is REDRAWN by us, never dropped.** So what is left is work, not a decision: draw the glyphs. The Flash budget frames it -- **534 B** stay under the guard as of 2026-08-23, and the ten glyphs are estimated at ~500 B against 2646 for `logisoso26`. **And part of the work is not to draw at all**: `GravityFW` and FlexSeq are both **GPLv3**, so the original's `velvetscreen` glyphs may be reused, attribution being the only duty. `tools/decode-velvetscreen.py` decodes them one by one -- the clock (`w`), Play (`r`) and Stop (`t`) are the three the navigation needs |
| 30 | **A fast turn lags on the module, and the cause read from the code is NOT the cause.** libGravity multiplies the accumulated movement by 3 below 16 ms and by 2 below 32 ms, and `oneStep()` collapses any magnitude to 1, so reading the code says detents are lost. **Measured on the module 2026-08-23** with `env:encoderprobe`: every one of 24 callbacks carried `|change| = 1`, none carried 2, 3, or more. The acceleration needs more than 62 detents per second for the factor 3, and more than 31 for the factor 2 -- rates a hand does not reach. The main-loop pass measured 19 ms at worst, with none at 32 ms or above, so detents do not accumulate between two polls either | medium | **the code path is closed, the symptom is open.** ADR 0003 records the decision: FlexSeq does not try to recover a detent count. What is left is whether libGravity loses detents **upstream**, in its quadrature decoding or its interrupt -- it carries no debounce and says so. The measurement that would settle it needs a KNOWN physical amount of rotation, which hand-counted detents do not give: two attempts on 2026-08-23 disagreed even in direction (9 counted for 13 movements, then 10 counted for 6). The encoder has no end stop and the schematic names no part (`Device:RotaryEncoder_Switch`, no manufacturer reference, no datasheet), so the detents per revolution are not documented anywhere. **Left aside by the owner on 2026-08-23** for want of a reliable measurement  **Seen again on the module on 2026-08-23, after both corrections**: the tempo sometimes sticks on a value while the encoder keeps turning, then moves on. It therefore **survives** the deferred rate change and the new gesture map, and it shows best on the tempo, where one detent is exactly 1 BPM. Left aside by the owner |
| 37 | **`MAX_SKIP_CHANCE` is 10 in the code and 9 in the PRD.** PRD §16 capped the skip chance at **9** on 2026-08-23, which is the 90 % the original shows and the value its interface clamps to (`Interactions.ino:162`). `include/flexseq/ChannelMode.h:15` still declares `MAX_SKIP_CHANCE = 10`. The original's two ceilings are both real and they govern different quantities: the value the user sets is clamped to 9, the value **plus its CV modulation** is clamped to 10 in the generator. FlexSeq's constant governs the value the user sets, so the PRD wins | medium | **lot 16**, by the owner's decision of 2026-08-23. The contradiction is identified, not reconciled. It became reachable on 2026-08-23: lot 19 put the skip chance on SHIFT plus a rotation from the tab bar, so the gesture currently offers eleven steps instead of ten |
| 31 | **SHIFT plus a rotation does nothing on the tab bar, where the original changes the tab's MAIN parameter.** The owner's gesture card for the original reads: hold shift and rotate to change the selected parameter, *or the main parameter if you are in the tabs menu*. `handleTabBar()` handles `EVENT_ROTATE` and `EVENT_PRESS` only. This is what made SHIFT look dead on the module: on the tab bar it is | medium | **lot 19**, with lot 15 for the inventory. The owner also **moved the ratchet gesture onto an original one on 2026-08-23**: SHIFT plus rotation in EDIT, on a step that is selected **and active**, sets the step's ratchet. `Press plus rotation` is **abandoned**, so `EVENT_ROTATE_HELD` loses its only client. Two decisions belong to the audit: where the **channel change in EDIT** goes, since SHIFT plus rotation is now taken, and whether SHIFT plus a long press in EDIT (which **clears the pattern**) is on the original's card at all |
| 35 | **The step render promises what the channel cannot play.** The ratchet digit is drawn on an inactive step, and on a step whose ratchet the channel's rate makes impossible. `PatternScreenModel` does not carry the rate, so the renderer cannot know | medium | **lot 22**, on the table the owner validated 2026-08-23: a hollow triangle for an inactive triplet, and no digit in either case. One more field in the model |
| 38 | **The onset debt is correct, and its footprint is SMALLER than its own commit message says.** `79cc2ca` states that at 200 BPM the loss "becomes routine". That is wrong. An onset is lost only when two fall in the SAME main-loop pass, and the shortest playable sub-onset slot is two ticks -- `ratchetFitsStep()` refuses less -- so 4.17 ms at the maximum tempo of 300 BPM, against a pass of about 0.2 ms on the main screen. On the EDIT screen, where a pass costs 6 to 15 ms, the very condition that makes the old code lose (slot shorter than a pass) is the one that stops the new code emitting on time: a pulse costs 5 ms plus a pass. **The real ceiling is the emission rate, not the accounting.** What the debt does buy stays true and was worth doing: no onset disappears in silence, and the cap at six makes the limit explicit instead of hiding it | low | **nothing today.** The correction lives here because the Git history is not rewritten and a commit message cannot be amended. To revisit if a fast SUBDIV is ever driven from the EDIT screen on the module, which is the only place the deformation could be heard |
| 12 | **The trigger pulse no longer measures what this line said, and the explanation no longer fits either.** It was 8.8 ms against a configured 5, blamed on the auto-off sitting at the end of `loop()` -- 5 ms rounded up to the next pass. Measured again on 2026-08-23, with the loop far shorter since the main screen stopped redrawing: **4.70 ms**, which is *below* the 5 ms configured. A round-up cannot go below the value it rounds, so the account is wrong somewhere | low | **nothing today** -- 0.9 % of a step at 120 BPM in `/1`, and no musical consequence. What is owed is the explanation, not a fix: read how libGravity's `DigitalOutput::Process()` compares against `millis()` before trusting either number. To revisit anyway once fast SUBDIV exists. **Two more figures, 2026-08-23**, from the two courses of the same run: **4.78 ms in CLOCK** and **5.01 ms in SEQ**. The width therefore straddles the 5 ms configured, and it moves with the mode, which no round-up to the next pass explains either |

## What is closed or accepted

These lines **wait for nothing any more**. They stay written so that nobody
reopens them without knowing what has already been established or costed.

| # | Subject | State | By what |
|---|---|---|---|
| 11b | **Nothing wired had been exercised on the module** | **closed 2026-08-23, on the module** | FlexSeq was flashed -- 28774 bytes written and verified in 9 seconds -- and the owner then drove it. Both rotation directions, the eight gestures, PLAY starting and stopping the clock with the six LEDs following, the encoder switch, the step cursor, the ratchet and triplet editing, and the playhead advancing in EDIT. The wiring is no longer proven by tests alone. What that run produced is not silence but **six new lines**, 26 to 31 |
| 33 | **The ratchet-by-SUBDIV behaviour was thinly covered** | **closed 2026-08-23** | lot 20. 13 C++ tests and 13 TypeScript ones now pin the step duration for all 25 rates, the trigger count for the 125 rate-by-code pairs, and the tick gaps of the trigger train. Every figure was measured on the firmware first, then written as a literal. 16 mutants make them load-bearing |
| 34 | **A sub-onset landed on a truncated slot, and the error multiplied with the rank** | **closed 2026-08-23** | lot 21. The position is now `(stepTicks * k) / triggers`, and the worst error over the whole matrix is **0.667 tick**, measured. The exact-division guard became a **two-tick floor**, so 6 pairs are refused instead of 13, all at the three fastest rates. R6 at `x24` stays impossible: 4 ticks cannot carry 6 distinct instants. Measured at `x3`: a ratchet 3 now uses 10 + 11 + 11 ticks, the whole step, where it used to leave the last third empty |
| 36 | **The C++ and the TypeScript placed a sub-onset with different arithmetic** | **closed 2026-08-23** | lot 21, and the obligation was met before the guard was removed. Both sides call a `subOnsetTick` that divides integers explicitly -- `Math.floor` on the TypeScript side, which the float division did not do. Proof: the two suites produce the same gap train at `x3`, `11 11 32 32 32 10`, where the two arithmetics would differ |
| 32 | **A ratchet on an inactive step, and what it means** | **decided and asserted 2026-08-23** | the owner's rule: the ratchet is **kept** and returns with the step, and a triplet on an inactive step keeps its two units -- a **triplet rest**. Both were already the behaviour; what was missing was any assertion. Measured and pinned: an inactive triplet adds 192 ticks to the gap that spans it, so a train reads `96 96 288 96 96 288` |
| 25 | **The path `pattern content -> output`, exercised again** | **closed 2026-08-23** | `run-trigger-probe.sh` now makes **two courses** on the same firmware, one per channel mode. The mode and the pattern arrive through a **preloaded EEPROM image**, built by `tools/eeprom-image.cpp` with the domain code itself: the harness holds no copy of the format and pokes no private struct. Measured over 20 s of simulation with the SSD1306 slave attached: CLOCK gives **38/38 regular gaps**, SEQ gives **11/11 gaps** that form a cyclic rotation of the pattern's own gaps, both at **499.96 to 499.97 ms** per step against 500.00 expected. The jitter moves from run to run, 1.01 to 1.29 ms over four runs, so it is a bound and not a value: 0.26 % of a step at worst, against a 2 % budget. The phase is never assumed. `MUTATE=7` adds a step to the image and not to the expectation: SEQ reddens (6/14) while CLOCK stays green (38/38), and that **asymmetry is itself the proof** that CLOCK ignores the bank. `DROP=3` reddens both. Both paths exit 1 |
| 23 | **Restoring the original firmware, the project's escape hatch** | **closed 2026-08-22, on the module** | run end to end. Four blocking criteria green, the decisive one being the readback: 27648 bytes identical to the backup over `0x0000`-`0x6BFF`, and the bootloader's 512 bytes untouched. Then the part no tool covers, confirmed by the owner with his eyes: the original **boots**, the **eight patterns are there**, the **tempo reads 120**, and the **channel settings are intact**. The tempo is what proves the EEPROM survived -- `-D` means no chip erase, so nothing below address 384 was ever at risk. The trim to `0x0000`-`0x6BFF` did its job: optiboot was never sent its own pages. `tools/run-original-restore.sh`, and PRD §2 for the geometry it relies on |
| 21 | **Encoder direction: libGravity's default is the opposite of the original's** | **closed 2026-08-22, on the module** | `InputAdapter` calls `SetReverseDirection(true)`, and the module turns the way the original does -- left and right both correct. The other half of the old line 21, exposing `rotateScreen` and `reverseEncoder` as settings, moves to the settings page (lot 14 of `WORKPLAN.md`) and is tracked there |
| 20 | **The encoder's missing debounce, and what it actually costs** | **measured 2026-08-22, and the old claim is REFUTED** | the dependency has no debounce and says so (`_rotate_change()` carries `// Validation (TODO: add debounce check)`). What this line used to assert -- that two fast detents produce **no event at all**, the position returning to where it started -- **does not reproduce**. Measured with `env:bringup`: two detents give exactly **−2**, zero reversals; fifteen seconds of fast one-way rotation give **−917 with zero reversals**. Nothing cancels. What is real: bounces occur **only at a fast turning point**, the fastest measured reversal being **2 ms**, while ten deliberate reversals at a comfortable pace produced **nine** recorded reversals -- no spurious ones -- spread over **509 to 1003 ms**. `EncoderFilter`'s 12 ms window is therefore validated on facts, six times above the bounce and forty times below the gesture. **The felt symptom has another cause**: the dependency's ACCELERATION (x3 under 16 ms), which turns one detent into three moves, with events arriving in the same millisecond (`S0`). Absorbed in `UiController`, not in the filter |
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

**A page rebuilt from a design loses features; a page rebuilt from the original
does not.** Three times now, FlexSeq shipped a screen that was assembled from a
description of what the screen should hold, and each time it held less than the
original: the three channel modes were absent from the domain (2026-08-22), the
BPM tab kept one field out of four, and the tab bar lost the Play/Stop indicator
(both found on the module, 2026-08-23). The design was not wrong -- it was
incomplete, and nothing compared it to the source. **Read the original's own
drawing code for the screen, field by field, before declaring the screen done.**
The project rule this serves is the owner's, restated on 2026-08-23: keep every
original feature and every original page; only the SEQ mode evolves.

**A glyph must not look like another function.** The eighth tab draws a filled
5x5 square, which the owner read as a Play/Stop indicator on the module -- a fair
reading, since that is what a stop indicator looks like. The name in the code
(`drawSettingsGlyph`) carried the intent that the pixels did not.

**A declared field is not a feature.** Twice on 2026-08-22, reading the original
firmware's `channel` struct led to a wrong conclusion about its behaviour: two
`CVxTarget` fields read as two offered routings, when the interface enforces
mutual exclusion; and two `CVxRange` fields read as a depth setting, when they
are never read at all. Both times the real code was **simpler** than the struct
suggested, and both times the correction came from reading the interaction and
generation code rather than the declaration. Read what the code *does* with a
field before deciding what the field *means*.


Each cost a real mistake, and each holds beyond the subject that produced it.

**A deadline is a backstop, not a result.** The mutation probe counts a run that
exceeds its deadline as a mutation detected, because a mutant that removes a loop
guard turns a `while` into an infinite loop. On 2026-08-23 two mutants of lot 18
were reported that way, and the report was honest but weak: what actually hung
was a **test helper**, `gotoTab()`, walking the interface with a rotation the
mutant had made inert. The assertion that covers the property never got to run,
and each hang cost 300 s. **A helper that walks a state machine must be bounded
and must fail**, so that the assertion, and not the clock, is what reddens.

**Measure where, not only how much.** A bimodal distribution says *how many*
passes are slow, never *which ones*. I inferred from "one pass in seven is long"
that it was the row of 12 steps; it was the title. The cost-by-position
measurement established it in a single run (PRD §14).

**A write to an external document must be verified by re-reading it, and the cause
has a name.** Notion normalises the bold around a code span: an anchor written with
bold wrapped around an inline code is stored with the markers doubled, so the
anchor no longer matches. On 2026-08-23 one update of four failed that way while
the tool reported success for all four. **Prefer a short anchor with no formatting
inside**, and re-read every time. On
2026-08-23 the Notion update tool returned success **twice** for an edit whose
anchor text no longer matched, so the change was simply not applied. The symptom
is the worst kind: a success report and an unchanged document. One of the two
would have left the PRD saying 307 bytes in one section and 306 in another. Read
the section back and compare the strings; a returned success is not evidence.

**An assertion must not compare against the constant it tests.** On 2026-08-23
the test suite did not detect one mutation in a complete round. The mutation score
was 31/32. The TypeScript test compared the clamped offset to `MAX_OFFSET`, so
moving the constant moved the expectation with it. The test was self-confirming.
The C++ test wrote `255` in plain sight and detected the same mutation. Write the
literal value when the value itself is the claim.

**One measurement cannot separate two behaviours.** The probe that watches the six
outputs now makes **two courses** on one firmware, one per channel mode. CLOCK
green while SEQ reddens on the same mutated pattern is what proves that CLOCK
ignores the bank; either course alone proves only its own half.

**A green test proves nothing until it has been red.** Every important assertion
in this repository has been verified by mutation — removing the model freeze,
inverting the band conversion, lowering the reserve. Two of them passed for the
wrong reason before that check.

**A mutation harness must survive its own mutants, and must restore what it
edited.** Two failures in one run on 2026-08-23, both mine. A mutant that removed
a loop guard turned `while (true)` into an infinite loop: with no per-run timeout
the harness waited twenty minutes on a program that was never going to answer, so
**every long-running step now carries an explicit deadline**, and a hang counts
as a mutation *detected* rather than as a blockage. Then interrupting it left the
mutant **in the source file** — twice, because the harness edited the real file and
restored it only on the happy path. It now snapshots every target in memory before
the first mutant and restores in a `finally` and on signal. The general shape is
the one above: a tool must not depend on the good behaviour of the thing it
measures.

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
