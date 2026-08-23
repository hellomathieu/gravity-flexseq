# 0001 — Non-blocking main loop: the OLED render spread across its 8 bands

- **Status:** accepted
- **Date:** 2026-08-20
- **Supersedes:** —
- **Superseded by:** —

## Context

PRD §12 decided to **reuse libGravity's display object**, `gravity.display`, of
type `U8G2_SSD1306_128X64_NONAME_1_HW_I2C`, rather than instantiate a second U8g2
object. The `_1_` mode allocates only **128 bytes** of buffer where the screen is
128 × 64 pixels, that is 1024 bytes.

Direct consequence of that choice: U8g2 renders a frame as **8 successive
horizontal bands** through `firstPage()` / `nextPage()`. The drawing function
therefore runs 8 times per refresh and must stay pure — a constraint already
documented in `include/flexseq/PatternScreen.h`.

Facts measured, or verified in the sources:

- The I2C bus runs at **400 kHz**. U8g2's SSD1306 descriptor declares
  `i2c_bus_clock_100kHz = 4` (`u8x8_d_ssd1306_128x64_noname.c:348`) and the
  Arduino backend applies that value on every transfer (`U8x8lib.cpp:1361` and
  `:1367`). Neither libGravity nor FlexSeq sets it: `initDisplay()` merely calls
  `display.begin()`.
- A full frame is 1024 bytes of display data, on the order of **25 ms** of bus
  time at 400 kHz. *Estimate of 2026-08-20, corrected the same day by
  measurement: **36.8 ms** of bus, the estimate counting only the bits and not
  the per-chunk software overhead.*
- The 8 bands were chained **within a single call** (`src/main.cpp`): the main
  loop was blocked for the whole frame. That is what this decision replaces; it
  has been implemented since 2026-08-20.
- The render is limited to one frame every 40 ms, and only when the screen has
  changed (`UI_MIN_INTERVAL_MS`). That limit reduces the **frequency** of the
  blockages, not their **duration**.
- The main loop is where ticks are drained and triggers emitted, and an output
  can only be re-armed once per drain.
- **`Gravity::Process()` touches neither the display nor `Wire`** — only the
  buttons, the encoder, the CVs (`analogRead`) and the outputs
  (`libGravity.cpp`). That is what makes the interleaving safe: between two
  bands, nothing else uses the display object or the I2C bus, so no frame can be
  corrupted by what runs between two passes.
- **Not measured at the date of the decision:** rendering and timing had never
  run together. `src/simavr_main.cpp` contains no rendering, so the step
  durations validated under simavr were validated without a display.
  *Update, 2026-08-21:* the complete firmware, rendering included, has since run
  under simavr — the **stack** was measured there (peak **159 B**, PRD §15; the
  120 B first published was measured before the two blind spots of that
  measurement were closed). The **real blocking was measured** by a dedicated
  harness (`tools/simavr-ssd1306/`) wiring simavr's `ssd1306_virt` slave onto the
  TWI: figures in the consequences below.

## Decision

The render is **spread: one band per pass of the main loop**, and the renderer
**draws only what falls inside the current band**.

The renderer stays pure — that is what makes the spreading possible — but it now
receives the band as a parameter (`Band{y0, y1}`, defaulting to the whole screen)
and discards out-of-band elements up front. Without that clipping, the 24 steps,
their digits and the title were recomputed **eight times**: U8g2 clips what it is
sent, but the call happens anyway, and it cost as much as the I2C transfer itself
(figures below). The band is **passed** to the renderer rather than asked of the
canvas, so it stays dependency-free and testable on any band; `PagedScreen` gets
it from the display (`getBufferCurrTileRow()`, `getBufferTileHeight()`).

⚠️ **The band obtained is in DISPLAY coordinates, the renderer works in LOGICAL
coordinates.** libGravity constructs its object with `U8G2_R2`: U8g2 rotates by
180° *before* clipping (`u8g2_draw_l90_r2` transforms, then clipping happens
against `pixel_curr_row`). `PagedScreen::bandOf()` therefore applies the inverse
— under R2, `logical = 63 − display`, which swaps the bounds. Omitting that
inversion gave every band **the inverse half** of what it displays: the screen
stayed almost blank. The defect lived for one commit, and it was reading the
panel's memory (`tools/run-screen-dump.sh`) that found it — no native test could
see it, all of them supplying the band already in logical coordinates.
`test_paged_screen` now models the rotation in its fake display, and
`test_the_band_conversion_is_the_right_way_round` pins the direction.

The sequence lives in `include/flexseq/PagedScreen.h`, **templated on the display
type** as `PatternScreen` is on the canvas: the firmware instantiates it on
`gravity.display`, a native test on a fake display. The `Display` is passed on
every call rather than stored, so as not to pay 2 bytes of reference. Two calls
only: `begin()` freezes and draws the first band, `advance()` transfers the
current band and draws the next.

`src/main.cpp` and `src/wokwi_main.cpp` **both** use it: the visual validation
harness must exercise the real render path, otherwise it no longer validates the
firmware (PRD §14).

The model is **frozen at the start of the frame** and the 8 bands are drawn from
that copy: `PatternScreenModel` (8 bytes) **plus a copy of the `Pattern`**
(15 bytes), so **23 bytes**. Copying the content is necessary because the pattern
is shared and editable during playback (PRD §6.3): without it, two bands of the
same frame could show different content.

## Consequences

- Worst-case blocking goes from the whole frame down to **one band**. A frame
  occupies 9 passes: one for the freeze and the first band — `firstPage()`
  transfers nothing — then 8 transfers.
- **Measured 2026-08-20** (`tools/run-blocking-probe.sh`, real SSD1306 slave in
  simavr). ⚠️ **Revised figures**: a first series had been taken on the build
  whose band conversion was wrong, hence where the screen **drew almost nothing**
  — "47 ms per frame, 7.74 ms worst, draw at 1.24 ms" mostly measured the absence
  of drawing. Comparison with correct rendering on both sides, ADC outside
  interrupt or its artefact corrected:

  | | full render on every band | clipped to the band |
  |---|---|---|
  | median pass | 8.52 ms | **6.48 ms** |
  | worst pass | 16.16 ms | **15.32 ms** |
  | whole frame | 74.0 ms | **59.1 ms** |
  | one band's transfer | 4.60 ms | 4.62 ms |

  The gain is therefore real but **more modest** than announced: −24 % on the
  median pass and −20 % on the frame, instead of the factor of 2 first reported.
  And the **worst** case barely moves.
- **Skipping unchanged bands (2026-08-20).** The `firstPage()/nextPage()` loop is
  replaced by a **manual** page loop — `setBufferCurrTileRow` + `clearBuffer` +
  draw + `sendBuffer` — which does exactly what U8g2's own loop does, but makes it
  possible to **not do that cycle at all** for an unchanged band. The SSD1306 is a
  memory display: a band that is not sent keeps showing what it was showing.
  ⚠️ **The cycle is indivisible**: clearing without redrawing and then sending
  would make the band disappear for the duration of a frame. That is the only
  visible defect this optimization could produce, and it is ruled out by
  construction.
  What gets skipped is the **title** band, whose rasterization costs 8.8 ms while
  it only changes when the selected pattern changes. The condition is
  **geometric** — a band entirely above the header rule can contain only the
  title — hence bound to the layout: were the layout to change, the condition
  would simply stop applying and we would fall back on the full render.
  **The skip becomes two-sided with the wired UI (2026-08-22, ADR 0002).** The
  same geometric argument holds at the bottom of the screen: a band entirely
  below the lowest ratchet digit can only contain the footer. Measured geometry —
  the second row's lowest drawn pixel is its ratchet digit at y 47, and band 7
  spans y 56 to 63, so that band is empty today. A footer whose baseline sits at
  63 lands exactly there, with 8 pixels of clearance, and carries the channel and
  the tempo: two values that change rarely, which is what makes the skip pay.
  Band 7 is already cleared and sent on every frame, so only the rasterization is
  added. Cost: a second hash and a second flag.
  **Implemented and measured 2026-08-22, and it saves TWO bands, not one.** The
  predicate is "the band lies entirely below the lowest pixel the grid can draw",
  and **two** of the eight bands satisfy it: the one carrying the footer (y 56–63)
  and the one between the grid and the footer (y 48–55), which carries nothing at
  all and never will. Both are governed by the footer string, so both are skipped
  together. A routine frame therefore sends **5 bands instead of 7**: frame
  **44.2 → 32.1 ms**, corrected p90 **8.44 → 8.46 ms** against a 12 ms budget,
  median 6.82 ms. Cost **RAM +11 bytes, Flash +184 bytes**, drift deliberately
  accepted. Stack peak re-measured at 160 bytes.
  The always-empty band is why the predicate is named for the geometry
  (`belowGrid`) and not for the footer: calling it a footer band would have
  described only one of the two.
  ⚠️ One mutation cannot be detected and it is an **equivalent mutant**: `>=` in
  place of `>` against y 47. No band starts at y 47 — they start at 0, 8, … 56 —
  so the two predicates are indistinguishable on the real set of bands. Recorded
  rather than papered over: the sweep is 7 of 8, not 8 of 8.
  **Safety net:** one frame in 16 is rendered in full, whatever the comparison
  says. Not out of doubt about the display's model, which is certain, but against
  a defect in our own logic: an omission then repairs itself.
  Measured: median pass **6.48 → 5.79 ms**, p90 **15.3 → 7.96 ms**, frame
  **47 → 41.7 ms** — **simulated** figures, ADC off. Cost: **+9 B of RAM,
  +160 B of Flash**.
  **Hardware estimate, corrected 2026-08-21** (simavr's ADC tax was itself
  mismeasured, see `CLAUDE.md`): routine pass **6.13 ms** median / **8.44 ms**
  p90, routine frame **44.2 ms**, and the full-refresh peak **15.31 ms** on a
  **59.5 ms** frame. The "14.5 ms" published before that date was the *simulated*
  peak, presented elsewhere as though it were the hardware figure. The net's ratio
  is now **measured rather than postulated**: 1 frame in 16.0 over 32 s of
  simulation, full frames being recognized by their **8** bands sent instead of 7.
- **Verified that no band ever blanks**: 20 000 samples of the panel's memory, one
  every 0.5 ms (`WATCH=` on `tools/run-screen-dump.sh`). The low but non-zero
  minima are states **mid-transfer** — a band goes out in 6 Wire transactions, so
  the panel briefly shows a mixture of old and new. Inherent to any partial
  update, and predating this change.
- **Why the worst case resists: the clipping CONCENTRATED the drawing.** The
  distribution is bimodal — six bands out of eight carry no element and cost only
  the transfer (~5.5 ms), while those carrying a row of 12 steps cost ~15 ms. The
  p90 is 15.3 ms, about one pass in seven. Reducing the total without reducing the
  peak was the expected result of the technique; the peak therefore remains the
  target of a later optimization, distinct from this decision.
- **Floor reached.** A pass can no longer go below the ~4.6 ms of one band's
  transfer, which now dominates. Going lower would require sending less than a
  band per pass — U8g2's granularity is the tile row — or pushing the bus beyond
  400 kHz, outside the SSD1306's specification.
- Initial estimates corrected: "~3 ms per band" and "~25 ms per frame" were
  **low**. A band goes out in **6 Wire transactions** of ~21 bytes, and the gaps
  between chunks cost ~107 µs each. The bus throughput is indeed the one announced
  (22.5 µs per byte at 400 kHz, plus 4.55 µs of TWI ISR); it was the chunking that
  the calculation was missing.
- Cost of the clipping: **+522 B of Flash**, **0 B of RAM** (20236 → 20758 B, RAM
  unchanged at 1490 B).
- **Property verified by test**: the union of the 8 bands renders **exactly** the
  complete frame, pixel for pixel (`test_pattern_screen`). One pixel short and an
  element would have vanished from the screen. The fake canvas clips there the way
  page mode does — without that clipping the test would be wrong, an element
  straddling two bands being drawn twice.
- The main benefit is not the display but **trigger timing**: during a blockage,
  ticks accumulate and onsets bunch up on the next drain.
- Makes the **CV → RESET per channel** destination practicable (PRD §10).
- Cost **measured** (`nanoatmega328` build): RAM **1466 → 1490 B** (+24: the 23
  of the freeze plus the state flag), Flash **20100 → 20236 B** (+136). A frame
  now spreads over several passes: the display lags reality by that much.
- Covered by `test/test_paged_screen` (6 assertions): a frame occupies exactly 8
  bands, the screen stays busy until the last one, `advance()` outside a frame
  transfers nothing, and **an edit occurring during the frame does not tear it**.
  That last assertion was verified by mutation — with the freeze removed it turns
  red (500 pixels on the first band, 707 on the next).
- **Consequence for CV — since closed.** With CV sampled once per pass through
  `gravity.Process()`, a pulse shorter than the worst pass could go unnoticed, and
  this decision lowered that threshold without removing it: a 1 to 5 ms pulse
  stayed exposed. PRD §10.6's fallback — sampling the converter under interrupt —
  was **adopted and implemented on 2026-08-20**. CV no longer depends on the loop
  at all: each channel is sampled every ~208 µs, and a 1 ms pulse is guaranteed
  (`tools/run-cv-capture-probe.sh`). See the third alternative below, which this
  reverses.

## Alternatives set aside

- **A second U8g2 object in `_F_` mode** (1024-byte buffer, a single pass):
  +1024 bytes of RAM, outside the budget, and contrary to PRD §12.
- **Lowering the refresh rate**: reduces the frequency of the blockages, not
  their duration. The problem is the duration.
- **Sampling CV in a dedicated interrupt** (timer, or end of conversion): would
  take ownership of the converter away from libGravity, on which `cv1.Process()`
  depends through `analogRead`. Set aside at the date of this ADR, kept as a
  fallback should measurement demand it — and **measurement did demand it**: this
  alternative was **adopted on 2026-08-20** (PRD §10.6). FlexSeq now owns the
  converter, `main.cpp` calls the pieces of `Gravity::Process()` instead of the
  function itself, and the cost is 5.6 % of CPU on hardware. Recorded here as
  reversed rather than deleted, so the reasoning stays readable.

## References

- PRD §10 (CV), §10.6 (sampling under interrupt), §12 (UI / display constraint),
  §14 (validation levels), §15 (memory footprint).
- `include/flexseq/PagedScreen.h`, `src/main.cpp`, `src/wokwi_main.cpp`,
  `include/flexseq/PatternScreen.h`, `test/test_paged_screen/`.
- U8g2: `clib/u8x8_d_ssd1306_128x64_noname.c`, `U8x8lib.cpp`.
- libGravity `9be88be1f4`: `libGravity.cpp` (`initDisplay`).
