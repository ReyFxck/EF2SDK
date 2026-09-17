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
- a background-only PCRTC smoke test that requires no framebuffer, GIF DMA or gsKit;
- CI builds for every push and pull request;
- automatic packaged artifacts;
- automatic GitHub Releases for `v*` tags;
- automatic release notes grouped by PR labels.

The current `examples/boot` build should show a vivid blue screen when the minimal GS/PCRTC path works. Until that is visually confirmed, the video milestone remains experimental.

## Bootstrap policy

EF2SDK does **not** link against PS2SDK in the PoC.

For now, CI borrows the existing R5900 compiler/binutils only as a bootstrap toolchain. The produced ELF is linked with `-nostdlib -nostartfiles -nodefaultlibs` and EF2SDK's own startup/linker files.

CI resolves the current official `ps2dev-ubuntu-latest.tar.gz` release asset, verifies its published SHA-256 digest, caches only the EE toolchain, and places `ee/bin` in `PATH`. PS2SDK headers, startup objects and libraries are not used by the PoC build.

The long-term goal is to reduce external bootstrap dependencies as EF2SDK matures.

## Build

With the PS2 EE cross-toolchain in `PATH`:

```sh
make
make check
```

The PoC ELF is written to:

```text
build/ef2-boot.elf
```

## Smoke test

The current example selects NTSC, interlaced field mode, resets the GS, programs the CRT through the EE kernel syscall and switches the PCRTC merge output to `BGCOLOR`.

A successful boot should produce a solid blue screen. This stage intentionally does **not** allocate a GS framebuffer or submit GIF packets yet.

## Releases

Pushes and pull requests produce short-lived CI artifacts.

A tag such as:

```sh
git tag v0.0.1-alpha.1
git push origin v0.0.1-alpha.1
```

builds, packages and publishes a GitHub Release automatically with generated release notes.

## Repository layout

```text
include/             Public EF2SDK headers
src/ee/runtime/      EE startup/runtime code
src/ee/kernel/       Raw EE kernel interface
src/ee/gs/           GS/video implementation
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

See [`docs/ROADMAP.md`](docs/ROADMAP.md).

## Independence

EF2SDK is not affiliated with Sony Interactive Entertainment, ps2dev, or the PS2SDK project.

PlayStation and PlayStation 2 are trademarks of Sony Interactive Entertainment.

## License

MIT. See [`LICENSE`](LICENSE).
