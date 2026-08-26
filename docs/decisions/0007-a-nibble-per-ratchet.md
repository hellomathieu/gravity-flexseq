# 0007 — A nibble per ratchet, not three bits

- **Status:** accepted
- **Date:** 2026-08-23
- **Supersedes:** —
- **Superseded by:** —

## Context

A ratchet code has **six values**: `RATCHET_NONE`, 2, 3, 4, 6 and `TRIPLET`.
Ratchet 5 is deliberately absent, because at 96 PPQN a fifth is exact on only 2 of
the 25 rates (PRD §6.3).

Six values need three bits, so the **nibble** FlexSeq stores today wastes one bit
in four. At 32 steps a nibble array is 16 B against 12 B for a packed field.

**The owner first chose three bits, on 2026-08-23, and then reversed it the same
day. The reason is worth recording, because it is a lesson about order.**

The three-bit choice was made while **RAM was the binding constraint**: the bank of
sixteen patterns was resident, so four bytes per pattern meant 64 B. ADR 0006 then
moved the bank into EEPROM and left only **six** instances resident. The RAM cost
of a wider field was divided by nearly three, and the comparison changed sign.

**The budget, measured on 2026-08-23.**

| | Free |
|---|---|
| Flash | **534 B** under the 95 % guard |
| RAM | 321 B, against a measured peak stack of 206 B |

And the plan's own estimate: **800 to 1600 B of Flash** for the remaining lots.
**Flash is the binding constraint. RAM is not.**

## Decision

Keep **one nibble per step**. At 32 steps that is 16 B of ratchets, plus 4 B of
steps, so `sizeof(Pattern) == 20`.

## Consequences

**No new code.** The nibble is what `Pattern` already does, so the packing costs
nothing in Flash and nothing in cycles. `refreshStepTiming()` reads the ratchet at
every step boundary, and it keeps a single mask.

**It spends what we have to save what we lack.**

| | Nibble | Three bits | Difference |
|---|---|---|---|
| Ratchets per pattern | 16 B | 12 B | −4 B |
| RAM, 6 resident instances | 120 B | 96 B | **−24 B** |
| EEPROM, 16 templates | 320 B | 256 B | **−64 B** |
| Flash | **0** | +50 to +150 B, estimated | **+50 to +150 B** |
| Cycles per step boundary | today's | more | more |

24 B of RAM out of 321, against 50 to 150 B of Flash out of 534. The trade is
clear on those numbers, and it would have been the opposite before ADR 0006.

**The EEPROM gets tighter, and it still fits.** Templates and instances together
reach about 874 B of 1024 counting from address 384. That is 150 B of margin, and
it is worth watching if the format grows again.

**One bit per step stays unused.** It is not a reserve to be spent lightly: a
seventh ratchet code would break the two-tick floor arithmetic of PRD §6.3.1 on
the fast rates, which is the reason ratchet 5 was excluded in the first place.

## Amendment — 2026-08-26

The 36-step foundation (commit `58e238a`) changed the figures of this ADR. The
decision — one nibble per step — does not change.

| | At 32 steps | At 36 steps |
|---|---:|---:|
| Step bytes | 4 | **5** |
| Ratchet bytes | 16 | **18** |
| `sizeof(Pattern)` | 20 | **23** |
| RAM, 6 resident instances | 120 | **138** |
| EEPROM, 16 templates, content only | 320 | **368** |

ADR 0006 counts 384 bytes for the same sixteen records. Each record adds one
length byte, which is storage and not content.

⚠️ **This ADR asked to watch the EEPROM margin if the format grew again. The
format grew.** Templates and instances now reach address 969. The free space
falls from about 150 bytes to **53 bytes**, from address 970 to address 1022.
Address 1023 carries the original firmware's `memCode`.

**The unused bits of the fifth pattern byte.** The fifth pattern byte contains
the data for the final four steps. Bits 36 through 39 do not belong to any step,
and they are not part of the pattern content.

These four bits are canonical:

- the firmware forces them to zero when it writes a pattern;
- the firmware masks them when it loads a pattern;
- content operations ignore them, empty-slot detection included.

A persistence test must inject non-zero values into bits 36 through 39. The test
must verify that the loaded pattern does not expose them as content.

## Alternatives set aside

**Three bits per step.** Set aside on the numbers above. It was the right answer
while the bank was resident, and it stopped being the right answer when ADR 0006
moved the bank out. It would also have needed an acceptance criterion of its own:
a mutant that shifts the field by one bit must be detected, because a read that
strays into the neighbour's bits could still satisfy the ratchet matrix on some
rates.

**A sparse list of ratchets.** Four slots of one byte each, five bits of index
plus three of code, would cost 4 B per pattern. It was the largest saving on paper.
Set aside because it turns `Pattern` from an array into a list: a lookup replaces a
direct index in the hot path, the number of ratchets per pattern becomes capped,
and the saving is no longer needed now that RAM is not the constraint.

## References
- PRD §5.0 and §6.3; §6.3.1 for the two-tick floor.
- ADR 0006, which moved the premise of the first choice.
- `WORKPLAN.md`, lot A.
