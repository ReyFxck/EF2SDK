# EF2SDK

[![CI](https://github.com/ReyFxck/EF2SDK/actions/workflows/ci.yml/badge.svg)](https://github.com/ReyFxck/EF2SDK/actions/workflows/ci.yml)

EF2SDK is an independent, community-driven SDK experiment for PlayStation 2 homebrew.

The project starts from a deliberately small target: boot a freestanding EE ELF without PS2SDK startup objects or libraries, then grow the runtime and hardware layers one subsystem at a time.

## Current status

**PoC / pre-alpha.** EF2SDK currently provides:

- an independent EE entry point and linker script with loader-aware argc/argv normalization, clean KExit on main return, an EF2-owned freestanding heap allocator and an opt-in minimal libc compatibility layer with setjmp/longjmp error recovery plus bounded `snprintf`/`vsnprintf` formatting and sink-backed `printf`/`fprintf`;
- an opt-in EE crash vector that captures registers/COP0 state, appends a postmortem record to the RAM log and renders a crash screen when video is available;
- explicit `.bss` initialization in the freestanding startup;
- raw EE kernel syscall wrappers for thread, semaphore and core system services, plus public INTC/DMAC critical-section helpers, direct EE timer primitives and lightweight COP0 Count profiling counters;
- initial GS privileged-register definitions;
- explicit NTSC/PAL video-mode selection plus ROMVER-based automatic default selection;
- a direct GIF FIFO path plus preferred GIF DMA transport with explicit fallback diagnostics, without gsKit or PS2SDK libraries;
- configurable RGBA32/RGB16 framebuffer scanout and native/scaled framebuffer layouts with optional VSync-synchronized double buffering, solid rectangle/line primitives, page-safe texture VRAM allocation, RGBA32/PSMT8/PSMT4 uploads, CSM1 CLUTs, textured sprites, UV cropping and alpha blending;
- a reusable `libef2.a` static library;
- an initial pinned upstream zlib 1.3.2 in-memory port producing a separate `libz.a`;
- a source-rate-agnostic EF2Audio core with fixed-point resampling and S16 mixing;
- a minimal independent EE SIFCMD/RPC client and in-memory IRX loader, an EF2-owned direct-SIO2 storage service, native PS2 memory-card geometry discovery, MMCE control/filesystem support, and MX4SIO SD block I/O;
- an `ef2audio.irx` service with its own IOP ring buffer and private RPC protocol;
- an audible generated 32 kHz -> 48 kHz melody smoke test;
- EF2Audio runtime volume, pause/resume, stop, flush, stats and configurable queue latency;
- direct EF2-owned SPU2/DMA streaming without a runtime LIBSD dependency;
- an initial EF2Pad controller layer with its own SIO2 transport, timeouts, reconnect handling, held/edge helpers, analog deadzone normalization, direct DualShock rumble, dual-port polling and experimental Multitap slot support;
- CI builds for every push and pull request;
- automatic packaged artifacts;
- automatic GitHub Releases for `v*` tags;
- automatic release notes grouped by PR labels.

The alpha.24 explicit transport diagnostic confirmed the GIF DMA path remains active with zero FIFO fallbacks in NetherSX2 on 2026-09-18. Alpha.26-alpha.28 then validated cropped UVs, source-over alpha blending, PSMT8/256-color CLUTs and PSMT4/16-color CLUTs in the same emulator. By alpha.28, UV cropping, alpha blending, PSMT8/256-color CLUT and PSMT4/16-color CLUT rendering were also visually confirmed in NetherSX2. The alpha.1 background-only PCRTC experiment remained black in NetherSX2 and has been retired. The alpha.2 framebuffer/GIF path was visually confirmed in NetherSX2 on 2026-09-17. The embedded EF2Audio path was audibly confirmed in NetherSX2 on 2026-09-18, including the legacy LOADFILE compatibility patch and 32 kHz -> 48 kHz melody stream. EF2Pad's direct SIO2 polling, button edges and reconnect-safe RPC path were also confirmed in NetherSX2 on 2026-09-18.

Alpha.47 pivots storage to an EF2-owned embedded `ef2storage.irx`: direct native-card geometry probing, MMCE v1 product/control/filesystem commands, and a PIO-first MX4SIO SD block backend now share one public `ef2_storage_*` device model. The old ROM MCMAN/MCSERV client remains only as a compatibility path. Alpha.46 hardens the memory-card bring-up after the first NetherSX2 run exposed a pre-bind failure: it now tries the classic SIO2MAN/MCMAN/MCSERV family first, preserves raw bind errors and logs each ROM module's loader/start result before falling back to the X module family. Alpha.45 adds initial memory-card access through EF2SDK's own synchronous SIFRPC client and the console ROM's MCMAN/MCSERV service, with slot type/free-space/formatted diagnostics and no PS2SDK runtime library. Alpha.44 keeps RGB16 enabled and switches the smoke to a region-relative half-height framebuffer (640x224 NTSC or 640x256 PAL) so the GS vertical magnification path is tested without disturbing the established horizontal diagnostic layout. The Alpha.44 640x224 NTSC scaled RGB16 layout was visually validated in NetherSX2 on 2026-09-18 with the complete graphics/audio/pad/crash smoke still intact. Alpha.43 adds public framebuffer format/layout configuration with RGBA32 and RGB16 scanout, page-correct VRAM sizing and integer-scaled layouts; its smoke deliberately runs RGB16 while preserving the existing drawing diagnostics. The Alpha.43 RGB16 native-layout smoke was visually validated in NetherSX2 on 2026-09-18, including primitives, RGBA32/PSMT8/PSMT4 textures, alpha blending, GIF DMA, audio/pad continuation and the deliberate crash screen. Alpha.42 added ROMVER-based automatic NTSC/PAL default selection through EF2SDK's own SIF/IOP-heap path, with an NTSC fallback if detection is unavailable. Automatic detection was validated in NetherSX2 on 2026-09-18 with both PAL (640x512, resolved standard 3) and NTSC (640x448, resolved standard 2) BIOSes. Alpha.41 expands the independent raw-kernel surface with thread, semaphore, cache/COP0 and basic machine/memory syscalls, and the boot smoke now verifies current-thread status plus a create/poll/signal/delete semaphore round trip. The Alpha.41 kernel smoke was validated in NetherSX2 on 2026-09-18: current thread/status, semaphore poll/signal/delete, 32 MiB memory reporting and the existing timer/video/audio/pad path all completed before the deliberate crash trap. Alpha.40 added reusable COP0 Count profiling counters plus GIF DMA completion/wait telemetry and VSync wait timing without changing the validated render path. Its telemetry was validated in NetherSX2 on 2026-09-18 with 21 GIF DMA submissions/completions, 558 QWC, zero GIF timeouts and a successful VSync timing sample. Alpha.39 added direct EE Timer0/1/2 primitives, COP0 Count profiling ticks and public INTC/DMAC handler/critical-section helpers. Alpha.35 added the buffer-only integer/string formatting core. Alpha.36 added sink-backed `ef2_printf`/`ef2_fprintf`, opt-in standard aliases, and a bounded-timeout EE SIO sink. Alpha.37 added an always-on 8 KiB RAM diagnostic ring, optional SIO mirroring that fails open instead of hanging the app, and structured boot/subsystem milestones. Alpha.38 adds an opt-in synchronous EE crash vector with register/COP0 capture, raw RAM-log postmortem output and a framebuffer crash screen. Filesystem-backed streams remain deferred until the VFS layer exists.

## Bootstrap policy

EF2SDK does **not** link against PS2SDK in the PoC.

For now, CI borrows the existing R5900 compiler/binutils only as a bootstrap toolchain. The produced ELF is linked with `-nostdlib -nostartfiles -nodefaultlibs` and EF2SDK's own startup/linker files.

CI resolves the official ps2dev bootstrap bundle and pins a PS2SDK source revision for IOP build rules/import headers. The EE ELF remains freestanding and does not link PS2SDK startup objects or libraries.

The long-term goal is to reduce external bootstrap dependencies as EF2SDK matures.

The PS2SDK source tree is still used at **build time** for IOP IRX rules/import headers, but EF2Audio no longer imports or loads the console ROM's `LIBSD` service. The IOP service owns its SPU2 register setup, DMA channel and DMA interrupt directly. The EE ELF remains freestanding and does not link PS2SDK libraries, and EF2Audio does not use `audsrv`.

## Build

With the PS2 EE cross-toolchain in `PATH`:

```sh
make
make check
make host-test
```

The build produces:

```text
build/ef2-boot.elf
build/libef2.a
build/ports/zlib-target/libz.a
build/ef2audio.irx
build/ef2pad.irx
build/ef2storage.irx
```

## Smoke test

The current example selects the console's ROMVER-derived NTSC/PAL default in interlaced field mode, resets the GS and GIF, programs the CRT through the EE kernel syscall, configures `DISPFB2`/`DISPLAY2`, then draws the current configured framebuffer through packed GIF packets. Alpha.44's smoke selects RGB16 plus a half-height region-relative framebuffer to exercise non-native vertical scaling. It now exercises the GIF DMA path with a clipped solid rectangle and line before entering the audio/input smoke loop.

A successful current boot reaches a teal-green framebuffer and plays the generated melody. This validates the framebuffer/GIF path, freestanding EE SIFRPC client, embedded IRX loading, EF2Audio RPC/ring buffering and SPU2 streaming in NetherSX2.

## Releases

Pushes and pull requests produce short-lived CI artifacts.

A tag such as:

```sh
git tag v0.0.1-alpha.4
git push origin v0.0.1-alpha.4
```

builds, packages and publishes a GitHub Release automatically with generated release notes.

## Repository layout

```text
include/             Public EF2SDK headers
src/ee/runtime/      EE startup/runtime code
src/ee/kernel/       Raw EE kernel interface
src/ee/gs/           GS/video implementation
src/ee/audio/        Rate conversion, mixer and IOP backend
src/ee/input/        EF2Pad EE API/backend
src/ee/sif/          Minimal SIFCMD/RPC client
src/iop/audio/       EF2Audio IOP streaming service
src/iop/pad/         EF2Pad SIO2 controller service
ld/                  EF2SDK linker scripts
examples/            Hardware/emulator smoke tests
docs/                Architecture and roadmap
scripts/             Packaging and CI helpers
.github/              CI, releases and community automation
```

## Project direction

The intended order is:

1. EE startup/runtime
2. kernel/syscall wrappers
3. GS/video
4. input
5. IOP/RPC
6. audio
7. storage/filesystems
8. libc/runtime integration
9. modern third-party library ports
10. optional independent assembler/linker/toolchain work

See [`docs/ROADMAP.md`](docs/ROADMAP.md) and [`docs/AUDIO.md`](docs/AUDIO.md).

## Independence

EF2SDK is not affiliated with Sony Interactive Entertainment, ps2dev, or the PS2SDK project.

PlayStation and PlayStation 2 are trademarks of Sony Interactive Entertainment.

## License

MIT. See [`LICENSE`](LICENSE).
