# Conformity to the original firmware — the inventory

**Lot 15, 2026-08-23.** Tracking document, **never normative**. Every decision it
raises belongs to the Notion PRD; this file only says what was read and where.

## Why it exists

Three times, FlexSeq shipped a screen that held less than the original's. The
three channel modes were absent from the domain (2026-08-22), the BPM tab kept
one field of four, and the tab bar lost its Play/Stop indicator (both found on
the module, 2026-08-23). Each time the screen had been built from a design
instead of from the original's own drawing code.

The owner's rule, restated on 2026-08-23 and now PRD §1: **keep every original
feature and every original page; only the SEQ mode evolves.**

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
| Main parameter, CLOCK | value `/N` or `xN`, label `SUBDIVISION` (`UI.ino:190-200,213-219`) | pattern name, always | **omission** — lot 11 |
| Main parameter, RAND | value `N0%`, label `SKIP CHANCE` | pattern name, always | **omission** — lot 11 |
| Main parameter, SEQ | value `A1` to `B8`, label `PATTERN` | pattern name | conform in content, not in size |
| `MODE` | `CLOCK` / `RAND` / `SEQ` (`UI.ino:126,145-150`) | absent | **omission** — lot 11 |
| Field 2, CLOCK | label `OFFSET:`, value `offset/pulsesPerStep` (`UI.ino:128,151-159`) | absent | **omission** — lot 11 |
| Field 2, RAND | label `SUBDIV:`, value `/N` or `xN` (`UI.ino:130,160-165`) | absent | **omission** — lot 11 |
| Field 2, SEQ | label `EDIT PATTERN`, a filled box as the entry marker (`UI.ino:132,174-178`) | an entry field | conform |
| `MOD` | `OFF` / `CV1` / `CV2` (`UI.ino:134,166-172`) | absent | **omission** — lot 13 |
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

## The gestures

| Gesture | Original | FlexSeq | Verdict |
|---|---|---|---|
| Rotate, tab bar | changes tab, **clamped** 0 to 6 (`Interactions.ino:110-116`) | changes tab, **wraps** | **divergence to decide** |
| Rotate, inside a tab | moves the menu item (`Interactions.ino:178-184`) | moves the cursor | conform |
| Rotate, a field open | changes the value | changes the value | conform |
| Press, short | enters the tab, or opens the field (`Interactions.ino:13,51`) | same | conform |
| Press, long | goes back one level, threshold **300 ms** (`Interactions.ino:73-82`) | goes back, threshold **750 ms** | **divergence to decide** — question 3 |
| SHIFT + rotate, tab bar | changes the tab's **main parameter** (`Interactions.ino:117-171`) | changes the tab's main parameter: SUBDIV in CLOCK, the skip chance in RAND, the pattern in SEQ, the tempo on the clock tab | **conform** — closed by lot 19, 2026-08-23. The value is edited before it is **displayed** for CLOCK and RAND: the display is lot 11 |
| SHIFT + rotate, in a tab | changes the selected value | changes the value | conform |
| SHIFT + press, editor | **toggles the step** under the cursor (`Interactions.ino:397-399`) | encoder press toggles it | **divergence to decide** — question 2 |
| SHIFT held long, editor | **clears the pattern**, threshold **500 ms** (`Interactions.ino:411-415`) | clears it, threshold 750 ms | conform in substance |
| SHIFT + encoder over 2 s | opens SETTINGS (`Interactions.ino:83-84`, `416-419`) | absent | **omission**, kept by decision 2026-08-23 — lot 16 |
| Press, editor | starts and stops **RECORDING** (`Interactions.ino:53-57`) | toggles the step | **conflict** — question 2 |
| PLAY | starts and stops the clock (`Interactions.ino:369+`) | same | conform |
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
| Fonts | two: `stkL` for the main value, `velvetscreen` for the rest | one, `u8g2_font_5x7_tr` | **omission** — line 29 |

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

## The modes, parameters and values of the original

The inventory of the three channel modes -- which parameter each carries, which
values it takes, and where its CV goes -- lives in `docs/original-modes.md`. It
was supplied by the owner on 2026-08-23 and checked line by line against the
pinned original. It also carries the two corrections that check produced: the
original runs at **24 PPQN**, and the skip chance has two ceilings.

## What the audit did NOT cover

- `Gravity.ino`'s generation loop beyond the two points named above. The trigger
  path is covered by the domain tests and by `run-trigger-probe.sh`.
- The MIDI expander and the expansion header. PRD §16 leaves them out.
- CV calibration arithmetic. One defect of the original is recorded in
  `docs/upstream-defects.md`.
