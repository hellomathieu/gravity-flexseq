# 0008 — libGravity is a pinned fork, with a charter

- **Status:** accepted
- **Date:** 2026-08-24
- **Supersedes:** —
- **Superseded by:** —

## Context

`CLAUDE.md` held one rule about the dependency: libGravity is pinned at
`9be88be1f4`, and its anomalies are constraints, never bugs to correct. That
rule made the adapter layer necessary (ADR 0002) and it made the 68
characterization assertions meaningful.

Two defects then showed that the rule was too narrow. Neither can be worked
around from the adapter layer.

**`Clock::SetSource()` attaches a serial handler with the wrong type.**
`NeoHWSerial::isr_t` is `bool (*)(uint8_t, uint8_t)`; libGravity passes
functions that return `void` (`clock.h:29, 89, 115, 159`). PlatformIO's Arduino
AVR builder adds `-fpermissive`, so the type error becomes a warning.
`_rx_complete_irq()` then does `saveToBuffer = _isr(data, status)`, and the
callee sets no return value. Every received MIDI byte is buffered or dropped at
random. Nothing reads that ring, so no behaviour is observable today, but the
read is undefined on each byte.

FlexSeq cannot repair this from outside. In MIDI clock mode the handler that
forwards the clock is `static` inside the class, so replacing it would lose the
MIDI input.

**libGravity declares `uClock` in `depends` while it carries its own copy.**
The copy is in `src/uClock/`, and every include of it is quoted and relative, so
it is the one that compiles. The registry copy is a different version — the
library runs **2.2.1** while `depends` resolves **2.3.0** — and it is never
compiled. A top-level pin in `platformio.ini` does not fix that: it compiles a
second copy, so a second definition of the same ISR joins the link. Measured on
2026-08-24.

The dependency also has no version constraint on U8g2 or RotaryEncoder, so the
largest Flash consumer of the build could move on its own.

Editing `.pio/libdeps` is not an option, and not because a rule forbids it. It
is a build cache. PlatformIO rebuilds it when `lib_deps` changes, on
`pio pkg update`, after a clean, and on a fresh clone. A repair there disappears
in silence and the suite stays green without containing it.

## Decision

libGravity is resolved from **`github.com/hellomathieu/libGravity`**, a fork of
`awonak/libGravity`, pinned by commit in `lib_deps`.

The fork licence stays **MIT**, with Adam Wonak's copyright intact.

### The charter — what the fork may change

- correct what FlexSeq cannot work around from its adapter layer;
- correct a defect **whose repair removes a FlexSeq workaround**;
- move to `PROGMEM` what has no reason to sit in `.data`;
- correct the package metadata;
- add a compilation guard.

### The charter — what the fork must not change

- **the seven audited anomalies.** They are the reason `InputAdapter` exists
  (ADR 0002), and 68 assertions describe them. A repair would invalidate the
  adapter layer and `docs/upstream-defects.md` at the same time;
- an API. FlexSeq must keep compiling against upstream without a change;
- a musical behaviour. uClock stays as it is.

### Per commit

One correction per commit. Each commit names the entry of
`docs/upstream-defects.md` that it treats. Each correction is offered upstream,
and the state of the offer goes in that document.

## Consequences

**The existing guard becomes the safety net of the fork, and no tool was
written for it.** `run-libgravity-tests.sh` compares the failing assertions to a
versioned `EXPECTED`, and it fails **in both directions**. A fork commit that
repairs an audited anomaly by accident turns the suite red on its own.

**A pin bump is a re-audit, and a test already forces it.** `test_gravity` pins
the composition of `Gravity::Process()` for exactly this moment. The re-audit
list is: the characterization conform with `EXPECTED` unchanged, the four suites
at the same counts, the resolved tree identical to the archive of the commit,
and the memory measured.

**The pin lives in one place.** The five AVR environments extend a `[deps]`
section, so a bump is one line. `run-libgravity-tests.sh` reads the commit from
`platformio.ini` instead of holding a copy, and refuses to run when it cannot
find it.

**The fork must disappear.** Every correction accepted upstream brings that
closer. The fork is a working tool, not a divergence to maintain.

**The audited base moved on 2026-08-24**, from `9be88be1f4` to `5c0c34f`, the
fork head. The whole compiled surface of the move is seven added lines in
`src/encoder.h`. The re-audit passed. The move cost RAM +2 and Flash +34, both
attributed: the new `on_long_press` member inside the global `gravity`, and the
new branch of `Encoder::Process()` inlined into `main`.

## Alternatives set aside

**Vendor libGravity inside the FlexSeq repository.** One repository, one clone,
and the audited code becomes versioned. Set aside on two grounds: it puts MIT
code inside a GPLv3 repository, which blurs the provenance of another author's
work, and it makes offering a correction upstream much harder. A pinned fork
gives the same guarantee — the commit is the version.

**A git submodule.** A clone without `--recursive` builds nothing. The project
already has one trap of that shape, since `CLAUDE.md` survives no clone. A
second one is one too many.

**Fork U8g2 as well.** 48 MB and hundreds of drivers, for a two-line repair that
returns 101 bytes of RAM. Its `u8x8_d_ssd1306_128x64_noname.c` declares its
init sequence and its display info without the `U8X8_PROGMEM` macro, which the
library itself defines. That one goes upstream, and the saving waits.

## References
- PRD §1 and §18; `CLAUDE.md`, the section on libGravity integration.
- ADR 0002 for the adapter layer that the audited anomalies justify.
- `docs/upstream-defects.md` for the defect list and the state of each offer.
- `tools/run-libgravity-tests.sh` for the drift guard.
