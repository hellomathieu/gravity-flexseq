# `1.2-dev` — a catalogue of features, not the reference

## Status, and it matters

**`main` @ `40d4aac` is the behavioural reference.** The owner decided this on
2026-08-23. That is the public release, the newest commit, and the version every
other document describes.

**This document describes a different branch**, `1.2-dev`, read at commit
`f7b2150acf` (2025-03-11). It is a **catalogue of candidate features**, nothing
more. No line here is a requirement, and no line here overrides
`docs/original-conformity.md` or `docs/original-modes.md`.

Two facts to keep in mind before reading:

- `1.2-dev` is **older** than `main` and was **never merged**. `main` is
  2026-03-10; the tip of `1.2-dev` is 2025-04-08. The branches diverged: 4
  commits on one side, 28 on the other.
- The owner's module therefore most likely ran `main`. Nobody has identified the
  version on the module's backup.

The file layout differs too: `Software/Gravity/` here, `src/Gravity/` on `main`.
Every line reference below is in `Software/Gravity/`.

## Five channel modes instead of three

```cpp
byte mode : 3; //mv: 7. 0 - CLK, 1 - RND, 2 - SEQ, 3 - SWING, 4 - Gate
```

| Mode | Value | Main parameter | Field 2 |
|---|---|---|---|
| `CLOCK` | 0 | `SUBDIVISION` | `OFFSET:` |
| `RAND` | 1 | `SKIP CHANCE` | `SUBDIV:` |
| `SEQ` | 2 | `PATTERN` | `EDIT PATTERN` |
| **`SWING`** | 3 | `SWING` | — |
| **`GATE`** | 4 | the computed gate length | `SUBDIV:` |

### SWING — the odd sixteenths move

SWING plays sixteenths, like the sequencer:
`channelPulsesPerCycle = (PPQN / 4) - 1`, so the counter runs 0 to 5. The output
fires at counter 0 on an even step, and at counter `swing` on an odd one:

```cpp
(mode == 3 && channelPulseCount[i] == 0 && stepIsOdd == 0)
|| (mode == 3 && channelPulseCount[i] == channels[i].swing && stepIsOdd == 1)
```

`swing` is 3 bits, so 0 to 7 — but a sixteenth is only 6 pulses at 24 PPQN.

⚠️ **Read, not measured: a swing of 6 or 7 appears to fire nothing** on the odd
sixteenths, because the counter wraps at 5 and never reaches the value. This is
an observation from the code, and it is not confirmed on hardware.

### GATE — a length, not a pulse

GATE fires on the step boundary like the others, but the output stays high for a
**share of the step** instead of a fixed pulse:

```cpp
gateLengthTime[channel] =
    ((channelPulsesPerCycle[channel] + 1) * (pulsePeriod - 1) / 100) * channels[channel].gate;
```

`gate` is 7 bits, and it is used as a percentage. The main loop treats the mode
apart: `if (channels[i].mode != 4 && tickCount >= PULSE_LENGTH)` turns the output
off for every mode **but** gate, and gate uses its own countdown.

This is the only mode whose output is not a fixed-width trigger.

## Mute, per channel

```cpp
bool isMute : 1;
```

`SHIFT` plus `PLAY` toggles it (`Interactions.ino`, comment
`//shift + play = mute channel`). It wraps the whole emission block, so a muted
channel is silent in every mode.

## A seventh channel

```cpp
const byte outsPins[] = { 7, 8, 10, 6, 9, 11, clockOutPin };
byte extraChannel = 0; // 0 - off, 1 - pulse out = 7th channel
```

The clock output becomes a seventh channel when the flag is on, and the loops run
`for (byte i = 0; i < (extraChannel ? 7 : 6); i++)`. SETTINGS carries the switch:
`7TH CHANNEL: ON` / `OFF`, an entry `main` does not have.

## Patterns: 32 steps, and the pattern owns its length

```cpp
struct sequence {
  uint32_t sequence;
  uint8_t length : 5;   // "don't forget to add 1", so 1 to 32
};
```

Sixteen patterns, up to **32 steps** each, and **the length belongs to the
pattern**. `main` has sixteen fixed 16-step arrays and no length at all.

The editor draws `length + 1` steps, eight per row:

```cpp
for (byte i = 0; i <= sequences[patternToEdit].length; i++) { ... }
u8g2.drawFrame(16 + (stepNumSelected % 8) * 12, 10 + ((stepNumSelected / 8) * 11), 11, 11);
```

So up to four rows of eight, against two rows of eight on `main` and two rows of
twelve in FlexSeq.

## The channel record is bit-packed — five bytes

```cpp
byte mode : 3;  byte subDiv : 5;  byte random : 4;  byte seqPattern : 4;
byte CV1Target : 3;  byte CV2Target : 3;
uint8_t swing : 3;  uint8_t offset : 7;  uint8_t gate : 7;  bool isMute : 1;
```

Forty bits. **`CV1Range` and `CV2Range` are gone**, so the two dead fields that
`docs/upstream-defects.md` records for `main` do not exist here.

## Constants that differ from `main` and from FlexSeq

| | `main` | `1.2-dev` | FlexSeq |
|---|---|---|---|
| PPQN | 24 | 24 | 96 |
| SUBDIV values | 20 | **24** (adds 9, 10, 11, 12) | 25 (those 24 plus `x16`) |
| Outputs | 6 | 6 or **7** | 6 |
| Steps per pattern | 16, fixed | **1 to 32**, per pattern | 24 |

Only the last three rows are differences **between the branches**. Two constants
that look like `1.2-dev` specifics are not: `MAXBPM 200` / `MINBPM 20` and
`PULSE_LENGTH 120`, that is 12 ms, are in **both** branches. They are therefore
divergences of **FlexSeq** against the reference, and they belong in
`docs/original-conformity.md`, not here.

FlexSeq's SUBDIV list **contains** the one in `1.2-dev`, so nothing has to be
removed on that point.

## What this catalogue does not do

It takes no decision. The decisions each of these features would force -- on the
PRD, on the RAM budget, on the EEPROM format -- are listed in `WORKPLAN.md`,
under the review of the reference version.
