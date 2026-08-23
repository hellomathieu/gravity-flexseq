# Upstream defects

Defects found in the two Sitka Instruments sources FlexSeq depends on or refers
to. This file exists to make an upstream contribution possible: each entry names
the defect, where it is proven, and how large the fix looks.

It is **not** a source of truth for FlexSeq's own design. What FlexSeq does about
each defect belongs to the PRD (§18 for the reachability audit) or to
`docs/open-risks.md` (lines 14, 20, 21). Here we describe the dependency, not our
workaround.

## Scope and honesty

**`libGravity` is audited; the original firmware is not.** The libGravity list
below is complete against commit `9be88be1f4` and every entry is reproduced by a
test in `env:native_libgravity`. The original firmware has only ever been read as
a behavioural reference, so its single entry is what happened to surface, not the
result of a sweep. Do not read the short list as a clean bill of health.

## libGravity, pinned at `9be88be1f4`

Verified 2026-08-20: none of these is fixed upstream three commits after the
pinned one.

| # | Defect | Proven by | Fix looks like |
|---|---|---|---|
| 1 | `AnalogInput::IsRisingEdge()` never reports a negative-to-positive crossing. `old_read_` is `uint16_t` while `read_` is `int16_t`, so a negative previous value becomes a large positive and "it was already high" is true whenever it was in fact negative. That is the most ordinary case on a bipolar input | `test_analog_input`, 2 assertions | one type |
| 2 | `Button` loses a release that falls inside the debounce window | `test_button`, 1 assertion | moderate — the state machine has to remember the release |
| 3 | `Encoder` starts in an unsafe state: `previous_pos_` is not initialised, so the first poll can report a movement that never happened | `test_encoder`, 2 assertions | one initialisation |
| 4 | `Encoder` has no debounce at all, and the code says so: `_rotate_change()` carries `// Validation (TODO: add debounce check)`. ⚠️ **The consequence written here until 2026-08-23 was REFUTED on the module**: two fast detents do NOT cancel each other. Measured, they give exactly −2, and fifteen seconds of fast rotation give −917 with zero reversals. What is real is a bounce **at a turning point**, the fastest measured reversal being 2 ms | hardware, `docs/open-risks.md` line 20 | an enhancement, not a one-liner |
| 5 | `DigitalOutput::Init()` does not force the pin OFF, and does not turn off an output that is already active | `test_digital_output`, 2 assertions | one write |
| 6 | `Gravity::Process()` has an uninitialised loop index — `for (int i; i < OUTPUT_COUNT; i++)` — which is undefined behaviour | read; `test_gravity` pins the function's composition | one initialisation |
| 7 | `Clock::SetSource()` does not handle the `SOURCE_LAST` sentinel, which surfaces as a compiler warning rather than wrong behaviour | read | one guard |
| 8 | Packaging: a consumer must declare `NeoHWSerial @ 1.6.9` explicitly; the library does not pull it | build | manifest |

Six of the eight are one to three lines.

**Encoder direction is not on this list, deliberately.** It depends on how the
two pins are wired, and both projects expose the setting —
`Encoder::SetReverseDirection(bool)` here, `reverseEnc` in the original. Their
defaults are simply opposite. See PRD §4.1.

## Original firmware (`GravityFW`)

| # | Defect | Established by |
|---|---|---|
| 4 | **`CV2Calibration` is dead in the reading path: both inputs are conditioned by `CV1Calibration`.** The variable exists, is saved to EEPROM and is loaded back (`Gravity.ino:53,596,653`), but the code that turns an ADC reading into a value compares input 2 against `CV1Calibration` (`Interactions.ino:432-438`). So calibrating CV2 changes a stored byte and nothing else, and CV2's zero point is CV1's | read, 2026-08-23, during the conformity audit |
| 3 | **The encoder acceleration is present and commented out** (`Interactions.ino:100-104`), so the original accelerates nothing. Its only filter drops a reversal under 200 ms. This is not a defect of the original; it is recorded here because FlexSeq inherited an acceleration from libGravity that the original never had, and cancels it | read, 2026-08-23 |
| 2 | `channel::CV1Range` and `channel::CV2Range` are **dead fields**: declared in the struct, never read, never written, never displayed. The per-channel CV amplitude is hard-coded instead (`map(randMod, 0, 1023, -5, +5)`). They are nonetheless persisted, so 2 of every channel's 9 EEPROM bytes carry nothing -- 12 bytes across the six channels | read, 2026-08-22 |
| 1 | `saveState()` rewrites the whole state — more than 300 bytes — on every recording keystroke, so an EEPROM write of ~3.4 ms lands in the middle of a musical event | read; PRD §11 records the consequence for FlexSeq's own persistence |

## Before proposing anything upstream

Three things to settle first, none of them technical.

**Where.** Neither project is on GitHub. Both live on `git.sitkainstruments.com`,
so the contribution mechanism has to be checked before a patch is written.

**Our tests would invert.** `env:native_libgravity` documents these defects: 7 of
its 68 assertions fail *by construction*, and the runner fails on drift **in
either direction**. An upstream fix would therefore turn our suite red, and the
`EXPECTED` set plus `test/README` must be updated in the same move. That is the
intended behaviour, not an obstacle — but it is work.

**Nothing reaches us on its own.** FlexSeq pins `9be88be1f4` by decision. An
accepted fix changes nothing here until the pin is deliberately moved, and moving
it requires re-auditing `Gravity::Process()` (PRD §18).
