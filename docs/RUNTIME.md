# Runtime and formatted output

EF2SDK keeps formatting separate from transport.

## Buffer formatting

`ef2_snprintf()` and `ef2_vsnprintf()` provide the bounded integer/string
formatter used by the compatibility layer.

## Stream output

Alpha.36 adds a tiny stream object:

`ef2_FILE` contains a write callback and caller-owned context.

The default stdout/stderr streams intentionally have no sink. Applications
can install their own output transport with `ef2_stdio_set_stdout()` and
`ef2_stdio_set_stderr()`.

`ef2_printf()`, `ef2_vprintf()`, `ef2_fprintf()`, `ef2_vfprintf()` and
`ef2_puts()` format through the existing core and then write all produced
bytes to the selected stream. Small messages use a 256-byte stack buffer;
larger messages use the EF2 heap temporarily.

The opt-in compatibility header also exposes `FILE`, `stdout`, `stderr`,
`printf`, `fprintf`, `vprintf`, `vfprintf` and `puts`.

## SIO debug sink

`ef2_debug_use_sio_stdio(115200)` configures the EE serial port directly and
installs it as stdout/stderr.

The SIO backend uses bounded TX waits instead of an infinite busy loop. If
hardware/emulator serial output does not become ready, the write fails rather
than hanging the application.

This is a debug transport, not the future filesystem stdio implementation.
When VFS lands, file-backed FILE streams can be layered on the same formatter.


## Alpha.37 always-on diagnostic log

Alpha.37 turns the formatting/SIO pieces into a usable diagnostic path.

`ef2_debug_init()` installs a tee sink for stdout/stderr. Every byte is first
stored in an 8 KiB circular RAM log. SIO is only an optional mirror: if the
serial transmitter times out, the serial side is disabled and the application
continues logging to RAM.

This avoids the traditional failure mode where debug output itself hangs the
program because no serial receiver/debugger is present.

`ef2_debug_printf(tag, ...)` emits structured prefixes such as
`[EF2][VIDEO]`, `[EF2][AUDIO]` and `[EF2][PAD]`.

`ef2_debug_copy_recent()` copies the newest retained bytes for a future crash
screen, file dump or network transport. `ef2_debug_get_stats()` reports ring
usage, total bytes, serial status and serial failures.

The boot smoke test now records subsystem milestones, video DMA/fallback state,
remaining texture VRAM, audio initialization/configuration, pad initialization,
resampler setup and audio start/pause/resume/stop events.


## Alpha.38 crash diagnostics

EF2SDK now has an opt-in level-1 crash handler installed with the EE kernel's
vector syscalls. `ef2_crash_install()` hooks TLB refill causes 1..3 and
common synchronous causes 4..7 and 10..13.

The low-level vector switches to a dedicated 8 KiB emergency stack and
captures the low 64 bits of all general registers plus Status, Cause, EPC,
ErrorEPC and BadVAddr before calling C code. It never attempts to resume the
faulting instruction.

Crash reporting has two independent paths:

- a raw postmortem record is appended directly to the always-on RAM log and
  mirrored to SIO only when the bounded SIO mirror is active;
- when the video subsystem is already initialized, a dark-red framebuffer
  screen shows EXC/CAUSE/EPC/BADV/SP/RA/GP and recent RAM-log text.

The crash screen intentionally disables double buffering and uses a small
CPU-generated 320x224 RGBA32 diagnostic texture so it does not depend on a
font library or heap allocation.

The smoke test exposes a deliberate trap for emulator validation:
hold SELECT + L1 + R1 and press TRIANGLE. This calls
`ef2_crash_trigger_test()`, which raises ExcCode 13 (Trap).


## Alpha.38 emulator validation

The deliberate crash path was validated in NetherSX2 on 2026-09-18.

The test produced:

- exception code 13 (Trap);
- Cause 0x00000034;
- EPC 0x0010093c;
- BadVAddr 0x00000000;
- captured SP, RA and GP values;
- the recent in-RAM structured boot log on the crash screen.

The alpha.38 ELF symbol table places `ef2_crash_trigger_test` exactly at
0x0010093c, so the captured EPC resolves directly to the deliberate `teq`
instruction rather than to an unrelated fault. This validates the complete
emulator-side path from EE exception vector entry through register capture,
emergency-stack dispatch, RAM-log preservation and framebuffer postmortem
rendering.
