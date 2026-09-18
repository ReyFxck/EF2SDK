# EF2 kernel primitives

EF2SDK keeps the public hardware/kernel layer deliberately small and explicit.

## Interrupts

Alpha.39 adds public INTC and DMAC helpers without linking libkernel.

`ef2_interrupt_suspend()` mirrors the EE EIE state and disables global
interrupt delivery with the R5900 `di` instruction. The returned state must
be passed to `ef2_interrupt_resume()`, making nested critical sections safe
when callers follow the save/restore pattern.

INTC and DMAC handlers are registered through the EE kernel's existing handler
syscalls, but EF2SDK owns the public ABI and wrapper code.

The API includes add/remove and enable/disable operations for both INTC and
DMAC sources.

## Hardware timers

Alpha.39 exposes EE timers 0, 1 and 2 directly. Timer 3 remains intentionally
unexposed because the EE kernel uses it for its alarm service.

The first timer API supports:

- BUSCLK, BUSCLK/16 and BUSCLK/256 sources;
- HBLANK clocking on timers 0/1;
- compare values;
- zero-on-compare;
- compare and overflow interrupt enables;
- start/stop and count access;
- compare/overflow status and selective acknowledgement.

Status bits are handled with their write-one-to-clear semantics, so starting or
stopping a timer does not accidentally acknowledge a pending event.

`ef2_cpu_count()` also exposes the R5900 COP0 Count register for lightweight
profiling. The elapsed helpers use unsigned subtraction so one natural timer
wrap is handled without special branching.

## Alpha.39 smoke test

The boot example installs an EF2 INTC handler on Timer0, configures Timer0 at
BUSCLK/256 with a 4096-tick compare, starts it, waits for a compare interrupt,
acknowledges the event inside the handler and tears the test down again.

A successful boot log contains an `[EF2][TIMER]` line with a nonzero
`irq_hits` value. This checks the direct timer MMIO path, handler registration,
INTC enable/disable and global interrupt save/restore together.

## Alpha.40 lightweight profiling

Alpha.40 builds on COP0 Count with a small reusable `ef2_profile_counter`.
Callers can reset a counter, record elapsed tick samples, or use the inline
begin/end helpers. The counter keeps sample count, last/min/max ticks and a
64-bit accumulated tick total without requiring floating-point math.

The GIF DMA transport now records submissions, completed transfers, submitted
QWC, wait calls/timeouts, DMA completion latency and wait latency. EF2 Video
also records every bounded GS VSync wait in `ef2_video_frame_stats`.

The boot smoke resets GIF statistics after video initialization, renders the
existing validated diagnostic frame unchanged, presents it once, then emits
`[EF2][PROFILE]` lines for GIF DMA and VSync. These counters are diagnostic
telemetry only; they do not change transport fallback or rendering behavior.


## Alpha.40 NetherSX2 validation

The profiling telemetry was exercised in NetherSX2 on 2026-09-18. The captured
boot log reported 21 GIF DMA submissions and 21 completions, 558 submitted
QWC, zero DMA timeouts, one VSync timing sample and zero VSync timeouts. The
smoke then continued through audio and pad initialization before the deliberate
manual trap test.

## Alpha.41 general kernel syscall surface

Alpha.41 expands the EF2-owned raw syscall layer without adding a libkernel
runtime dependency. The public header now includes EE thread and semaphore ABI
structures plus wrappers for thread lifecycle/status/wakeup operations,
semaphore create/delete/signal/wait/poll/status operations, cache control,
COP0 access, CPU configuration, machine type and installed memory size.

The boot smoke makes only reversible calls: it queries the current thread,
retrieves its status, creates a one-count semaphore, polls it, signals it back
and deletes it. A successful run emits an `[EF2][KERNEL]` line before the
existing timer/video/audio/input tests. No new scheduler thread is left
resident after the smoke.


## Alpha.41 NetherSX2 validation

The Alpha.41 smoke was validated in NetherSX2 on 2026-09-18. The captured
kernel line reported thread id 1, successful thread-status lookup, a complete
create/poll/signal/delete semaphore round trip, 33554432 bytes of installed
memory and machine type 0. The existing Timer0, GIF DMA, VSync, audio and pad
smokes then continued successfully before the deliberate manual trap.
