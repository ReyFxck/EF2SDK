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
