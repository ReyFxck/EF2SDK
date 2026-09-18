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


## Runtime entry ABI

The freestanding entry point preserves the launcher's original `$a0/$a1`
registers before clearing BSS. EF2SDK then normalizes common launcher forms:

- direct C-style `argc/argv`;
- ps2link/PS2SDK-style `sargs_start`;
- a direct `sargs` pointer used by some custom loaders.

The normalized values are passed to `main(int argc, char **argv)` and are
also available through `ef2_runtime_get_args()`. The original register
values are retained in the public runtime info for diagnosing unusual
loaders.

Returning from `main` now calls the EE kernel KExit syscall through
`ef2_runtime_exit()` instead of entering the old permanent halt loop.
EF2SDK deliberately does not make SetupThread mandatory yet; the startup path
that has already been validated remains otherwise unchanged.


## Heap

The default EE runtime initializes an EF2-owned heap after
`__ef2_image_end`. The EE kernel `SetupHeap(start, -1)` and
`EndOfHeap()` syscalls are used only to establish the safe process-memory
range; allocation metadata and policy are implemented by EF2SDK.

The first allocator is a 16-byte-aligned first-fit free list with block
splitting, adjacent-block coalescing, calloc overflow checks and in-place
realloc growth when the following block is free. It intentionally exposes
`ef2_malloc`/`ef2_free` rather than overriding libc symbols yet.

The core allocator can also be initialized over an arbitrary caller-owned
buffer, which lets CI regression-test allocation/coalescing behavior on the
host without emulating EE kernel services.


## Minimal libc compatibility layer

EF2SDK keeps its native API namespaced, but ports often expect standard C
symbols. The first libc layer therefore uses two levels:

1. `<ef2/libc.h>` exposes EF2-owned memory/string primitives;
2. `include/ef2/compat/` contains opt-in `string.h`, `stdlib.h` and
   `stddef.h` compatibility headers for third-party builds.

The static library also exports standard `malloc/free/calloc/realloc` and
basic string/memory symbols. Because `libef2.a` is an archive, those objects
are pulled only when an application or port has unresolved references to
them. Existing EF2 code is not forced through the compatibility surface.

Third-party builds can opt in with:

`-I/path/to/EF2SDK/include/ef2/compat -I/path/to/EF2SDK/include`

The host CI compiles the memory/string implementation with standard aliases
disabled so the test executable cannot accidentally replace the host C
runtime's allocator/string functions.


## Third-party port: zlib

The first real third-party port is upstream zlib 1.3.2. EF2SDK does not vendor
the upstream source tree. The port target downloads the pinned release archive,
checks its SHA-256 digest and builds a separate `libz.a`.

Only the in-memory compression core is enabled initially. The gz*/stdio layer
is intentionally excluded until EF2SDK has a filesystem/stdio strategy.

The port is compiled with `-mlong32` and a generated, plainly marked
EF2-specific zconf.h. This is necessary because the R5900 EABI normally gives
C `long` 64-bit width, while zlib's traditional `uLong` public scalar and
many internal arithmetic assumptions are naturally 32-bit. The packaged
zconf.h fixes `uLong` to unsigned int so application code built with normal
EF2 flags sees the same ABI as libz.

Allocation and memory operations resolve through EF2SDK's minimal libc layer.
CI runs an actual host compress/uncompress roundtrip and also links the same
test against the R5900 `libz.a + libef2.a` with no libc/libgcc defaults.
