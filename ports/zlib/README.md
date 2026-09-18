# EF2SDK zlib port

This directory contains the EF2SDK build glue for upstream zlib.

- Upstream: https://github.com/madler/zlib
- Version: 1.3.2
- Release archive SHA-256:
  `bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16`

The upstream source is not vendored into the EF2SDK repository. The port build
downloads the pinned release archive, verifies the published SHA-256 digest,
then builds only the in-memory compression/decompression core.

The gz*/stdio file layer is deliberately not built yet.

## EF2 ABI adaptation

The PS2 EE EABI uses 32-bit pointers and ints but 64-bit `long` by default.
Upstream zlib defines `uLong` as `unsigned long`, although its checksum and
length ABI expects a 32-bit-or-more scalar.

The EF2 port builds zlib with `-mlong32` and generates a clearly marked
EF2-specific zconf.h where public `uLong` is `unsigned int`. This keeps the
zlib public ABI stable when applications themselves are built with EF2SDK's
normal compiler flags.

CRC braiding is forced to 32-bit words and Adler division uses the no-divide
path to avoid accidental R5900 libgcc helper dependencies.

The generated `libz.a` resolves allocation and memory primitives through the
minimal EF2 libc compatibility layer.
