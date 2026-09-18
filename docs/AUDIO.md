# EF2Audio design

EF2Audio is designed around a simple rule:

> The application's native sample rate is application data, not a hardware
> restriction.

The PlayStation 2 output backend may ultimately feed the SPU2 at 48 kHz, but
an emulator, tracker, game or media player must not be forced to synthesize at
48 kHz just to satisfy the SDK.

## Source-rate-agnostic streams

A stream describes what the producer actually generates:

```c
ef2_audio_stream_config snes = {
    .sample_rate = 32000,
    .channels = 2,
    .format = EF2_AUDIO_SAMPLE_S16,
    .resampler = EF2_AUDIO_RESAMPLE_LINEAR,
};

ef2_audio_stream_config tracker = {
    .sample_rate = 44100,
    .channels = 2,
    .format = EF2_AUDIO_SAMPLE_S16,
    .resampler = EF2_AUDIO_RESAMPLE_LINEAR,
};
```

Rates such as 22050, 32000, 32768, 44100, 48000 or unusual core-specific
values are not special cases. A non-zero rate is accepted by the converter.

## Current alpha.3 foundation

The first audio layer is deliberately independent from SIF, IOP and SPU2 so
it can be tested before the hardware backend exists.

It currently provides:

- S16 mono/stereo stream descriptions;
- nearest-neighbour and linear rate conversion;
- Q32 phase accumulation to avoid long-term rate drift from low-precision
  stepping;
- freestanding integer implementation with no libc and no floating point;
- a saturating Q15-gain S16 mixer helper;
- host-side regression tests in CI.

The 64/32 division used to calculate the Q32 step is implemented locally so
the freestanding EE build does not gain a hidden libgcc dependency.

## Planned pipeline

```text
producer A (32 kHz) ---- rate converter ----\
producer B (44.1 kHz) -- rate converter -----+--> bus mixer --> IOP ring --> SPU2
producer C (48 kHz) ---- zero-cost copy -----/
```

The hardware backend will be separate from the producer API. Exact 48 kHz
streams bypass interpolation.

## Planned buses

The intended mixer model is:

```text
Master
├── Game
├── Music
├── SFX
└── UI
```

Pausing or changing one bus must not require stopping every audio producer.

## Planned IOP/SPU2 backend

The IOP side should keep enough queued PCM to continue feeding SPU2 while the
EE briefly performs expensive work such as filesystem access, ROM loading or
texture conversion.

The backend is expected to expose useful diagnostics instead of hiding them:

- queued frames / milliseconds;
- underrun and overrun counters;
- measured output latency;
- producer rate and backend rate;
- per-stream and per-bus state.

## Resampler roadmap

The initial modes are intentionally small:

- nearest: useful for diagnostics and extremely cheap paths;
- linear: baseline real-time converter.

Higher-quality modes such as cubic and band-limited/sinc conversion can be
added behind the same stream API. Applications should be able to choose the
quality/cost tradeoff without rewriting their audio backend.

## Why this belongs in the SDK

Ports should not each need their own 32 kHz -> 48 kHz converter, private ring
buffer, underrun workaround and mixer policy. Those are platform services.

EF2Audio treats the SPU2 rate as a backend detail and keeps emulator/core
timing at its native rate.


## Alpha.4: first audible hardware path

Alpha.4 connects the existing source-rate-agnostic converter to a real IOP
streaming service:

```text
generated melody @ 32 kHz
          |
          v
EE Q32 rate converter
          |
          v
48 kHz stereo S16
          |
          v
EF2 SIF/RPC client
          |
          v
ef2audio.irx
          |
          v
8192-frame IOP ring
          |
          v
SPU2 block transfer
```

The melody is generated procedurally in the ELF and is not an embedded music
file. This intentionally exercises a non-48-kHz producer before the hardware
backend.

The current IOP service imports the console ROM's `LIBSD` only as a
low-level SPU2 bootstrap. It does not use `audsrv`; EF2SDK owns the RPC
protocol, ring buffer, backpressure, resampling and statistics. Replacing
`LIBSD` with direct SPU2/DMA control remains a planned milestone.

The smoke ELF uses screen color as a simple hardware diagnostic:

- **blue**: GS/video passed and audio initialization is starting;
- **red**: SIF/IOP/audio initialization or submission failed;
- **teal-green**: the IOP service initialized, the ring was prefilled and
  SPU2 streaming was started.

The first ring capacity is 8192 stereo frames, about 171 ms at 48 kHz. It is
large on purpose so a short EE stall does not immediately become an audible
underrun.


## Alpha.5 loader diagnostics

The standalone ELF now retries embedded IRX loading through a small EF2-owned
legacy LOADFILE patch when the ROM service rejects function 6
(LoadModuleBuffer). This keeps the smoke test self-contained without linking
libsbv into the EE executable.

The boot screen now distinguishes initialization stages:

- yellow: EE SIFCMD/RPC initialization;
- purple: embedded IRX / legacy LoadModuleBuffer patch;
- orange: ef2audio RPC registration or bind;
- red: ef2audio reached the IOP but audio/SPU2 initialization failed;
- teal-green: streaming started.

This is intentionally a temporary bring-up diagnostic until text rendering is
available in the freestanding smoke ELF.


## Alpha.6: deterministic RPC registration

The first alpha.5 hardware test reached the orange diagnostic stage: the
embedded IRX loaded, but the EE could not bind the EF2Audio RPC service.

The server used to be registered from the newly-created IOP worker thread,
which meant the EE could resume after module start and race the worker before
it had registered the SID. Alpha.6 registers the queue and server
synchronously inside the IRX _start routine before returning
MODULE_RESIDENT_END. The worker thread now only runs sceSifRpcLoop().

The EE bind loop also tolerates a much longer registration window so future
IOP services do not depend on scheduler timing.


## Alpha.7: LOADFILE module-start result diagnostics

LOADFILE returns both a module identifier and a module-start result
(`modres`). Earlier smoke builds only checked the identifier, which can make
"IRX loaded" look like "IRX stayed resident" even when its `_start` returns a
non-resident status.

Alpha.7 exposes both values in the EF2 loader and treats
`MODULE_RESIDENT_END == 0` as a required condition before attempting to bind
the EF2Audio SID.

The blue-violet diagnostic specifically means the embedded IRX was accepted
by LOADFILE but its `_start` did not remain resident. Orange now means the
IRX *did* report resident, so the remaining fault is genuinely RPC
registration/binding.


## Alpha.8: RPC-thread readiness handshake

Alpha.7 confirmed that `ef2audio.irx` returned
`MODULE_RESIDENT_END`, while the EF2 SID was still not visible to the EE.
The same EE bind implementation had already bound the stock IOP heap and
LOADFILE services successfully, which narrows the fault to IOP-side EF2Audio
registration.

Alpha.8 follows the conventional IOP RPC-server pattern used by established
PS2 modules: the RPC worker thread itself calls `sceSifInitRpc`,
`sceSifSetRpcQueue(..., GetThreadId())` and `sceSifRegisterRpc`.
A private semaphore makes `_start` wait until registration has completed
before returning `MODULE_RESIDENT_END`, removing both the scheduler race and
the nonstandard queue-registration context.


## Alpha.10: IOP emulog instrumentation

Alpha.10 adds targeted IOP-side Kprintf tracing with the fixed prefix
`[EF2AUDIO]`. The trace covers module start, semaphore/thread creation,
SIFRPC initialization, queue insertion, both SID registrations, readiness
handshake, RPC-loop entry and later audio/SPU2 initialization.

This is deliberately not a permanent verbose mode. It exists to make emulator
logs identify the exact IOP-side stage instead of encoding every failure only
as a framebuffer color.


## Alpha.11: host-loader and stdio diagnostic build

For emulator diagnosis, the boot ELF now first tries
`host:ef2audio.irx`. When the IRX file is placed next to the ELF in the
emulator host directory, NetherSX2 should print the path-based load operation
and its module result directly in emulog.

If the host file is unavailable or fails to remain resident, EF2SDK falls back
to the embedded IRX path, so the standalone behavior is preserved.

IOP tracing now emits through both `printf` (stdio) and `Kprintf` using the
same `[EF2AUDIO]` prefix. This is diagnostic-only duplication intended to
maximize visibility across emulators and BIOS/debug configurations.


## Alpha.12: SIFCMD stage telemetry

Android SAF-backed `host:` loading was removed from the diagnostic path after
NetherSX2 rejected a same-directory content URI as "outside of ELF directory".

Alpha.12 keeps the embedded IRX path and adds an RPC-independent telemetry
channel. The IOP module sends stage numbers 1..13 to EE SREG 31 using the
existing SIFCMD system command. After the embedded load attempt, the EE reads
that SREG and deliberately asks LOADFILE for a non-existent ROM module named
`EF2DBGxx`. NetherSX2 logs that path request, so emulog now exposes the last
IOP stage even when IOP printf/Kprintf output is hidden.

Stage 00 means no IRX stage message reached the EE. Stage 13 means the worker
registered both RPC SIDs, signalled readiness, and reached `sceSifRpcLoop`.


## Alpha.13: direct IOP-memory black-box

Alpha.12 reported `EF2DBG00`. That result was ambiguous because its very
first debug action called `sceSifSendCmd`, so a broken or unavailable
debug import could stop the module before the debugger could report anything.

Alpha.13 removes the debug-only SIFCMD, stdio and Kprintf dependencies from
the IRX. Instead, `ef2audio.irx` maintains a plain global
`ef2audio_debug_stage` and writes stage numbers directly into its own IOP
RAM.

During the build, EF2SDK extracts that symbol's module-relative address from
the unstripped IRX ELF and generates an EE header automatically. The EE then
finds the resident `ef2audio` module in the IOP module list and reads the
debug word directly through the IOP memory window.

The emulator log marker now has one additional meaning:

- `EF2NOMOD`: the EE could not find/read the resident ef2audio module at all;
- `EF2DBG00`: module exists, but its stage word is still zero;
- `EF2DBG01..13`: exact last stage reached without depending on SIFCMD/RPC/logging.

This path is deliberately independent of every subsystem currently under
investigation.


## Alpha.14: reject false-success LoadModuleBuffer calls

Alpha.13's `EF2NOMOD` marker proved that the embedded IRX was not present in
the resident IOP module list after the supposed successful module-buffer load.

The cause was a legacy LOADFILE compatibility edge case. Older LOADFILE
services do not dispatch function 6 (`LoadModuleBuffer`). The SIFRPC request
can still complete without an output payload. EF2SDK previously reused the
same input/output structure, so the untouched request pointer was interpreted
as a positive module id and the zero argument length was interpreted as
`MODULE_RESIDENT_END`.

Alpha.14 explicitly detects an unchanged request pointer and reports the call
as unsupported. The audio bootstrap then enters the existing EF2 legacy
LOADFILE patch path and retries the embedded IRX load instead of accepting a
false success.


## Alpha.15: validated cleanup and latency pass

The alpha.14 smoke test was audibly validated in NetherSX2 on 2026-09-18:
the framebuffer reached teal-green and the generated melody played correctly.

With bring-up complete, alpha.15 removes the temporary EF2DBG/EF2NOMOD
telemetry, the alternate diagnostic RPC SID and build-time debug-symbol
plumbing. The production RPC receive buffer is now sized to the actual submit
packet instead of a 4096-byte scratch allocation.

The SPU2 block geometry remains intentionally unchanged. Comparison against
the current ps2sdk audsrv implementation confirms a 4096-byte loop buffer
containing two 2048-byte blocks and 512 stereo frames per refill, matching
EF2Audio's working layout.

The IOP ring is reduced from 8192 to 4096 stereo frames (about 85 ms at
48 kHz), and the smoke-test startup prefill is reduced from 4096 to 2048
frames (about 43 ms). This cuts IOP RAM use and startup latency while retaining
multiple SPU2 refill blocks of buffering. Underrun/overrun counters remain
available through the existing stats call.


## Alpha.16: playback controls and configurable latency

EF2Audio now exposes runtime controls for volume, pause/resume, stop, flush
and target queue latency. The IOP ring remains physically 4096 stereo frames,
while the effective queue limit can be reduced at runtime in 512-frame SPU2
block increments.

Pause preserves queued PCM and feeds silence to SPU2 without incrementing the
underrun counter. Stop halts the block transfer but preserves the queue, while
flush explicitly discards queued PCM. Volume is expressed in native SPU2
units from 0 through 0x3fff.

The stats RPC now also reports the effective latency, current volume and
started/paused state. The smoke test requests an approximately 43 ms queue and
uses a 0x3000 output volume, exercising the new control path before playback.
