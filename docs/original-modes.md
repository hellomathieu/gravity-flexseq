# The original firmware's channel modes — a verified reference

## Why it exists

FlexSeq keeps the three channel modes of the original, and the PRD keeps their CV
destinations (PRD §1, §4.2, §10). To keep them, we must know them exactly: which
parameters each mode carries, which values each parameter takes, and where the CV
goes.

The owner supplied this inventory on 2026-08-23. **Every claim below was then
checked against the pinned copy of the original firmware**, and the file and line
are given. Two claims needed a correction, and both are recorded at the end.

⚠️ **Which version this describes.** `main` @ `40d4aac` (2026-03-10, "clean up and
going public"), the public release and the **behavioural reference**, decided by
the owner on 2026-08-23. Its files are `src/Gravity/`, and every line reference
below is in that tree.

A second branch exists, `1.2-dev`, older and never merged. It carries five channel
modes, mute, a seventh channel, and patterns of up to 32 steps that own their
length. It is a **source of candidate features, not the reference**:
`docs/original-1.2-dev-features.md`.

This document is **tracking, never normative**. The normative sources are the
`PRD.md` for product decisions and `docs/decisions/` for architecture. It
describes the **original**, not FlexSeq.

## The shape

```
CHANNEL
├── MODE: CLOCK
│   ├── SUBDIVISION          <- main parameter
│   ├── OFFSET
│   └── MOD -> SUBDIVISION
├── MODE: RAND
│   ├── SKIP CHANCE          <- main parameter
│   ├── SUBDIV
│   └── MOD -> SKIP CHANCE
└── MODE: SEQ
    ├── PATTERN              <- main parameter
    ├── EDIT PATTERN         (an action, not a value)
    └── MOD -> PATTERN
```

`byte mode; //0 - CLK, 1 - RND, 2 - SEQ` (`Gravity.ino:63`), verified.

## The parameters, mode by mode

| MODE | PARAM | VALUES | CV destination |
|---|---|---|---|
| `CLOCK` | `SUBDIVISION` | `x24 … x2`, `/1 … /128` | `SUBDIVISION` through `MOD` |
| | `OFFSET` | `0 … PulsesPerStep - 1` | none |
| | `MOD` | `OFF` / `CV1` / `CV2` | — |
| `RAND` | `SKIP CHANCE` | `0 %` to `90 %`, in steps of 10 % | `SKIP CHANCE` through `MOD` |
| | `SUBDIV` | the same 20 values | none |
| | `MOD` | `OFF` / `CV1` / `CV2` | — |
| `SEQ` | `PATTERN` | `A1 … A8`, `B1 … B8` | `PATTERN` through `MOD`, **inside its bank** |
| | `EDIT PATTERN` | an action | — |
| | `MOD` | `OFF` / `CV1` / `CV2` | — |

The CV is not a parameter beside the main one: `MOD` selects which input drives
it. The targets are `byte CV1Target; //0 - Off, 1 - Subdiv, 2 - RND, 3 -
SeqPattern` (`Gravity.ino:65`), verified.

## SUBDIVISION — the 20 values, and their pulses

`subDivs[20]` (`Gravity.ino:56`), verified word for word:

```
-24 -12 -8 -6 -4 -3 -2  1  2  3  4  5  6  7  8  16  24  32  64  128
```

A negative value multiplies, a positive one divides. **The original runs at
`#define PPQN 24`** (`Gravity.ino:13`), so:

| Internal | Display | Pulses per step |
|---:|---:|---:|
| `-24` | `x24` | 1 |
| `-12` | `x12` | 2 |
| `-8` | `x8` | 3 |
| `-6` | `x6` | 4 |
| `-4` | `x4` | 6 |
| `-3` | `x3` | 8 |
| `-2` | `x2` | 12 |
| `1` | `/1` | 24 |
| `2` | `/2` | 48 |
| `4` | `/4` | 96 |
| `8` | `/8` | 192 |
| `16` | `/16` | 384 |
| `128` | `/128` | 3072 |

`channelPulsesPerCycle[i] = (playingModes[i] * PPQN) - 1` for a division, and
`(PPQN / abs(playingModes[i])) - 1` for a multiplication (`Gravity.ino:512-517`),
verified.

## OFFSET has no fixed range

It is bounded by the rate: `if (offset >= PulsesPerStep) offset = PulsesPerStep -
1;` (`Interactions.ino:150-151`), verified. So `x24` allows 0 alone, `/1` allows 0
to 23, and `/128` allows 0 to 3071. Turning down from 0 wraps: the value is a
`uint8_t`, and the code catches 255 to send it back to 0
(`Interactions.ino:258-259`).

## The master clock is not a channel mode

| Master clock | Parameter | Values |
|---|---|---|
| `INT` | `MOD` | `OFF` / `CV1` / `CV2` |
| `INT` | `RANGE` | 10, 20, 30, 40, 50 BPM |
| `EXT` | `PPQN` | 24 or 4 |
| `MIDI` | — | none |

`masterClockMode` is 0, 1 or 2 for INT, EXT and MIDI; `extClockPPQN` is 0 for 24
and 1 for 4, that is a sixteenth (`Gravity.ino:119`). `RANGE` is
`bpmModulationRange`, held between 1 and 5 (`Interactions.ino:232-236`) and
applied as `range * 10` BPM. Verified.

## The two corrections this inventory produced

**1. The original runs at 24 PPQN, FlexSeq at 96.** ADR 0004 and PRD §6.1.1 both
said "`pulseCount` counts 0 to 95 per quarter note". That was wrong: it counts 0
to 23. The conclusion did not change — `pulseCount == 0` is still the beat — and
both documents are corrected.

**2. The skip chance has two ceilings, and both are real.** The value the user
sets is clamped to **9** (`Interactions.ino:162-163`), which is the 90 % the
screen shows. The value **plus its CV modulation** is clamped to 10 in the
generator (`Gravity.ino`). They are different quantities, so the code is not
inconsistent.

⚠️ **FlexSeq's `MAX_SKIP_CHANCE` is 10, and it applies to the value the user
sets.** PRD §16 decided 9 on 2026-08-23. The contradiction is identified, not
reconciled: the owner assigned the fix to **lot 16**. Lot 19 made the value
reachable through SHIFT plus a rotation on the tab bar, so the gesture currently
offers eleven steps instead of ten.

## What this document does not cover

- The **layout** of each screen, field by field: that is
  `docs/original-conformity.md`.
- The **gestures**: same document.
- FlexSeq's own evolution of SEQ — 32 steps, LENGTH, ratchets: PRD §5 and §6.
