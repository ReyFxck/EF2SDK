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
