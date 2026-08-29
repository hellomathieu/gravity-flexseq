# Open risks and watch items

**Last review: 2026-08-29.** Thirty open lines, thirty-one closed or accepted.

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

Twenty-six lines, and each one states **what it is waiting for and from whom**. A line
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
| 39 | **Two tools assume the dependency lives under `.pio/libdeps/`, and neither says so.** `run-build-memory.sh` classifies a warning by testing `^\.pio/`, so a dependency resolved from a local path produces warnings that fall into **neither** bucket and vanish without a word — seen on 2026-08-25 while measuring the display transport against a local clone. And `env:native_adapter` and `env:native_libgravity` hardcode `-I.pio/libdeps/nanoatmega328/libGravity/src`, which a `symlink://` dependency never creates, so both suites fail with an include error that names nothing | low | **nothing today**, since `lib_deps` points at a git URL and the path exists. What is owed is that both tools should derive the path or refuse rather than mis-classify. To revisit if a local checkout is ever used again for a measurement |
| 40 | **The pin is a SHA, so a rewritten history on the fork breaks the build.** Happened on 2026-08-25: a commit pushed as `63bdc15` came back as `4c5b4d0` on a different parent, and `env:bringup` and `env:mainscreen` failed with `VCS: Could not parse object`. **The tree was verified identical** before re-pinning, so nothing was lost | low, and the failure mode is the good one | **nothing to build.** The error is loud, immediate, and impossible to mistake for a pass. What is owed is a habit rather than a tool: a force-push on the fork must be followed by a bump of `[deps]` in `platformio.ini` |
| 12 | **The trigger pulse no longer measures what this line said, and the explanation no longer fits either.** It was 8.8 ms against a configured 5, blamed on the auto-off sitting at the end of `loop()` -- 5 ms rounded up to the next pass. Measured again on 2026-08-23, with the loop far shorter since the main screen stopped redrawing: **4.70 ms**, which is *below* the 5 ms configured. A round-up cannot go below the value it rounds, so the account is wrong somewhere | low | **nothing today** -- 0.9 % of a step at 120 BPM in `/1`, and no musical consequence. What is owed is the explanation, not a fix: read how libGravity's `DigitalOutput::Process()` compares against `millis()` before trusting either number. To revisit anyway once fast SUBDIV exists. **Two more figures, 2026-08-23**, from the two courses of the same run: **4.78 ms in CLOCK** and **5.01 ms in SEQ**. The width therefore straddles the 5 ms configured, and it moves with the mode, which no round-up to the next pass explains either |

| 41 | **The module emits MIDI Start, Stop, Start, Stop on the bus before the user presses PLAY.** Observed on 2026-08-25 with `run-drift-probe.sh`, on the production binary: START at 1.33 ms (`Clock::Init()` starts uClock), STOP at 351.40 ms (the first `apply()` sees the engine stopped), START at 351.76 ms and STOP at 352.12 ms. The middle pair comes from libGravity: `Clock::SetSource()` ends with `if (was_playing) uClock.start()`, so selecting the source restarts a clock that FlexSeq then stops again on the next pass | low, and only for connected gear | **nothing today.** A slaved device would see two spurious transport messages at power-up, one loop pass apart. It costs nothing musically because the module is silent until PLAY. To revisit when the MIDI expander enters the path, and to weigh against the fork charter of ADR 0008: the cause is in the dependency, not in FlexSeq |

| 42 | **A greedy matcher reported 1441 lost onsets where the raw count said 12.** The overload sweep of 2026-08-25 paired each edge with an expected onset under the rule "an edge at or after the next expectation means the previous one is missing". Under overload the debt pays one onset per loop pass, so a delay exceeds the interval and the pairing shifts, then propagates: `dropped` and `unexpected` came out almost equal, 1441 against 1429, with negative grid errors | **closed for the instrument, open as a method rule** | `reconcileTimeline()` counts first and pairs second, and returns **ambiguous** rather than choosing. The identity of an onset is **not recoverable**: `owed_[ch]` is a counter, not a queue, so no matcher can do better. The rule this leaves: **an instrument must not answer a question it can no longer decide** |

| 43 | **The fourth witness of the gesture probe rests on a symbol that the build does not promise to keep.** `suppressedLong` is an internal counter of libGravity long presses that FlexSeq's own guard **absorbs**, and `run-gesture-probe.sh` reads it to separate "no long press fired" from "a long press fired and was swallowed" -- a distinction no behavioural witness can make. Measured 2026-08-25 on the production binary: the exported accessor `flexseq::input::suppressedLongPresses()` is **absent from the ELF**, removed by `--gc-sections` for want of a caller, and the whole image holds exactly **eight** references to the counter, all inside the two functions that increment it. **Nothing reads it.** Neither C++ nor the build guarantees that a non-`volatile` object with internal linkage and no reads survives LTO, so its presence today is an outcome of this toolchain, not a property. Observed stable over three builds, two of them from clean: same `firmware.hex` md5, same address, a **2-byte** object in SRAM at `0x0220` | **instrumentation, not firmware.** No functional behaviour of the module depends on this counter | **nothing to do on the firmware, and no fix is proposed.** The probe resolves the symbol by name on **every run** and never hardcodes its address; the validity conditions are the ones established during FRACT -- a single matching symbol, size exactly 2, type `.bss` or `.data`, the address converted by `VMA - 0x800000` and bounded by `avr->ioend` and `avr->ramend`, a readable flag distinct from the value, and a non-negative delta. If any of them fails, the witness is **unavailable**: the criterion is `INVALID`, never a firmware `FAIL`. The day the symbol disappears, the probe says so instead of concluding. Details and measurements: `docs/gesture-injection.md` |
| 44 | **A SHIFT burst of more than six detents is not reliable on the LENGTH field, and the mechanism is NOT identified** | **measured 2026-08-25 (D-h, D-i), scope limited, one contradiction open** | on the LENGTH field of a channel tab, a single burst applies **exactly** its detents up to **6**, and becomes erratic from **7**: measured 3, 5, 7, 5, 1 and 4 applied for 7 to 12 requested. The threshold is bracketed to one detent. The loss is **not** the cursor displacement -- the selection frame stays on LENGTH in all fourteen courses -- and it is **not** a timing effect: 6 detents slowed to a 654 ms hold stay exact, 8 detents accelerated to a 222 ms hold stay wrong with the **same** deficit as at 462 ms. The I2C traffic shows the events are emitted, and it is highest on the course that loses nothing. **Do not read a cause into this.** No buffer, no queue size and no firmware component is named by any measurement. ⚠️ **The limit is NOT universal, and a verified contradiction says so**: on the SUBDIV field, a burst of **7** (the `rig` phase) and a burst of **8** (recipe R11) apply **all** their detents -- their literal expectations, 6 ticks and 4 ticks, are reachable in no other way, `kSubdivChoices` placing `/1` at index 8. The two measurements stand as they are; nothing in the campaign settles which field property separates them. What is safe today is a burst of **6 detents or fewer**; what is proven unreliable is a burst above 6 **on LENGTH**. It blocks the restore leg of R5, which targets LENGTH. ⚠️ **R7 targets SUBDIV, not LENGTH** -- `rotate(avr, 2, 1)` selects field index 2 -- so this line says nothing about it: the upper boundary on SUBDIV is unmeasured, and R7 stays unvalidated for want of a measurement, not through a proven defect. See `WORKPLAN.md` |
| 50 | **The five criteria of the gesture probe's bootstrap course are observations, not falsifiable guards.** The course boots the firmware on an image whose version byte and one template byte are deliberately corrupted, and it establishes something causal: that byte reads `0x11` again after `setup()`, a value no supplied image carried, so `seedFactoryTemplates()` ran on the AVR. But seven mutations on 2026-08-28 failed to turn any of the five red. `boot_repare` and `boot_instances` watch template 0, from which the six instances derive, so any mutation that breaks them breaks `controlOk` first and the course aborts before they are evaluated. `boot_semis` cannot see an omitted seed because the test image ALREADY carries the sixteen factory records. `boot_version` does not discriminate `markDirty()`, which has a second caller in the main loop | **low.** They report correctly at nominal, and two witnesses ARE proven falsifiable: `controlOk`, which returns INVALID rather than FAIL, and the EEPROM template witness | **accepted on 2026-08-28, and deliberately not fixed.** Making them falsifiable would mean corrupting several templates in the scenario, which mixes validating the version 3 mechanism with improving the harness. The owner chose to record the limit. Do NOT weaken `controlOk` to make a weaker witness falsifiable |
| 54 | **A template that carries only ratchets counts as EMPTY, so it can be overwritten with no confirmation.** PRD 5.0 point 10 defines empty by computation -- "les 36 cases inactives" -- and `isTemplateEmpty()` implements exactly that: it reads the five step bytes, masks the four canonical bits above the last step, and never looks at the eighteen ratchet bytes. A record whose steps are all off but whose ratchets are set is therefore reported empty, and the interface will overwrite it without asking | **low, and it is a CONSEQUENCE of the specification, not a defect of the code.** No interface calls the query yet | **decided on 2026-08-29 (D6), literal reading.** The wider reading -- empty means no active step AND no ratchet -- was refused because it would invent a rule the PRD does not write, and would silently settle a product decision the PRD leaves open: PRD 12.1 records that `toggleStep()` does not clear the ratchet of a step it deactivates, and says what becomes of that ratchet "est une decision produit a poser". **This line closes when that product decision is taken**, and the query follows it |

| 53 | **A template written by the scheduler can TEAR: the 24 bytes are read from the live instance over about 24 passes.** `PersistenceScheduler` writes one byte per `advance()` and reads each byte from `engine_.instanceForChannel(channel)` at the moment it writes it, not at the moment the request was armed. If anything edits that instance while the record is in flight -- roughly 82 ms at 3.4 ms per byte -- the template receives a MIX of the old content and the new one. ⚠️ **The scope is exactly this: a torn template record, not a general property of the persistence.** The periodic image scan carries the same hazard and the project accepted it, but with a difference that matters: the scan REPEATS at the next `markDirty`, while a torn template stays torn | **low today, and NOT armed.** No production path calls `requestTemplateWrite()`: the SAVE gesture does not exist. A second request is refused while one is in flight, and the write stays bounded to one byte per pass, so nothing else is at risk | **accepted on 2026-08-29 (D5), and deliberately NOT fixed.** A 23-byte snapshot taken at arming time would close it, and it was refused: it costs 23 bytes of the 87 that sit above `RAM_RESERVE` at the peak of lot B4b, to secure a case that cannot happen yet. PRD 5.0 point 10 also asks for an on-screen confirmation before an occupied slot is overwritten, which gives the interface a natural moment when nothing edits. **Re-evaluate when the SAVE gesture is actually wired**, and take the snapshot then if the interface cannot guarantee the instance is still |

| 51 | **The seed of the factory templates has its OWN address producer, and its bound is enforced by nothing.** The firmware holds a single EEPROM write primitive, `EepromStorage::write` (`include/flexseq/EepromStorage.h:24`), and two callers. The scan takes its address from `image.addressAt(cursor_)` (`Persistence.h:298`). The seed takes it from `persist::v3::templateAddress(index, offset)` (`Persistence.h:233`), a second producer. Its arithmetic gives `384 + 1 + index * 24 + offset`, so `[385, 768]` for the current loop bounds -- inside the `[384, 971]` window. **No `static_assert` and no runtime check impose that**: the guarantee rests on `TEMPLATE_COUNT` and `TEMPLATE_RECORD`, which a future change could move without a compiler error | **low, and it is a NON-IMPOSED GUARANTEE, not an observed violation.** `run-eeprom-boundary-probe.sh` exercised that very path -- the seed writes 384 of the 588 bytes -- and observed zero byte changed outside the window | **audited 2026-08-28 during lot B4b.5.d.7, and deliberately not fixed there**: closing it means touching `include/`, which that lot forbade itself. It waits for a lot allowed to change `Persistence.h`, with the unit tests that go with it. The owner's criterion for that lot said "every write produces its address through `addressAt()`"; measured against the code, that criterion is **NOT met to the letter**, and the constat is the deliverable |
| 52 | **`run-drift-probe.sh` does not notice that its EEPROM image was refused.** Forced to version 2 on a version 3 firmware (`FLEXSEQ_FORMAT_FORCE=2`), it returns **RC=0**. The chain is established, not supposed: the firmware refuses the image, falls back to the factory defaults and therefore to CLOCK, and emits **708 fronts where 222 are expected**. The probe sees it -- `ecart brut -486`, `en trop 486` -- and its own code marks that line `INDICATIF`, so the verdict ignores it | **medium.** The probe never claimed to prove the image was loaded; the risk is that a reader takes its green for that proof | **accepted 2026-08-28.** Making the excess fatal is a change of SEMANTICS of the probe, not a repair of an oracle, and lot B4b.5.d.6 refused to mix the two. What the green establishes is the tick grid and nothing else. Three probes out of four do turn red under the lever, each naming its own cause |

| 47 | **The gesture probe's bank witness loses the structural size check that lot B4a gave it.** Lot B4a made the resolution strict: one symbol, a data type, and `st_size` EXACTLY the size the harness announces, or `INVALID`. Lot B4b.4 replaces the observed object: the six instances are a PRIVATE member of `SequencerEngine`, so the ELF holds no symbol for them, and the probe reaches them through a 2-byte instrumentation pointer, `flexseq::probe::instanceBase`. `st_size` therefore proves the symbol is the pointer, and says **nothing** about the 138 bytes that are read at the address it holds | **medium.** Instrumentation, not firmware; but a witness that proves less than it appears to is the failure this document exists to prevent | **accepted on 2026-08-27, with a replacement guarantee.** The structural check becomes a BEHAVIOURAL one: if the pointer is wrong, or names a representation that is not the instances, the factory-content check must fail, because the expected bytes will not be there. A counter-proof must show that `st_size == 2` alone does not pass the witness. Exporting a size beside the pointer was examined and REFUSED: the firmware could expose a pointer and a wrong size together, so it would grow the instrumentation interface without restoring the guarantee. See also line 43, and the method rule on a write-only symbol |
| 48 | **The gesture probe's instrumentation pointer ships in the production firmware.** `probe::instanceBase` costs **2 bytes of RAM and 18 bytes of Flash**, measured 2026-08-28 by a counter-build with a control. No functional path reads it: `setup()` writes it once. It exists because `instances_` is private, so no other symbol makes the six channel instances observable from outside. The pointer is therefore a test dependency inside production code. **Waits for** the instrumentation cleanup, before the final firmware. |
| 46 | **The eight frozen templates could leave the EEPROM, and that would return 192 bytes.** `A1` to `A8` carry the original's factory content and PRD §5.0 point 1 freezes them: their content can never change. A record that cannot change does not need a writable store -- it could be served from the PROGMEM table that already holds it, 16 bytes for the eight masks. The template zone would fall from **384 to 192 bytes** and the version 3 image from **588 to 396** | **an opportunity, not a risk** | **do not reopen this now.** The freeze itself is **not implemented**: the rule `index < 8` exists nowhere in the code, and the only `index < 8` in the tree picks the letter `A` or `B` on the main screen. Turning an unwritten interface decision into a storage decision would be premature. Two conditions make it decidable: the freeze implemented, and confirmed **definitive**. It would then reopen **PRD §11.1** and **ADR 0006**, both normative on the 16 x 24 layout. Decision of 2026-08-26: the sixteen templates stay persisted |

## What is closed or accepted

These lines **wait for nothing any more**. They stay written so that nobody
reopens them without knowing what has already been established or costed.

| # | Subject | State | By what |
|---|---|---|---|
| 57 | **Four SELFTEST cases of the gesture probe were red, and nothing said so.** `SELFTEST=1` costs about 68 minutes, so it is in no script that a lot runs. It ran on 2026-08-29, inside lot B4b.7, for the first time since 2026-08-28: 70 green, 4 red. ⚠️ **All four were older than B4b.7.** Two of them were replayed on the tools of `7508d6a` and gave the same exit codes. ⚠️ **The first attempt at that replay was itself invalid**: a two-path `git checkout` was joined into one pathspec, the checkout failed, and the runs used the tools of `HEAD` while the script announced the old ones. The second attempt checked that the switch had taken effect BEFORE it measured anything | **closed 2026-08-29**, commit `0d95c88` | (1) the case `l.` looked for `instances : template inchange`; the criterion is called `instances : templates EEPROM stables`, so the pattern could never match. (2) The last line said that a burst-split mutation had escaped; it is gated on the global counter, so any failing case printed it while the three mutants were green. `SELF_FAILED` becomes a count and the line reports it. (3) `instances : templates d usine` reported a **wrong rig** as a firmware defect: `eepromTemplateFactoryDiff()` reads the buffer given to the machine BEFORE the firmware runs, so a difference there is a rig fault by construction. It becomes INVALID, and the drift criterion depends on it. (4) The `bootstrap` course had **no pointer guard** where six other courses have one, so an out-of-range channel produced a firmware defect; the guard now covers the one criterion that reads the pointer, and the four that read the EEPROM keep their own paths. Each red path was exercised: `TEMPLATE_MUTATE=0` FAIL/1, `IMAGE_MUTATE=1` INVALID/5 with zero defect, `INSTANCE_CHANNEL_FORCE=6` INVALID/5 with zero defect where it reported one before. Nominal PASS 103, the criteria count unchanged. Full SELFTEST after the fix: 32/32 cases, 3/3 mutations, RC 0 |
| 55 | **Twenty-two of the 230 mutants could not run, and the series reported itself as complete.** The first end-to-end pass, on 2026-08-29, stopped on the first of them. Sixteen anchors had gone stale when the channel record codec moved from member methods to free functions -- `engine_.` became `engine.`, the TypeScript indentation dropped from six spaces to four, and `applyChannelRecordByte` collapsed each `case` onto one line. Six more had become ambiguous when version 3 duplicated the `resetToDefaults` of version 2, line for line. A later pass then crashed on ten mutants of lot B4b.6 carrying the suite tag `ts-all`, which `SUITES` does not define. ⚠️ **None of the three faults could report itself as an undetected mutation** -- the probe raises an error, as it should -- **but none of them could run either**, and only a full pass, which nobody had ever completed, would have surfaced them | **closed 2026-08-29**, commit `097aa24` | the 22 anchors are re-targeted, never deleted: the 16 follow the codec, the 6 are anchored on what is unique to the version 3 body. The ten suite tags become `ts`. `--only` also recovers its 29 missing mutants -- three families carry a two-word prefix, `ts drift:`, `ts reconcile:`, `ts gestures:`, and the predicate compared the whole prefix -- so the counts become **108 cpp, 122 ts, 230 total**. **`--check-anchors` makes the check permanent**: it reports, without mutating and without compiling, an unreadable file, an absent anchor, an ambiguous anchor, and an unknown suite tag. Each red path was exercised and restored |
| 56 | **The version 3 fallback to defaults was not covered: nothing asserted that it restores the mode, the offset and the skip chance.** Version 2 was covered by `test_a_wrong_version_byte_returns_the_defaults`; the only version 3 fallback test checked the pattern content of the instances and nothing else. The format that actually ships had the weaker assertions | **closed 2026-08-29**, commit `d79966e` | found by mutation, and by accident. Six mutants on the defaults were re-targeted from the version 2 body to the version 3 one to lift an ambiguity, which **moved their criterion of detection** without anyone intending it. They then survived the first complete pass, and their survival named the missing assertion. ⚠️ **The re-targeting was wrong as a re-targeting; the coverage defect it revealed was real.** One assertion per language now dirties all six channels -- RANDOM, offset 7, skip chance 9 -- before forcing the fallback, because a `resetToDefaults` that did nothing would otherwise still pass, the initial values being the defaults already. The series then runs complete: **230 anchors applicable, 230 detected, no survivor**, the first score of this project that is both complete and replayable. ⚠️ It says every mutant that exists is detected. It does **not** say the coverage is exhaustive: a probe only finds the holes a mutant aims at |
| 45 | **The Flash cost of the version 3 persistence codec was unknown, and lot S could not be decided without it.** The codec was declared and tested, but no firmware path called it, so the linker removed it entirely: RAM +0 and Flash +0 measured on 2026-08-26, mangled symbols absent from the ELF | **closed 2026-08-28**, commit `815546b` | activating version 3 in `main.cpp` costs **Flash +2 bytes**. The linker dropped the version 2 image in exchange: `avr-nm` finds `PersistentImageV3::addressAt` and no `PersistentImage::` symbol, and `loadFactoryPatterns` has no caller left. The firmware carries ONE image implementation, which is why the net cost is almost nothing. Extracting `bootstrap()` from `main.cpp` into `Persistence.h` cost 32 bytes more; that figure is measured, its cause is not. Measured footprint: RAM 1699/2048, Flash 27164/30720 |
| 49 | **Two assertions of the input-adapter suite read the shared bank, and the suite was red.** `test_a_long_press_on_shift_clears_the_pattern` and `test_a_rotation_while_shift_is_held_spares_the_pattern` toggled a step through the UI, then read `bank.getPattern(0)`. Since lot B4b.4 the UI writes the channel instance, so the bank stayed empty and both assertions failed. The firmware was not at fault: the oracles were stale, exactly like the gesture probe's oracles that lot B4b.4.5 repaired. The suite lives in `env:native_adapter`, which `run-cpp-tests.sh` does not run, so a green C++ run hid it for the whole of lot B4b.4 | **closed 2026-08-28**, commit `b0b220a` | the three reads became `engine.instanceForChannel(0)`, the form `test_ui_controller` and the TypeScript suite already used. Only the test file changed: 3 lines, no firmware change. Both assertions were observed **red before and green after**, on the same production binary, so the counter-proof is measured and not assumed. `run-all-tests.sh` then returned RC=0: env:native 404/404, env:native_adapter 5/5, TypeScript 392/392, typecheck clean, libGravity conform. ⚠️ The method rule this line produced stays open below: **a green suite is not a green repository** |
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

**A tolerance turns a bound into a false pass.** `run-screen-dump.sh` walked the
steps up to `Pattern::DEFAULT_TOTAL_STEPS` and asked for **1 pixel** where a step
sits beyond LENGTH, because such a step is drawn as a single dot. When lot A took
the pattern to 32 steps on 2026-08-25 while the grid stayed at 24, the harness
looked for eight steps that the screen never draws, found ink from the title and
the footer at those coordinates, and reported **32/32 steps in place**. It read
green on a question it could no longer answer. The harness now walks
`screen::GRID_STEPS`, which is what the screen draws, and the criterion was
verified red -- `LENGTH=32` gives 20/24. **A check whose bound comes from one
source and whose subject comes from another will pass for the wrong reason.**

**A witness that a safety net can mask is not a witness.** Measured 2026-08-25
on a deliberately broken gesture harness: a `SHIFT` hold of 1434 ms fired **two**
long presses, and the pattern came back **intact**, because
`onShiftLongPress()` returns early when `rotatedWhileShiftHeld` is set. Every
behavioural criterion said PASS on a gesture that was physically invalid. The
firmware's own `suppressedLong` counter is what separates "no long press" from
"long press absorbed", and it went 0 to 2. The rule: **when a defensive path
swallows the effect you are watching for, watch the defensive path instead** --
and when it cannot be read, the verdict is `INVALID`, never `PASS`.

**A constant that is only printed is not a rule.** `SHIFT_BURST_DETENTS = 20`
was defined in the harness, published in its report and in
`docs/gesture-injection.md` as the safety rule for long `shiftRotate` gestures,
and **applied nowhere**: `shiftRotate()` injected every detent under one hold.
The value itself was also wrong -- 20 detents last 1110 ms against a 750 ms
threshold. Nothing failed only because no gesture had ever reached that size. A
published bound must be enforced by something that can turn red: here five
`static_assert`, a pre-injection guard that exits 4, and three replayable
mutations under `SELFTEST=1`.

**Reading a firmware structure from outside is limited to what a `static_assert`
guarantees, because the build gives nothing else.** Measured on 2026-08-25:
adding `-g` leaves the `.hex` **byte-identical** — proven on two clean builds,
same md5 — but produces **no usable type information**: `.debug_info` stays at
its 4.5 kB of library content and carries **zero** `DW_TAG_member`. The cause is
LTO: the objects hold GIMPLE, not code, and the link does not regenerate the
types. `-Wl,-g` and `-fno-lto` change nothing that can be used without changing
the measured binary. What remains readable is therefore what the firmware itself
asserts — `sizeof(Pattern) == 23`, `PatternBank` being `Pattern[16]` — and
everything else must be observed on the pins.

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

**A lever must act inside the window it suspects.** Diagnostic D-d varied the
rest **between** two `SHIFT` bursts, over six values and three boot phases, and
concluded nothing: the suspect window is the 15 ms **inside** `shiftBurst()`,
between the last detent and the `SHIFT` release, which that rest never touches.
The signature was the result itself -- **not monotone**, a longer rest giving a
worse outcome. Read that shape as "the wrong parameter is moving", not as "the
phenomenon is random". Eighteen runs bought one fact, and it was a negative one.

**A symbol search proves nothing without a positive control.** Measured
2026-08-26, while checking that the version 3 codec stays out of the firmware.
The first search ran `avr-nm -C` and looked for `contentByte`. It found nothing,
and the conclusion "the codec is absent" was bad reasoning: this `avr-nm` **does
not demangle**, so the same search also found no `PersistentImage::`, which is
linked. **A negative result from a search whose matching semantics have not been
validated is not evidence of absence.**

The repeat used the **mangled** names -- `2v3`, `11contentByte`, `8stepByte` --
and carried a **positive control** in the same run:
`_ZN7flexseq7Pattern10setRatchetEhh` and `_ZNK7flexseq7Pattern8readStepEhRb` are
present. The absence then means something.

**The object files cannot answer this question at all.** The AVR build uses LTO.
`Persistence.cpp.o` holds seven symbols, among them `__gnu_lto_slim`, because the
object carries GIMPLE and not code. Only the final ELF is authoritative for `nm`.
`platformio.ini` declares no `-flto`; the flag comes from the board or the
platform. The same fact explains why `PersistentImage::byteAt` appears nowhere:
LTO inlined it into `main`.

**A zero drift is not a proof of absence either.** `run-build-memory.sh` reported
RAM +0 and Flash +0 for the whole of lot B2, which fits an eliminated codec, but
two changes that cancel out would report the same. The drift and the symbol
search answer different questions, and only the second names what is in the
binary.

**A mutation score is only worth the assertions that were collected.** The sweep
that counter-proves the literals of a test file walks its functions. A collector
that ended a function on a line equal to `}` also ended it on the close of a
`for` loop, so every assertion after the first loop escaped the sweep. Three
scores announced during lot B -- 66/66, 14/14 and 11/11 -- were therefore
**partial**. They were not wrong, they were incomplete, and they were presented
as complete. Re-run with brace counting, the whole set of version 3 assertions
gives **64/64**. Announce a score only after checking **what** was collected.

**An instrumentation symbol that is only WRITTEN does not survive the link.**
Measured 2026-08-27, on the pointer that lot B4b.4 adds so the gesture probe can
find the six pattern instances. The pointer is a global in a named namespace, and
`setup()` writes it. That was believed to be enough, because the object is not
merely unread like `suppressedLong` (line 43): it receives a value. **It is not
enough.** Without `volatile` the symbol is ABSENT from the ELF and RAM does not
move by a single byte -- the link removes the store together with the object,
because no read makes the store observable. With `volatile` the symbol is
present, 2 bytes, type `b`. **The instrumentation therefore depends on one
keyword**, and a future edit that removes it removes the witness without any
error. Test the ELF for the symbol, never the source for the write.

Its full cost was measured on 2026-08-28, by a counter-build **with a control**:
one copy of the tree left intact, one copy without the pointer, each built in its
own build directory. The intact copy reproduces the repository footprint to the
byte, so the delta is the instrumentation and nothing else: **RAM +2 bytes, Flash
+18 bytes**, and the Flash cost sits entirely inside `main`. An earlier figure of
14 bytes of Flash carried no control and is not reproducible today. **A counter-
build without a control measures the harness as much as the subject.**

**A counter-proof that matches a source pattern goes stale in silence.**
Two mutants of the gesture probe aimed at a line that the burst policy refactor
removed on 2026-08-25. The SELFTEST could therefore not run for two days, and
nobody knew, because no routine run calls it. The guard did its work: an absent
pattern exits 2 and is **never** a detected mutation -- without it the two
mutants would have reported success while testing nothing. Two lessons follow.
Check every source pattern against the current code before you trust a score.
And re-target a stale pattern rather than delete the mutant: the criterion of
detection must not move when the point of injection does.

**The buffer you hand a simulator is not the state it keeps.** `AVR_IOCTL_EEPROM_SET`
COPIES the caller's buffer into simavr's own EEPROM model; it does not alias it. A
witness that compared that buffer before and after a run therefore compared two
arrays the firmware never touches, and was green whatever happened. The proof was
already in the repository: `stack_probe.c` reads the EEPROM back through a second
ioctl, `AVR_IOCTL_EEPROM_GET`, and its comment says why. Read the state back
through the interface the model offers, never from what you gave it.

**A mutation is a counter-proof only if it is DIFFERENTIAL.** Three of seven
mutations on 2026-08-28 proved nothing, each for a different reason, and none of
them was visible on the source alone. `markDirty()` removed from `bootstrap()`
changed nothing, because the main loop calls it again whenever the UI revision
moves. Skipping one template in the seed changed nothing, because the test image
already carried that template. Two firmware mutations were caught by a STRONGER
witness upstream and never reached the one they targeted. Before trusting a
mutation, look for a second path to the same effect, check that the fixture does
not already carry the expected value, and check which witness fires first.

**A guard built from one breakdown only covers that breakdown.**
`--check-anchors` was written on 2026-08-29 to catch anchors that had gone
absent or ambiguous. It was run, it returned green, and the very next full pass
crashed on ten mutants whose SUITE TAG did not exist -- a different way of being
unrunnable, which the fresh guard did not look for. The guard now covers four
categories: an unreadable file, an absent anchor, an ambiguous anchor, an unknown
suite tag. **When a new failure reveals another category of unrunnability, widen
the guard to the CATEGORY, and prove the widening with a falsifiable
counter-proof** -- not to the single case that was just observed.

⚠️ A related trap from the same day, and it cost a wrong claim: a counter-proof
that does not break what it claims to break proves nothing. Appending ` // x`
after the semicolon of an anchored line left the anchor a substring of the line,
so the guard stayed green and the red path looked unreachable. Break the
expression itself, not its surroundings.

**An oracle that locates a constant by its POSITION in a file goes stale in
silence.** `run-stack-probe.sh` read the active persistence format by cutting
`include/flexseq/Persistence.h` before `namespace v3 {` and keeping what
remained. That was correct while version 3 was declared and inactive. The day
version 3 was activated, the same code expected version 2 and 304 bytes on a
version 3 firmware, and printed a red criterion that said nothing about the
firmware -- the failure looked like a regression and was an oracle. The fix is
not a better pattern: it is to ask the artefact that is actually under test.
`tools/active-format.sh` identifies the linked image by its symbols in
`firmware.elf`, then takes the constants from the COMPILER through
`tools/persistence-format.cpp`. Two linked images, or none, stop the caller
without a verdict, and a binary that `avr-nm` cannot read is named as such: a
format that was not looked for is not a format that is undecidable.

**A shared resolution must be re-proven where it lands.** Moving that detection
into a file shared by four probes moves the counter-proofs with it. They were
re-run: no image symbol, both images, an invalid force value, a missing binary,
an unreadable binary, and the nominal case. A guard proven in its old home
proves nothing in its new one.

**A suite in another environment is not run by the script you reach for.**
`run-cpp-tests.sh` runs `env:native`. The input-adapter suite lives in
`env:native_adapter` and only `run-all-tests.sh` runs both. Lot B4b.4 was
validated for eight sub-steps on the narrower script, and line 49 stayed red the
whole time. **A green suite is not a green repository.**

**A tool must judge on one criterion, not two.** `run-stack-probe.sh` printed a
green persistence line and exited 1 at the same time, for four days. The last
line rebuilt the verdict against the literal `'1'` and overwrote the correct
value computed above it. The firmware writes version 2, so the criterion was
false whatever the report showed. Commit `3dc230f` wrote that line when the
format was version 1; commit `7677755`, titled *the stack probe reads the format
instead of assuming it*, corrected the display line only. A human read green, an
automatic caller read red, and neither was wrong. **Compute the verdict once,
then print it and exit on the same value.** Fixed 2026-08-27, with both red
paths exercised.

**A search pattern goes ambiguous when a new format doubles a declaration.** The
version 3 layout declares `FORMAT_VERSION` and `CHANNEL_RECORD` a second time
since commit `1077e52`. Two tools broke on the same day, and neither said so:
`run-stack-probe.sh` read `FORMAT_VERSION` over the whole header and held
`"2\n3"`, and one mutant of `run-mutation-probe.py` mutated version 2 while its
label named no version. The same shape hit lot B3.3 by hand: a replacement on
`if (offset == RECORD_LENGTH_AT) {`, which the file holds three times, mutated
`templateByte` instead of `factoryTemplateByte`, and the mutation read as
undetected. **A pattern that selects a target must be unique, and the tool must
refuse the ambiguity instead of taking the first occurrence.**
`run-mutation-probe.py` now exits 2 on a pattern seen more than once, as it
already did on an absent one.

**A validation gate that is not part of any routine pass rots without a signal.**
`SELFTEST=1` of the gesture probe costs about 68 minutes, so no script a lot runs
executes it. Four of its cases were red for at least one day, three of them since the
version 3 format changed under them, and the repository reported itself green
throughout. This is the second time: the two mutants of the burst split aimed at a line
that the `burst::` module had removed, and the SELFTEST was unrunnable for two days
without anyone knowing. ⚠️ **The guard that episode produced did not cover this one.**
Run an expensive gate at the START of a lot that touches its subject, not at the end. A
red found first is a fact about the past; a red found last is confused with the work in
hand — lot B4b.7 spent two runs proving that four failures were not its own.

**A tool that switches its own inputs must prove the switch before it measures.** The
replay of two SELFTEST cases on the tools of an older commit announced the switch and
did not perform it: `git checkout <sha> -- <two paths>` reached the proxy as one
pathspec, failed, and the runs used the current tools. The error printed before the
confirmation line, so the transcript read as a success. The second attempt asserted a
content marker of the old file and refused to measure without it. **Same rule as the
mutation probe: a lever that does not apply must stop the run, never return a result.**
