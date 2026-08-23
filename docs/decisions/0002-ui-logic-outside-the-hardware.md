# 0002 — UI logic outside the hardware, and one place for the dependency's flaws

- **Status:** accepted
- **Date:** 2026-08-22
- **Supersedes:** —
- **Superseded by:** —

## Context

Nothing in the production firmware is controllable. `main.cpp` attaches the
96 PPQN clock handler and nothing else: no callback on either button, none on the
encoder, no clock source, no tempo. Wiring the UI and the transport is therefore
the next piece of work, and it is the moment when several known flaws of the
pinned `libGravity` stop being latent.

Four of them bear on the controls, and they are not hypotheses. Three were
measured on the module on 2026-08-22, with `env:bringup`:

- the encoder turns the **wrong way** relative to the original firmware, because
  the two libraries default in opposite directions;
- the encoder has **no debounce**, and the dependency says so — its
  `_rotate_change()` carries `// Validation (TODO: add debounce check)`.
  ⚠️ **The observation that first justified this was REFUTED on 2026-08-22**: two
  fast detents do not cancel, they give exactly −2, and fifteen seconds of fast
  rotation accumulate −917 with zero reversals. Bounces are real but occur **only
  at a fast turning point**, measured at 2 ms against 509 ms for the fastest
  deliberate reversal — so the 12 ms window is validated on facts, and it fires far
  more rarely than the design assumed;
- the encoder **accelerates**, ×3 under 16 ms, and two events can arrive in the
  same millisecond. On a continuous value that is wanted; on discrete navigation
  it turns one detent into three moves, which is what the owner described as
  chaotic on the module. This is the dependency's real cost at the input, and it
  is absorbed in `UiController` rather than in `EncoderFilter`;
- `Encoder` may report a **false first movement**, its `previous_pos_` being
  uninitialised;
- `Clock::SetSource()` does not handle the `SOURCE_LAST` sentinel, so a source
  field that cycles through the enum must never reach it.

A fifth constraint is not a flaw but a shape: `Button`'s callbacks fire on
**release**, which is what lets short and long presses be told apart. A control
that must react while held — the SHIFT modifier — therefore has to read the pin
with `On()` rather than wait for a callback.

The interaction model itself is a product decision and lives in **PRD §12**. This
ADR is only about where the code that implements it goes.

## Decision

**The UI logic is a pure component in the domain, and the hardware's flaws are
absorbed in exactly one file.**

Three pieces:

**`UiController`** holds the UI state — current tab, level, cursor, open field,
selected channel — and acts on `SequencerEngine` and `PatternBank`. It takes
**eight abstract events** as input: `Rotate`, `RotateHeld`, `Press`, `LongPress`,
`ShiftRotate`, `ShiftPress`, `ShiftLongPress`, `PlayPress`. It knows nothing of
Arduino, of libGravity, or of the display, so it is tested in `env:native` like
the rest of the domain.

**`InputAdapter`**, in `src/hal/`, is the **only** piece that touches
libGravity's controls. It attaches the callbacks, reads SHIFT's pin state, and
turns all of it into those eight events. Every constraint listed above is handled
here and nowhere else.

It is **split in two**, and for a reason that only appeared when the tests were
planned (decided 2026-08-22). Testing the adapter natively would need the Arduino
stubs and libGravity's headers — the setup of `env:native_libgravity`, whose
criterion is that **exactly** seven assertions fail by construction. Adding
FlexSeq acceptance tests there would break that invariant, and a third
environment is not worth creating. So the delicate logic moves into
**`EncoderFilter`**, a pure component that takes deltas and timestamps and
applies the debounce and the false-first-movement suppression: it is tested in
`env:native` like everything else. What remains in `InputAdapter` is glue thin
enough that hardware verification is the appropriate check for it.

**`UiController` is mirrored in TypeScript** (decided by the owner 2026-08-22).
The parity rule of PRD §13 applies: the two sides must not become two independent
specifications. The mirror takes the same eight events, so the simulator
exercises the same state machine rather than a second one written for a mouse.

⚠️ **What it does not give.** A simulator driven by keyboard and mouse cannot
reproduce a **held modifier**, a **rotate-while-pressed**, or the **mechanical
bounce of a detent** — which is exactly the regime where the dependency's missing
debounce bites. So the mirror verifies the **logic**, never the gesture. It is a
comfortable place to check intended behaviour, not a substitute for the module.
The owner said as much on 2026-08-22, and the distinction is recorded here so
nobody later mistakes a green simulator for a verified interaction.

The cost is real: roughly a doubling of this component's work, and every scenario
covered on both sides.

**The renderer** gains a footer band, and `PagedScreen` gains a second skippable
band. See below.

## Consequences

**Every gesture becomes a native test.** "SHIFT held plus rotation changes
channel" is verifiable without plugging anything in. That matters more here than
elsewhere: the interaction model has many small rules, and a rule that only the
hardware can check is a rule nobody checks.

**One file to re-read at the next pin bump.** The four constraints live together.
Today they are scattered across an audit table (PRD §18) and three lines of the
risk index; after this they are a single unit of code with a single set of tests.

**The band skip of ADR 0001 becomes two-sided, and its argument stays the same
shape.** That ADR skips the title band when the title has not changed, on a
**geometric** argument: a band entirely above the header rule can only contain
the title. The same argument holds at the bottom — a band entirely below the
lowest ratchet digit can only contain the footer. Measured geometry: the lowest
drawn pixel of the second row is the ratchet digit at y 47, and band 7 spans
y 56 to 63, so it is empty today. A footer with its baseline at 63 lands exactly
in band 7, with 8 pixels of clearance.

The cost is a second hash and a second flag, a few bytes of RAM. The benefit is
the same as for the title: the footer carries the channel and the tempo, both of
which change rarely, so the band is skipped on most frames. Band 7 is already
cleared and sent on every frame, so only the rasterisation is added — about
0.59 ms per character.

**The encoder long press needs a `Button` of our own.** libGravity's `Encoder`
exposes the short press, the rotation and the rotate-while-held, and nothing
more; its internal `Button` knows about long presses but is private. A second
`Button` on `ENCODER_SW_PIN` gives us the long press, and the two do not collide:
`Encoder` fires its callback only on `CHANGE_RELEASED`, ours only on
`CHANGE_RELEASED_LONG`. A short press wakes one, a long press the other.

✅ **Verified on the module, 2026-08-22.** From the tab bar: a short press enters
the tab **once**, a rotation moves the cursor, and a long press returns to the bar
**once**. Neither gesture fires twice, so the two `Button` objects do not answer
the same event -- exactly as reading the code predicted. The fallback of driving
the pin directly is not needed and stays unused.

The encoder direction was verified in the same session: `SetReverseDirection(true)`
turns the module the way the original does, left and right both correct.

**The audited `Button` anomaly gets no treatment** (decided by the owner
2026-08-22, on the condition that neither the legacy behaviour nor FlexSeq is
affected). The analysis that satisfies the condition: the anomaly's worst effect
is a **missed callback**, never a stuck state — after a lost release the pin reads
as released, so the next press is seen normally. RECORDING (PRD §5.5) reads SHIFT
as a **pin state** rather than a callback, so it is not on this path at all. And
the original firmware reads its buttons with a direct `digitalRead`, so it does
not share the path either. The anomaly needs a contact shorter than
`DEBOUNCE_MS = 10`, which a finger does not reach.

What would **falsify** that analysis, and therefore belongs in the acceptance
criteria of the flash milestone: the same series of fast taps run against the
**production** loop, which polls about 125 times a second. `env:bringup` blinds
itself for part of every cycle, so its losses measure the diagnostic; with that
window gone, only the real 10 ms threshold remains.

**Estimated cost: about 15 bytes of RAM** — 7 for the UI state, 2 for the footer
hash, 6 for the adapter. PRD §15 budgeted ~16 bytes for the wired UI against 264
available, so the estimate holds.

**Measured 2026-08-22, and the estimate was low.** `UiController` exists on both
sides now. Wired through a temporary call site in `main.cpp` and then reverted,
the AVR cost is **RAM +26 bytes, Flash +1224 bytes** — against 15 estimated
above, and against the 2 to 4 kB PRD §15 allows for the whole UI. The state
machine alone therefore spends between a third and two thirds of the Flash
envelope, which is the number the Flash arbitration of PRD §12.1 needs. Nothing
calls it yet, so the committed build is unchanged at 1528 / 21404 and the drift
baseline is untouched: the figure is a measurement, not a regression.

**Two structural points the implementation settled.** `selectedChannel` is
**derived** from the current tab rather than stored, so the tab bar and EDIT's
channel change cannot disagree about which channel is current. And the value
clamp lives in `UiController` even though `SequencerEngine` already rejects
out-of-range values: the two are not redundant, because libGravity multiplies a
fast detent by 2 or 3, and an unclamped delta of +3 from LENGTH 23 is *refused*
by the engine where a clamp lands on 24. A mutation sweep is what surfaced this
— the first pass did not detect two mutations, for exactly that reason. The test
they exposed now exists on both sides. The mutation score is 22 of 22 on each
side.

## Alternatives set aside

**Putting the UI logic in `main.cpp`.** It is where the wiring naturally lands,
and it would spare an indirection. Set aside because nothing in `main.cpp` can be
tested: it needs Arduino and libGravity to compile. The interaction model would
then be verified only by hand, on hardware, one gesture at a time.

**Handling each flaw where it is felt.** Reversing the encoder where the cursor
moves, debouncing where the ratchet is set, and so on. Set aside because the
flaws belong to the dependency, not to the features: scattering them would mean
finding them all again at the next pin bump, and PRD §18 exists precisely so that
this list has one home.

## References

- PRD §12 — the interaction model, the tab bar, the eight gestures, the font
- PRD §8 — the transport and its four wirings
- PRD §18 — the reachability audit of libGravity's anomalies
- ADR 0001 — the spread render, whose band skip this decision generalises
- `docs/open-risks.md` lines 14, 20, 21 — the constraints, and where each was measured
- `docs/upstream-defects.md` — the same flaws seen from the dependency's side
- Measured 2026-08-22 on the module with `env:bringup`
