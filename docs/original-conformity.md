# Conformity to the original firmware — the inventory

**Lot 15, 2026-08-23.** Tracking document, **never normative**. Every decision it
raises belongs to `PRD.md`; this file only says what was read and where.

## Why it exists

Three times, FlexSeq shipped a screen that held less than the original's. The
three channel modes were absent from the domain (2026-08-22), the BPM tab kept
one field of four, and the tab bar lost its Play/Stop indicator (both found on
the module, 2026-08-23). Each time the screen had been built from a design
instead of from the original's own drawing code.

The owner's rule, restated on 2026-08-23 and now PRD §1: **keep every original
feature and every original page; only the SEQ mode evolves.**

## Which version, and why it matters

⚠️ Everything below compares FlexSeq to **`main` @ `40d4aac`** (2026-03-10), the
public release. The owner made it the **behavioural reference** on 2026-08-23.

`1.2-dev` @ `f7b2150acf` is a second, **older** branch that was never merged. It
carries features `main` does not have, and it is catalogued separately in
`docs/original-1.2-dev-features.md`. A verdict here that says "does not exist" or
"addition" means **it does not exist in `main`**; several of those do exist in
`1.2-dev`, and the catalogue says which.

## Method

Read the original's **drawing and input code**, screen by screen and field by
field. Not the PRD: the PRD is what missed three times.

Sources, in the neighbouring clone `GravityFW/src/Gravity/`:

- `UI.ino`, 353 lines — the three screens;
- `Interactions.ino`, 439 lines — every gesture;
- `Gravity.ino`, 684 lines — the state and the generation.

Each row below carries the original's line, so the reading can be checked.

## Screen 0 — the tab bar

| Element | Original | FlexSeq | Verdict |
|---|---|---|---|
| Tab count | 7: clock glyph `w` then digits 1 to 6 (`UI.ino:239-260`) | 8: clock, 6 channels, global config | **assumed divergence** — the owner moved the settings page into the bar, 2026-08-23 |
| Play/Stop indicator | separate glyph at x=121, `t` stopped and `r` playing, **only when the clock is internal** (`UI.ino:262-267`) | absent | **omission** — line 28, lot 16 |
| Selected tab | inverted, and inverted again while SHIFT is held (`UI.ino:240-244`) | inverted | **divergence to decide** — FlexSeq does not react to SHIFT in the bar |
| Rule under the bar | `drawHLine(0, 53, 128)` (`UI.ino:234`) | present | conform |
| Tab at power-on | **the clock tab**: `byte displayTab = 0` (`Gravity.ino:125`) | **the first channel**: `currentTab_(TAB_FIRST_CHANNEL)` (`UiController.cpp:89`) | **divergence to decide, found 2026-09-04** during step 4 of lot 11. At power-on the original shows the tempo and FlexSeq shows a channel. Nothing decided it, and no document carried it. It is one line to align, in either direction |

## Screen 0 — the BPM tab

| Element | Original | FlexSeq | Verdict |
|---|---|---|---|
| Main parameter | `bpm` in the **large font** `stkL`, label `BPM` below in `velvetscreen` (`UI.ino:88-108`) | tempo in the single 5x7 font, no label | **omission** — line 29 |
| Main parameter, EXT or MIDI | shows `EXT` or `MIDI` in place of the number (`UI.ino:81-86`) | absent | **omission** |
| `MODE` | `INT` / `EXT` / `MIDI` (`UI.ino:31,51-56`) | absent | **omission** — line 27, lot 17 |
| `MOD` | `OFF` / `CV1` / `CV2`, internal clock only (`UI.ino:33,57-62`) | absent | **omission** — line 27, lot 17 |
| `PPQN` | `24` or `4`, external clock only (`UI.ino:34,63-66`) | fused into `SRC` | **decided 2026-08-23**: the separate field returns, with the **four** libGravity rates |
| `RANGE` | `bpmModulationRange` 1 to 5, shown x10 (`UI.ino:35,67-68`) | absent, and no EEPROM byte | **omission** — line 27, lot 17 |
| Field count | 1, 2 or 3 according to the mode (`UI.ino:20-27`) | 1, fixed | **omission** — the layout is fixed at PRD §12.1: INT gives `MOD` then `RANGE`, EXT gives `PPQN`, MIDI gives none |
| `SRC` | does not exist | 6 values, MIDI included | **replaced** by `MODE` plus `PPQN`, 2026-08-23. The two extra rates survive as `PPQN` values |

## Screen 0 — a channel tab

| Element | Original | FlexSeq | Verdict |
|---|---|---|---|
| Main parameter, CLOCK | value `/N` or `xN`, label `SUBDIVISION` (`UI.ino:190-200,213-219`) | same, in `stkL`, since 2026-09-04 | **conform** — lot 11 |
| Main parameter, RAND | value `N0%`, label `SKIP CHANCE` | same, in `stkL`, since 2026-09-04 | **conform** — lot 11 |
| Main parameter, SEQ | value `A1` to `B8`, label `PATTERN` | pattern name, and the SEQ tab keeps the FlexSeq layout | conform in content, not in size — **lot 12**, by decision 2 of PRD §12.1 |
| `MODE` | `CLOCK` / `RAND` / `SEQ` (`UI.ino:126,145-150`) | same, line 1 in the three modes, since 2026-09-04 | **conform** — lot 11. It is also what makes the mode reversible: PRD §12.1 decision 10 |
| Field 2, CLOCK | label `OFFSET:`, value `offset/pulsesPerStep` (`UI.ino:128,151-159`) | same, since 2026-09-04 | **conform** — lot 11. The bound is `ticksPerStep - 1` capped by 255, applied by the engine |
| Field 2, RAND | label `SUBDIV:`, value `/N` or `xN` (`UI.ino:130,160-165`) | same, since 2026-09-04 | **conform** — lot 11 |
| Field 2, SEQ | label `EDIT PATTERN`, a filled box as the entry marker (`UI.ino:132,174-178`) | an entry field | conform |
| `MOD` | `OFF` / `CV1` / `CV2` (`UI.ino:134,166-172`) | line 3 drawn since 2026-09-04, navigable, and it always reads `OFF` | **partial** — the field exists, the mechanism is lot 13. Its destinations depend on the mode: PRD §10.2 |
| `LEN` | does not exist, 16 steps fixed | present | **addition**, PRD §1 |
| `SEP` | does not exist | present | **addition**, PRD §6.2, moves into EDIT at lot 12 |

## Screen 1 — the pattern editor

| Element | Original | FlexSeq | Verdict |
|---|---|---|---|
| Title | `PATTERN A1`, framed and centred (`UI.ino:273-283`) | `A1`, top left | **divergence to decide** |
| Grid | 16 steps, 2 rows of 8, glyphs `q` active and `p` inactive (`UI.ino:284-295`) | 24 steps, 2 rows of 12, disc and ring | **assumed divergence** — PRD §1, the core of the project |
| Cursor | frame around the step (`UI.ino:296-303`) | frame | conform |
| Ratchets | do not exist | digits and triangles | **addition**, PRD §6.3 |
| Footer | none, except `RECORDING` while recording (`UI.ino:305-308`) | `CH1 120BPM` | **addition** |
| `RECORDING` | present | absent | **omission**, deferred by PRD §5.5 |

## Screen 2 — SETTINGS

| Element | Original | FlexSeq | Verdict |
|---|---|---|---|
| Title | `SETTINGS` and the version number (`UI.ino:313-316`) | absent | **omission** — lot 14 |
| `CALIBRATE CV INS` | calls `calibrateCVs()` (`UI.ino:322`, `Interactions.ino:58`) | absent | **omission** — lot 14 |
| `ROTATE SCREEN` | toggles, saves, applies (`Interactions.ino:60-63`) | preference stored, no page | **omission** — lot 14 |
| `REVERSE ENCODER` | toggles, saves, shows `DONE` (`Interactions.ino:64-68`) | preference stored, no page | **omission** — lot 14 |
| `FACTORY RESET` | writes `memCode - 1` at 1023 then reboots (`Interactions.ino:69-71`) | absent | **omission** — lot 14 |
| `DONE` overlay | a framed box after an action (`UI.ino:341-350`) | absent | **omission** — lot 14 |
| `DONE`, its geometry | a box of **64 x 29** centred at `x=64`, baseline 32, with a **drop shadow of 2 px**, over two cleared boxes at `(18,13,93,32)` and `(18,16,96,30)` (`UI.ino:341-350`) | absent | **omission** — lot 14. ⚠️ **The clear starts at `x=18` while the labels start at `x=8`, so ten pixels of every line survive** to the left of the box, which reads as about two letters. **FlexSeq clears the full width instead — decided by the owner on 2026-09-04.** The stubs carry no information and a reader takes them for a defect. This removes no feature: it is a remainder of the original's clear rectangle, not a function |
| `DONE`, what clears it | **the next release of the encoder button**, and that release does nothing else — it is consumed (`Interactions.ino:10-11`). **There is no timer** | absent | **omission** — lot 14. **FlexSeq keeps the behaviour of the original: no timer, decided by the owner on 2026-09-04.** An automatic return after two seconds was proposed and set aside |
| `DONE`, when it appears | after `CALIBRATE CV INS` and after `REVERSE ENCODER`. ⚠️ **Not after `ROTATE SCREEN`**, which only flips the image (`Gravity.ino:679-685`) — the flip is its own answer. Not after `FACTORY RESET` either, which reboots | absent | **omission** — lot 14. The asymmetry is deliberate in the original and FlexSeq keeps it |
| `FACTORY RESET`, no confirmation | writes `memCode - 1` at 1023 and **reboots at once** (`Interactions.ino:69-71`). One press destroys every pattern and every setting | absent | **omission, and FlexSeq ADDS a confirmation — decided by the owner on 2026-09-04**, conditional on the cost being close to zero. The estimate is 60 to 150 bytes of Flash, because the `DONE` machinery already exists and the addition is one state, one text and a branch with two outcomes. ⚠️ **That figure is an estimate and not a measurement**: it is measured inside the lot, and the owner accepts or refuses it then |

## The gestures

| Gesture | Original | FlexSeq | Verdict |
|---|---|---|---|
| Rotate, tab bar | changes tab, **clamped** 0 to 6 (`Interactions.ino:110-116`) | changes tab, **wraps** | **divergence to decide** |
| Rotate, inside a tab | moves the menu item (`Interactions.ino:178-184`) | moves the cursor | conform |
| Rotate, a field open | changes the value | changes the value | conform |
| Press, short | enters the tab, or opens the field (`Interactions.ino:13,51`) | same | conform |
| Press, long | goes back one level, threshold **300 ms** (`Interactions.ino:73-82`) | goes back, threshold **750 ms** | **divergence DECIDED on 2026-09-04 by the owner: FlexSeq keeps 750 ms.** The 300 ms of the original is now measured and not deduced, which is what PRD §12.1 asked for: it read the gesture card, which says "about 1 s", and the card is wrong by a factor of three. FlexSeq is therefore **slower than the original by 450 ms** on every "go back", and that is a choice. ⚠️ The rule that the long press **closes an open field before it goes up a level** is conform: `Interactions.ino:74` tests the open field before everything else |
| SHIFT + rotate, tab bar | changes the tab's **main parameter** (`Interactions.ino:117-171`) | changes the tab's main parameter: SUBDIV in CLOCK, the skip chance in RAND, the pattern in SEQ, the tempo on the clock tab | **conform** — closed by lot 19, 2026-08-23. The value is edited before it is **displayed** for CLOCK and RAND: the display is lot 11 |
| SHIFT + rotate, in a tab | changes the selected value | changes the value | conform |
| SHIFT + press, editor | **toggles the step** under the cursor (`Interactions.ino:397-399`) | encoder press toggles it | **divergence to decide** — question 2 |
| SHIFT held long, editor | **clears the pattern**, threshold **500 ms** (`Interactions.ino:411-415`) | clears it, threshold 750 ms | conform in substance |
| SHIFT + encoder over 2 s | opens SETTINGS (`Interactions.ino:83-84`, `416-419`) | absent | **omission**, kept by decision 2026-08-23 — lot 16 |
| Press, editor | starts and stops **RECORDING** (`Interactions.ino:53-57`) | toggles the step | **conflict** — question 2 |
| PLAY | starts and stops the clock, and **only when the clock is internal**, `if (masterClockMode == 0)` (`Interactions.ino:369-388`) | same, since 2026-08-25 | **was wrong, now conform**. This row said "conform" while FlexSeq drove the ENGINE and not the clock: uClock kept running, the MIDI Clock kept going out, and no MIDI Start or Stop was ever sent. Fixed by T4, together with the two rows below, because they depend on each other |
| Boot state | starts **stopped**: `isPlaying` is a global at zero (`Gravity.ino:110`) | starts stopped, since 2026-08-25 | **was an omission**, fixed by T4. FlexSeq called `transport.start()` in `setup()`, so triggers fired at power-on |
| External clock start | starts by itself on the first external pulse (`Gravity.ino:321-322, 338-339`), and it does **not** reset: `isPlaying = true` alone, so a restart continues from the current position | starts by itself since 2026-08-25 (T4), **through `Transport::start()`, which is a global reset and then run** | **partial, and the earlier "same" hid the nuance** — found on 2026-09-02 during the D79 review. The auto-start itself is conform. The reset is not: after a stop, an external restart continues in the original and goes back to step 0 in FlexSeq. The divergence predates D79, which only settles the first onset. To align the restart is a separate compatibility decision, not taken |
| MIDI Start `0xFA` | starts the clock on the message (`clock.h:143-146`, `uClock.start()`) | the transport starts WITHOUT it, on the first `0xF8` | **conform in effect, different in mechanism.** Measured 2026-09-03 by the `midiclock` course with `MIDI_SEND_START=0`: every criterion stays green. The start comes from the external pulse counter of `TransportAdapter`, not from the MIDI message, so a device that sends clock without start still plays. Whether the original does the same is **not established** |
| External pin under the MIDI source | not read: the original selects one source | **the pin drives the clock as well** | **divergence, and it is not decided.** `transport::begin()` calls `AttachExtHandler` once for every source, so the interrupt of PD2 stays attached in MIDI mode. Measured 2026-09-03: an image with the MIDI source and pulses on PD2 alone passes the five criteria of the external clock course. Both inputs are therefore live at once. No decision was taken, and nothing says the original behaves otherwise |
| SEQ playhead | **one** shared `currentStep` for the six channels, wrapping at 15 (`Gravity.ino:99, 446-450`) | one `localStep` per channel | **assumed divergence**, written to the PRD §7 on 2026-08-25. Per-channel LENGTH and SUBDIV require it: two channels of different lengths cannot share a position |
| SUBDIV list | 20 values (`Gravity.ino:56`) | 25 values, libGravity's `CLOCK_MOD`, an exact **superset** of the original's | **assumed addition**, already in PRD §6.1 with its ordering, which is normative because the EEPROM stores the index |
| Encoder acceleration | **none** — the code is commented out (`Interactions.ino:100-104`) | libGravity accelerates x3; `oneStep()` cancels it **everywhere**, the tempo included since lot 19 | **conform** — and the acceleration never fires by hand anyway, measured on the module: ADR 0003 |
| Reversal filter | ignores a reversal under **200 ms** (`Interactions.ino:104`) | `EncoderFilter`, **12 ms** | **divergence to decide** — question 4 |
| Press plus rotate | **does not exist** | **removed** by lot 19; `EVENT_ROTATE_HELD` and `AttachPressRotateHandler` are gone, verified by compilation | **conform** |
| SHIFT + rotate, editor | sets the **ratchet** of the step, on a step that is selected **and active** | same; on an inactive step the gesture does nothing | **conform** — the ratchet is a FlexSeq addition, PRD §6.3, and the gesture is the original's |
| SHIFT + PLAY | reserved for RECORDING | recognised as a ninth gesture, with no client; PLAY alone still drives the transport | **conform in the gesture**, the function is part of RECORDING |
| Live trigger | SHIFT fires the channel's output, **only while recording** (`Interactions.ino:394-396`) | absent | **omission**, part of RECORDING |

## Behaviour, not layout

| Subject | Original | FlexSeq | Verdict |
|---|---|---|---|
| Skip chance, stored | the UI clamps to **9**, so 0 % to 90 % (`Interactions.ino:157-161`) | 0 to **10**, so up to 100 % | **divergence to decide** — question 5 |
| Skip chance, effective | `random + randMod` clamped to 10 at generation (`Gravity.ino:479-483`) | same bound | conform |
| Offset clamp | **inconsistent**: to `pulsesPerCycle` when edited (`Interactions.ino:260`), to `pulsesPerStep - 1` when the rate changes (`Interactions.ino:150`) | always `min(ticksPerStep - 1, 255)` | **FlexSeq is consistent where the original is not** — assumed |
| Mode change | **clears** both CV targets (`Interactions.ino:249-250`) | the routing survives | **assumed divergence**, PRD §10 validated |
| Saving | `saveState()` on **every** change | after a 3 s quiet delay | **assumed divergence**, PRD §11.1, with its reason |
| Fonts | two: `stkL` for the main value, `velvetscreen` for the rest | the same two, byte for byte, since 2026-09-04 | **conform** — lot 11. u8g2's own font left the production image; `env:bringup` keeps it |

## The six decisions — ANSWERED by the owner, 2026-08-23

| | Subject | Decision |
|---|---|---|
| 1 | The `SRC` fusion | **restore the separation**, as in the original: `MODE` with INT / EXT / MIDI, and a `PPQN` that shows only in EXT. `PPQN` exposes the **four** libGravity rates, not the original's two |
| 2 | The step toggle | **keep the encoder press**. RECORDING gets a new gesture: **SHIFT and PLAY pressed together** |
| 3 | The long-press threshold | **keep 750 ms** |
| 4a | The encoder acceleration | **remove the exception**: the tempo no longer accelerates either |
| 4b | The reversal filter | **keep 12 ms** |
| 5 | Skip chance | **cap at 9**, so 90 % |
| 6 | `RANGE` | **it returns now**, and the format goes to version 3 |

**What follows from decisions 1 and 6, by arithmetic and not by choice.** The global
zone holds 3 bytes today: the tempo on two, the clock source on one. Restoring the
BPM tab needs `MOD` (the channel that modulates the tempo) and `RANGE`; `MODE` and
`PPQN` are two views of the source byte and cost nothing. The zone goes to **5
bytes** and the image from 304 to **306**, version byte **3**.

**The detail decision 1 left open, settled the same day: `PPQN` keeps the four
rates.** The original offers `24` and `4`; libGravity offers 24, 4, 2 and 1. Two
extra values take nothing away, the original's user finds `24` and `4` where he
expects them, and the two others are capabilities the dependency already provides.
It is an **addition**, and it is assumed as one.

**`MODE` and `PPQN` are two views of ONE byte, and that is worth writing down.**
libGravity's list is exactly their product: `SOURCE_INTERNAL`, then EXT at 24, 4,
2 and 1, then `SOURCE_EXTERNAL_MIDI`. Two consequences follow, neither of them a
choice. `PPQN` costs **no EEPROM byte** -- the source byte carries both. And **no
inconsistent state is representable**: two separate bytes would allow `MODE = INT`
with `PPQN = 4`, which means nothing, while a derived view cannot lie.

So the version 3 image gains only `MOD` and `RANGE`: **306 bytes**.

⚠️ **`PPQN` NAMES THE INPUT, NEVER THE ENGINE.** The engine runs at **96 PPQN in
every mode**, INT included: `clock.h:59` calls `setOutputPPQN(PPQN_96)` once and
`SetSource()` never touches it. `SetSource()` only calls `setInputPPQN`, and only
for the external modes. One received pulse becomes 4 internal ticks at `PPQN = 24`,
24 at `4`, 48 at `2`, and 96 at `1`.

In `INT` there is no signal to read, so the field has no subject -- it is not
"`PPQN` = 96". In `MIDI`, `clock.h:114` forces the input to 24, the MIDI clock
standard, so the field has no subject either. That is exactly why the original
shows `PPQN` in EXT only, and a single field in MIDI.

**What decision 4a costs, written once so nobody rediscovers it.** The tempo range
is 30 to 300, so **270 values**. Without acceleration, crossing it takes 270
detents. The owner accepted that in exchange for one rule instead of one rule and
an exception.

**What decision 2 needs, and what it does not.** A simultaneous combination is not
a callback: libGravity reports a release. But `input::shiftHeld()` already exists,
so PLAY only has to ask whether SHIFT is down. The transport keeps PLAY alone.

## What the audit owed, and what it answered

**1. The `SRC` fusion.** The original separates `MODE` (INT / EXT / MIDI) from
`PPQN` (24 or 4 only). FlexSeq fused them into five values **and added** `EXT2`
and `EXT1`, which the original does not offer. Rule §1 restores the split; the
two extra rates are an addition to keep or to drop. **Owner's decision.**

**2. The step toggle, and the conflict it hides.** The original toggles a step
with **SHIFT**, and its encoder press in the editor starts **RECORDING**.
FlexSeq toggles with the encoder press, so the gesture RECORDING needs is
already taken. Deciding this now costs nothing; deciding it when RECORDING
arrives costs a change of habit. **Owner's decision.**

**3. The long-press threshold.** 300 ms in the original, 750 ms in FlexSeq. The
gesture card says about one second, so the card is not the source either.
**Owner's decision.**

**4. The encoder.** The original has **no acceleration** -- the code is there and
commented out -- and filters a reversal under **200 ms**. FlexSeq cancels
libGravity's acceleration everywhere except the tempo, and filters under 12 ms.
Its 12 ms rest on a measurement (`docs/open-risks.md` line 20); the original's
200 ms rest on nothing written. **Owner's decision**, and it frames lot 18.

**5. Skip chance to 100 %.** The original's UI stops at 90 %; only CV modulation
reaches 100 %. FlexSeq lets the user set 100 % directly. **Owner's decision.**

**6. `RANGE` returns, and the format follows.** The original has it, rule §1 keeps
it, and the version 2 image has no byte for it. The image would go to 305 bytes
and the version byte to 3. **Owner's decision** on the bump, not on the field.

## Two constants where FlexSeq diverges

Both were read in **`main`** (`Gravity.ino:14-16`), and `1.2-dev` has the same
values, so these are FlexSeq's own choices, not a branch difference.

| Constant | Original | FlexSeq | Verdict |
|---|---|---|---|
| Tempo range | `MINBPM 20` to `MAXBPM 200` | **20 to 300** since 2026-09-03 | **divergence DECIDED**, PRD §16 decision 8. The minimum aligns on the original; the maximum stays at 300, an addition that removes nothing, of the class of the four external clock rates. ⚠️ Keeping 300 also makes a persistence question disappear: the new range contains the old one, so no stored tempo can fall outside it |
| Trigger width | `PULSE_LENGTH 120`, so 12 ms | 5 ms | **divergence to decide**, and `docs/open-risks.md` line 12 already measures FlexSeq's real width at 4.7 to 5.0 ms |

The tempo range matters twice over: PRD §16 removed the encoder acceleration, so
the range is crossed detent by detent. 20 to 200 is 180 detents; 30 to 300 is 270.

## The large font of the original — decoded on 2026-09-04

`stkL` draws the main parameter of every tab. Its header was decoded from
`Gravity.ino:173`, so these figures are read and not estimated:

| Property | Value |
|---|---|
| Glyph count | **21** |
| Maximum glyph width | **15 px** |
| Glyph height | **23 px** |
| Weight | **569 bytes** |

**The 21 glyphs are accounted for**: the ten digits, then `/`, `x` and `%`, then
`A` and `B` for a pattern name, then `E`, `X`, `T`, `M`, `I` and `D` — because the
clock tab writes `EXT` and `MIDI` large in place of the number (`UI.ino:81-86`).

## Where the layout of the channel tab is read in the original — 2026-09-04

The lines of the original are not invented. Each part of the layout FlexSeq
rebuilds has one place in `Gravity.ino` and its companions:

| Part of the layout | Source in the original |
|---|---|
| the labels of the three lines | `UI.ino:136` |
| the values of the three lines | `UI.ino:180` |
| the baselines of the three lines | `UI.ino:126` |
| the value of the main parameter, large | `UI.ino:225` |
| the label under the main parameter | `UI.ino:200` |
| the three modes and their names | `UI.ino:126,145-150` |
| the destinations of `MOD`, internal clock only | `UI.ino:33,57-62` |

These references belonged to comments in `include/flexseq/MainScreen.h` until
2026-09-04. They live here now, because a code file carries no comment in this
repository and a reference lost with its comment is a documentary regression.

## What the two fonts of the original really cost — measured 2026-09-04

The original draws with **two** fonts: `stkL` for the main parameter, and
`velvetscreen` for every label. FlexSeq uses u8g2's `5x7` for everything.

PRD §12.1 estimated the change at "about 200 bytes", from the arithmetic
437 + 569 = 1006 against our single 804. **That estimate assumed the `5x7` font
would be dropped, which is only true if nothing reads it.** Three counter-builds
settle it, and `avr-nm` counted the fonts that survive in each image:

| Configuration | Flash | RAM | Fonts in the image |
|---|---|---|---|
| u8g2 `5x7` alone, today | 26236 | 1502 | 1 |
| `velvetscreen` alone, for the labels | **25870** | 1502 | 1 |
| `velvetscreen` and `stkL`, both | **26464** | 1502 | 2 |

✅ **THE SWITCH IS DONE since 2026-09-04, step 5a of lot 11, and the render is
re-established.** `velvetscreen` draws every label of `main.cpp`, of `env:wokwi` and of
`env:mainscreen`. The diagnostic firmware `env:bringup` keeps u8g2's font: it is
hardware-validated, it has no pixel proof, and its budget is separate.

**Measured, and each figure is read and not deduced:**

| What | Before | After |
|---|---|---|
| Flash | 26308 | **25942** — the switch RETURNS 366 bytes |
| RAM | 1506 | **1506** — unchanged |
| Ink on the EDIT screen | 808 px | **786 px** |
| The title | 155 px over 7 rows, y 56..62 | **99 px over 5 rows, y 56..60** |
| Steps at their place | 36/36 | **36/36** |

⚠️ **The steps did not move, and the reason is structural**: `drawDisc` draws them with
primitives, not with font glyphs. Only the text shrank.

**Two constants were coupled to the font by accident, and the switch exposed both.**
`TAB_BOX_Y` was `TAB_TOP_Y - 1`, so the highlight box of a tab depended on the height of
the glyphs; it now derives from the bottom of the screen, which is what the TypeScript view
already did. And `GLYPH_ASCENT` was a hardcoded 6; it now derives from the height of the
font. ⚠️ **That second one was called dead code and it was not**: `tools/screen_dump.cpp`
reads it, and a survey that covered `include/`, `src/`, `test/` and `sim/` missed `tools/`.
The compilation of the harness caught it.

✅ **The `tab_top_y` divergence of ADR 0012 is CLOSED.** C++ and TypeScript now both hold
59, and the line of the vector file moved from `cpp` to `both`.

**Moving the labels off u8g2's `5x7` RETURNS 366 bytes**, which matches
804 − 437 = 367 to within one byte: the unread font is genuinely dropped. Adding
`stkL` then costs 594. **The net cost of restoring the typography of the original
is therefore +228 bytes of Flash**, and the estimate of §12.1 was good.

⚠️ **RAM does not move at all**, in any of the three. u8g2 holds one font pointer,
and that field already exists.

✅ **FLEXSEQ REUSES THIS FONT, decided by the owner on 2026-09-04.** Two facts made
the decision. `GravityFW` and FlexSeq are both **GPLv3**, so the reuse is
compatible and **attribution is the only duty** — PRD §12.1 already settled that
for the navigation glyphs. And the width fits by construction: the original itself
draws `/128` inside the 55 px box of the main parameter.

⚠️ **This supersedes the estimate of PRD §12.1**, which planned **ten** glyphs of
our own at about 500 bytes, for the pattern name alone. The real need is **21**
glyphs, and the original's font answers it for 569 bytes. Against the Flash margin
of 3869 bytes measured on 2026-09-04, the cost is affordable.

## The modes, parameters and values of the original

The inventory of the three channel modes -- which parameter each carries, which
values it takes, and where its CV goes -- lives in `docs/original-modes.md`. It
was supplied by the owner on 2026-08-23 and checked line by line against the
pinned original. It also carries the two corrections that check produced: the
original runs at **24 PPQN**, and the skip chance has two ceilings.

## Runtime cost against the original — 2026-08-25

This pass answers one question: has FlexSeq made the module **slower or less
precise** than the firmware it replaces? The sources are the original firmware at
`main` @ `40d4aac`, the upstream library at `5c0c34f`, and measurements on the
current build.

### The fork changes no behaviour of the library

The pin `4c5b4d0` differs from the upstream head `5c0c34f` by **two files**: the
README, and 21 lines of `libGravity.h`. The lines make the display transport
selectable. A consumer that defines neither macro gets the previous class, so no
existing build changes. This is the charter of ADR 0008, and the diff holds to it.

### Where FlexSeq costs LESS than the original

| | The original | FlexSeq |
|---|---|---|
| Timer interrupt | FlexiTimer2 at **10 kHz** (`Gravity.ino:224`), and the trigger work runs in it | uClock's Timer1 at **48 Hz** at 120 BPM (`OCR1A = 41665`, prescaler 8) — about 200 times fewer interrupts |
| CV reading | up to **six** blocking `analogRead` calls per pass, the same pin read three times (`Interactions.ino:423-434`), about 104 µs each, so up to **0.6 ms of the pass** | a free-running ADC interrupt at about 9.6 kHz, and **zero** `analogRead` in the loop, which a test asserts. The interrupt is not free: **4.4 to 5.6 % of the CPU**, measured. It is paid outside the loop, and it samples continuously instead of once per pass |
| Display | a **whole frame** in one call (`u8g2.firstPage()` / `nextPage()`, `UI.ino:3` and `UI.ino:353`) | **one band per pass** (ADR 0001), so a render never blocks the loop for a whole frame |
| Flash | application **region** of 27648 B, read from the module backup on 2026-08-21. It is an upper bound, not the used size: the region above the last written page is blank | **27120 B** measured by the linker |

The first two lines are read from the sources. They are not measurements of the
original, which has never been run under the probes. The interrupt line compares
the **timer** only: FlexSeq adds an ADC interrupt the original does not have, and
the CV line carries its cost.

### Where FlexSeq costs MORE, and why that is accepted

**The trigger leaves the interrupt.** The original emits its triggers inside the
0.1 ms timer interrupt and clears the outputs there too (`Gravity.ino:263-312`),
so its trigger grid is 0.1 ms whatever the loop is doing. FlexSeq emits in the
main loop, so its precision is bounded by a loop pass. **Measured on 2026-08-25**:
jitter **1.00 to 1.23 ms**, median 0.57 to 0.65, and a step of **499.95 to
499.97 ms** against 500.00 expected at 120 BPM, which is 0.01 %. The pass itself
is p90 **6.50 ms** on the EDIT screen.

So the timing is about one order of magnitude coarser than the original's, and it
stays two orders below the step. Band rendering is what keeps it there: a whole
frame in one call would put the worst case at 46 ms.

**The playhead moves.** The original redraws only on a user event. FlexSeq
animates the playhead on EDIT, so the loop works where the original's is idle. The
main screen carries no time-varying element and almost never redraws, which is
why the probe measures the EDIT screen.

**Two constants diverge on purpose**, both already recorded below: the pulse is
5 ms configured (4.70 to 4.83 measured) against 12 ms, and the tempo range is 30
to 300 against 20 to 200.

### Lot A added no runtime cost

The 32-step pattern changed no per-pass work: the loop walks channels, not steps.
Measured after the lot, and equal to the figures before it: p90 **6.50 ms**,
median 5.01, band 3.35, whole frame 23.5 current and 46.0 complete, worst 13.87.
Flash **−50 B**, RAM **+85 B**.

⚠️ **One figure moved and it is the measurement, not the firmware.** The ADC's
share of the hardware CPU printed **4.4 %** in this run against **5.6 %** recorded
on 2026-08-21. The method is a ratio of medians over two regimes, so it varies
between runs. Re-read it the day it matters; do not treat either number as exact.

## What the audit did NOT cover

- `Gravity.ino`'s generation loop beyond the two points named above, and beyond
  the runtime comparison in the section above. The trigger
  path is covered by the domain tests and by `run-trigger-probe.sh`.
- The MIDI expander and the expansion header. PRD §16 leaves them out.
- CV calibration arithmetic. One defect of the original is recorded in
  `docs/upstream-defects.md`.
