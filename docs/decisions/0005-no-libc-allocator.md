# 0005 — FlexSeq supplies its own allocator, and it is four bytes wide

- **Status:** accepted
- **Date:** 2026-08-23
- **Supersedes:** —
- **Superseded by:** —

## Context

FlexSeq allocates nothing. The domain uses fixed-size storage, and the memory
rules forbid dynamic allocation in firmware code. Yet the binary carried
avr-libc's allocator: `malloc` at 312 bytes and `free` at 274, so **586 bytes of
Flash**.

**One allocation explains it, and the audit found it in the pinned dependency.**
`uClockClass::init()` (`uClock.cpp:90`):

```cpp
if (ext_interval_buffer == nullptr)
    setExtIntervalBuffer(1);
```

and `setExtIntervalBuffer` (`uClock.cpp:233`):

```cpp
if (ext_interval_buffer != nullptr)
    return;
// alloc once and forever policy
ext_interval_buffer = (uint32_t*) malloc( sizeof(uint32_t) * ext_interval_buffer_size );
```

So the whole firmware performs **one allocation of four bytes, at boot**. A guard
makes a second one impossible, the dependency's own comment states the policy, and
nothing ever frees it.

**Nothing else allocates.** A search for `malloc`, `new`, and `operator new` across
every pinned dependency matches uClock only: u8g2, Wire, NeoHWSerial and
libGravity all use static buffers. `avr-objdump` confirmed a single call site in
the binary, and no `operator new` was linked.

## Decision

FlexSeq defines `malloc` and `free` itself, in `src/hal/HeapStub.cpp`. The linker
then prefers our object and never extracts avr-libc's.

- `malloc` hands out words from an **8-byte static pool** and returns `nullptr`
  beyond it;
- `free` does nothing.

The pool is sized for uClock's four bytes, with one word to spare.

## Consequences

**554 bytes of Flash returned**, measured: 29062 to 28508 bytes. RAM is unchanged:
the 10 bytes of pool and cursor replace avr-libc's own bookkeeping.

**The allocation is now resolved at compile time.** The compiler inlines our
`malloc`, sees the constant size, and computes the address. **No call to an
allocator remains in the binary.**

**FlexSeq has no heap.** The stack probe scans downward because the bottom of free
RAM used to be the heap start; that assumption is now vacuous, and free RAM is
stack space alone. Re-measured after the change: peak stack **206 bytes**, against
322 bytes free.

**The risk is bounded, and it is a real risk.** The decision rests on a fact that
is true today: one allocation, four bytes, at boot. Code that allocated more would
get `nullptr`. The failure is therefore **visible**, not silent, and it fails at
the allocation rather than corrupting memory later.

CAUTION: before you raise `ext_interval_buffer_size`, or add any library that
allocates, resize the pool. uClock's own `handleExternalClock` dereferences that
buffer, so a `nullptr` there would fault.

**The clock was verified after the change, not assumed.**
`tools/run-trigger-probe.sh`: 6/6 outputs, 11/11 gaps, step 499.97 ms against
500.00 expected at 120 BPM, jitter 1.00 ms. uClock is exactly the code that
allocates, so its correct operation is the proof that matters.

## Alternatives set aside

**Patch the dependency to use a static buffer.** Forbidden: libGravity is pinned at
commit `9be88be1f4` and must not be edited.

**Leave the allocator and raise the Flash guard.** The guard exists to make growth
a deliberate act. Returning 554 bytes of code that nothing runs is better than
accepting a smaller reserve.

## References
- `src/hal/HeapStub.cpp`; `platformio.ini`, environments `nanoatmega328` and `encoderprobe`.
- Pinned uClock: `uClock.cpp:90`, `:233`.
- `tools/run-build-memory.sh`, `tools/run-stack-probe.sh`, `tools/run-trigger-probe.sh`.
- CLAUDE.md, "Memory discipline".
