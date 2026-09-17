# Architecture

EF2SDK starts with the Emotion Engine and grows outward.

## Bootstrap boundary

The PoC accepts one temporary external dependency: an R5900-capable compiler, assembler and linker.

It deliberately does not consume PS2SDK startup objects, headers or libraries when linking the boot ELF.

```text
C / assembly
    |
    v
R5900 bootstrap toolchain
    |
    v
EF2SDK crt + linker script
    |
    v
freestanding EE ELF
```

The boundary is intentional. It lets the project replace runtime and SDK behavior first without also having to implement a compiler on day one.

## Planned layers

```text
Applications
    |
Modern ports / convenience APIs
    |
EF2SDK subsystems
    |-- video / GS
    |-- input
    |-- audio
    |-- filesystems
    |-- IOP / RPC
    |-- networking
    |
EE runtime + kernel interface
    |
PlayStation 2 hardware / ROM kernel
```

## Design rules

1. Hardware behavior wins over emulator-only behavior.
2. Public APIs should be explicit about ownership, buffers and blocking behavior.
3. Every subsystem needs a minimal standalone smoke test.
4. Upstream third-party libraries should stay as close to upstream as possible.
5. CI must verify that accidental PS2SDK link dependencies are not introduced.
6. Documentation lives beside the implementation and changes with it.
