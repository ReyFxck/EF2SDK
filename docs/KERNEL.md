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
