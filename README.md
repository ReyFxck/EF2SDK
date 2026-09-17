# EF2SDK

EF2SDK is an independent, community-driven SDK experiment for PlayStation 2 homebrew.

The first goal is intentionally small: boot a freestanding EE ELF built without PS2SDK startup objects or libraries, then replace the remaining bootstrap pieces one subsystem at a time.

## Current status

**PoC / pre-alpha.** The repository currently provides:

- a tiny EE runtime entry point;
- an EF2SDK-owned linker script;
- a freestanding boot example;
- CI builds for every push and pull request;
- automatic packaged artifacts;
- automatic GitHub Releases for `v*` tags;
- automatic release notes grouped by PR labels.

## Bootstrap policy

EF2SDK does **not** link against PS2SDK in the PoC.

For now, CI borrows the existing R5900 compiler/binutils only as a bootstrap toolchain. The produced ELF is linked with `-nostdlib -nostartfiles -nodefaultlibs` and EF2SDK's own startup/linker files.

The long-term goal is to reduce external bootstrap dependencies as EF2SDK matures.

## Build

With the PS2 EE cross-toolchain in `PATH`:

```sh
make
make check
```

Or with the public ps2dev container used by CI:

```sh
docker run --rm \
  -v "$PWD:/src" \
  -w /src \
  ghcr.io/ps2dev/ps2dev:latest \
  sh -lc 'apk add --no-cache make file && make clean all check'
```

The PoC ELF is written to:

```text
build/ef2-boot.elf
```

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
