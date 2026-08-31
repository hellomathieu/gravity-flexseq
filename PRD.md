<!--
THE NORMATIVE SOURCE OF THE PRD. IT LIVES IN THIS REPOSITORY SINCE 2026-08-30.

Decision of the owner, 2026-08-30: the PRD lives in the repository. This file
replaces the Notion page as the normative source of the product requirements and
the product decisions.

Imported from Notion on 2026-08-30T09:29:24Z, page
https://app.notion.com/p/3bed2c2576ce80459448cc525a929d9d
The Notion page becomes a HISTORICAL source. Do not edit it. Do not consult it to
settle a question. On a divergence, this file is the authority.

Routing: .claude/rules/knowledge-persistence.md
Source hierarchy: CLAUDE.md, section "Source hierarchy"

LANGUAGE — DECIDED on 2026-08-31, decision D4 of the owner. This file moves to
English. Rule 3 of .claude/rules/github-conventions.md requires English for every
document that the project pushes to GitHub. That rule exempted the PRD because
the PRD was not pushed. The premise disappeared on 2026-08-30. The owner chose to
translate, and not to record an exception.

The translation runs section by section, one commit per section, and the owner
validates each section before the next one starts. The section numbers stay
frozen: 86 references of the form "PRD §N" live in the pushed documents. A
translation commit carries NO content change: a stale fact gets its own commit,
before or after. The method and the glossary live in
.claude/rules/prd-translation.md, and that file is not pushed.

The French text stays recoverable through the Git history only. The project keeps
no parallel French copy: that copy is what
.claude/rules/knowledge-persistence.md forbids.
-->

**An alternative firmware for the Sitka Instruments Gravity Eurorack module.** **Its Trigger Sequencer holds patterns of 36 steps, and the interface adjusts the length from 1 to 36 since lot SF3, 2026-08-30.**
> **Status:** the normative version. It carries the decisions the TypeScript POC and the C++ firmware alignment validated: the native tests green, the simavr verification, and the RAM measured on the ATmega328P.
> **Software base:** `libGravity` (Adam Wonak), resolved from the **fork of the project** `github.com/hellomathieu/libGravity`, frozen at the commit `4c5b4d0b4f38a9e04055ad48f1f7e2d90541c93c`. `platformio.ini` is the authority, and the charter of what the fork may repair is **ADR 0008**. The old pin `9be88be1f4` is obsolete.
> **Hardware:** the Gravity, strictly unchanged.
---
## 1. Vision
Gravity FlexSeq is an alternative firmware for the Gravity. The project develops it **without a change to the hardware**, and it builds technically on `libGravity`. It extends the original **Trigger Sequencer**, which holds 16 fixed steps. The patterns of the new sequencer hold a **capacity of 36 steps**. The interface adjusts the length from **1 to `MAX_LENGTH`**, which is **36 since lot SF3, 2026-08-30**. The temporal design is more general, and the module keeps its historical features.
⚠️ **This paragraph and the subtitle announced `MAX_LENGTH` as 24 until 2026-08-31, and they credited the cap of 36 to lot F.** Both facts were stale. Lot **SF3** raised `MAX_LENGTH` from 24 to 36 on 2026-08-30. Lot **F** extended the EDIT grid to 36 positions on the same day. The full account lives in §5.2 and in §12.1, and this paragraph does not copy it.
The project also aims at a **fast development cycle**. Each feature requires development, testing and a visual check **before** any flash on the physical module.
⚠️ **NORMATIVE RULE, given again by the owner on 2026-08-23 after a trial on the module.** The project **keeps** all the **features** and all the **pages** of the original firmware. Only the **SEQ mode** changes. The rest changes only as an effect of that change. This rule has priority over every screen design. If a page of FlexSeq holds less than the matching page of the original, FlexSeq is wrong.
**Three omissions were found, and all three belong to the same family.** The three channel modes were absent from the domain (§4.2, 2026-08-22). The BPM tab held one field out of four (§12.1, 2026-08-23). The tab bar lacked the Play/Stop indicator (§12.1, 2026-08-23). Each time, the page was rebuilt from a **design** and not from the **drawing code of the original**. The design was not wrong. It was incomplete, and nothing compared it to the source.
**Operational consequence:** a **conformity audit**, screen by screen and field by field, comes **before** the rest of the interface work. **Done on 2026-08-23: `docs/original-conformity.md`.** Each line there carries the line of the original firmware, so a reader can verify it. The audit produced **six open decisions** (§16) and seven findings that nothing had raised.
⚠️ **SECOND RULE, same day and same owner: the project keeps the visual DESIGN of the original pages**, with **the glyphs and the fonts that are already there**. If a font is too heavy for the Flash, **the project redraws it itself** to save space. Do not change the visual design to avoid a cost.
This rule **settles a question that was open**: the single font of FlexSeq (`setFont(u8g2_font_5x7_tr)`, called once and never changed) is not an acceptable simplification. The ten in-house glyphs, designed and not implemented, are therefore **work** and no longer a trade-off: ~500 bytes estimated against 2646 for `logisoso26`.
⚠️ **THIRD RULE, same day: the project keeps the GESTURES of the original**, and it adds the gestures of FlexSeq to them. See §12.1.
---
## 2. Hardware constraints
- MCU: **ATmega328P**, 16 MHz.
- RAM: **2048 B**, and it is the critical resource.
- Application Flash: **30720 B**.
- The Gravity hardware, the MIDI Expander and the Expansion Header: **unchanged**.
- No replacement of the MCU.
- Toolchain: PlatformIO / Arduino AVR. The project resolves `libGravity` from its own fork, frozen at commit `4c5b4d0b4f38…` (ADR 0008). The old pin `9be88be1f4` is obsolete.
>
> **The flash path — verified on the sources on 2026-08-21, then MEASURED on the module the same day. This path holds no way to brick the module.** The KiCad schematic of `GravityHW` puts an **Arduino Nano v2.x** on the board, and the upload goes through the **USB bootloader** of the Nano. It **writes no fuse**, and it does not use the ISP. The fuses are the only way to make an AVR unreachable. The original firmware is itself an Arduino project (`.ino`), and the manufacturer uploads it the same way: to flash FlexSeq is the operation the manufacturer performs. The successful read of the Flash confirms the argument, instead of only reasoning it.
>
> ⚠️ **The speed is 115200, and not the 57600 of the manifest — corrected on 2026-08-21 from a measurement.** The `nanoatmega328` manifest of PlatformIO gives `protocol: arduino` and 57600 baud. That is true of the manifest and **false of this module**, which carries an **optiboot** bootloader. The first backup attempt died on `stk500_getsync(): not in sync: resp=0x00`, ten times. At 115200 the board answers immediately: `Device signature = 0x1e950f`. `platformio.ini` therefore overrides `upload_speed = 115200` on the **four** environments that upload — `nanoatmega328`, `encoderprobe`, `bringup` and `eepromdump` — and on those only. ⚠️ **This sentence named three environments until 2026-08-31**, because `encoderprobe` did not exist when it was written. `simavr` and `wokwi` never leave the simulation. The two board manifests differ **only** by that speed: `maximum_size` is 30720 on both sides, so no figure of §15 changes.
>
> **The port is named `/dev/cu.usbserial-*` on macOS**, and it exists only while the module is connected. Look for it with `ls /dev/cu.*`. Under zsh, a glob pattern that matches nothing aborts the whole line, before the execution.
>
> **The pin directions are identical to the original, all ten of them.** The active configuration of `GravityFW/src/Gravity/Gravity.ino` is **Rev 2+**, because the Rev 1 block is a comment. It matches `peripherials.h` of libGravity pin for pin: encoder 17/4, switch 14, PLAY 5, SHIFT 12, EXT 2, CV1 A7, CV2 A6, outputs 7/8/10/6/9/11, PULSE 3. The original firmware sets only the six outputs and the clock to `OUTPUT`. FlexSeq sets no other pin, and it **holds no `pinMode` of its own**: everything goes through libGravity (`DigitalOutput` to OUTPUT, `AnalogInput` to INPUT, `Button` to INPUT_PULLUP). No conflict of direction is therefore possible, and such a conflict is the only mechanism that could damage the silicon.
>
> ⚠️ **Precondition: the module must be Rev 2+.** The **Rev 1** carries another pinout: CV on A2/A1, the clock on 13, and the outputs in another order. libGravity does not target it. You recognise it **by eye**: Rev 1 defines `SHIFT_BTN_PIN 100`, which means it **has no SHIFT button**. A panel that carries a SHIFT button is Rev 2+.
>
> ✅ **PRECONDITION SATISFIED, confirmed by the owner on 2026-08-21**: their panel carries a SHIFT button, so the module is **Rev 2+** and the pinout of libGravity is the correct one. Nobody has to do this verification again.
>
> **The user state lives entirely in the EEPROM.** The original firmware stores the bpm, the master clock mode, the channels, the **16 sequences A1–B8**, the CV calibration and the rotation preference. ⚠️ **FlexSeq WRITES to the EEPROM since 2026-08-28, commit `815546b`.** The version 3 image is active: `main.cpp` builds a `PersistentImageV3` and calls `bootstrap()`, and the firmware writes **588 bytes at address 384** (§11.1). FlexSeq never writes below address 384, so it never reaches the settings of the original firmware. `tools/run-eeprom-boundary-probe.sh` measured that on 2026-08-28: 588/588 bytes written inside the window, and zero byte changed outside it. An upload through the bootloader does not touch the EEPROM. ⚠️ **This paragraph said that FlexSeq writes no byte of EEPROM, and that was true until 2026-08-28.** Make the backup before the first flash anyway: "it should survive" and "it did survive" are not the same statement.
>
> ✅ **Backup of the Flash: DONE and VERIFIED on 2026-08-21.** 32768 bytes, an authentic interrupt vector table at the head, 14.8 % of `0xFF`. The project keeps it **outside the repository**: it is the GPLv3 firmware of the manufacturer plus the settings of the owner, and to version it would publish it. The restoration covers the **application region**. The bootloader itself cannot rewrite the bootloader region, and that region is never at risk, because none of our operations reaches it. ✅ **TESTED on the module on 2026-08-22.** `tools/run-original-restore.sh` ran end to end, and its four blocking criteria were green. The decisive one is the readback: 27648 bytes identical to the backup over `0x0000`–`0x6BFF`, and the 512 bytes of the bootloader untouched. The owner then confirmed by eye what no tool covers: the original boots, the eight patterns are there, and the tempo reads 120. `docs/open-risks.md` line 23 closed on that run. ⚠️ **This paragraph said "not tested" until 2026-08-31.**
>
> **The bootloader is measured now, and no longer deduced (2026-08-22).** A read of the backup gives this: the last 512 bytes (`0x7E00`–`0x7FFF`) carry **506 written bytes**, and the regions `0x7800`–`0x7DFF` are **completely empty**. It is therefore **optiboot, 512 bytes, at `0x7E00`**, which agrees with the 115200 baud. The original application stops at `0x6BFF`, 27648 B.
>
> ⚠️ **The restoration must be TRIMMED to `0x0000`–`0x6BFF`.** The bootloader is intact, and the bootloader is the writer: to send it its own pages would fail the verification. The backup HEX covers `0x0000`–`0x7FFF`, so nobody can restore it as it is.
>
> ⚠️ **A possible Flash lever, NOT VERIFIED.** If the bootloader occupies only 512 bytes, the application space is **32256** bytes, and not the **30720** that the PlatformIO manifest declares (32768 − 2048). That is **1536 bytes more**, and it would count, given the tension of the budget at §12.1. But the measurement covers the *content*, and not the reserved space. The reserved space depends on the `BOOTSZ` fuse, and the bootloader cannot read the fuses: they come out at `0x0`. To read `BOOTSZ` would require an ISP programmer. **The 30720 therefore stay the safe assumption, and they do not change.**
>
> ⚠️ **The bootloader cannot read the EEPROM, and it does not say so.** optiboot compiles that support out of the binary. `avrdude -U eeprom:r:` then returns **Flash content**, and it reports no error. The dump looked credible: 1024 bytes, and 1.2 % of `0xFF`. It then proved **identical byte for byte to the first 1024 bytes of the Flash (1024/1024)**. The criterion of a backup is therefore neither its size nor its entropy. The criterion is that the bytes are the bytes we asked for.
>
> **`env:eepromdump` — the firmware reads its own EEPROM.** `src/eeprom_dump_main.cpp` reads the 1024 bytes and sends them as **Intel HEX** on the serial port at **9600 baud**. That speed lets `cat` work without a configuration of the port under macOS. No dependency: no libGravity, and no `NeoHWSerial`, which would replace the ISR of the serial port. Read only, and no write of EEPROM. The output of avrdude itself validates the checksum algorithm: **1025/1025 identical records**. The dump goes out **three times**. Measured cost: **RAM 216 B, Flash 1844 B**.
>
> ✅ **Backup of the EEPROM: DONE and VERIFIED on 2026-08-21**, by `tools/run-eeprom-dump.sh`. It uploads, captures, verifies, **and then** writes, in that order. **Three blocking criteria, and a criterion that nobody can evaluate counts as a failure**, instead of passing in silence: all checksums valid and 1024 bytes covered · the capture **different** from the first 1024 bytes of the Flash · two consecutive dumps identical. The script writes nothing if one of them fails, and it never overwrites an existing backup without `FORCE=1`. Measured: 3 dumps, 64 records, **0 invalid checksum**, **40/1024** identical to the Flash, 68.1 % of cells never written. Replayed from zero, the script reproduces the backup **byte for byte**. Red paths exercised: a window too short, an absent Flash reference, and a backup already present.
>
> **The content is identified, and not only plausible.** `saveState()` of the original firmware (`GravityFW/src/Gravity/Gravity.ino`) writes the **bpm at address 0**, and we read **120** there. Then come `bpmModulationChannel`, `bpmModulationRange`, `masterClockMode`, `channels`, and then the 16 sequences `seqA1`…`seqB8`. This layout is what will make the **real CV calibration** recoverable for §10.
>
> ⚠️ **Three traps of the tooling, all found in use, and all the same fault: the tool depended on a state that it did not control.** (1) The dump went out only once, so the capture read the lag of the port buffer and started in the middle of a record. Hence the three copies, which the stability criterion needs anyway. (2) `stty` **does not survive** the close of the descriptor that it configures: the first capture was clean, and the next ones were scrambled at a speed nobody had set. `termios` now configures the port explicitly. (3) An upload **cannot synchronise while the board transmits**: avrdude reads the stale bytes as an answer, and it reports `not in sync`. To flush the buffer **is not enough**, because bytes arrive *between* the flush and the synchronisation. The script therefore **waits for the silence**: 2 s of inactivity, with a ceiling of 45 s. Remember this beyond the tool: to open a `/dev/cu.*` **resets the board**, so every measurement on the serial port starts with a restart.
>
> **The project sets the ISP paths aside, and here is the reason.** A USBasp, 5 to 10 €, or an Arduino in "Arduino as ISP" would read the EEPROM. But the ISP bus occupies D11/D12/D13, and the Rev 2+ pinout puts a **trigger output on D11** and the **SHIFT button on D12**. The programming bus would therefore carry an output driver and a pull-up resistor. That works often, it fails sometimes, and it is hard to diagnose. The firmware path does not have this problem.
>
> ⚠️ **Consequence for §10, and the distinction is exact: the calibration data is RECOVERABLE, and the calibration is NOT APPLIED.** The backup of 2026-08-21 holds the calibration of this module, and `docs/open-risks.md` line 17 closed on it. The data is therefore out of the EEPROM and readable. What does not exist is the path that applies it. `main.cpp` configures `CvSampler` from `gravity.cv1.GetCalibrationLow()` and `gravity.cv2.GetCalibrationLow()`, so from the defaults of libGravity, `CALIBRATED_LOW = -566` and `CALIBRATED_HIGH = 512`. The codec persists `Preferences::cvCalibration`, and **no other code reads that field**: `src/domain/Persistence.cpp` lines 76 and 91 are its only readers. libGravity holds **no** notion of EEPROM, and that is verified on the source of the pinned commit. ⚠️ **This paragraph said the calibration was out of reach, and it pointed at line 17 as an open item.** Line 17 closed on 2026-08-21. Do not read this paragraph as a working calibration: nothing applies the stored value.
---
## 3. Software base and architecture
The project develops the firmware **from `libGravity`**, and it does not restart from the original Sitka firmware. That firmware stays a behavioural reference.
```javascript
libGravity (hardware + clock)
   ↓
Hardware Integration / Adapters
   ↓
FlexSeq Domain
   ├── Pattern            (content: 36 steps and 1 ratchet per step)
   ├── PatternBank        (16 shared patterns)
   ├── SequencerEngine    (masterPhase, state per channel)
   ├── Transport          (clock/MIDI → the engine)
   ├── TriggerSequencer   (onset and active step → trigger)
   ├── Musical Grid        (SUBDIV; graphical measure separation)
   ├── CV Mapping / Reset
   ├── UI Logic
   └── Persistence API
```
The domain is **testable without hardware**: the native tests and the simulator exercise it. The conversion from the clock to the logical progression belongs to **Transport**, and not to the Sequencer Engine.
---
## 4. Features kept
6 Multi-Mode Channels · Clock Mode · Random Skip Mode · 2 CV Inputs · External Clock · internal BPM · MIDI through the MIDI Expander · Expansion/Connectivity · Settings · CV Calibration. The project does not modify the MIDI Expander and the Expansion Header.
### 4.2 The channel modes — OMISSION CORRECTED on 2026-08-22, RAISED TO FOUR on 2026-08-23
⚠️ **This section describes three modes. There are four since 2026-08-23: `GATE` comes from `1.2-dev`, and `SWING` becomes a parameter of `SEQ` and not a mode. See §5.0.**
⚠️ **FlexSeq implemented no mode at all.** The owner found it **on the module**: the screen showed only `SEQ`. This was not a layout difference. The three modes did not exist in the domain, and `UiController` assumed in silence that the six channels are sequencers. §4 listed them all the same. **An omission, and not a decision.**
Constraint set by the owner: **keep every original feature, and extend `SEQ` only.**
**Facts read in the original firmware** (`GravityFW/src/Gravity/`, source #4):
- `byte mode; // 0 CLK, 1 RND, 2 SEQ`, and the **factory default is `CLOCK`** for the six channels, with `subDiv` at one;
- **`OFFSET`**: it fires when `channelPulseCount == offset`. This is a phase shift **in pulses** inside the cycle of the channel, and the screen shows it as `offset/pulsesPerStep`;
- **`SKIP CHANCE`**: `random(10)+1 > randAmount`. ⚠ **Two distinct bounds, found on 2026-08-23**: the original interface clamps the **stored** value to **9** (`Interactions.ino:157-161`), so 0 % to 90 %. The **generation** clamps `random + randMod` to **10** (`Gravity.ino:479-483`), so only the CV modulation reaches 100 %. FlexSeq let the user set 10 directly. **The owner settled on the cap of 9** (§16), and the effective value keeps 10 through the CV;
- **`SEQ` plays hardcoded 1/16 steps**, and the original SEQ screen exposes **no** SUBDIV. To expose it is a deliberate FlexSeq divergence (§6.1).
**Decided by the owner on 2026-08-22**: the three modes receive **a field AND a behaviour** at once, and not an inert field. `CLOCK` gives one trigger per step at the position `offset` · `RANDOM` gives one trigger at the position 0, skipped with the probability `skipChance/10` · `SEQ` keeps the current behaviour. The pseudo-random generator takes a **fixed seed**: the original does not seed, so its behaviour repeats from one start to the next.
State: **implemented on 2026-08-23**, in C++ and in TypeScript (lot 9). What is missing is the access. No `MODE` field exists in the interface before lot 11, so a module flashed today plays **six clocks and no pattern**. The domain is correct, and the interface does not reach it yet.
Three facts established at the implementation, and they belong to the behaviour of the module:
- **Only `SEQ` reads the bank.** `CLOCK` and `RANDOM` ignore the content of the pattern **and its ratchets**. In the original, the path of the sequencer is a branch separate from the path of the clock. A ratchet set on a channel in `CLOCK` is therefore inert, and it becomes active again when the channel returns to `SEQ`.
- **At `offset = 0`, the trigger IS the step boundary.** The position 0 and the boundary are the same instant, and to treat them separately would emit two triggers per step. The boundary emits it in that case, and the crossing of the offset emits it in every other case. This also lets a grouped `advance()` count every crossing that it steps over.
- **The offset is clamped to `ticksPerStep - 1`**, and that holds when the rate changes, as it does in the original. An offset **exactly equal** to the duration of the step would never be reached, and the channel would stay silent.
The draw of `RANDOM` is spent **once per onset**, in `TriggerSequencer::update()`, which runs right after `advance()`. The generator is a 16-bit xorshift with a fixed seed. A **golden vector of five values** is asserted on both sides, so C++ and TypeScript cannot diverge in silence.
Measured AVR cost: RAM +32 B, Flash +310 B. `WORKPLAN.md` holds the breakdown of the rest, lots 10 to 14.
### 4.1 Persisted preferences of the original — to take over (found on 2026-08-22)
The settings screen of the original firmware (`Interactions.ino`, `displayScreen == 2`) exposes **three** preferences, and `saveState()` stores all three in the EEPROM. FlexSeq must take them over instead of freezing them. Without that, a module whose owner changed one of them behaves differently under FlexSeq:
- **`rotateScreen`** — the logic is inverted: `false` gives `U8G2_R2`, and `true` gives `U8G2_R0`. FlexSeq writes `U8G2_R2` **hardcoded**. This is compatible while the preference is `false`. A module that switched it would show FlexSeq upside down. **To expose in the UI (§12).**
- **`reverseEnc`** — the direction of rotation of the encoder. **This is not a defect of libGravity**: the direction depends on the wiring of the two pins. Each library exposes the setting (`Encoder::SetReverseDirection(bool)` on the libGravity side, `false` by default and never called). Their defaults are **opposite**: found on the module, FlexSeq decrements on a turn to the right where the original increments. FlexSeq must therefore call `SetReverseDirection(true)` **to behave like the original by default**, and it must expose the setting. Tracked: `docs/open-risks.md` line 21.
- **`calibrateCVs()`** — the "CV Calibration" above. It is necessary although the converter is digital. The input analogue stage puts 0 V at the jack away from the middle of the scale. The divider, the offset and the tolerances of the resistors cause that. Measured on the module on 2026-08-22: FlexSeq reads **-27 / -28** at rest with nothing connected, so about 0.27 V of systematic error, and it agrees between the two inputs. The original stores one `uint16` per input. libGravity models it another way (`low` / `high` / `offset`), so the two formats do not transpose directly.
**EEPROM layout of the original, established on 2026-08-22** by a read of `saveState()` and verified on a real dump: `bpm` (1 B, address 0) · `bpmModulationChannel` (1) · `bpmModulationRange` (1) · `masterClockMode` (1) · `channels[6]` (a struct of 9 bytes, so 54, address 4) · `seqA1`…`seqB8` (16 × `bool[16]` = 256, address 58) · `CV1Calibration` and `CV2Calibration` (a `uint16` each, addresses 314 and 316) · `rotateScreen` (318) · `extClockPPQN` (319) · `reverseEnc` (320). **The address 1023 carries `memCode`**, which `loadState()` compares to `'D'` before it accepts the content. That byte is the version guard of the original, and it serves as a **proof of decoding**: a layout rebuilt wrongly does not make it come out right. This layout makes the 16 patterns of the original **importable** if §11 decides so. That would be a product decision, and nobody has taken it.
---
## 5. Trigger Sequencer — the main evolution
### 5.0 REVIEW OF THE REFERENCE VERSION — DECIDED on 2026-08-23
> **This section supersedes five earlier decisions.** They stay written where they were, and they are marked, so that the reasoning stays readable.
**The behavioural reference is `main` @ `40d4aac`** (2026-03-10, "clean up and going public"), the public firmware. The branch **`1.2-dev` @ `f7b2150acf`** (2025-03-11) is **older and never merged**. It serves as a **catalogue of candidate features**, with no normative value. `docs/original-1.2-dev-features.md` holds the inventory, verified line by line.
**1. Patterns: a template / instance model.** The 16 patterns are **templates stored in the EEPROM**. A channel in `SEQ` loads a template, and it then works on a **local copy in RAM**. To edit that copy affects neither the template nor the other channels. To load the template again overwrites the copy.
- **A1–A8 are frozen**: factory content, and the rule `index < 8` refuses an edit, with no extra byte.
- **B1–B8 are free**: empty at the start, and editable.
- **The factory content is the content of the original, and it is in place since 2026-08-24.** The eight patterns come from `Gravity.ino:83-90`: content on the steps 0 to 15, and silence above. They live in a PROGMEM table of eight 16-bit masks. The firmware seeds them when the persistence finds no valid image, which means a first start or an unknown format. Lot B will seed the EEPROM templates from the same table. **Without that content, the rule `index < 8` froze eight empty slots**: it removed half of the bank and it gave nothing. Measured cost: RAM +0 B, because the table is in PROGMEM and the bank stays in `.bss`, and Flash +78 B. Three independent derivations verify it: literal step lists in the assertions, masks computed by hand, and then a decoding of the table compared to the `.ino`. 8/8 identical, and B1–B8 confirmed empty. `setLowStepMask()` writes the masks, in one block over the first 16 steps: 16 calls to `writeStep` per pattern cost 104 bytes of Flash more. Measured on 2026-08-24. The TypeScript mirror carries the same surface, so that the two languages keep the same contract.
- **Memory consequence, with the figures of 2026-08-26.** The resident bank of 16 patterns leaves the RAM. It weighs **368 bytes** (16 × 23), and six instances weigh **138** (6 × 23), so the arithmetic gives **230 bytes of theoretical difference**. ⚠️ **230 is the theoretical net of the coexistence, and not the measured gain of the removal lot.** Measured on 2026-08-30: B4b.7 returns **370 B** against the firmware that entered it, at 1708 B. The 138 B of instances were already paid since B4b.3, and the pointer field `bank_` adds 2 B. ⚠️ **The figure of 230 is not a measurement of freed RAM.** The criterion stays the **AVR measurement**, and that measurement was not possible yet: the version 3 codec was declared and not called until 2026-08-28, so the linker removed it entirely from the binary. The RAM and Flash drift measured **0 / 0** on 2026-08-26. **The real figure was read at the activation, on 2026-08-28: Flash +2 bytes.** The linker dropped the version 2 image in exchange: `avr-nm` finds `PersistentImageV3` and no `PersistentImage::` symbol any more. The firmware therefore carries **one** image implementation, which explains a net cost close to zero. On the EEPROM side the figures are exact and computable: **384 bytes of templates** and **138 bytes of instances**, so 522 bytes for the patterns inside an image of 588. See §11.1. ⚠️ **The values "~144 bytes returned" and "256 bytes consumed", written here until 2026-08-26, came from the 32-step version.** They also came from a model with no stored length.
**2. Pattern: 36 steps, and one nibble per ratchet.** 5 bytes of steps, 18 bytes of ratchets, `sizeof(Pattern) == 23`.
⚠️ **A distinction not to confuse, decided on 2026-08-23, with the figures updated on 2026-08-26.** The length is **not** in the `Pattern` structure in RAM, because the channel already carries its `effectiveLength`. It is in the **EEPROM record of the template**, which therefore holds **24 bytes**: 23 of content plus 1 of length. `sizeof(Pattern)` stays at **23**.
⚠️ **The values 21 and 20, written here until 2026-08-26, came from the 32-step version.** The distinction itself does not change: the length is a fact of **storage**, and not of content. That is also why the **instance record** does not carry it: 23 bytes, the content alone. The channel that plays the instance already carries its effective length.
The project preferred the nibble to a 3-bit field **after** the template model took the bank out of the RAM. Only 6 resident instances stay, instead of 16 patterns, so the 4 extra bytes per pattern cost 24 bytes of RAM out of 321 available. The compact field would have cost 50 to 150 bytes of Flash out of 534. **The Flash is the constraint, and not the RAM.** ADR 0007.
**3. LENGTH: per channel, and the template stores it.** To load a template gives the channel **the length recorded in the template**. The channel can then edit it, and that length belongs to the channel.
The **deduction** — the last non-empty step plus one — now serves **one case only**: an empty slot that receives content for the first time.
**Why the template stores it now.** The template did not store it, to save RAM, while the bank was resident. The templates live in the EEPROM, so a length costs **0 bytes of RAM** and 16 bytes of EEPROM. And without it, the recording feature of §5.0 point 10 **would lose the length at every round trip**: a channel of length 20 whose active steps stop at the 5th would come back at 5.
Three states of a step, and one stored bit:
<table header-row="true">
<tr>
<td>Render</td>
<td>State</td>
<td>Counts in LENGTH</td>
<td>Storage</td>
</tr>
<tr>
<td>a filled square</td>
<td>active</td>
<td>yes</td>
<td>bit at 1</td>
</tr>
<tr>
<td>a hollow square</td>
<td>inactive, a **rest**</td>
<td>yes</td>
<td>bit at 0</td>
</tr>
<tr>
<td>`  • `</td>
<td>absent from the pattern</td>
<td>**no**</td>
<td>nothing: it is `index >= length`</td>
</tr>
</table>
A `•` is always a **tail**, and never a hole in the middle.
**4. Channel modes: four, and not three.** `CLOCK`, `RAND`, `SEQ`, and **`GATE`** taken from `1.2-dev`. GATE holds the output high over a **percentage of the step**, instead of sending a fixed pulse. It is the only mode whose output is not a trigger of constant width.
**SWING is not a mode.** It is a **parameter of `SEQ`**: a delay of the steps of **odd** index, from **0 to 49 %** of the duration of the step. It is **capped by the real resolution** of the current step. `ratchetFitsStep()` already refuses the ratchets that cannot be played, in the same way. ⚠️ **A deliberate divergence**: the SWING mode of `1.2-dev` disappears, and its effect stays. FlexSeq applies that effect to the pattern and to the rate of the channel, and not to fixed sixteenth notes.
**Set aside: the 7th channel** of `1.2-dev`, which turns the clock output into a channel. FlexSeq keeps that output for the expander.
**5. Gestures.** `SHIFT` + `PLAY` **mutes the channel**, a feature of `1.2-dev`. **RECORDING moves to `SHIFT` + a short press**, which was free. This choice sets aside a long press on `PLAY`, whose handler runs **on the release**: a `PLAY` held for 750 ms would have armed RECORDING instead of starting the transport.
**6. Tab bar: nine tabs and one indicator.** `clock · 1 2 3 4 5 6 · PATTERNS · CONF`, and then the **transport indicator flush right**. The slots are 12.9 px instead of 16.
- the indicator is **fixed**: ▶ when running, ■ when stopped. No blinking, so that the main screen holds no element that varies with time. That is what lets it redraw almost never;
- ⚠️ the redraw trigger must watch the **run state**, and not only the revision counter. A **MIDI Start** goes through no gesture, and the indicator would lie;
- the glyph of **CONF becomes a 7×7 cogwheel**. The current filled square reads as a stop indicator, which the owner verified on the module. To leave it two slots away from a real indicator would bring the ambiguity back, side by side;
- the glyph of **PATTERNS is a small grid of steps**, two rows of three dots in 7×5 px.
**7. Tempo aligned on the original, and the pulse kept.** `MIN_TEMPO` moves from 30 to **20**, and `MAX_TEMPO` from 300 to **200**. The pulse **stays at 5 ms**.
The original has 20–200 **and** 12 ms. The project takes its range, and not its width, and the arithmetic justifies that. At 200 BPM in `x24`, a step lasts 12.5 ms, so a pulse of 12 ms would fill **96 %** of it. That is a square signal, and not a trigger. With a triplet at that rate, the sub-onsets are 2.8 ms apart, and 12 ms would cover all of them. The original can afford it: it runs at 24 PPQN and it has no ratchets.
A side gain, since the acceleration of the encoder disappeared: **180 detents** to cross the range instead of 270.
**8. A grid of 36 steps on the screen — IMPLEMENTED on 2026-08-30, lot F.** 3 rows of 12, all full, so 36 slots and no dead one. The column pitch and **the size of the glyphs do not change**, because legibility comes first. **The footer disappears from the EDIT screen**, as it does in the original. Row centres: **18, 36 and 54**, a spacing of 18 px unchanged, and the last pixel of the grid at **63**.
⚠️ **Two vertical levers had been identified to free the space**: to raise the title line, and to reduce the space between the glyph and its ratchet digit. **The chosen geometry needed neither of them**: the removal of the footer is enough. The title keeps its baseline at 7, so it keeps its single band, and `DIGIT_DY` stays at 5. The two levers stay available for a future need, and they do not describe the implementation.
**9. The PATTERNS tab.** The edit of the templates lives in its own tab, with an audition through **CHAN 1**. It **reuses `LEVEL_EDIT`**, so there is no fourth interface level, with another pattern source and another title.
**10. The copy goes both ways.** From the PATTERNS tab, an **occupied** slot **loads** into a channel, and an **empty** slot receives the pattern of a channel. It is the same screen and the same selection, so there is **no new gesture**. The budget of nine gestures is full.
- an **occupied** slot can be overwritten, after a confirmation on the screen (**decided on 2026-08-26**). "Empty" is computed: the 36 inactive cells;
- **A1–A8 refuse** the write, as they do everywhere else;
- the write holds 24 bytes, so about 82 ms spread by the persistence scheduler.
### 5.1 Banque de patterns partagée
⚠️ **This subsection stays in French, on purpose, and it is the only one.** Its nature is an open decision of the owner: the block is superseded by §5.0, it also holds current content, and its code fence carries a stale figure. To translate it would make a superseded block look maintained. The decision comes first, and the translation follows it.
> **Décision validée.** Il existe **une banque unique de 16 patterns partagés** (A1–A8, B1–B8). Chaque channel possède un **sélecteur** `selectedPattern` (0–15). ⚠️ **SUPERSÉDÉ le 2026-08-23, voir §5.0.** Le modèle devient **template / instance** : les 16 patterns sont des **templates stockés en EEPROM**, et chaque channel travaille sur une **copie locale en RAM**. Éditer le pattern d'un channel n'affecte donc **aucun** autre channel.
Ceci reprend le modèle du firmware Sitka original (16 tableaux partagés `seqA1..seqB8` + `channel.seqPattern`). Le modèle « 16 patterns privés par channel » est **abandonné** (divergeait de la référence et coûtait \~560 B de RAM).
**Contenu d'un Pattern :** **36** steps binaires + **un code de ratchet par step, un nibble chacun** (décidé le 2026-08-23, `sizeof(Pattern) == 23` octets depuis le 2026-08-26, ADR 0007). **Aucune longueur** dans le Pattern, et **aucune séparation de mesure** (aide de lecture par channel).
```javascript
CHANNEL_COUNT = 6
PATTERN_COUNT = 16     (banque partagée)
STEP_COUNT    = 32     (grille)
```
### 5.2 LENGTH — per channel
> **Decision validated.** The **LENGTH is an execution state per channel**, and not a property of the Pattern. Two channels that play the same pattern can hold different lengths, which gives a polyrhythm out of common content.
- `1 ≤ effectiveLength ≤ MAX_LENGTH` per channel — **`MAX_LENGTH` is 36 since lot SF3, 2026-08-30**, the capacity of the `Pattern`. ⚠️ **The storage capacity and the interface cap stay two distinct quantities, and they now hold the same value.** The two bounds converged at 36. They do not read each other, and the two entry points stay separate — see the amendment of ADR 0009. The length is **deduced from the template at the load**, from the last non-empty step. ⚠️ **That deduction now serves one case only: an EMPTY slot that receives content for the first time.** Everywhere else the length comes from the record of the template, which STORES it: 24 bytes, 23 of content plus 1 of length. See §5.0 point 3, which is the authority. The channel can then edit it.
- A CV modulation can produce a temporary `effectiveLength` **in both directions**, to shorten **or** to lengthen. The clamp holds it to `[1, MAX_LENGTH]`, so to `[1, 36]` since lot SF3. **The project decided this on 2026-08-20. It tied the bound to the interface cap on 2026-08-28, and it raised the cap to 36 on 2026-08-30.** The value set on the screen and persisted is the **base**, and the CV never overwrites it. See §10.
- To reduce the length **does not destroy** the content of the pattern.
- To change the length **does not reset** `masterPhase`, and it **does not make** the playhead jump. See §7.
### 5.3 Pattern selection
Each channel selects one of the 16 patterns. The CV can target the pattern selection, through the chosen mapping.
### 5.4 Reset
The **global Reset** belongs to the external clock input, see Transport §8, and **not** to a CV destination. The CV can however target a **Reset per channel** (**decided 2026-08-20**): see §10.
### 5.5 RECORDING — to design
> **A planned function, and not a priority — written on 2026-08-20.** The original Sitka firmware holds a RECORDING mode with a record in time, and FlexSeq will take it over. The detailed design, the controls and the screen, is still to do. The points below are **already settled**, and nobody has to debate them again.
**The reference, taken from `GravityFW`** (`src/Gravity/Interactions.ino`, `Gravity.ino`): the entry into the mode **forces the play** (`isPlaying = true`, because nobody records while stopped). SHIFT then holds a **double role**, an immediate live trigger of the output of the displayed channel *and* a write into the pattern. The quantisation moves the hit to the next step, through `recordToNextStep`.
**Settled points of the design:**
- **The quantisation grid is the step of the recorded channel**, and not a global grid of 1/16 as in the original. A FlexSeq step is a unit of time, and its duration is the `ticksPerStep` of the channel. The threshold sits at **2/3 of `stepTicks`**, a value the engine already caches at every boundary. It generalises to a step stretched by a TRIPLET.
- **The 2/3 – 1/3 split is kept.** The original, at its resolution of `PPQN 24`, puts the threshold at 4 pulses out of 6. It tolerates the **delay** widely, and it anticipates only over the last third. That matches the way people really play: at 120 BPM on a step of 125 ms, about 83 ms of delay are tolerated, against about 62 for a symmetric split.
- **The firmware writes 1 bits only.** No record of a ratchet, and that is set aside on purpose: it is impractical at a fast BPM.
- **The erase behaves as in the original**: a long press empties the pattern, and there is no live erase.
- **The live trigger fires ON THE PRESS**, and not on the release. The constraint: the `Button` callbacks of libGravity fire on the **release**, which they need in order to separate a short press from a long one. The firmware must therefore read the **state** of the button, and not its callback. The original works around it the same way, with a direct `digitalRead`.
- **Deferred persistence** (§11).
- **LENGTH is FROZEN during a RECORDING, and the Length CV with it — decided on 2026-08-30, lot LCV.** The length that is active at the start of the record is the length that structures the recorded pattern. To change it in the middle would move the steps already written. The CV **stays sampled**, because the acquisition chain under interrupt (§10.6) holds no state to freeze. But its offset **is not applied** while the RECORDING lasts. On the exit, the modulation resumes **at the next step boundary**, through the general rule of §10.3. ⚠️ **The RECORDING mode does not exist in the code**: this decision bounds its design, and it does not anticipate it. The lot that implements it will add the guard **at the single point where the CV applies**, and it will not create a second path.
**Still to design: the assignment of the physical controls.** The inventory at the pinned commit holds `shift_button` and `play_button`, each with a short press and a long press at 750 ms. It also holds `encoder`, with a rotation, a press, and a rotation during a press. The encoder has **no** long press: the upstream commit `5c0c34f` adds it, and we do not have it. Note also the audited anomaly of `Button`, which loses a release that happens inside the debounce window.
---
## 6. Musical Grid — status
### 6.1 SUBDIV — validated (the official libGravity convention, 96 PPQN)
SUBDIV **divides or multiplies the global BPM**. It is the **only** parameter that sets the speed of a step. It never consults the measure separation, which is purely graphical (§6.2). It is the rhythmic reference of the step, **per channel**. It aligns on libGravity (`firmware/Gravity/channel.h` — `CLOCK_MOD` / `CLOCK_MOD_PULSES`): the unit `/1` is the **quarter note**, 96 ticks. The display follows the original Gravity: `/N` is a **division** and it is slower, `xN` is a **multiplication** and it is faster.
Mapping, validated, tested, and it reproduces `CLOCK_MOD_PULSES` exactly:
`ticksPerStep = subdiv > 0 ? 96 × subdiv : 96 / |subdiv|`
- `/1` → 96 ticks, the quarter note, and the **default per channel** since 2026-08-17. It aligns the original Sitka channel, whose default `subDiv` is the unit · `/2` → 192 · … · `/128` → 12288.
- `x2` → 48 · `x4` → 24, which is 1/16 and the historical step of the sequencer, and no longer the default · `x8` → 12 · … · `x24` → 4.
Each channel holds its own SUBDIV. The exposed list follows the list of libGravity, 25 values, `x24 … x2`, `/1`, `/2 … /128`, and it **replaces** the historical Sitka list of 20 values.
⚠️ **THE ORDER of this list is normative, stated on 2026-08-22**, because §11.1 persists the **index** and not the value. The chosen order is the one written above, **from the fastest to the slowest**. `x24` sits at index 0, and `/1` sits at **index 8**. It is the order of the TypeScript reference model (`sim/src/domain/subdiv.ts`), and the C++ was aligned on it (`include/flexseq/Subdiv.h`, `src/domain/Subdiv.cpp`). It is **not** the order of the raw libGravity table (`CLOCK_MOD`), which is the reverse: from the slowest to the fastest, with the unit at index 16. The **set** of the 25 values is identical in both cases. Only the order differs, and the order is what counts as soon as an index goes into the EEPROM. The table is in `PROGMEM` on AVR: **0 B of RAM**.
### 6.1.1 When a SUBDIV change applies — DECIDED on 2026-08-23
> **Decision validated.** A change of SUBDIV takes effect **on the next beat** while the transport plays. When the transport is stopped, or when the master phase already sits on a beat, it takes effect **immediately**. The **chosen** value is visible at once: the screen and the EEPROM carry the choice without a wait.
**The defect found on the module.** Two channels set to `/1` no longer fell together after one of them went through other rates and came back to `/1`. Measured on the reference model, after a return to `/1`: **0 ticks** through a division, **48 ticks** through `x2` or `x4`, and **64 ticks** through `x3`. At 120 BPM, 48 ticks make 250 ms and 64 ticks make 333 ms. The shift is audible and permanent.
**The cause.** A division lasts a multiple of 96 ticks, so its onsets stay on the grid of the quarter note. A multiplication lasts `96 / n` ticks and it puts its onsets between the beats. FlexSeq applied the rate at once, in the middle of a step, and it kept the accumulator. It folded that accumulator only when it went above the new duration. Nothing ever derived the phase from `masterPhase` again, so a phase off the grid survived without end.
**What the original firmware does**, found in two places that answer each other: `Interactions.ino:141` and `:275` apply the rate at once only while the transport is stopped (`if (!isPlaying) { calculateCycles(); }`). `Gravity.ino:454` puts it on the beat, under `if (pulseCount == 0)`. The comment of the author says it: *switching modes on the beat and resetting channel clock*. The original defines `PPQN 24` (`Gravity.ino:13`), so `pulseCount` counts 0 to **23** per quarter note, and that test **is** the beat. FlexSeq runs at 96 PPQN, so its own beat is `masterPhase % 96 == 0`.
**What FlexSeq does more, on purpose.** The alignment is **exact**: at the moment of the apply, the phase is derived from the beat again. The original puts its counter back on the pulse *after* the beat, and it keeps 1 tick of residual shift. That is 5.2 ms at 120 BPM. The re-derivation also makes the fix robust to draining: `advance()` can receive several ticks at once when the loop was blocked, so a beat can be crossed without being observed exactly.
**The consequence is accepted**: the rate takes effect up to one beat later, so 500 ms at 120 BPM. It is the behaviour of the original, and §1 keeps it.
**The alignment is on the beat, and not on the global origin.** Two channels set to the same division on different beats can still differ by a whole beat. The original does not realign a division either, so this is faithful. It is also much cheaper: an alignment on the origin needs `masterPhase % stepTicks`, a 32-bit modulo.
**Scope.** The same rule covers `setTicksPerStep()`. A global reset applies a pending rate instead of losing it. **Cost: 12 B of RAM**, 2 per channel, and **278 B of Flash**.
⚠️ **Five other paths were measured before they were asserted, and none of them shifts a channel**: an edit of LENGTH · the selection of another pattern · a change of mode · the edit of a ratchet on the current step · a round trip of `setTicksPerStep`. **The triplet does not shift one either**, against a first reading that suspected it: a triplet step lasts exactly twice `ticksPerStep` on the 25 rates, so it stays on the sub-grid of the channel, which divides 96.
The mechanism and the alternatives set aside live in **ADR 0004**. 14 assertions per language, and 10 mutants.
### 6.2 Measure separation — GRAPHICAL only
> **A complete revision (2026-08-17). It replaces METER / MEASURES entirely**, and those are **removed** from the domain. The rhythmic signature, the numerator and the denominator, `ticksPerMeasure`, the derived MEASURES and the compound beats: the whole module goes.
>
> **Principle: A STEP IS A UNIT OF TIME**, whatever "note value" a reader gives it. The BPM and the **SUBDIV** of the channel do the temporal work alone. The measure separation is therefore only a **reading aid**: it has **no** effect on the duration of the steps, and none on the subdivision.
>
> **Parameter:** one bar every **N steps**, `N ∈ {none, 2, 3, 4, 6}`, **per channel**, with the default `4`. Only these values are allowed, because they divide a row of 12 with no remainder. A bar therefore never falls across a line break. The labels appear in musical form (`2/4 · 3/4 · 4/4 · 6/4`).
>
> **Render:** a thin vertical line in the gutter between two columns, never at the edge of a row, and it **adds no position**.
### 6.3 RATCHETS — they replace the triplet groups
> **A complete revision (2026-08-17), implemented, tested and measured.** The model "a triplet group is 3 consecutive steps", with a start at 21 or below and no overlap, is **abandoned**. It made **3 grid positions** hold **1 unit of time**, which made the grid irregular in time.
>
> **A ratchet is a property of ONE step**, so it is content of the pattern, and therefore shared. The step keeps its position and its unit of time. The ratchet says **how many triggers** it emits:
>
> - `2 · 3 · 4 · 6` — N triggers evenly spaced **inside the duration of the step**. The duration does **not change**, and the step "plays faster". A **digit under the step** shows them.
> - `TRIPLET` (▲) — **3 triggers over TWO units**, because "a triplet of quarter notes is worth a half note". The step **lasts twice as long**, and it **shifts the rest of the pattern**. It is the only code that stretches the time. A **filled triangle** in place of the disc shows it.
⚠️ **THE MODEL IS RECONFIRMED ON 2026-08-23, on its own examples.** The owner gave the musical rule again — a triplet plays 3 notes in the time normally taken by 2 — and then illustrated it. A pattern of 6 steps whose 4th carries a triplet **is worth 7 steps**. Brought back to 4 steps, it **is worth 5 steps**. That is exactly what the code computes (`ratchetSpan(TRIOLET) = 2`, `ratchetTriggers(TRIOLET) = 3`, and the duration of the cycle is the sum of the `span`). The model above is therefore **not** revised, and **the drift of one unit per cycle is confirmed as accepted**.
**A triplet on an INACTIVE step keeps its two units and emits nothing**: it is a **triplet rest**. A ratchet `2/3/4/6` on an inactive step has no effect at all, because its duration is one unit. In both cases the pattern **keeps** the code, and it becomes active again with the step.
### 6.3.1 Placement of the sub-onsets — DECIDED on 2026-08-23
A ratchet N cuts **one** step into N slots, and the triplet cuts **two** steps into 3. At 96 PPQN a step is worth `ticksPerStep` (§6.1): **4 ticks at `x24`**, 96 at `/1`, and 12288 at `/128`. The slots therefore do not always fall on a whole tick.
**Three decisions, taken on figures.**
1. **The position of a sub-onset comes from `(stepTicks x k) / triggers`**, and not from `slotTicks x k` with a `slotTicks` already truncated. The first form keeps the error **under one tick** at every rank. The second form **multiplies** it by the rank. A measured example: R6 at `x6`, a step of 16 ticks. The truncated form puts the six notes at 0-2-4-6-8-10, and it **leaves the ticks 11 to 15 empty**. The correct form gives 0-2-5-8-10-13.
2. **A slot must be worth at least TWO ticks.** That is the floor the owner chose. At 120 BPM, two ticks are worth 10.4 ms against a pulse of 5 ms, so two notes stay clearly distinct up to 240 BPM. The floor is expressed **in ticks and not in milliseconds**: the tick follows the tempo and the pulse does not, and a ratchet must not disappear because the tempo goes up.
3. **A ratchet that the rate of the channel makes impossible cannot be selected**: the list of choices drops it. To signal it on the screen was **set aside** as too expensive.
**Six combinations are refused by the floor**, and all of them sit at the three fastest rates: `x24` with R3, R4 and R6 · `x16` with R4 and R6 · `x12` with R6. From `x6` upward everything passes, because the smallest slot there is worth 2.67 ticks. **The triplet passes everywhere**, because its shortest slot is 2.67 ticks at `x24`.
**One combination alone is impossible by arithmetic, and not by the floor: R6 at `x24`.** Four ticks cannot carry six distinct instants, and no formula creates ticks.
⚠️ **THE DORMANT RATCHET — a consequence of the shared bank, settled on 2026-08-23.** The ratchet is **content**, so it is shared, and the rate is an **execution state per channel**. The same pattern seen from a channel in `/1` and from a channel in `x24` therefore does not hold the same legality. To refuse at the input does not make the state unreachable. When the rate of a channel makes an already placed ratchet impossible:
- the pattern **keeps** it;
- the screen **does not show** it on that channel;
- the engine emits **one** trigger;
- a return to a slower rate **makes it visible and playable** again.
It is the same rule as the ratchet of an inactive step: **dormant, and never lost**.
> ⚠️ **The pattern grows longer — THIS IS THE WANTED BEHAVIOUR, and not a side effect.** A `▲` **extends the total duration of the pattern by one unit** on purpose. The grid of 24 positions is therefore no longer strictly regular in time. A channel that carries a `▲` falls behind the others at every turn. That is exactly the tool that lets a user "slow the rhythm down artificially". **Do not "correct" this shift**: it is an explicit decision of the owner of the PRD (2026-08-19), confirmed after somebody reported the consequence. Only a global reset realigns the channels.
> **No constraint** on the position and none on the overlap: any step can carry a ratchet.
> **Ratchet 5 — SET ASIDE (decided 2026-08-20).** A measured fact: at 96 PPQN (= 2⁵ × 3) no factor 5 exists. A fifth is exact on **2 of the 25** SUBDIV values (`/5`, `/10`), and everywhere else it would drift. The ratchets 2/3/4/6 are exact on 21 to 25 of the 25 values. The owner of the PRD settled it on that basis: **the ratchet 5 is not exposed**, so the current implementation conforms.
> **A documented fallback:** if a sub-slot does not fall on a whole tick, the firmware **ignores** the ratchet for that combination. That happens at the very fast SUBDIV values `x24`, `x16`, `x12`, `x6` and `x3`. Never a fractional tick, and never a drift.
> **Engine:** the `SequencerEngine` receives the shared bank (`setPatternBank`). At every step boundary it caches the duration (`stepTicks`) and the spacing of the sub-onsets (`slotTicks`), so there is **no division in the critical path**. A previous attempt taught that lesson: a 16-bit division per tick had shifted the simavr signal from 40 to 43 ms. `onsetCount(ch)` exposes the number of triggers of the last `advance()`. `masterPhase` stays unchanged.
> **An edit while the transport plays — KEPT (confirmed 2026-08-20).** `refreshTiming()` reads the ratchet of the current step again after a change of the content. Without it the edit would take effect only on the next pass. The project questioned the ability to edit while the transport plays, and then **kept** it: the original Sitka firmware implements it in **two** ways, a toggle of the step under the cursor, and the RECORDING mode (§5.5). To remove it would therefore be a regression against the reference. `refreshTiming()` also applies an edit made while stopped, before a restart.
> **Verified on a real playback** (the simulator, 240 BPM, SUBDIV `/1`): normal steps **250 ms** · a **▲ triplet step: 511 ms**, which is exactly 2 units.
---
## 7. Temporal model — `masterPhase`
> **Validated decisions**, and they were "to validate":
- **Unit:** a monotonic counter of **ticks at 96 PPQN**, the internal resolution of libGravity, and the historical SUBDIV values are exact there. The 1/16 is **not** frozen: each channel divides by its own `ticksPerStep`.
- **Representation: `uint32`.** It overflows after about 221 days at 140 BPM, so no normalisation is necessary. The limit is documented.
- **Independence:** `masterPhase` is independent of the LENGTH, of the pattern and of the local phase. `stop()` keeps the phase. The global reset puts it back to 0 and it realigns every channel.
- **Projection to `effectiveStep`, the smoothed local phase:** each channel holds a local position, `localStep` plus an accumulator of ticks. A change of LENGTH **does not make** the playhead jump: `localStep` is kept, and it is folded (`% newLength`) only when it falls outside the new length. That is the role of the `local phase` and of `phaseOffset`. An accepted consequence: `effectiveStep` depends on the history of the LENGTH changes, and a global reset realigns it to 0.
- ⚠️ **ONE POSITION PER CHANNEL: this is an accepted divergence, written on 2026-08-25.** The original firmware holds **one** current step only, `currentStep`, global and shared by the six channels, and it wraps at 15 (`Gravity.ino:99` and `446-450`). Its six channels in `SEQ` are therefore **locked together**, and they cannot drift apart. FlexSeq gives each channel its own `localStep`, which the LENGTH and the SUBDIV per channel **require**: two channels of different lengths cannot share one position. The musical consequence is real and wanted. Patterns of different lengths go out of phase and cross again, and the original cannot do that. The **global reset** stays the point where everything realigns.
**Engine API, tested:** `start()`, `stop()`, `reset()`, `advance(ticks=1)`, `setPatternBank(bank)`, `refreshTiming()`. Per channel: `selectedPattern`, `effectiveLength`, `subdiv` and `ticksPerStep`, `barLength`, `effectiveStep`, `hasStepped`, **`onsetCount`** (the triggers of the last `advance()`, ratchets included), `currentStepTicks`, `currentStepTriggers`.
---
## 8. Transport
The conversion from a *clock event* to a *progression* belongs to **Transport**. The source is unified: the **96 PPQN output** callback of libGravity, where the internal and the external clock both surface, gives 1 tick.
<table header-row="true">
<tr>
<td>Event</td>
<td>Transport</td>
<td>Effect on the engine</td>
</tr>
<tr>
<td>MIDI Start</td>
<td>`start()`</td>
<td>a global reset, and then run</td>
</tr>
<tr>
<td>MIDI Continue</td>
<td>`resume()`</td>
<td>run with no reset</td>
</tr>
<tr>
<td>MIDI Stop</td>
<td>`stop()`</td>
<td>stop with no reset, and the phase is kept</td>
</tr>
<tr>
<td>External Reset</td>
<td>`reset()`</td>
<td>a global reset, the phase goes to 0</td>
</tr>
<tr>
<td>MIDI / Ext Clock</td>
<td>`tick(n)`</td>
<td>`advance(n)`</td>
</tr>
</table>
**Implementation:** the ISR callback accumulates a `volatile` counter of ticks. The main loop drains it **atomically** into `Transport.tick(n)`, so only the main-loop context mutates the engine.
> **Deferred:** libGravity does not expose the separate hooks for MIDI Start, Continue and Stop, and none for External Reset, at the frozen commit. uClock handles start and stop internally. `resume`, `stop` and `reset` are implemented and tested, and nothing triggers them yet: only `start()` at the boot and `tick` are wired.
### 8.1 Wiring — VALIDATED on 2026-08-22
Four wirings, and one of them was never made:
<table header-row="true">
<tr>
<td>What we wire</td>
<td>How</td>
</tr>
<tr>
<td>The 96 PPQN output to the engine</td>
<td>`AttachIntHandler` — **already done**</td>
</tr>
<tr>
<td>The external clock input</td>
<td>`AttachExtHandler(onExtClock)`, and `onExtClock` calls `clock.Tick()`</td>
</tr>
<tr>
<td>Run and stop</td>
<td>a short press on PLAY (§12.1)</td>
</tr>
<tr>
<td>Tempo and source</td>
<td>the fields of the `◔` tab (§12.1)</td>
</tr>
</table>
⚠️ **`AttachExtHandler` attaches the interrupt on `EXT_PIN`, but the callback is OURS.** libGravity does not call `uClock.clockMe()` itself, which we read in `clock.h` on 2026-08-22. Without this wiring, **an external clock produces nothing**. It is the piece `main.cpp` never put in place, and neither the native tests nor the simulation show it.
**PLAY starts from zero — decided by the owner on 2026-08-22.** A press on PLAY after a stop calls `Transport::start()`, so a **global reset and then run**, and not `resume()`. Two reasons. That is what a sequencer of short patterns is expected to do. And it **gives a gesture to the realignment of the channels**, which would otherwise have none. A step in TRIPLET stretches the time, so the channels drift, and §6.3 says that only a global reset realigns them. `resume()` and `stop()` are not thrown away: they stay the entry point of MIDI Continue when the hooks are exposed.
**The clock source takes TWO FIELDS, `MODE` and `PPQN` — revised on 2026-08-23.** The previous text exposed the six values of libGravity in a single field `SRC`. The owner settled on the separation of the original: `MODE` carries **INT / EXT / MIDI**, and `PPQN` appears **only in EXT**.
**`PPQN` exposes the FOUR rates of libGravity** — 24, 4, 2 and 1 — where the original offered two. It is an **accepted addition**, and it removes nothing. A user of the original finds `24` and `4` where they expect them. The two others are capabilities the dependency already provides.
**`MODE` and `PPQN` are TWO VIEWS OF ONE BYTE**, the byte of the source. The grid of libGravity is exactly their product:
- `SOURCE_INTERNAL` → `MODE = INT`;
- `SOURCE_EXTERNAL_PPQN_24` → `MODE = EXT`, `PPQN = 24`;
- `SOURCE_EXTERNAL_PPQN_4` → `MODE = EXT`, `PPQN = 4`;
- `SOURCE_EXTERNAL_PPQN_2` → `MODE = EXT`, `PPQN = 2`;
- `SOURCE_EXTERNAL_PPQN_1` → `MODE = EXT`, `PPQN = 1`;
- `SOURCE_EXTERNAL_MIDI` → `MODE = MIDI`.
Two consequences, and they are consequences and not choices. **No EEPROM byte is added for `PPQN`**, because the source byte is enough. And **no incoherent state is representable**: to store the two separately would allow `MODE = INT` with `PPQN = 4`, which means nothing, while a derived view cannot lie.
⚠️ **`PPQN` NAMES THE INPUT, AND NEVER THE ENGINE.** The engine runs at **96 PPQN in every mode**, INT included: `clock.h:59` calls `setOutputPPQN(PPQN_96)` **once only**, and `SetSource()` never touches it. `SetSource()` calls `setInputPPQN` alone, and only for the external modes. The field therefore says how many pulses per quarter note the **received signal** carries, and uClock multiplies them up to 96:
- `PPQN = 24` → one received pulse is worth **4** internal ticks;
- `PPQN = 4` → **24** ticks;
- `PPQN = 2` → **48** ticks;
- `PPQN = 1` → **96** ticks.
In **INT** there is no signal to interpret: the field has no subject, so it is not "`PPQN = 96`". In **MIDI**, `clock.h:114` forces the input to 24, the standard of the MIDI clock, so the field has no subject either. That is exactly why the original shows `PPQN` in EXT only, and one field in MIDI.
⚠️ **Never pass `SOURCE_LAST` to `SetSource()`.** It is the sentinel that ends the enumeration, and the `switch` of libGravity does not handle it. This is an audited anomaly, §18. The field must therefore stop on the six valid values, and it must never wrap through the sentinel. `InputAdapter` absorbs it (ADR 0002).
**The tempo is bounded 30–300.** The API accepts 1 to 400. Below 30 the sequencer is no longer playable. Above 300 the fast SUBDIV values fall below one millisecond per step. The original stored the bpm on one byte, so these bounds stay compatible with its format (§4.1).
---
## 9. Trigger generation
> **Decision validated.** A channel emits a **trigger** when it crosses the onset of an **active step** of its selected pattern: `triggered(ch) = onset(ch) ∧ pattern[selectedPattern(ch)].step(effectiveStep(ch))`. The firmware turns that into a pulse on the output (`DigitalOutput.Trigger()`, 5 ms by default).
The chain is verified in **simavr**: `clock → Transport → SequencerEngine → TriggerSequencer → DigitalOutput → GPIO`, on VCD CH1, with a period that conforms to the test pattern.
---
## 10. CV
> **Settled and validated on 2026-08-20.** The contradiction found on 2026-08-17 is **resolved**: LENGTH, RESET and STEP are accepted as **FlexSeq extensions**. The Phase 2 design stays the reference for the *mechanism*: the recentring, the additive offset and then the clamp, the mutual exclusion of CV1 and CV2, and the global BPM modulation. It is **not** the reference for the list of destinations.
### 10.1 Mechanism — taken from Phase 2
The 2 inputs stay bipolar, ±5 V. `AnalogInput::Read()` returns a value **already recentred and calibrated** inside ±512 (`Voltage() = read / 512 × 5`): the domain **never** sees the raw value of the converter. The neutral point is calibrated (`SetCalibrationLow/High`, `SetOffset`, the "CV Calibration" of §4). The old wording "512 is the neutral" named the **raw** domain, where the measured zero is 538.
⚠️ **THE MUTUAL EXCLUSION IS LIFTED — decided by the owner on 2026-08-22.** A channel can route **CV1 AND CV2 at the same time**, each one to its own destination. Two modulations that target the **same** destination **add up and then clamp**, which is a direct generalisation of the "additive offset and then clamp" below.
**This is a FlexSeq ADDITION, and not a restitution, and the owner settled it knowing that.** The check was made in `Interactions.ino`: the original **imposes** the mutual exclusion. Its field `MOD` cycles over three states only, `OFF` · `CV1` · `CV2`. Its counter `channelCV` takes 0, 1 or 2 alone, and the default branch puts **both** targets back to zero. The two fields `CV1Target` and `CV2Target` of its structure are an implementation detail, and not two routings offered to the user. Accepted cost: +12 B of EEPROM, and +24 B of RAM.
⚠️ **NO modulation amount per channel exists in the original, against what its structure suggests.** `CV1Range` and `CV2Range` appear at the declaration only: never read, never written, and never displayed. The amplitude is hardcoded there (`map(randMod, 0, 1023, -5, +5)`). The format of §11.1 therefore needs **no** range byte, and §10.4 below — uniform zones over the full scale — loses nothing. The amount does exist in the original, but **at the global level of the tempo** (`bpmModulationRange`, 1 to 5, displayed ×10). The project keeps it as it is.
— *the previous wording, replaced on 2026-08-22:* Each channel routes **at most one** source out of `none / CV1 / CV2`, a mutual exclusion, as in Phase 2. The modulation is an **additive offset and then a clamp**, applied to a **base** set on the screen and persisted: the CV never overwrites that base.
The **global BPM modulation** of Phase 2 is kept as it is: global, and not per channel. It composes with the SUBDIV of the channel, so there is **no priority to settle**. The mention "and the priorities" of the old wording was a false problem.
### 10.2 Destinations — per channel
Destinations: `none / PATTERN / LENGTH / RESET / STEP`. The choice is **explicit**. Phase 2 deduced it from the **mode** of the channel, and that is no longer enough since LENGTH became an execution state per channel (§5.2): a channel in sequencer mode now holds **two** modulatable parameters instead of one.
- **PATTERN** — `selectedPattern = clamp(base + f(cv), 0, 15)`, at the step boundary.
- **LENGTH** — `effectiveLength = clamp(base + f(cv), 1, MAX_LENGTH)`, at the step boundary. `MAX_LENGTH` is **24 until lot SF3**, and then 36.
- **STEP** — a **read shift**: `readStep = (localStep + f(cv)) % effectiveLength`. The clock keeps driving `localStep`, and **nothing is mutated**, so a return of the CV to zero puts the read exactly where it would be. §9 stays intact.
- **RESET** — per channel: `localStep = 0` and the accumulator of ticks to 0. It applies **at once** (§10.3).
**The absolute position is set aside** for STEP (`localStep = quantize(cv)`). The channel would stop advancing on its own, the CV would become its clock, and §9 (`triggered = onset ∧ active step`) would collapse for want of an onset. That would be a **new channel mode**, and not a CV destination.
**The routing survives a change of mode**: it is kept, it is ignored while the mode does not suit it, and it applies again on the return. Phase 2 erased it because it was only **implicit** there. A setting placed explicitly does not disappear in silence.
### 10.3 When it applies
**At the step boundary, except for RESET.** Nothing changes in the middle of a step, because the current step would otherwise change its content, ratchet included, on the way. This lock also bounds the dependency on the history that §7 accepts: **at most one** fold of the playhead per step, instead of several hundred per second under a continuous CV.
The boundary of a **step**, rather than the end of a **pattern**, is a deliberate choice. "The end of the pattern" is not a shared moment: each channel holds its LENGTH and its SUBDIV, and a TRIPLET ratchet **stretches its time** (§6.3), so the ends of the loops drift independently. To wait for the end of the pattern would mean to wait for an unpredictable delay. That delay reaches `MAX_LENGTH` steps — 24 today, and 36 at lot SF3.
**RESET is the exception: it is immediate.** A reset that waits is not a reset. It also puts the step clock of the channel back in phase, which is the hard synchronisation people expect. The step 0 is however **armed**, and it comes out on the **next onset**, and never in the instant: to fire at once would send a burst of triggers on a jittery input, and it would break §9.
### 10.4 Quantisation (PATTERN, LENGTH, STEP)
Uniform zones over the range, with a **hysteresis** of about a quarter of a zone: a value must cross a boundary clearly to change, and it must come back as far to change again. That is the idiom of the Eurorack quantisers. For PATTERN, ±5 V covers a shift of −15 to +15, so 31 zones over 1024 steps. That is about **33 steps (~0.32 V) per zone**, well above the noise of the converter. Combined with the lock per step, the jitter cannot produce more than one change per step.
A consequence of the "base + offset" model: at the base A1, which is 0, the negative half of the CV has **no** effect, because the clamp crushes it. The project therefore puts the base **in the middle of the bank**, to hold both directions.
### 10.5 RESET — edge detection
`AnalogInput::IsRisingEdge()` **must not be used**. `old_read_` is declared `uint16_t` there while `read_` is `int16_t`: a negative previous value becomes a large positive one. So "it was high" is true **every time it was in fact negative**, and a crossing from negative to positive produces **no** edge. That is the most natural case on a bipolar input. The anomaly is audited, `test_analog_input` reproduces it, and it is **not fixed upstream** — verified on 2026-08-20: `analog_input.h` is identical 3 commits after the pinned commit.
FlexSeq therefore implements its own detection:
- **a Schmitt threshold**: it arms above **+1 V** and it re-arms below **+0.5 V**, so **+102** and **+51** in `Read()` units. +1 V is the usual design point of the Eurorack trigger inputs: above the noise, and below the 5 to 10 V that the modules send. The gap of 0.5 V **is** the hysteresis, so no debounce is necessary. The `GATE_THRESHOLD = 0` of libGravity is unusable as it is: it puts the threshold in the middle of the scale, where a signal that wanders makes the detection chatter;
- **the previous state bit is initialised explicitly** to "low", which removes the false edge at the start. `read_` and `old_read_` are not initialised in libGravity;
- **an edge only, and never a level**: a gate held high produces **one** reset, and not a stream;
- **a latch**: the event is held as soon as it is seen, and the engine then consumes it. That latch is what makes a short pulse survive a long pass of the loop. The pulse must be **seen**, and not seen at the right moment.
#### VERIFIED ON THE MODULE — 2026-08-22
`CvGate` is validated on hardware, in **both** regimes, through `env:bringup`:
- **Edge.** An external clock into CV1: one edge per pulse, and the rhythm **follows the tempo** across a change of 4:1, at 30, 60 and 120 BPM. No internal artefact can scale with the tempo of an external source. That is what makes the check conclusive, although the count was made by eye.
- **Level.** An output of the Gravity looped back into its own CV1: the input rises to 512 and falls back to 153, once every 6.4 s, dozens of times, and it produces **zero** edge. An edge and not a level, demonstrated with a signal whose shape we produce ourselves.
- **A cold start.** The counter reads 0 and it stays there: **no false edge at the initialisation**. The explicit initialisation of the previous state bit, placed against the anomaly of libGravity, holds on the hardware.
**The noise at rest is measured: −26 ± 3**, identical on the two inputs, with nothing connected. The arm threshold at +102 therefore keeps **128 points of margin**, so more than forty times the amplitude of the noise. The choice of +1 V is no longer a design principle: it is a figure.
### 10.6 Cost and dependencies
**Cost:** 2 bytes of state, one comparison per routed channel and per pass of the loop, and **no extra read of the converter**. `Gravity::Process()` already calls `cv1.Process()` and `cv2.Process()` at every pass. No division, and no new interrupt.
**A useful property:** several channels can route the **same** source to RESET. One pulse resets them all, which gives a nearly global reset with no extra mechanism. RESET also composes cleanly with STEP, which is only a shift at the read.
**SETTLED AND IMPLEMENTED on 2026-08-20: the CV is sampled UNDER INTERRUPT, and the guarantee is 1 ms.**
The measurement drove the decision, in two steps. The CV was read once per pass of the loop, and the worst pass goes above 15 ms with the OLED render active (§14). A shorter pulse could therefore pass unnoticed, while a Eurorack trigger usually lasts 1 to 10 ms. The first path tried was to reduce the cost of the drawing (§12): a real gain on the median, but the **worst** case hardly moves. The owner of the PRD therefore settled it: **the project guarantees 1 ms**, and the converter goes under interrupt.
**What that implies.** FlexSeq takes **ownership of the ADC**. `Gravity::Process()` calls `cv1.Process()` and `cv2.Process()`, so a blocking `analogRead`, and that is incompatible with conversions driven by an ISR. `main.cpp` no longer calls that function, and it calls its pieces instead, the buttons and the encoder. The outputs were already driven explicitly, so FlexSeq no longer depends on `Gravity::Process()` at all, and not on its uninitialised loop index either (§18). The calibration stays read on the `AnalogInput` objects of libGravity, which hold it.
**Rate.** A prescaler of 128 gives 13 × 128 cycles, so **104 µs** per conversion, with the two inputs in alternation. That is one input every **~208 µs**, so 4 to 5 samples inside a pulse of 1 ms. The **ISR restarts the conversions**, and they are not left free-running: to change `ADMUX` while free-running races with the start of the next conversion, and the sample could go to the wrong input. No timer is used, because uClock owns Timer1.
**Cost.** An ISR of about 57 cycles every 104 µs, so **5.6 % of CPU** on hardware. That fraction is *measured*, §14. The figure of 4.9 % published before 2026-08-21 came from a badly measured ISR rate. RAM **+24 B**, Flash **+354 B**.
**Verified:** `tools/run-cv-capture-probe.sh` — pulses of 1 ms injected under a real load, with the OLED render active: **27/27 seen, 0 missed**. The edge detection itself, `CvGate`, is a pure component, tested natively with 13 assertions.
---
## 11. Persistence
> **Impact of the new model, to design.**
- **Content:** 16 shared patterns, **15 bytes** each, 3 for the steps plus 12 for the ratchets in nibbles. The firmware saves them **once only**, so **240 B**, and not 6×16.
- **Per channel:** `selectedPattern` plus the **base of LENGTH** plus `subdiv` plus `barLength` plus the **CV routing**, the source and the destination (§10). The firmware saves the length **per channel**, and no longer "with the pattern". And it persists the **base**, and never the value the CV modulates.
- No dynamic allocation, and inside the RAM and Flash budget. The EEPROM format is **frozen on 2026-08-22 and implemented on 2026-08-23** — see 11.1.
### 11.1 Format — FROZEN on 2026-08-22
⚠️ **To overwrite the EEPROM of the original would be IRREVERSIBLE with our tools.** The optiboot bootloader can neither read nor write the EEPROM (§2). If FlexSeq wrote over the data of the original firmware, nobody could put that data back. It would take an ISP programmer, or a restoration firmware still to write. And §17 and §19 set the **restoration of the original firmware** as a constraint of the project: we know how to restore the binary, because the Flash backup exists, and we do not know how to restore its settings.
**Hence the decision, and it costs nothing: FlexSeq writes from the address 384.** The original firmware occupies **0 to 320** plus the byte **1023**, a layout established at §4.1. 702 bytes stay free, and FlexSeq asks for 304. The consequence: we can flash the original firmware again and **find its patterns, its calibration and its settings intact**, with no extra hardware.
**THE FORMAT IS AT VERSION 2 — designed on 2026-08-22, implemented on 2026-08-23 (lot 10).** The arrival of the three modes (§4.2) and of the second CV routing (§10.1) takes the record per channel from **6 to 9 bytes**: the 4 that existed, the pattern, the LENGTH, the SUBDIV index and the separation, plus **mode**, **offset**, **skip chance**, **CV1 target** and **CV2 target**. No range byte — see §10.1. The image goes from **286 to 304 bytes**, and the version byte from **1 to 2**. At 384 + 304 = 688, we stay far from the byte 1023 of the original.
⚠️ **THE FORMAT MOVES TO VERSION 3 — decided on 2026-08-23.** The conformity audit produced two decisions that touch the global zone (§16): the separation of `MODE` and `PPQN` comes back, and `RANGE` comes back. `MODE` and `PPQN` are **two views of the source byte**, so they cost nothing (§8.1). Only `MOD` and `RANGE` add themselves. The global zone goes from **3 to 5 bytes** — the tempo (2), the source (1), `MOD` (1) and `RANGE` (1). **The version byte goes from 2 to 3.**
⚠️ **The total of 306 bytes announced here is SUPERSEDED.** It came from before the 36-step foundation, and from before the template / instance model. The global zone of 5 bytes is kept as it is. **The size of the v3 image is fixed further down, at 588 bytes**, and that table is the authority.
**The two decisions share the same change of version**, and that is why the project takes them together: one return to the defaults instead of two.
**What the return to the defaults takes away**: the FlexSeq state written on the module since the first production flash. What it does not take away: the settings of the original firmware, below the address 320, which no write of FlexSeq reaches.
**This had to happen BEFORE the first production flash, and it is done.** A change of format starts from the defaults again and it loses the patterns created **inside FlexSeq**. Those of the original firmware, below the address 384, are not at risk. The production firmware never ran on the module, so no v1 image exists anywhere: the move from 1 to 2 lost nothing.
**Three facts established at the implementation.**
- **The two CV target bytes are reserved, and not live.** They return 0, and a stored value is ignored. That is exactly the reason to reserve them now: the CV routing (§10.2) will fill them **without a change of format**, so without a return to the defaults.
- **A byte out of range is refused, and never applied**, and the rest of the record loads all the same. A mode at 3, a skip chance at 99: the previous value stays, and the neighbouring record reads normally.
- **`resetToDefaults()` puts the three new fields back to their default.** Without that, a refused version byte would leave the previous mode in place.
**The offset holds on ONE byte, and the project keeps that limit as it is.** A decision of the owner, 2026-08-23. The original firmware declares `uint8_t offset` (`Gravity.ino:69`), so the domain stores a `uint8_t` too: the cap of 255 is the type. An accepted consequence, and **not a defect to fix**: above SUBDIV `/2` a step lasts more than 256 ticks. The offset then no longer reaches the end of the step, exactly as in the original.
**Measured AVR cost of lot 10:** RAM **−6 B**, because the offset moves to one byte over six channels, and Flash **+236 B**. The footprint is 1725 / 28774 B, the stack peak 207 B, and 410 B stay under the guard. 32 mutants, and 32 detected.
**304 bytes from the address 384 — version 2, in service since 2026-08-23:**
<table header-row="true">
<tr>
<td>Zone</td>
<td>Size</td>
<td>Content</td>
</tr>
<tr>
<td>Header</td>
<td>1 B</td>
<td>the version byte, checked before any load</td>
</tr>
<tr>
<td>Patterns</td>
<td>240 B</td>
<td>16 × 15 B, the shared bank</td>
</tr>
<tr>
<td>Per channel</td>
<td>54 B</td>
<td>6 × 9 B: the chosen pattern, the base LENGTH, the SUBDIV **index**, the separation, the mode, the offset, the skip chance, the CV1 target, the CV2 target</td>
</tr>
<tr>
<td>Global</td>
<td>3 B</td>
<td>the tempo on 2 B, the clock source on 1 B</td>
</tr>
<tr>
<td>Preferences</td>
<td>6 B</td>
<td>the screen rotation, the encoder direction, the calibration offset per input</td>
</tr>
</table>
**Three choices explained.** The **version byte** takes the principle of the `memCode` of the original: if the byte does not match, the firmware ignores the content and starts from the defaults. Without it, a change of format would read old bytes as if they were valid. **SUBDIV as an index and not as a value**: the list holds 25 values, and an index fits one byte where the raw value would ask for two, so 6 more. **The calibration as an offset alone**: libGravity models `low`, `high` and `offset`, so 6 bytes per input. But the default measured on the module is an offset, −26 on both inputs with a correct scale, so 2 bytes per input are enough. That is to confirm at §10 when the calibration is implemented.
**The quiet delay is 3 seconds** after the last change, decided by the owner on 2026-08-22. It is long enough not to write while somebody turns the encoder, and short enough that a power cut just after an edit loses nothing.
- **A deferred write (decided 2026-08-20).** The project keeps the semantics of the original: the `EEPROM.put` of Arduino **compares before it writes** (`update()` byte by byte), so it writes only the bytes that really changed. **Wear is therefore not the problem** — one byte per newly activated step, against 100 000 cycles per cell. The problem is the **time**: an EEPROM write on AVR takes about 3.4 ms, **and the loop waits during them**, plus the read-back for the comparison at every call. The firmware therefore writes **after a quiet delay** that follows the last change, and **never right after a musical event**. The original called `saveState()` on every recording hit, and it rewrote the whole state. That meant more than 300 bytes read back each time, and it put the block at the worst place.
**⚠️ FORMAT IN SERVICE — version 3, settled on 2026-08-26, ACTIVATED ON 2026-08-28 by the commit `815546b`.** The table above describes version 2, which no longer runs. `main.cpp` builds a `PersistentImageV3` and calls `bootstrap()`. The version byte actually written is **3**, at the address 384, observed on a simulated AVR and not deduced.
**There is no migration from version 2.** A valid v2 image is **refused** and the defaults are taken, which starts the FlexSeq state from zero once. The settings of the original firmware, below the address 320, are never touched.
**The image holds 588 physical bytes, and the periodic scan covers 204 logical bytes only.** Those are the header, the six instances, the channels, the global zone and the preferences. The **384 bytes of templates** stay OUT of the scan, and a gesture reads or writes them (ADR 0006). `PersistentImageV3::addressAt()` turns a logical index into an address, and it is the **only** place where that mapping lives.
**⚠️ The version holds the LAST logical index, 203, so it is the last write of a scan.** It validates the whole image, and not only its own byte: a power cut anywhere leaves a version that is not 3, and the next start begins again. Version 2 wrote it **first**, and that contract does not change for the v2 class, which the tests still exercise.
<table header-row="true">
<tr>
<td>Zone</td>
<td>Size</td>
<td>Offset</td>
<td>Address</td>
</tr>
<tr>
<td>Header — the version byte</td>
<td>1 B</td>
<td>0</td>
<td>384</td>
</tr>
<tr>
<td>**Templates** — 16 × **24 B**: 5 of steps, 18 of ratchets, **1 of length**</td>
<td>**384 B**</td>
<td>1</td>
<td>385 – 768</td>
</tr>
<tr>
<td>**Instances** — 6 × **23 B**, the pattern each channel plays, **with no length**</td>
<td>**138 B**</td>
<td>385</td>
<td>769 – 906</td>
</tr>
<tr>
<td>Per channel — 6 × 9 B, unchanged</td>
<td>54 B</td>
<td>523</td>
<td>907 – 960</td>
</tr>
<tr>
<td>**Global** — the tempo (2), the source (1), **`MOD` (1)**, **`RANGE` (1)**</td>
<td>**5 B**</td>
<td>577</td>
<td>961 – 965</td>
</tr>
<tr>
<td>Preferences — unchanged</td>
<td>6 B</td>
<td>582</td>
<td>966 – 971</td>
</tr>
<tr>
<td>**Total, from the address 384**</td>
<td>**588 B**</td>
<td></td>
<td>**384 – 971**</td>
</tr>
</table>
**51 bytes stay free, from 972 to 1022.** The address **1023** carries the `memCode` of the original firmware, and FlexSeq never writes it.
**`MOD` and `RANGE` are RESERVED, and that is a decision of format, and not an arithmetic consequence.** The owner decided it on 2026-08-26. The two bytes exist in version 3 from now on, and they are **inert**: the firmware neither reads them nor writes them while the BPM tab (§16) does not exist. The reason is explicit. The size of an image is part of the contract of the format. To reserve the place now avoids a later rework of the layout, and above all it **avoids a second return to the defaults**. It is the same choice as the two CV target bytes of the channel record, reserved since version 2 and still inert.
**The instances are persisted** (decided on 2026-08-26). A power cut therefore does not lose the work of the user. See ADR 0006 and its amendments.
**The content of a pattern occupies 23 bytes** — 5 of steps, and 18 of ratchets. The **bits 36 to 39** of the fifth byte belong to no step: they are **canonically zero**. The codec forces them to zero on the write, and it masks them on the read. See ADR 0007 and its amendment.
**The length of a template runs from 1 to 36.** The upper bound is the capacity of the `Pattern`, and **not** the interface cap `MAX_LENGTH`, which is 24 until lot SF3. On the write, a length out of range is **clamped**. On the load, it is **refused**, and the content already read is not lost.
**A requirement on an earlier format.** The behaviour of the firmware in front of an image of a different version must be **explicit, deterministic and tested**. It is not left to the chance of a partial load.
---
## 12. UI
### 12.1 Interaction model — VALIDATED on 2026-08-22
**The chosen principle is hybrid.** The project keeps the structure of the original, and it redefines the edit only where the new model requires it: 24 steps, ratchets per step, and LENGTH per channel. The reason: §4 asks to keep the historical features, and the original already solved the problem of three controls for many functions. A constraint the owner set: **minimal cognitive effort** — to create or edit a pattern must ask for the fewest gestures possible.
**The tab bar IS the navigation**, as in the original:
```javascript
────────────────────────────
◔  [1] 2 3 4 5 6            ■
```
`◔` is the clock tab and the global parameters · `1` to `6` are the six channels, and the active one is inverted · `⚙` is the **global configuration page** · `▶` and `■` are the **Play/Stop indicator, OUT of the navigation**.
⚠️ **NAVIGATION SETTLED ON 2026-08-23, on the module.** The bar carries **eight navigable tabs**: the clock, the six channels, and then the **global configuration**. **At the far right, and out of the navigation, sits the Play/Stop indicator.** The encoder never stops on the indicator.
**What was missing, and what the code does today.** FlexSeq draws eight navigable tabs, and the eighth is a `drawBox(cx - 2, cy - 2, 5, 5)`, so a **filled square of 5x5**. The owner read it as a Play/Stop indicator on the module: that is exactly what a stop indicator looks like, and the name in the code, `drawSettingsGlyph`, carried an intent the pixels did not. **Two** things are therefore missing: the cogwheel, and the indicator.
**The original, for reference, read in its drawing code.** Seven tabs — the glyph `w` and then the digits `1` to `6` — and a **separate** status glyph at `x = 121`: `t` when stopped, `r` when playing, and drawn **only while the clock is internal** (`masterClockMode == 0`).
**THE CLOCK TAB — layout settled on 2026-08-23.** The number of fields **changes with the mode**, as in the original, whose `lastMenuItem` holds 1, 2 or 3 depending on the state:
- **`INT`**: the main parameter is the **tempo**. The fields are `MOD`, and then `RANGE` **if `MOD` is not `OFF`**;
- **`EXT`**: the main parameter is `EXT`, written large in place of the number. The field is `PPQN`;
- **`MIDI`**: the main parameter is `MIDI`, and there is **no field**.
The reason `PPQN` disappears in INT and in MIDI is not cosmetic: in those two modes the field **has no subject**, because the input is absent, or the MIDI standard forces it to 24. See §8.1.
**THE GESTURES OF THE ENCODER — kept as they are, 2026-08-23.** Taken from the original card the owner supplied:
- **turn**: move through the current menu, or through the values of the selected parameter;
- **press**: enter the tab, or enter the edit of the value of the selected parameter;
- **long press** (about 1 s): go back;
- **SHIFT plus turn**: change the selected parameter quickly. **On the tab bar it changes the MAIN parameter of the tab.** **In EDIT PATTERN, on a step that is selected AND active, it changes the RATCHET of that step.**
⚠️ **THE RATCHETS MOVE ONTO A GESTURE OF THE ORIGINAL — decided on 2026-08-23.** The gesture invented for them, `press plus turn`, is **abandoned**. The ratchets and the triplet are now set by `SHIFT` plus turn, in EDIT, on an **active** step.
**This is not a saving of a gesture: it is a reading of the card.** The card says that `SHIFT` plus turn changes "the selected parameter". In EDIT, the selected parameter **is** the step under the cursor, and the ratchet is its value. FlexSeq therefore no longer needs a new gesture, and the rule of §1 no longer carries an exception: **the evolution of the SEQ mode happens with the gestures of the original**.
**Two consequences to settle at the audit, and not to discover afterwards.**
1. `SHIFT` plus turn in EDIT **changes the channel** today, a FlexSeq addition absent from the card. The combination is taken, so that gesture disappears or moves.
2. **The condition "active" does not exist in the code.** `adjustRatchet()` does not read the state of the step, and `toggleStep()` does not clear the ratchet when it deactivates a step. An inactive step therefore carries a ratchet in silence, and §11.1 persists it. What becomes of that ratchet — cleared at the deactivation, or kept and rendered with the step — is a **product decision** to take.
⚠️ **THE CHANNEL CHANGE INSIDE EDIT DISAPPEARS — decided on 2026-08-23.** `SHIFT` plus turn served there to change the channel, a FlexSeq addition absent from the original card, and the ratchets now take that combination. Nothing in the original firmware changes channel from the pattern editor, so this removal **restores** the navigation of the original. An accepted price: from EDIT, to change channel asks for a long press, a long press, a turn, a press and a press.
**THE SHORTCUT OF THE ORIGINAL TO THE SETTINGS IS KEPT — decided on 2026-08-23.** `Interactions.ino:84` opens the SETTINGS with **SHIFT plus a press longer than 2 s on the encoder**. That gesture stays, **on top of** the cogwheel tab: two paths to the same page, and one of them comes from the original.
**RETURN TO THE PREVIOUS PAGE — VALIDATED on 2026-08-23.** The long press keeps its meaning of "go up one level", and the rule below concerns the shortcut alone:
- the **shortcut** `SHIFT` plus 2 s stores the tab and the level it left — **one byte**;
- a long press on the settings page **returns there**, instead of falling onto the tab bar;
- to arrive at the settings **by turning** the bar stores nothing, and the long press goes up normally;
- the existing rule stays **first**: the long press closes an open field before anything else;
- to leave the settings **by turning** forgets the return point.
**One return point only is stored**, the one of the last shortcut. This is not a navigation history: the general history was **set aside**, because it costs RAM per level and it makes the "go back" gesture unpredictable. It would stop being "go up one level", which is what the card of the gestures announces.
⚠️ **One gesture is missing, and it explains an observation on the module.** `SHIFT plus turn` **does nothing on the tab bar**: `handleTabBar()` handles `EVENT_ROTATE` and `EVENT_PRESS` alone. The owner reported on 2026-08-23 that SHIFT seemed not to react, and on the bar it indeed does not. Line 31 of `docs/open-risks.md`.
**Two minor differences to check at the audit.** The long press of FlexSeq lasts **750 ms** where the card says about 1 s. Measure that in the original, and do not deduce it from the card. And two gestures of FlexSeq do not appear on the card: `SHIFT plus turn` in EDIT changes the **channel**, and `SHIFT plus a long press` in EDIT **clears the pattern**. If they are additions, this document must accept them.
**The navigation glyphs are the glyphs of the ORIGINAL.** A decision of the owner, 2026-08-23: the project takes the shapes of the original for the clock and for Play/Stop. And it **creates** a glyph for the global configuration, a **cogwheel**, because the original has no tab for that page. The original shapes are recoverable: `tools/decode-velvetscreen.py` decodes the `velvetscreen` font of the original firmware glyph by glyph. **The provenance must be kept** — the font is GPLv3, like FlexSeq itself, so the reuse is compatible and only the attribution is due.
⚠️ **FOUR SCREENS FROM 2026-08-22, AND NOT THREE.** The owner **reverses** the sentence below, "no separate CONFIG PATTERN screen exists": they want one. The count becomes **the main screen · EDIT PATTERN · CONFIG PATTERN · the settings**.
**The page of a channel takes the shape of the legacy**: one large parameter on the left with its label under it, and **three** lines on the right. That large parameter **changes its nature with the mode**, as in the original:
\| \| CLOCK \| RANDOM \| SEQ \|
\|---\|---\|---\|---\|
\| large plus label \| SUBDIV · `SUBDIVISION` \| skip · `SKIP CHANCE` \| `A1` · `PATTERN` \|
\| 1 \| `MODE:` \| `MODE:` \| `MODE:` \|
\| 2 \| `OFFSET:` \| `SUBDIV:` \| `EDIT` \|
\| 3 \| `MOD:` \| `MOD:` \| `CONFIG` \|
**`CONFIG` opens the fourth page**, which carries `LENGTH`, `SUBDIV` and `MOD`. A happy consequence: the SEQ page no longer exposes SUBDIV, which brings it **closer** to the original, which had none either.
**THE RENDER OF A STEP — validated on 2026-08-23.** Five cases, in this order of priority:
- **active plus triplet**: a **filled** triangle;
- **active, another ratchet**: a filled disc **and the digit** under the step;
- **inactive plus triplet**: an **empty** triangle, and it is the only glyph to create;
- **inactive, every other case**: a ring;
- **the digit is never drawn on an inactive step**, and never when the rate of the channel makes the ratchet impossible (§6.3.1).
**The asymmetry is wanted**: a triplet acts on the time even when it is off, so to hide it would mislead. A ratchet `2/3/4/6` that is off acts on nothing, so to show it would clutter. The screen then shows exactly what has an effect.
**A cost to plan**: `PatternScreenModel` does not carry the rate of the channel today. It needs **one more field** to apply the last rule.
**The measure separation goes down into EDIT PATTERN**: it is a reading aid of the grid, a FlexSeq addition the original does not have.
**The glyph on the right of the bar is `▶`**, as in the original, and not the `■` written further down.
**Our own glyphs draw the large pattern name**, as they already draw the ratchet digits. Ten are enough: `A`, `B` and `1` to `8`. About 500 B estimated against 2646 for `logisoso26`, and no GPLv3 data.
State: **designed, and not implemented**, except the v2 format, which is implemented since 2026-08-23 (§11.1, lot 10). The layouts of CLOCK and RANDOM, and the navigation **inside** CONFIG, are **assembled by Claude from the original**. They wait for a validation of the whole. `WORKPLAN.md` holds the breakdown, lots 11 to 14.
— *the previous wording, partly replaced:*
**Three screens**, exactly those of the original:
1. **The main screen** — the tab bar and the parameters of the active tab. **No** separate "CONFIG PATTERN" screen exists: the settings of a channel are the content of its tab.
2. **EDIT PATTERN** — the grid of 24 steps, reached from the entry `EDIT PATTERN` of the tab of a channel.
3. **The settings** — reached through the `■` tab. Deferred: the screen rotation, the encoder direction, and the CV calibration (§4.1).
**Two levels inside the main screen**, as in the original: you are either **on** the tab bar, or **inside** a tab.
**The eight gestures:**
<table header-row="true">
<tr>
<td>Gesture</td>
<td>On the bar</td>
<td>Inside a tab</td>
<td>Inside EDIT PATTERN</td>
</tr>
<tr>
<td>Turn</td>
<td>changes the tab</td>
<td>changes the field</td>
<td>moves the cursor</td>
</tr>
<tr>
<td>Short press on the encoder</td>
<td>enters the tab</td>
<td>opens the field, or enters EDIT</td>
<td>activates / deactivates the step</td>
</tr>
<tr>
<td>**Long press on the encoder**</td>
<td>—</td>
<td>returns to the bar</td>
<td>returns to the tab</td>
</tr>
<tr>
<td>Turn while pressing</td>
<td>—</td>
<td>—</td>
<td>sets the ratchet of the step under the cursor</td>
</tr>
<tr>
<td>SHIFT held plus turn</td>
<td>—</td>
<td>changes the value of the field</td>
<td>changes the channel</td>
</tr>
<tr>
<td>SHIFT long press</td>
<td>—</td>
<td>—</td>
<td>—</td>
<td>it empties the pattern, identical to the original, §5.5</td>
</tr>
<tr>
<td>PLAY short press</td>
<td>run / stop</td>
<td>run / stop</td>
<td>run / stop</td>
</tr>
</table>
A **short press on `SHIFT` stays free**: the project gives it no use rather than occupying it without a reason.
**A long press goes up ONE level, everywhere — validated on 2026-08-22.** The table above did not say what the gestures do while a field is **open**, and that is the heart of the state machine. The rule the owner settled is single, and it holds at the three levels:
- a field is open → the long press **closes the field** and it leaves you in the tab;
- a field is closed, inside a tab → the long press returns to the **tab bar**;
- inside EDIT PATTERN → the long press returns to **the tab**.
The short press does the opposite: it goes down one level, or it toggles the step in EDIT. One rule to learn instead of three, and that is the constraint of **minimal cognitive effort**. `SHIFT` held plus a turn stays a **shortcut**: it changes the value **without opening** the field.
**The positions wrap, and the values stop at the bounds — validated on 2026-08-22.** To wrap a tempo from 300 to 30 would be a musical accident. To refuse to advance at the last tab would be a dead end. So: the tab, the field cursor, the step cursor and the channel change **wrap**. The tempo, the source, the pattern, the LENGTH, the SUBDIV, the separation and the ratchet **clamp**.
⚠️ **The clamp is not redundant with the controls of the engine, and a mutation proved it.** libGravity **accelerates** a fast turn, ×3 under 16 ms and ×2 under 32 ms, so one detent is sometimes worth 3. Without a clamp, +3 from LENGTH 23 is **refused** by `SequencerEngine::setEffectiveLength` and the value does not move. With the clamp it lands on 24. Two mutations had survived the first pass for exactly that reason.
**The `■` tab is inert while its content is deferred.** A press on a tab that holds no field does **nothing**, rather than entering an empty level that would look broken.
**`selectedChannel` derives from the current tab, and nothing stores it.** The tab bar and the channel change inside EDIT therefore cannot contradict each other, and it is one byte less.
**Measured state (2026-08-22).** `UiController` exists in C++ and in TypeScript, with 35 assertions on each side, and **22 mutations out of 22 detected** on each side. The AVR cost was measured with a temporary call site in `main.cpp`: **RAM +26 B, Flash +1224 B**. Compare that to the 15 B that ADR 0002 estimated, and to the 2 to 4 kB that §15 estimates for the complete UI. Nothing calls it yet, so the delivered build stays at 1528 / 21404.
⚠️ **`SHIFT` held is read as a STATE, and not through a callback.** The callbacks of `Button` fire on the release, which is the condition for separating a short press from a long one. A held modifier therefore needs `On()`, which reads the pin. Verified on the module.
⚠️ **The long press of the encoder does not exist in the API of libGravity** at the pinned commit: `Encoder` exposes the short press, the rotation and the rotation during a press, and its internal `Button` is private. FlexSeq therefore needs **its own `Button`** on `ENCODER_SW_PIN`. The two do not get in each other's way: `Encoder` fires on `CHANGE_RELEASED` alone, and ours on `CHANGE_RELEASED_LONG` alone. The coexistence is **reasoned on the code, and not measured** — a native test and a check on the module are required. The fallback is to read the pin directly. See ADR 0002.
**Content of the tab of a channel:** the selected pattern · the **LENGTH** · the **SUBDIV**, displayed as the original Gravity does, `/N` for a division and `xN` for a multiplication · the **measure SEPARATION**, graphical, none/2/3/4/6 · the entry `EDIT PATTERN`. Later the CV source and destination (§10.2). **No global setting inside the tab of a channel**, so that no confusion is possible — a decision of the owner. No SPEED field, and no METER and no MEASURES, which are removed — see §6.2.
**Content of the `◔` tab:** the tempo, bounded **30–300**, and the clock source. See §8.
**LAYOUT SETTLED on 2026-08-22 — FIVE fields fit with no scrolling.** The question that stayed open at the breakdown is resolved by **two columns** under the large font: `LEN` and `SUB` on one row, `SEP` and `EDIT PATTERN` on the next. The large font **is itself the field 0**, so the 30 px it occupies are selectable and not decorative: the pattern name on the tab of a channel, and the **tempo** on the `◔` tab. No scrolling, so no extra gesture to learn.
Geometry: the tab bar at the **bottom**, eight cells of 16 px, and the active tab inverted · a rule at y 52 · the field rows at y 32 and y 40 · the large font centred, with the baseline at y 28. The cursor is a frame, and an open field is **inverted**. On the tab bar, no field cursor is drawn.
**Labels of the six clock sources, to confirm**: `INT`, `EXT24`, `EXT4`, `EXT2`, `EXT1`, `MIDI`. They match the enumeration of libGravity (`SOURCE_INTERNAL`, `SOURCE_EXTERNAL_PPQN_24/4/2/1`, `SOURCE_EXTERNAL_MIDI`), and `SOURCE_LAST` is never reachable (§8.1). The naming was chosen at the implementation, and the owner has not validated it yet.
**The fonts stay out of the renderer**: the model carries two font handles, so the component is pure and a test passes it sentinels instead of U8g2 tables. The width of the large string is carried like the width of the EDIT title, because `getStrWidth()` costs about 1 ms per call.
⚠️ **A `switch` costs RAM on avr-gcc, and that is measured a second time.** The compiler emits a `CSWTCH` table in `.data`, so in RAM, for a `switch` over small values. `PatternScreen.h` already documented the trap for its ratchet digits. Here it cost **10 B** for the source labels, and comparisons with `if` returned them against 12 B of Flash. The two choice tables of `UiController` were converted for the same reason.
⚠️ **THE LARGE FONT IS REMOVED — a decision of the owner on 2026-08-22.** It **REPLACES** the one taken a few hours earlier and reported below. The first arbitration rested on an **incomplete** measurement: it covered the main screen and the font, but **neither the persistence nor the wiring of the UI**. Together the whole measures **28242 B, 91.9 %**, above the guard. The pattern name and the tempo therefore appear in the same 5×7 font as the rest, and the renderer **never changes font again**: the two handles leave the model.
**Measured, everything wired: Flash 25510 / 30720 (83.0 %)**, so **2138 B** of margin, and **RAM 1639** with 409 B free against a reserve of 256. That is better than the 25596 B the arithmetic predicted. The removal of the font also removes the glyph decoding code it used inside U8g2.
**What is lost, and no embellishment**: the pattern name no longer reads at a glance from a distance, and that was exactly the object of the decision on `logisoso26`. The fallbacks stay costed below if the budget loosens, and line 24 of `docs/open-risks.md` would return 1536 B on its own if somebody verified it.
**The layout tightens** rather than leaving a hole where the 26 px glyphs were: the header goes to 10 px, and the two field rows move up to y 14 and y 22. What stays below is not empty space: it is **the place of the CV source and destination fields of §10.2**. A test asserts that exactly two more rows fit there, so those fields will not require a rework.
**A REPLACED decision, kept because its figures stay useful:** measured on the production build with **both** screens wired and then reverted: **RAM 1607 / 2048 (78.5 %)**, **Flash 26060 / 30720 (84.8 %)**, of which **2646 B** for the font itself. 1588 B stay before the refusal of the guard at 90 %. Three lots are still to build — 2 the adapters, 5 the transport, 6 the persistence — estimated at 1200 to 1500 B. It passes, **with no margin**.
✅ **FINAL STATE, measured on 2026-08-22 on the complete firmware** — two screens, eight gestures, the transport, the persistence, everything wired: **RAM 1699 / 2048 (83.0 %)** · **Flash 28050 / 30720 (91.3 %)** · a stack peak of **206 B**, so 80 % of the reserve of 256.
⚠️ **THESE FIGURES ARE STALE. Measured on 2026-08-25: RAM 1429 / 2048 (69.8 %) · Flash 27170 / 30720 (88.4 %) · the stack still 206 B · a margin of 413 B · 2014 B under the guard of 95 %.** Three changes of the same day, and the first weighs more than everything before it. **The transport of the screen**: the SSD1306 is write only, but Arduino Wire brought a bidirectional driver, under interrupt, and able to act as a slave. A polled, one-way TWI transport returned **1678 B of Flash and 216 B of RAM**, and the loop became **faster**. Measured: p90 8.80 then 6.50 ms, and one display band 4.88 then 3.36 ms. It needed one **additive** line in the fork, plus the `U8X8_NO_HW_I2C` switch of u8g2: without it the global Wire object stayed, because its constructor sits in `.init_array`, which the linker never removes. **The serial buffers**, 64 → 16 bytes each: **96 B of RAM**, and no cost in Flash. And `adjustFieldValue`, which stopped repeating its guard and its conversions: **94 B**. ⚠️ Those 94 B belong to this campaign of 2026-08-25, and they are **distinct** from the 126 B of lot S, measured on 2026-08-30.
**The Flash ceiling moves from 90 to 95 %**, decided by the owner on that figure. It is not a ceiling pushed back for comfort: the **real** limit is 30720 B, and 95 % still warns 1134 B before it. What the project accepts is a smaller reserve, and never a risk of bricking, because the linker would refuse long before. The **drift** guard stays at +512 B: this does not become a right to grow in silence.
**Two savings campaigns preceded the decision, and did not follow it**, and they returned **1378 B**: `-mcall-prologues` (534 B, with the cost measured on the four probes — the stack 161 → 175 B, and p90 8.52 → 8.96 ms) · the reduced glyph set `u8g2_font_5x7_tr` instead of `_tf` (808 B, and the panel proves the render identical: **863 pixels of ink before and after**) · a shared number formatter (36 B).
**Measured and set aside, so that nobody retries it**: the constructor of the engine is worth **88 B** out of the 1646 B of global constructor code. The ELF confirms it on 2026-08-30: it has **no symbol**, because LTO inlines it into `main`.
**⚠️ One sentence of this paragraph is CORRECTED on 2026-08-30, because a measurement refutes it.** It said: "**every** inlining lever returns zero or worse under `-Os` with LTO". What is established, and nothing more:
- the **global inlining settings of the compiler**, tried during the Flash campaign of 2026-08-25, produced no usable saving under `-Os` with LTO. **A measured fact**;
- a **targeted de-inlining** of three helpers of `UiController.cpp` produces a saving of **126 B** — **a measured fact**, 2026-08-30, ADR 0010;
- the two experiments act on **different mechanisms** — **a technical fact**. A global setting acts on the whole binary. `noinline` on a named function materialises **more** functions and still reduces the size, because the duplication of the inlined code cost more than the calls;
- the exact scope the original wording gave to "every" — **unknown**, and this text does not rebuild it.

**A global compiler setting and a targeted de-inlining of one function are two distinct strategies, and a result on one says nothing about the other.**
**An announced saving must rest on a measured size.** Before you announce or keep an estimate of a saving, identify the symbol in the ELF and measure its size with `avr-nm`. If the compiler inlines the code, the symbol does not exist and no saving belongs to it. A symbol size is not a saving on its own: a comparison of builds must demonstrate the gain. A refuted estimate stays refuted in the decision document.
**Also refuted by lot S**: a `PROGMEM` table of the bounds in place of the `switch` of `adjustFieldValue`. The bounds cost nothing, because each one is an immediate value in a clamp. And the seven cases are not uniform, so a table cannot carry them.
**Two large consumers are measured and NOT qualified**: `main` (**5976 B**) and `PagedScreen::renderFrom` (**2490 B**), read with `avr-nm` on 2026-08-30, so 8466 B together and 31 % of the Flash in use. These are **sizes, and not savings**: no analysis of their content exists, and no gain belongs to them. Tracked at line 61 of `docs/open-risks.md`.
⚠️ **The list that follows is the list of 2026-08-25, and it is STALE on one point**: the 574 B of `Wire` left with the whole path of its transport on 2026-08-25. What stays is either the pinned dependency, or the two renderers the interface needs. The dependency holds the uClock ISR at 974 B · Wire at 574 B · u8g2 at about 1500 B · **586 B of allocator** that uClock imposes by allocating four bytes at the init.
⚠️ **A consequence the owner accepts**: the Flash is measured at **every** remaining lot, and not only at the end. The fallback is one line of code, and the decision is then taken again on the real figure: `logisoso22_tr` returns 420 B (the same family, 22 px instead of 26), and `profont22_tr` returns 984 B (another family, and the look changes). The guard at 90 % is **not** lifted: to lift it would not create space, it would remove a warning. ⚠️ **That threshold of 90 % is HISTORICAL**: the owner raised it to **95 %** on 2026-08-22, on the real figure of the complete firmware. An unverified lever: `docs/open-risks.md` line 24.
**An end-to-end render check**: `run-screen-dump.sh` covers this screen, and it deduces which of the two it checks from the environment, because the criteria have nothing in common. Read in the memory of the panel: **8/8** tab cells at their place after `U8G2_R2` · the tab bar at panel y 0..7 · the large font at panel y 33..62 · the rule at y 11. Verified **red** by removing the large font: 379 px → 0.
**The footer of EDIT PATTERN.** The grid leaves the **band 7** free: the lowest pixel the second row draws is its ratchet digit at **y 47**, and the band 7 covers **y 56 to 63**. A line whose baseline sits at 63 fits there with 8 pixels of clearance. It will carry the channel and the tempo. **The header therefore does not change**: it stays `EDIT PATTERN A1`, 15 characters, and the decision of 2026-08-20 to keep it explicit is untouched. The band skip becomes **two-sided**, on the same geometric argument as at the top — see ADR 0001, amended.
**IMPLEMENTED AND MEASURED on 2026-08-22, and the skip saves TWO bands, and not one.** The criterion is "the band sits entirely below the lowest pixel the grid can place", and **two** of the eight bands answer it: the band of the footer (y 56–63) and the band that separates the grid from the footer (y 48–55), which carries nothing and never will. Both depend on the same footer chain, so both skip together. A routine frame sends **5 bands instead of 7**: the frame goes **44.2 → 32.1 ms**, the corrected p90 **8.46 ms** against a budget of 12 ms, and the median 6.82 ms. Cost **RAM +11 B, Flash +184 B**, with the drift acknowledged, and the stack peak re-measured at 160 B.
⚠️ **STALE SINCE 2026-08-30, lot F.** The footer left the EDIT screen, and the third row occupies the bands it freed. The skip loses its second path: `belowGrid` can no longer be true, because `GRID_BOTTOM_Y` is 63. **A routine frame sends 7 bands out of 8**, and the only skippable one is the band of the title. Measured: a routine frame **33.0 ms** · the p90 of the pass **6.61 ms** against a budget of 12 · the worst pass 13.99 ms on the full refresh. The paragraph above stays the state of 2026-08-22.
**The rotation check rests on three assertions, and all three discriminate — rebuilt on 2026-08-30.** The old criterion pinned the title at the bottom of the panel and the **footer** at the top. The footer left EDIT, and the grid now reaches the last logical line, so the top of the panel is no longer empty. The criterion is rebuilt on layout constants, with no literal coordinate in the harness:
**C1** — the line `rotY(HEADER_LINE_Y)` is the most inked line of the panel, and it carries at least `HEADER_LINE_W − 20` pixels. The **position** of that line is the discriminant. The threshold is a **guard on the integrity of the anchor**, and not what separates R2 from R0.
**C2** — the line `rotY(HEADER_LINE_Y − 1)` is empty.
**C3** — the title inks its band, and the centre of the last row inks its own.
All three fail under `ROTATION_MUTATE=1`, which reads the memory of the panel turned by 180°, so the image a firmware in `U8G2_R0` would have produced. The pinned dependency is untouched. Measured on 2026-08-30: nominally 36/36 steps and C1, C2 and C3 green. Under the mutation: 16/36 steps, C1 3 px instead of 120, C2 15 px instead of 0, and C3 the last row 0 px instead of 15.
⚠️ **The horizontal axis is not covered by these three assertions**: the geometry check covers it, and it tests the `colX` position of every step. Neither of the two covers both axes alone.
⚠️ **The footer left the EDIT screen on 2026-08-30**, as §7 point 8 requires. The paragraph that follows describes its state until that date, and it stays valid for the screens that keep theirs.
The footer was **aligned left** at x = 4, like the header rule: to centre it would have needed one more `getStrWidth()`, at about 1 ms per call on this MCU. **The caller supplied its content**, so the renderer stayed pure and knew neither the channel nor the tempo.
**What its removal takes off the EDIT screen**: the current channel and the tempo, which it showed as `CH1  120BPM`. That is the wanted conformity to the original, and not an accidental loss.
**A large font for the pattern name — decided on 2026-08-22, and the cost is MEASURED.** The original shows `A1` in a large font. FlexSeq does the same with `u8g2_font_logisoso26_tr`, and never with the font of the original, whose data is GPLv3 and set aside. Measured on a production build: **+2688 bytes of Flash**, and **0 bytes of RAM**. Of those, 2646 go to the table of the font, and 42 to the code U8g2 pulls in. The Flash goes to 24092 / 30720 (78.4 %), so **3556 bytes before the refusal of the guard at 90 %**. §15 estimates a complete UI at 2 to 4 kB. The upper bound would pass the threshold by about 450 bytes, which would need an explicit acknowledgement. **A deliberately reversible choice**: `profont22_tr` costs 1704 B and `logisoso22_tr` costs 2268 B, if the budget tightens. The digits-only variants, 387 to 548 B, are not enough: ten glyphs are necessary, `A`, `B` and `1` to `8`.
**EDIT PATTERN** — **36 positions in 3 rows of 12** since 2026-08-30, lot F, and 24 in 2 rows before that. **A complete revision (2026-08-17)**: the representation follows the Wokwi POC and the glyphs of the original firmware.
⚠️ **The grid, the capacity of the `Pattern` and the EXECUTION all hold 36 since lot SF3, 2026-08-30.** `screen::GRID_STEPS`, `UiController::STEP_COUNT` and `SequencerEngine::MAX_LENGTH` hold **36**, and **none of these three constants reads the others**: five mutants hold that independence, and each one brings one constant back to 24 while the others stay at 36.

⚠️ **The paragraph below describes the state BETWEEN lot F and lot SF3, and it is kept because it explains the separation.** From 2026-08-30, lot F, to 2026-08-30, lot SF3, `MAX_LENGTH` was **24** while the grid was 36. The steps 24 to 35 were then **visible, editable and persisted, but never played**: the screen drew them as single dots, the representation of a step beyond the length. Four mutants held that separation in both directions.

**The real execution of the steps 24 to 35 is MEASURED, and not only deduced from the domain tests.** `tools/run-trigger-probe.sh` runs with `LENGTH=36` and an active step at 26. It observes **20 gaps out of 20**, forming a cyclic rotation of the gaps of the pattern, on the pins of the simulated firmware. The wrap gap is `36 − 26 + 0 = 10`: an engine that still played 24 steps would never fire the step 26, and it would give another sequence.
>
> **The reference geometry is the Wokwi POC** `flexseq-oled-playground/sketch.ino` (SSD1306 128×64, rotation R2): a horizontal pitch of **10 px**, a **centred** grid, **5×5** glyphs identical to the original firmware, and a **9×9** selection frame. ⚠️ **The POC carried 24 steps in 2 rows of 12. FlexSeq carries 36 in 3 rows since lot F.** What stays borrowed from the POC is the horizontal axis and the shape of the glyphs, and not the number of rows.
>
> ⚙️ **Scope of the sketch — a precision (2026-08-19).** The sketch is the authority for the **geometry and the glyphs only**. Its display object (`U8G2_..._2_HW_I2C`, a buffer of 256 B) was an **experimental visual test** alone, and it is **not** normative.
>
> **An implementation constraint (a validated decision):** the firmware **reuses the object of libGravity**, `gravity.display`, of type `U8G2_SSD1306_128X64_NONAME_1_HW_I2C`. That is the **`_1_` mode**, with a buffer of **128 B already paid** in the measured footprint. **Never instantiate a second U8G2 object**: that would be +256 B of RAM for nothing. The project keeps what libGravity allows, and the `_1_` mode simply implies more `firstPage()/nextPage()` iterations. **RAM cost of the display object: 0 B more** — and do not read that as "the render costs nothing": the spread over 8 bands, below, costs 24 B of freeze.
>
> **Spacing by band (2026-08-20).** The renderer draws only the elements that fall inside the current band: it receives that band as a parameter (`Band{y0, y1}`, and the whole screen by default). Without that, the 24 steps, their digits and the title were computed **eight times**. U8g2 cuts what we send it, but the call happens all the same.
>
> ⚠️ **The band arrives in DISPLAY coordinates, and the renderer works in LOGICAL coordinates.** `U8G2_R2` turns the image by 180° *before* the cut, so `PagedScreen` applies the inverse. To omit that inversion gave each band the **inverse half** of what it displays, and the screen stayed almost blank. The defect lived one commit, and a read of the memory of the panel found it (§14). The native tests could not see it, because they supplied the band already in logical coordinates.
>
> **Measured gain, with a correct render on both sides** (§14): the median pass **8.52 → 6.48 ms**, and the whole frame **74.0 → 59.1 ms**. That is −24 % and −20 %, and not the factor of 2 announced first. The **worst** pass hardly moves, 16.16 → 15.32 ms: the **title** carried it, and the spacing does not make the title cheaper, because it does not avoid it. Cost: **+522 B of Flash, 0 B of RAM**. A test verifies the property: the union of the 8 bands renders **exactly** the complete image, pixel for pixel.
>
> **THE HEADER STAYS EXPLICIT — decided 2026-08-20.** `EDIT PATTERN A1` is kept whole. Its rasterisation cost 8.8 ms per frame (§14), and the owner of the PRD **set aside** two paths: to shorten the header, where 9 characters or fewer would have been enough to hold the budget, but at the price of the clarity. And to put the rendered band in a **RAM cache** of 128 B, refused because the RAM must stay available for features.
>
> **The skip of the unchanged band (2026-08-20).** The chosen solution costs **no RAM**: the title changes only when the selected pattern changes, so its band is neither cleared, nor drawn, nor sent while it is identical. The SSD1306 is a screen with memory, so a band that is not sent keeps showing what it showed. The condition is **geometric** — a band entirely above the header rule can hold the title only — so it is tied to the layout. **ADR 0001** details two design points: the clear-draw-send cycle is **indivisible**, and one frame in sixteen is rendered whole, as a net against a defect of our own logic. Gain: the routine pass **15.4 → 8.44 ms**, and the frame **56 → 44.2 ms**, for **+9 B of RAM and +160 B of Flash**. The baseline of the title moves from 8 to **7**, so that it fits inside a single band. Its width is measured once per frame only.
>
> **The spread render — ADR 0001 (2026-08-20).** These 8 iterations no longer follow each other inside one call. The page loop is now **manual**: `setBufferCurrTileRow` · `clearBuffer` · the drawing · `sendBuffer`. `firstPage()/nextPage()` does not allow a band to be skipped: **one band per pass** of the main loop, with the model **frozen** at the start of the frame. The freeze holds `PatternScreenModel` 8 B plus a copy of the `Pattern` 15 B, so **23 B**. A state flag brings it to **24 B**, measured at the build. The freeze is necessary, because the content is shared and editable while the transport plays — §6.3. The reason: the I2C bus runs at **400 kHz**, declared by the U8g2 descriptor of the SSD1306 and applied at every transfer. Neither libGravity nor FlexSeq sets it. A full frame is therefore about 25 ms of bus, and the loop is blocked during them. The ticks then pile up, and the onsets bunch together. See `docs/decisions/0001-boucle-principale-non-bloquante.md`.
>
> **Vertical spacing of the grid — SETTLED (2026-08-20):** `cy = 20 / 38`, and not the `22 / 35` of the Wokwi POC. The gap, 18 px instead of 13, is imposed by the ratchet digit that sits under the step. To go back to the POC would need digits 4 px high.
>
> **Legend (validated 2026-08-17):**
> - `○` a ring 5×5 — a step that is **not selected**
> - `●` a filled disc 5×5 — a **selected** step, and it is active
> - `▲` a filled triangle 5×5 — an active step in **TRIPLET**, so 3 triggers over 2 units, and the tempo "slows down"
> - `.` 1 pixel — a position **beyond LENGTH**
> - **a digit under the step** — the **ratchet** `2/3/4/6`, so N triggers inside the duration of the step
> - **a vertical bar** in the gutter — the **measure separation**, graphical only
> - **a 9×9 frame** — the step under **edit**
> - **the central pixel inverted** — the **played step**, white on an active step and black on an empty one
>
> No step number, and **no added position**. The previous render, a "note legend plus marks" with glyphs from the whole note to the demisemiquaver in the header, is **abandoned**: the note value is no longer a notion of the model. A step is a unit of time, and the SUBDIV makes the tempo.
### 12.9 To save a pattern — the `SAVE` flow, decided on 2026-08-26
The instance a channel edits lives in RAM, and the firmware persists it. `SAVE` exists to **publish** that instance into a template, so that the other channels can load it.
1. the edited instance is a **local copy** of the loaded template;
2. the firmware can save it into **any slot `B1` to `B8`**;
3. an **occupied** `B` slot is replaced **after a confirmation on the screen**;
4. `A1` to `A8` always refuse the write;
5. `SAVE` appears only when the instance has **changed**;
6. after a successful save, the channel **adopts the destination slot** as its new reference template, and the change flag falls back;
7. the choice of the destination **never** destroys the edited instance before the save.
The detailed design — the button in the header, the destination selector, the confirmation modal — belongs to lot E.
---
## 13. Software architecture and workflow
Three separate roles:
- **TypeScript** — the reference model, the tests, the scenarios, and the visual **Gravity Simulator**, for fast feedback.
- **C++ AVR** — the real firmware, the libGravity integration, PlatformIO, and the ATmega328P target.
- **simavr / avr8js** — they run the compiled AVR firmware, for a validation before the hardware.
The TypeScript model and the C++ **must not become two independent specifications**: the project keeps a behavioural parity across Pattern, PatternBank and SequencerEngine.
```javascript
Fast iteration:        TypeScript → tests → Gravity Simulator → feedback
Firmware validation:   C++ → PlatformIO → .elf → simavr/avr8js
Physical validation:   C++ → flash → Gravity (at the minimum)
```
The simulator reproduces the OLED screen, with the real `velvetscreen` font decoded, so that a reader can judge the final render with no hardware.
---
## 14. Tests and verification
Every piece of domain code has a unit test.
- **Native C++** (`pio test -e native`, with no hardware): the pure domain, with no Arduino and no libGravity. **It must be green**: a phase is not finished while one of these fails. ⚠️ **This entry no longer names the modules — decision D5 of the owner, 2026-08-31.** `platformio.ini` is the normative source of the inventory, through the `test_filter` of each environment, and an execution proves what it collected. This section carries the validation requirement, and not the inventory. It named 7 modules where the filter collected 19.
- **libGravity characterization** (`pio test -e native_libgravity`): it describes the **real** behaviour of the frozen dependency, anomalies included, so it is **partly red by construction** (§18). The criterion is the **conformity to the audit**, and not the absence of a failure.
- **TypeScript** (vitest): the same contracts mirrored, plus the simulator.
- **simavr**: the real AVR firmware → a VCD → assertions on the signals, CH1 for example. **It cannot show the screen**, because it checks GPIO signals and not an I2C render.
- **Wokwi** (a VS Code extension: `wokwi.toml` and `diagram.json` at the root): a **VISUAL validation of the OLED render**. Wokwi runs the **real firmware** that PlatformIO compiles (`pio run -e wokwi` → `src/wokwi_main.cpp`): the same domain, the same renderer, and the same `gravity.display` object. **No copy of the code**, so no drift is possible. `env:wokwi` is the real firmware plus a preloaded demonstration content, and `main.cpp` stays empty, on the same principle as `simavr_main.cpp` for the signals.
	**The SCHEMATIC handles the rotation**: `diagram.json` applies `"rotate": 180` to the screen, which models the physical mounting, an OLED upside down. The firmware therefore keeps its `U8G2_R2` **with no divergence at all**. The OLED sits above the board: turned, its pins go to the bottom and face the Nano, so the wires do not cross the screen.
	⚠️ **State of the verification, stated on 2026-08-20.** Three distinct things, and nobody must confuse them:
	· **On the firmware side: VERIFIED.** `tools/run-screen-dump.sh` reads the memory of the SSD1306 panel inside simavr. It **asserts** that the image there is turned by 180°, with the title at the bottom and the top of the panel empty. It also asserts that the 24 steps sit at their expected position after `U8G2_R2`. That is no longer a piece of reasoning but a test: a change of the rotation constant would turn it red.
	· **On the Wokwi side: still NOT verified.** Nobody observed how the `board-ssd1306` part handles `"rotate"`, and nobody observed the real routing of the wires. The assistant cannot run the VS Code extension. That point is however **no longer on the critical path**: the visual validation of the render no longer depends on Wokwi. If the screen stays inverted there, the fallback is `setDisplayRotation(U8G2_R0)` in `wokwi_main.cpp` **only**, and never in `main.cpp`.
	· **On the hardware side: not verifiable in simulation.** Whether the turned image lands upright depends on the physical mounting of the OLED. The PRD states that it is mounted upside down. No simulator can confirm that, and only the module will say — see `env:bringup`.
- **avr8js**: planned, to run the firmware inside the UI of the simulator.
- **The CV capture probe** (`tools/run-cv-capture-probe.sh`): it injects CV pulses of a given width, with the SSD1306 slave attached, and it checks that the firmware sees them all. The firmware is **not** instrumented: the harness reads and clears the latch in the simulated RAM, and it plays the consumer the loop will be. Result of 2026-08-20: **every pulse of 1 ms seen, 0 missed**, so 27/27 at the current setting. The injection period must go above the grace window of the harness. Without that, a pulse still waiting for its consumer is declared lost at the next injection. That had read as 42 % of capture for a latency of 116 ms. The consumption latency is measured and reported **separately** from the loss: the latch guarantees that the pulse is **seen**, and not that it is consumed fast.
	**Two reservations on the fidelity, and a wrong result diagnosed each one.** The firmware structure must carry `vcc`, `avcc` and `aref` at 5000 mV. They normally come from the `.mmcu` section, which a `.hex` does not have. Without them simavr keeps a reference of about 3.3 V, and the rest reads 813 instead of 537, so above the arm threshold. The gate then arms once and never re-arms. And the ADC of simavr is about 8× too fast. It schedules the end of a conversion after `prescale` cycles instead of 13 × `prescale`. The simulation therefore validates the **logic** — the edge, the latch, the re-arm, under load — and the **arithmetic** carries the guarantee to the hardware. The verdict checks both.
- **The blocking probe** (`tools/run-blocking-probe.sh`, `tools/simavr-ssd1306/`): it measures the real duration of a pass of the loop with a **real SSD1306 slave** on the I2C bus. It corrects the artefact of the ADC through a measurement in two regimes, and it prints the cost **per position in the frame**. That last figure is the one that says which band pays. It does not modify the firmware. The verdict tests the routine pass, the corrected p90, against `PASS_BUDGET_MS`, 12 ms by default: **green, at 8.44 ms**.
- **Musical function — the six outputs** (`tools/run-trigger-probe.sh`, 2026-08-21): the only check that answers "does the firmware emit the written pattern?". **No binary exercised that path.** `main.cpp` emits the triggers, but its bank is empty (`patterns{}`) and no UI writes into it. `wokwi_main.cpp` carries content with no `TriggerSequencer`. The firmware is not instrumented: the content is written into the simulated RAM, at the address `avr-nm` gives for `patternBank`, as the CV probe does with the latch. **The probe does not assume the phase of the playhead.** A first version that assumed it declared a correct firmware "off grid", because `transport.start()` happens in `setup()`. What holds without the phase is the **sequence of gaps between the pulses, up to a cyclic rotation**. That is also the only claim with a musical meaning. **Revised on 2026-08-23 (lot 10): TWO COURSES on the same firmware, one per channel mode.** The mode and the content of the pattern now arrive through a **preloaded EEPROM image** in the simulated machine. `tools/eeprom-image.cpp` builds it, and it links the code of the domain. The harness therefore holds no copy of the format, and it no longer writes into `patternBank`. The **CLOCK** course checks a regular train, one gap per step, with the pattern staying in the bank **and not played**. The **SEQ** course checks that the sequence of gaps is a cyclic rotation of the sequence of the pattern.
Measured on 2026-08-23, 20 s per course: **6/6 outputs** in both · **38/38 gaps** in CLOCK and **11/11** in SEQ · **499.96 to 499.97 ms** per step against 500.00 expected · the edges coincident inside 200 µs. The jitter **changes from one course to the next**, 1.01 to 1.29 ms over four courses: it is a bound, and not a value, and the budget is 2 %.
**The asymmetry is the proof.** `MUTATE=7` adds a step to the image and not to the expectation: SEQ turns red (6/14) and CLOCK stays green (38/38). No single course could establish that CLOCK ignores the bank, and two courses on one firmware do. `DROP=3` turns both red, and `JITTER_BUDGET_PCT=0.1` turns the jitter red. `MUTATE` bites **under** the played LENGTH only, 16 by default.
Two findings reported and not asserted. The pulse was measured at **8.8 ms** on 2026-08-21 against the 5 ms of `DEFAULT_TRIGGER_DURATION_MS`. That was read as 5 ms rounded up to the next pass, because the auto-off sits at the end of `loop()`. **Re-measured since: 4.70 ms, then 4.78 ms in CLOCK and 5.01 ms in SEQ.** It therefore goes **below** the configured value, and it **changes with the mode**. The explanation no longer holds, and the figures stay. Tracked at line 12 of `docs/open-risks.md`. And **`PULSE` stays silent**, because `main.cpp` does not drive `gravity.pulse`.
- **A dump of the memory of the panel** (`tools/run-screen-dump.sh`): it reads the image the SSD1306 really shows, and it asserts the geometry after `U8G2_R2`, 24/24 steps at their place, and the rotation. `WATCH=` samples the panel continuously. It checks that **no band, once it has carried ink, comes back empty**, which is the only way the band skip (§12) could flicker. **20 000 readings, one every 0.5 ms: none.** The minima that are low and not zero are states **in the middle of a transfer**, because a band goes out in six transactions. That is inherent to every partial update.
- **The stack probe** (`tools/run-stack-probe.sh`): it measures at run time the stack the **production** firmware really consumes, which the linker cannot see. A C harness paints the RAM before the first cycle and injects interrupt traffic, and the firmware is not instrumented. The result and the method are at §15.
> ⚠️ **A gap in the verification — found on 2026-08-20.** The render and the timing had **never run together**: `src/simavr_main.cpp` holds no render, so the step durations validated in simavr (§6.3: 250 ms and 511 ms) were validated **with no display**. And a full frame is about 25 ms of I2C bus. **To measure:** the real blocking of the main loop, and the minimum CV pulse width really captured (§10.6, ADR 0001). That measurement depends on no product decision.
>
> **The gap is LIFTED on 2026-08-20.** The complete firmware, the render included, runs under simavr, and that had never happened. The **stack** is measured there (§15), and the **real blocking** too, with a real SSD1306 slave on the bus.
>
> **How.** `tools/run-blocking-probe.sh` and `tools/simavr-ssd1306/` wire the `ssd1306_virt` slave of simavr onto the TWI, in both directions, and they time the transfers. The generic `run_avr` binary attaches no part: a transfer there aborts on a NACK at the address byte, and any duration measured there would be far too short. That is what made this document say, wrongly, that only Wokwi or the module could settle it. The firmware is not instrumented: during a render each pass sends exactly one band (ADR 0001), so the I2C traffic bounds the answer directly. The bands are grouped **by the protocol, and not by a threshold of time**: the U8g2 control byte is `0x40` for the data and `0x00` for the commands. The check of the 128 bytes per band validates the split.
>
> **Results, an estimation on hardware, up to date on 2026-08-21.** The transfer of one band **4.62 ms** · a pass of the loop during a render **6.13 ms** in the median and **8.44 ms** in the routine regime (p90) · a routine frame **44.2 ms**, one pass per band. A band goes out in **6 Wire transactions** of about 21 bytes.
>
> The peak of **15.31 ms** survives on the **periodic full refresh** alone, a frame of **59.5 ms**. The verdict therefore tests the routine pass, and it reports that peak separately, with its frequency.
>
> **That peak is measured now, and no longer labelled, and so is its frequency.** It concerns one frame in sixteen, and the frames are about 470 ms apart. At the former default duration of 8 s, **no** full frame fell inside the regime with no ADC. The maximum of a **routine** frame then inherited the caption "periodic full refresh". The two populations are separated by the protocol: a full frame sends **8** bands, and a routine one fewer. The harness says explicitly when it observed none, and `DURATION` is **32 s** so that it observes some. The ratio comes out at **1 frame in 16.0**, read for the first time on a measurement and not on `FULL_REFRESH_EVERY`.
>
> **The duration of a frame is measured in one regime only.** Mixed, the distribution is bimodal: about 41.7 ms with no ADC, against about 55.7 ms with it. The two populations sit at roughly equal weight. The median flipped from one mode to the other with the length of the run: 41.8 ms at 8 s, and 55.7 at 32 s. The "41.8 ms per frame" published on 2026-08-20 was the side of a tossed coin.
>
> **The page addressing of the SSD1306 delimits the frames** (`0xB0 | page`, looked for among the bytes of a command transaction, because U8g2 groups several of them). That removed the last threshold of time from this measurement. It became necessary as soon as the band skip made the number of bands per frame variable. That had made the probe announce frames of 504 ms.
>
> **The artefact of the ADC is CORRECTED**, by a measurement in **two regimes inside one run**: simavr fires its ADC ISR about 4× too often, so halfway through the run the harness clears the `ADIE` bit of `ADCSRA`. And because the ISR is what restarts the conversions, to cut it stops them all. The correction is a ratio of **CPU fractions**, and not a subtraction of maxima, because their extreme values come from different events: **22 % stolen in simulation, and 5.6 % on hardware**, with the simulated rate *measured* on the conversion counter of the firmware.
>
> **The rate is measured over an inner window, and it has to be.** It used to come from a division of **all** the first half of the run by the conversions completed in it. But those conversions only start at `cv::start()`, after the global constructors, the `init()` of Arduino, and the initialisation of the display. To count that dead interval as conversion time made **the rate depend on the duration of the measurement**: 31.5 µs at 4 s, 28.5 at 8 s, and 27.2 at 16 s. Measured between a quarter and a half of the run, it is **26.0 µs at all three**. That is where the correction from 4.9 % to 5.6 % comes from.
>
> ⚠️ **Figures published earlier: INVALID.** "47 ms per frame, 7.74 ms at worst, and the drawing at 1.24 ms" were read on the build whose band conversion was inverted (§12). The screen drew almost nothing there, and those figures mostly measured the absence of drawing.
>
> ⚠️ **A cause was asserted here, and then a measurement denied it.** This document said the peak came from the pass that draws a **row of 12 steps**. That came from the fact that one pass in seven was long, and that a frame holds seven intervals. That was a **geometric inference, and it was false**. A measurement of the cost per POSITION in the frame shows that the expensive band is the band of the logical lines 0-7: the **title**. It was attributed by experiment — with `title = nullptr` that band falls from 15.30 to 5.49 ms — and then decomposed: `getStrWidth` about 1 ms, and `drawStr` **about 8.8 ms**, so **about 0.59 ms per character** with `u8g2_font_5x7_tf` on this MCU. A rule of method: a bimodal distribution says *how many* passes are slow, and never *which ones*.
>
> **Two corrected estimates.** "About 25 ms of bus per frame" and "about 3 ms per band" were **low**: the calculation counted the bits alone, and not the splitting. The intervals between the chunks cost about 107 µs each. And the bus does run at its 400 kHz, so 22.5 µs per byte plus 4.55 µs of TWI ISR. The real total is **36.8 ms of bus** per frame.
>
> **What that changes for the CV: see §10.6.**
>
> **Three tools set aside, and each one verified:** `-fstack-usage` returns empty files, because `-flto` moves the code generation to the link · the `avr-gdb` of the PlatformIO toolchain, from 2019, links against Python 2.7 and no longer starts on a current macOS · the `sram16` trace of simavr emits timestamps alone, **with no value** (`$var wire 0`), and `portpin` is the only usable one. A related trap: `run_avr` **does not honour an absolute VCD path**. The harnesses are written in C now and they read the simulated memory directly, so these limits no longer cost anything.
>
> ✅ **"The heap is corrupted" was a one-byte read out of bounds inside simavr — resolved on 2026-08-21.** AddressSanitizer named it 3 times out of 3: simavr arms `AVR_UART_FLAG_STDIO` by default, and it accumulates the bytes written to `UDR` into a buffer of 256 B. It then passes that buffer to `vfprintf` as `%s` **without terminating it** when it is exactly full. Our firmware feeds that path, because the MIDI goes out over the serial port. The fault landed inside `libsystem_malloc` because the read crossed the metadata of the allocator, and not because one of our allocations was at fault.
>
> The four harnesses disarm that flag right after `avr_load_firmware()` (`tools/simavr-ssd1306/simavr_uart_quiet.h`). simavr exposes the switch itself, so the pinned dependency is untouched. A measurement harness has no use for a console log either, because no measured datum travels through it. Before and after on `blocking_probe`: **2 SIGSEGV out of 5 → 0 out of 5** · **3 ASan reports out of 3 → 0 out of 3** · **256 UART bytes dumped into the report → 0**. Those 256 bytes were the buffer flushed into `stdout` in the middle of the report. They are why the scripts read their log with `errors='replace'`.
>
> Two habits stay, on their own merit and not as a workaround: the statistics arrays are **static**, because nothing would be gained by allocating them. And the script reports an abnormal exit rather than throwing away a complete report. A corollary that still holds: a redirected `stdout` is buffered in blocks, so an unbuffered output is indispensable. Without it the report disappears at the crash, and somebody looks for the defect where it is not.
Levels: the Domain → the Virtual and the Simulator → the AVR firmware (simavr for the signals, Wokwi for the screen) → the real Hardware.
---
## 15. Memory footprint (measured)
The build is `nanoatmega328`, with `libGravity` frozen at the commit `4c5b4d0b4f38…` of the fork of the project.
**⚠️ CURRENT FIGURES, measured on 2026-08-30 after lot S.** Everything that follows is earlier, and it is kept as history.
**RAM 1317 / 2048 (64.3 %)**, 731 B free · **Flash 27030 / 30720 (88.0 %)**, **2154 B** before the guard of 95 %, which sits at 29184 B · **stack peak 203 B**, a margin of 528 B, covered 1.3× by the reserve of 256 · C++ tests 458, adapter 12, EEPROM image 14, TypeScript 452, a clean typecheck, and the libGravity characterization conforming · **mutation 230/230** · the probes: gestures 103, drift 222/222, EEPROM boundary 588/588, and the render **36/36 steps at 808 pixels of ink** · 7 AVR environments compile.
**Lot S returns 126 B of Flash and costs 0 B of RAM**, measured on 2026-08-30. It is a **targeted de-inlining**: `clampIndex`, `wrapIndex` and `oneStep` carry `noinline`, and `clampRange` stays inline. The stack peak does not move. The decision, the variants tried and the counter-proofs live in **ADR 0010**, and this paragraph does not copy them.
**The deltas of the last two lots do not merge.** Lot **F**, the 3 × 12 grid: RAM **+0 B**, Flash **+0 B**, and the stack **−2 B**. The third row stores nothing, and the `rowCY()` formula replaces a ternary that used one register more. Lot **F.5.5**, the removal of the footer: RAM **−21 B**, Flash **−290 B**. Do not read the −21 and the −290 as the cost of the row.
The figures they replace: after lot S, RAM 1338 and Flash 27320. After lot B4b.7, RAM 1338 and Flash 27446.
The figures of 2026-08-28 they replace: RAM 1699 / 2048 (83.0 %), 349 B free · Flash 27164 / 30720 (88.4 %), a margin of 144 B · C++ tests 422 and TypeScript 415.
**⚠️ The transitional peak of lot B4b is OVER since 2026-08-30.** The resident bank of 368 B and the six instances of 138 B coexisted from B4b.3 to B4b.7. Nothing is removed before the tests prove that it can be. **B4b.7 returns 370 measured B, and not 230**: 368 B for the bank and 2 B for the pointer field `bank_`, which leaves only with the API itself. The drift record was acknowledged on 2026-08-30, commit `4e2a24d`, and the guard of `run-build-memory.sh` is green.
**⚠️ `PatternBank` stays in the repository.** The lot removes the dependency of the engine on the bank, and not the bank from the project: `PersistentImage` v2, `loadFactoryPatterns`, the image generator, `gestureRecipes` and `PATTERN_COUNT` still use it.
Figures **re-measured on 2026-08-22**, on the **complete** firmware: two screens, the eight gestures, the transport and the persistence, all wired.
Figures **re-measured on 2026-08-23**, after lot 9, the three channel modes: **RAM 1731 / 2048 (84.5 %)**, 317 B free · **Flash 28538 / 30720 (92.9 %)** · **stack peak 207 B**, covered 1.2× by the reserve of 256 · drift +0/+0 · **269 C++ assertions**, and 226 TypeScript.
Figures **re-measured on 2026-08-23**, after the lots 20 and 21, the coverage of the rates and then the placement of the sub-onsets: **RAM 1713 / 2048 (83.6 %)**, 335 B free · **Flash 28916 / 30720 (94.1 %)** · **stack peak 210 B**, covered 1.2× by the reserve of 256 · drift +0/+0 · **297 C++ assertions**, 254 TypeScript, and a **mutation score of 54/54**. **79 B** of RAM stay above the reserve, and **268 B** of Flash under the guard.
Lot 21 cost **Flash +142 B** and **RAM −12 B**. The RAM falls because the field `slotTicks` leaves the engine, two bytes per channel.
The figures of lot 10 they replace: RAM 1725 (84.2 %), Flash 28774 (93.7 %), the stack 207 B, and 278 C++ assertions and 235 TypeScript. The lot cost **RAM −6 B / Flash +236 B**. The RAM falls because the offset moves to one byte, over six channels. **67 B** of RAM stay above the reserve, and **410 B** of Flash under the guard.
The figures of lot 9 they replace: RAM 1731 (84.5 %) · Flash 28538 (92.9 %) · 269 C++ assertions and 226 TypeScript · 61 B of RAM above the reserve and 646 B of Flash under the guard. That guard sits at **95 %** since 2026-08-22, and no longer at 90 %: everywhere the rest of this section writes 90 %, read 95 %.
The figures of 2026-08-22 they replace: RAM 1699 (83.0 %), Flash 28228 (91.9 %), the stack 206 B, and 245 C++ assertions and 202 TypeScript.
⚠️ **Everything that follows in this section dates from 2026-08-21, and it describes a firmware where the UI was not wired.** Measurements can now replace the estimates there: the wired UI cost **RAM +26 B / Flash +1224 B**, estimated at about 16 B · the persistence **+10 B / +1044 B**, estimated at about 8 B · the transport **+10 B / +1126 B**, estimated at about 0 B because the `clock` object was already allocated, but `Clock::SetSource` was not. The announced margin "of about 5×" did not hold: **93 B** of RAM stay above the reserve, and not 264.
>
> **THE BUDGET IS SIZED, AND NOT ONLY WATCHED (2026-08-21).** This document said "under guard" without ever saying how much was left, and for what. That left the question open at every feature.
>
> **Static RAM: 1528 B out of 2048, so 520 B free.** The stack reserve is 256 B, with a measured peak of **159 B**, covered 1.6×, so **264 B stay for new static data**. Where the 1528 went, by size: `gravity` **300 B**, the libGravity object · `patternBank` **240 B**, 16 × 15 · `NeoSerial` **159 B** · the U8g2 page buffer **128 B** · the four TWI buffers **128 B** · `engine` **110 B** · `uClock` **67 B** · the u8x8 init sequence **53 B** · `uiScreen` **34 B**.
>
> **What is left to build fits there, with a margin of about 5×.** This is an estimate, its base is given, and nobody must confuse it with the measurements above:
>
> \| To build \| Estimated RAM \| Base of the estimate \|
> \|---\|---\|---\|
> \| The wired UI (§12) \| \~16 B \| the cursor, the edit mode, the menu index, the encoder accumulator \|
> \| Transport, EXT and MIDI (§8) \| \~0 B \| `uClock` and the `clock` object are **already** allocated \|
> \| Persistence (§11) \| \~8 B \| the bank is **already** in RAM, and the EEPROM write reads it in place, with no copy \|
> \| CV destinations (§10.2) \| \~24 B \| 6 channels × 2 inputs × 1 target byte, plus the quantisation state \|
> \| RECORDING (§5.5) \| \~4 B \| one flag and one pending step \|
> \| **Total** \| **\~52 B** \| against **264 B** available \|
>
> **Flash: 21404 B out of 30720.** The guard refuses at 90 %, so at 27648 B: **6244 B** stay before the refusal, and 9316 B before the hard limit. Points of comparison measured in this repository: the CV sampling cost **+354 B**, the spacing by band **+522 B**, and `PagedScreen` **+160 B**. A complete UI with its menus is the only really expensive item to come, of the order of **2 to 4 kB**. That passes, and the guard reports every step.
>
> **The trigger is explicit**, so nobody judges it case by case any more: a failure above **+16 B of RAM** or **+512 B of Flash** unacknowledged, with ceilings at **256 B free** or **90 % of Flash**. `--accept` never happens without a look at the diagnostic by symbols.
>
> ⚠️ **The single hole of the stack measurement, and its obligation.** The probe measures what the firmware **executes during the run**. The EEPROM write of the persistence is not there because it does not exist. And it will not be there **automatically** on the day it exists either: the run will have to **provoke** it. That is the one thing not to forget while implementing §11. The old values "Pattern 4 B / 384 B / 578 B free" are **void**.
> ⚠️ **Always the AVR measurement, and never the native one.** `sizeof(SequencerEngine)` is 120 B compiled on an x86 host, and **110 B on AVR**, read with `avr-nm` on the `.elf`. The second one is the authority, under the rule of `CLAUDE.md`.
> The jump of Flash from 16316 B comes first from the **OLED render**, the U8g2 primitives plus the `u8g2_font_5x7_tf` font. Then it comes from the CV sampling under interrupt (§10.6), and then from the band skip (§12).
> ⚠️ **A drift guard exists since 2026-08-20**: every build is compared to `tools/memory-baseline`, which is versioned, and a growth above 16 B of RAM or 512 B of Flash **fails**. To accept it is a deliberate act (`--accept`). A ceiling that fires at 90 % of Flash alone let a feature of 3 kB pass without a word.
<table header-row="true">
<tr>
<td></td>
<td>The old model (6×16)</td>
<td>The current model (a bank of 16)</td>
</tr>
<tr>
<td>`sizeof(Pattern)`</td>
<td>7 B</td>
<td>**15 B** (3 of steps plus 12 of ratchets)</td>
</tr>
<tr>
<td>Pattern storage</td>
<td>672 B</td>
<td>**240 B**</td>
</tr>
<tr>
<td>`SequencerEngine`</td>
<td>—</td>
<td>**110 B**, and it holds the timing cache per channel</td>
</tr>
<tr>
<td>**Firmware RAM**</td>
<td>1758 B (85.8 %)</td>
<td>**1528 B (74.6 %)**</td>
</tr>
<tr>
<td>**Free RAM**</td>
<td>\~290 B</td>
<td>**520 B**</td>
</tr>
<tr>
<td>Flash</td>
<td>15800 B</td>
<td>**21404 B (69.7 %)**</td>
</tr>
</table>
The RAM constraint goes from **critical to comfortable**.
### The stack — measured, and no longer estimated (2026-08-20)
> **Measured peak: 159 bytes**, out of 520 free, so **361 B of margin**. It is read at run time on the **production** firmware, with no line of instrumentation (`tools/run-stack-probe.sh`).
>
> ⚠️ **Re-measure after every structural change**: the peak moved at each one of them, 120 → 154 → 159 B.
>
> **Method:** a C harness writes a pattern into the free RAM of the simulated machine **before the first cycle**. It lets the firmware run while it injects interrupt traffic, and it then reads back the boundary of the intact pattern. The scan starts from the **top**: `__heap_start` is `_end`, so an allocation would dirty the bottom and would make a reader conclude wrongly that the stack went down there.
>
> **The two blind spots are CLOSED (2026-08-20), and they were worth 43 B.** The previous version painted from inside the firmware at the top of `setup()`. It published the result as a pulse width, for want of a way to read the memory of the simulator. It announced 120 B. It ignored the stack used before `setup()`, so the global constructors and the `init()` of Arduino: **+24 B**. And it exercised no input ISR: **+19 B**. It needed a probe in `main.cpp` and a dedicated environment, and both are removed.
>
> **The coverage is verified, and not supposed.** The verdict requires that **the six families of ISR were entered**: PCINT1 and PCINT2 of the encoder · uClock · millis · MIDI on reception · the ADC. Those two PCINT pins are the only pins under PCINT in libGravity, because the buttons are polled. A silent vector fails the measurement, because that is exactly how it was incomplete in silence.
>
> ⚠️ **Still out of the measurement:** the paths the firmware does not take yet, and first of all the EEPROM write of the persistence (§11), which does not exist.
>
> **A consequence — the reserve threshold falls to 256 B (decided 2026-08-20).** The threshold of `tools/run-build-memory.sh` was 512 B, set by estimation before any measurement: it announced 46 B of margin while the real consumption sat well below the threshold itself. At 256 B, the really available budget is **264 B**, and the reserve covers the peak **1.6×**. The margin shrank as the measurements became complete, and it did not become narrow. Do not raise it without a **new measurement**.
>
> **A lever held in reserve:** `NeoHWSerial` guards its buffer sizes with `#if !defined(...)`, so `-DSERIAL_TX_BUFFER_SIZE=16 -DSERIAL_RX_BUFFER_SIZE=32` returns **80 B** through a compile option alone, with no change to the dependency. Two other levers would on the contrary need a patch of the dependencies, which is out of scope without a separate decision: the 160 B of `Wire` buffers, of which two are for reception and useless for a screen nobody reads · the 77 B of U8g2 tables declared with no `PROGMEM`.
---
## 16. Decisions — validated against open
⚠️ **The review of the reference version of 2026-08-23 (§5.0) supersedes five entries of this list**: the resident shared bank · the 24 steps · the LENGTH as a property of the channel alone · the three modes · `SHIFT` plus `PLAY` reserved for RECORDING. The decisions in force are the decisions of §5.0.
**Added on 2026-08-23:** `main` @ `40d4aac` is the reference and `1.2-dev` is a catalogue · patterns as a **template in the EEPROM, with an instance per channel** · **36 steps** and **one nibble per ratchet** (ADR 0007) · LENGTH **deduced at the load** and then owned by the channel — ⚠️ a stale wording, because the template STORES it (§5.0 point 3) · **A1–A8 frozen** · four modes with **GATE** · **SWING as a parameter of SEQ**, 0–49 %, capped · **mute** on `SHIFT` plus `PLAY` · **RECORDING** on `SHIFT` plus a short press · a bar of **9 tabs** plus a **fixed** transport indicator · a cogwheel for CONF and a small grid for PATTERNS · tempo **20–200** and a pulse of **5 ms** · a grid of **3 rows of 12** with no footer · a PATTERNS tab that reuses `LEVEL_EDIT` · the **7th channel set aside**
**Validated:** a bank of 16 shared patterns · LENGTH per channel · `masterPhase` (96 PPQN, `uint32`, a smoothed local phase) · **SUBDIV to ticksPerStep per channel** (the libGravity convention, `/N` and `xN`, with the default `/1`) · **a change of SUBDIV takes effect on the next beat** (§6.1.1, decided on 2026-08-23, ADR 0004) · Transport, the mapping of the 96 PPQN clock to the engine · the trigger generation, verified in simavr · **one step is one unit of time** · **a purely graphical measure separation** (none/2/3/4/6, per channel) · **RATCHETS per step** (2/3/4/6 plus the TRIPLET ▲ that stretches over 2 units) · the geometry and the legend of EDIT PATTERN, from the Wokwi POC · **the complete CV mapping** (§10: the destinations PATTERN / LENGTH / RESET / STEP per channel, an application at the step boundary except for RESET, the Schmitt thresholds +1 V / +0.5 V, and a routing that survives a change of mode) · **the ratchet 5 set aside** · **the edit while the transport plays, kept** · **the EDIT spacing `20 / 38`** · **the OLED render spread over its 8 bands** (ADR 0001) · **the RAM reserve threshold brought from 512 to 256 B** on the strength of a stack measurement (§15) · **the CV sampled under interrupt, with a guarantee of 1 ms** (§10.6) · **the header kept explicit** and **the skip of the unchanged band** (§12) · **a memory drift guard** with a versioned record (§15) · **the EEPROM persistence format v2** (§11.1, implemented on 2026-08-23: 304 B, 9 B per channel, and two CV target bytes reserved for §10.2) · **the offset on ONE byte**, faithful to the `uint8_t offset` of the original, with the limit kept as it is.
**The six decisions of the conformity audit, settled on 2026-08-23:**
1. **The separation of `MODE` and `PPQN` comes back**, as in the original: `MODE` carries INT / EXT / MIDI, and `PPQN` appears in EXT only. The merge into one field `SRC` of six values is abandoned. **`PPQN` exposes the FOUR rates of libGravity** (24, 4, 2, 1) and not the two of the original. That is an accepted addition, decided on 2026-08-23, and it removes nothing. The two fields are **two views of one source byte**, so `PPQN` costs no EEPROM byte, and no incoherent state is representable. The detail is at §8.1.
2. **The toggle of a step stays on the encoder press.** **RECORDING will take `SHIFT` plus `PLAY` together**, a **ninth gesture** the original does not have. It is not a function callback but a state: `input::shiftHeld()` already exists, so PLAY asks whether SHIFT is held. The transport keeps PLAY alone.
3. **The long press threshold stays at 750 ms**, the threshold of libGravity, and not the 300 ms of the original. An accepted divergence: 750 ms is **measured** on the module, ten deliberate presses and ten short presses, with no false long press. 300 ms is not measured, and the constant is not adjustable in the pinned dependency, so the adapter would have to time the press itself.
4. **The acceleration of the encoder disappears everywhere, the tempo included.** The original does not accelerate, so FlexSeq will no longer hold a rule and an exception, but one rule. **An accepted consequence**: the tempo range holds 270 values, so to cross it asks for 270 detents.
5. **The reversal filter stays at 12 ms**, and not the 200 ms of the original. An accepted divergence, and it is the only one that rests on a measurement of both sides: the fastest bounce is 2 ms, a deliberate reversal takes 509 to 1003 ms, and 200 ms would swallow a sharp correction.
6. **The skip chance caps at 9**, so 90 %, as the original interface does. The **effective** value still reaches 10 through the CV modulation, as in the original. A silent channel comes from a change of mode, and not from a setting of 100 %.
7. **`RANGE` comes back now, and the format moves to version 3.**
⚠️ **WHAT THE MOVE TO V3 COSTS, in bytes and in data.** The global zone of version 2 holds 3 bytes: the tempo on two, and the source on one. The restitution of the BPM tab asks for `MOD`, the input that modulates the tempo, and `RANGE`. `MODE` and `PPQN` cost nothing, because they are two views of the source byte (§8.1). The zone therefore goes to **5 bytes**: the tempo (2), the source (1), `MOD` (1), `RANGE` (1). That is arithmetic, and not a choice.
⚠️ **The two bytes are ALREADY in version 3, reserved and inert.** §11.1 is the authority for the layout and for the size of the image. The total of 306 bytes written here before is **superseded**: it predated the 36-step foundation and the template / instance model. The v3 image holds **588 bytes**.
A change of version starts **from the defaults again**: the FlexSeq state written on the module since the first flash is lost. The settings of the original firmware, below the address 320, are never touched. The owner chose to do it **now**, at the moment of the series when the module holds the least work.
**Abandoned:** METER / MEASURES, the rhythmic signature and its derived value · the triplet groups of "3 consecutive steps" · the note-value glyphs, from the whole note to the demisemiquaver · **CV to an absolute STEP position**, which would be a new channel mode and not a destination · **CV to a global Reset**, because the global Reset belongs to the external clock input · **any use of `AnalogInput::IsRisingEdge()`**, an audited anomaly (§10.5) · **a shorter EDIT PATTERN header**, because the clarity came first · **a RAM cache of the title band**, 128 B, because the RAM stays for features · **a call to `Gravity::Process()`** from the main loop, because it is incompatible with the ADC under interrupt, and its pieces are called separately (§10.6).
**Open or deferred:** the separate hooks for the MIDI and Ext transport events · the avr8js backend inside the simulator · **the merge of the clock source and the PPQN into one field `SRC`**: the original separates `MODE` (INT/EXT/MIDI) and `PPQN`, and FlexSeq merged them into five values (`INT`, `EXT24`, `EXT4`, `EXT2`, `EXT1`). That divergence was **never decided**, it appeared at the implementation of the transport, and the conformity audit must settle it · **one font for the whole screen**: `setFont(u8g2_font_5x7_tr)` is called once in `main.cpp` and never changed, so no parameter reads as the main one, and the ten in-house glyphs that were to replace `logisoso26` are designed and not implemented · **`RANGE` has no byte in the v2 format**: §10.1 keeps it and the original persists it, so its return would ask for an image of 305 bytes and a version byte at 3 · **the values `EXT2` and `EXT1`**: the original offers `24` and `4` in PPQN alone, and libGravity can do all four. Read to the letter, "as on the original" removes them, and it is the only item of the audit of 2026-08-23 that stays open · the **RECORDING mode** (§5.5: the design points are set, the assignment of the physical controls is to define, and it is not a priority) · the **CV destinations** (§10.2): the sampling mechanism and the edge detection exist and are verified (§10.5, §10.6), and the routing per channel, the quantisation and the application at the step boundary are still to implement · **a sampling conditioned on the routing**: the 5.6 % of CPU of the ISR are paid even when no channel routes a CV. **Deferred to §10.2 by a decision of the owner (2026-08-21)**, with its reason: alone, the conditioning would be inapplicable, because no channel routes a CV today, so to condition it would amount to cutting the CV · **controls not wired in `main.cpp` — found on 2026-08-21**: the production binary calls `clock.AttachIntHandler()` alone. Not `AttachExtHandler`, not `SetSource`, not `SetTempo`, and not `Start` or `Stop`. And the buttons and the encoder are only `Process()`-ed, because no callback is wired to an action. The firmware therefore plays the **internal clock at a fixed 120 BPM**, with the transport started by default. It runs six channels on the pattern 0, plus the screen. Nothing is controllable. That is not a defect but the state of progress: it is to wire with the UI (§12) and the transport (§8). A direct consequence for the first flash: it would validate the **hardware chain**, and not the features, which are not there yet.
- **the first physical flash** — **deferred by a decision of the owner (2026-08-21): the module is available, the wait is deliberate, and it no longer rests on a missing precondition.** Every precondition of §17 is met — the reference to "§12" was wrong, because §12 is the UI — and `env:bringup` makes that flash diagnosable. Two gestures come before it, established at §2: check that the module is **Rev 2+**, so that the SHIFT button is present, and **back up the Flash and the EEPROM**. What stays lifts only by flashing: the mounting of the OLED, the real CV conversion, the wiring of the outputs, and the external clock.
---
## 17. Constraints and rules of development
- Do not modify the hardware, and do not modify `libGravity`, without a separate and versioned decision.
- Watch the RAM and the Flash at every structural step.
- Avoid dynamic allocations in the embedded domain.
- Write a unit test for every new behaviour.
- Keep the ability to restore the original firmware.
- Do not make the firmware depend on the Mac, and not on Node.js. TypeScript2Cxx and avr8js are not runtime dependencies of the module.
- **The PRD cycle:** design → prototype and tests → decision → a review of the PRD → a normative update → implementation.
---
## 18. The libGravity dependency — audited anomalies
`libGravity.cpp` (`Gravity::Process`) holds a loop whose index is **not initialised**: `for (int i; i < OUTPUT_COUNT; i++)`, so undefined behaviour. libGravity is frozen and not modifiable, so FlexSeq **works around** it, and it drives the auto-off of the outputs explicitly in its main loop. To report upstream if an evolution of the dependency ever comes up.
> **The characterization suite — restored on 2026-08-20.** Prose alone no longer describes the audited anomalies: the PlatformIO environment `native_libgravity` **reproduces** them, with **7 red assertions by construction** out of 68, spread over `AnalogInput`, `Button`, `Encoder` and `DigitalOutput`. Since 2026-08-20, `test_gravity` also carries the **characterization of `Gravity::Process()`**: it reads exactly two analogue inputs, CV1 and CV2, once each, and it polls both buttons plus the button of the encoder. FlexSeq no longer calls that function (§10.6), so the risk is not to miss an upstream evolution, because a decision pins the dependency. The risk is to forget to re-audit it at the next bump of the pin. A twin test checks that the pieces called instead cover the same inputs with **zero** analogue read. The script `tools/run-libgravity-tests.sh` checks that the set of failures is **exactly** the audited one, and it fails on any drift in both directions: an anomaly that disappears counts as an unexpected failure. The normative list of the anomalies lives in `CLAUDE.md`, and the detail per test in `test/README`. Those six tests had stopped compiling for four months with no signal at all. A `test_filter` removed them from the collection, in the same commit that removed their include paths. **Verified on 2026-08-20:** none of these anomalies is fixed upstream, 3 commits after the pinned commit.
>
> **REACHABILITY AUDIT — 2026-08-21.** To reproduce an anomaly is not the same thing as to know whether it sits **on our path of execution**. Verified by a read of the code that is called, and by a separation of the active code from the comments:
>
> \| Anomaly \| On the path of the production binary? \|
> \|---\|---\|
> \| `AnalogInput::IsRisingEdge()` \| **no** — never called, and `CvGate` replaces it (§10.5) \|
> \| `Gravity::Process()`, the uninitialised index \| **no** in `main.cpp` · **yes** in `wokwi_main.cpp`, the only active call left \|
> \| `Clock::SetSource()`, `SOURCE_LAST` \| **no** — no source is chosen, and it shows up as a compiler warning \|
> \| `Button`, a release lost at the debounce \| **latent** — `Process()` is called, and no callback is wired \|
> \| `Encoder`, a false first movement \| **latent** — the same \|
> \| `DigitalOutput::Init()`, it does not switch off \| **latent** — see below \|
>
> **The three "latent" ones become active exactly when the UI and the transport are wired** (§12, §8). The adaptation layer must absorb them, and that is the trap not to rediscover at that moment.
>
> **`DigitalOutput::Init()` is harmless on a cold start, and the reason must be named**: an AVR reset leaves `PORT` at 0, so `pinMode(pin, OUTPUT)` pulls the pin low, and `gravity` is a **global** object in `.bss`, so `on_` starts false. **No output can therefore stay stuck high at the first flash.** It becomes real on a **soft reset** — the original firmware exposes a `reboot()` — or on a second `Init()`: the pin would stay HIGH while the firmware believes it is off, and nothing would ever switch it off. To handle if a software restart is ever added.
>
> The matching actions are tracked in `docs/open-risks.md`, lines 14 to 16.
---
## 19. Criteria of success
The final firmware must run on the **unchanged** Gravity hardware · keep the historical features the project retained · offer patterns of 1 to 24 steps with the `masterPhase` temporal model · stay inside the RAM and Flash budget · allow the restoration of the original firmware.
<page url="https://app.notion.com/p/3c7d2c2576ce813fa814eb6259949d5f">Conception — fractionnement des salves SHIFT</page>
