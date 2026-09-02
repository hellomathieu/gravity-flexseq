# gravity-flexseq-sim

TypeScript proof of concept, **isolated from the firmware**: a reference
behavioural model, mirror tests, and a visual simulator (an avr8js backend is
still to come).

> This workspace is **not** compiled by PlatformIO (`build_src_filter` covers
> `src/` only). It creates no runtime dependency of the firmware on
> Node.js / TypeScript.

## Role

- Reference functional model, a behavioural mirror of the C++ domain.
- Fast tests that run without hardware.
- Basis for the Gravity Simulator UI, with two backends: the TS model and the
  real AVR firmware executed by avr8js.

## Parity contract

TS ↔ C++ parity targets **behaviour, not memory**. Packing constraints
(`sizeof(Pattern) == 23`, `PatternBank == 368 B`) are checked on the **C++/AVR
side only**. Each `it(...)` in `test/Pattern.test.ts` corresponds to a
`RUN_TEST(...)` in `../test/test_pattern/test_pattern.cpp`.

## Where the mirror stops

The model carries the engine, the CV chain, the transport, the patterns, the
persistence codec and the UI controller. It does not carry everything, and the
gaps are deliberate.

**The hardware boundary stays out.** `CvGate`, `CvSampler`, `EepromStorage`,
`InputAdapter`, `TransportAdapter` and `TwiDisplay` have no mirror. The screens
do not either: `MainScreen`, `PagedScreen` and `PatternScreen` live on the C++
side, and the simulator draws its own view. `ChannelMode` and `Subdiv` ARE
mirrored, inside `SequencerEngine.ts` and `subdiv.ts`.

**One divergence sits INSIDE the shared domain, and it is a decision.** The C++
firmware holds a modulation buffer, `ModulatedPatternState`, which a round robin
fills from the EEPROM templates. The model holds none: `ModulatedPatternState`
has zero occurrence in `sim/src`. The buffers, the round robin and `Storage`
belong to the AVR firmware, and decision 5 of lot E3.6.4.4 left them out on
2026-09-01.

The consequence has a name. `patternForChannel()` exists on both sides and it
does NOT mean the same thing:

- in C++ it returns the modulation buffer when a template is loaded for that
  channel, and the instance otherwise;
- in TypeScript it always returns the instance.

**So P35 has no mirror.** P35 asks that the content and the ratchet of a step
come from the same pattern, on the boundary where the derived PATTERN index
changes. That boundary needs the buffer, so the model cannot express it. The
divergence is characterised on the C++ side alone, and `docs/open-risks.md`
line 77 carries it.

## What a green model proves, and what it does not

A green TypeScript suite says the two languages agree on what BOTH carry. It
does not say the firmware is correct.

- It proves nothing about memory: `sizeof`, Flash and the stack are measured on
  the AVR side, and the section above says so.
- It proves nothing about timing: the loop, the display bands and the ADC under
  interrupt are measured by the probes in `tools/`.
- It proves nothing about a path the model does not hold, and the buffer is the
  one such path inside the domain today.

Read a green suite for what it says. Do not read it as coverage of the firmware.

## Commands

```bash
cd sim
npm install
npm test          # vitest run (single pass)
npm run test:watch
npm run typecheck # tsc --noEmit
```

## State

- [x] `Pattern` — shared content (36 steps + per-step ratchets, no length). The
      EDIT grid shows the 36 of them in three rows of twelve since lot F, so
      `STEP_COUNT` and `GRID_STEPS` are 36 on both sides. `MAX_LENGTH` is 36
      too since lot SF3, 2026-08-30: a channel plays every step it stores.
- [x] `PatternBank` — **one shared bank of 16 patterns** (replaces the former 6×16).
- [x] `SequencerEngine.selectedPattern` and **per-channel** LENGTH; shared pattern editing.
- [x] Simulator UI — EDIT PATTERN skeleton, TS backend.
- [x] `SequencerEngine` — `masterPhase` (96 PPQN ticks), per-channel `effectiveStep`.
- [x] Transport in the simulator — Play/Stop/Reset and the playhead on the OLED.
- [x] Per-channel active pattern, and a multi-channel strip (6 simultaneous playheads).
- [x] `SequencerEngine.hasStepped` and `TriggerSequencer` (C++ parity); per-channel **trigger flash** in the sim.
- [x] **SUBDIV → ticksPerStep** per channel (libGravity's 96 PPQN convention) and a SUBDIV selector (C++ parity).
- [x] **2026-08-17 revision — one step is one time unit.** METER and MEASURES **removed**; bar separation is **purely graphical** (none/2/3/4/6, per channel).
- [x] **Per-step RATCHETS** (`2·3·4·6` = N triggers inside the step; `▲` triplet = 3 over 2 units, stretching time). Ratchet 5 set aside: not representable at 96 PPQN.
- [x] **EDIT PATTERN screen** aligned on the Wokwi proof of concept (`flexseq-oled-playground/sketch.ino`): 2×12, 10 px pitch, 5×5 `○`/`●` glyphs, `▲` triplet, `.` outside the pattern, ratchet digit, bar lines, 9×9 frame, centre pixel inverted for the step being played.
- [x] **2026-08-30 revision — the engine no longer knows the bank.** The two
      entries above that speak of *one shared bank* and of *shared pattern
      editing* record what was delivered then, and the model has since changed.
      `SequencerEngine` holds **six pattern instances**, one per channel, and
      `patternForChannel()` returns the instance: editing one channel affects no
      other. The templates live in EEPROM (PRD §5.0, ADR 0006). Lot B4b.7 removed
      `setPatternBank()` and the resident bank from the firmware, which returned
      **370 bytes of RAM**. ⚠️ The class `PatternBank` stays: `PersistentImage`
      v2, `loadFactoryPatterns`, the image generator and `PATTERN_COUNT` use it,
      so `PatternBank == 368 B` above is still true.
- [ ] Real Transport layer (clock/MIDI); C++ port of the engine.
- [ ] avr8js backend, wired onto the same `SimBackend` seam.

> The playhead on the EDIT screen is a simulator aid; the PRD does not require it
> as a firmware feature. The simulator's clock converts real time into 96 PPQN
> ticks according to the BPM — a role the Transport layer takes over later.

### Running the simulator

```bash
cd sim
npm run dev   # http://localhost:5173 (Vite, hot reload)
```

Architecture: `src/web/main.ts` (UI) → `src/sim/backend.ts` (`SimBackend`, the
seam for wiring avr8js in later) → `src/domain/*` → `src/sim/PatternView.ts`
(pure rendering, tested).

On the C++ side, the hardware-free test loop is `./tools/run-cpp-tests.sh`
(`pio test -e native`). Measured parity as of 2026-08-23: **TS 235 tests across
14 files**, **C++ native 278 assertions across 13 modules**. The counts are not
expected to match: the C++ suite also covers the paged screen and the CV gate,
which have no TS mirror.
