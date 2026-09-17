# EF2SDK

[![CI](https://github.com/ReyFxck/EF2SDK/actions/workflows/ci.yml/badge.svg)](https://github.com/ReyFxck/EF2SDK/actions/workflows/ci.yml)

EF2SDK is an independent, community-driven SDK experiment for PlayStation 2 homebrew.

The project starts from a deliberately small target: boot a freestanding EE ELF without PS2SDK startup objects or libraries, then grow the runtime and hardware layers one subsystem at a time.

## Current status

**PoC / pre-alpha.** EF2SDK currently provides:

- an independent EE entry point and linker script;
- explicit `.bss` initialization in the freestanding startup;
- a raw EE kernel syscall wrapper for `SetGsCrt`;
- initial GS privileged-register definitions;
- explicit NTSC/PAL video-mode selection;
- a direct GIF FIFO path for small GS packets, without gsKit or PS2SDK libraries;
- initial framebuffer scanout and a full-screen sprite clear smoke test;
- a reusable `libef2.a` static library;
- a source-rate-agnostic EF2Audio core with fixed-point resampling and S16 mixing;
- a minimal independent EE SIFCMD/RPC client and in-memory IRX loader;
- an `ef2audio.irx` service with its own IOP ring buffer and private RPC protocol;
- an audible generated 32 kHz -> 48 kHz melody smoke test;
- CI builds for every push and pull request;
- automatic packaged artifacts;
- automatic GitHub Releases for `v*` tags;
- automatic release notes grouped by PR labels.

The alpha.1 background-only PCRTC experiment remained black in NetherSX2 and has been retired. The alpha.2 framebuffer/GIF path was visually confirmed in NetherSX2 on 2026-09-17 with the expected vivid-blue framebuffer.

## Bootstrap policy

EF2SDK does **not** link against PS2SDK in the PoC.

For now, CI borrows the existing R5900 compiler/binutils only as a bootstrap toolchain. The produced ELF is linked with `-nostdlib -nostartfiles -nodefaultlibs` and EF2SDK's own startup/linker files.

CI resolves the current official `ps2dev-ubuntu-latest.tar.gz` release asset, verifies its published SHA-256 digest, caches only the EE toolchain, and places `ee/bin` in `PATH`. PS2SDK headers, startup objects and libraries are not used by the PoC build.

The long-term goal is to reduce external bootstrap dependencies as EF2SDK matures.\n\nAlpha.4 uses the PS2SDK source tree only at **build time** for current IOP IRX rules/import headers, and the resulting `ef2audio.irx` imports the console ROM's `LIBSD` service as a temporary low-level SPU2 bootstrap. The EE ELF still does not link PS2SDK libraries, and EF2Audio does **not** use `audsrv`. Ring buffering, RPC, resampling and stream policy are EF2SDK code. Direct SPU2 ownership is an explicit follow-up target.

## Build

With the PS2 EE cross-toolchain in `PATH`:

```sh
make
make check
```

The build produces:

```text
build/ef2-boot.elf
build/libef2.a
build/ef2audio.irx
```

## Smoke test

The current example selects NTSC interlaced field mode, resets the GS and GIF, programs the CRT through the EE kernel syscall, configures `DISPFB2`/`DISPLAY2`, then draws a full-screen sprite into a 32-bit framebuffer using a small packed GIF packet.

A successful boot produces a solid vivid-blue screen. This was confirmed in NetherSX2. The smoke test writes the GIF FIFO directly; GIF DMA remains the next graphics transport milestone.

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
src/ee/sif/          Minimal SIFCMD/RPC client
src/iop/audio/       EF2Audio IOP streaming service
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
