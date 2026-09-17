# Roadmap

## P0 - Repository and boot PoC

- [x] repository structure
- [x] independent linker script
- [x] independent EE entry point
- [x] freestanding build
- [x] CI artifacts
- [x] tag-driven releases
- [ ] boot confirmation in ARMSX2
- [ ] boot confirmation on real hardware

## P1 - EE runtime

- [ ] BSS initialization
- [ ] argc/argv handoff
- [ ] clean exit path
- [ ] cache helpers
- [ ] kernel syscall wrappers
- [ ] interrupt primitives
- [ ] timers

## P2 - Video

- [ ] GS register definitions
- [ ] safe video-mode initialization
- [ ] PAL/NTSC handling
- [ ] framebuffer API
- [ ] DMA/GIF primitives
- [ ] first visible hardware smoke test

## P3 - Input and IOP

- [ ] SIF/RPC base
- [ ] IOP module loading
- [ ] controller API
- [ ] memory card access
- [ ] USB mass storage path

## P4 - Audio

- [ ] low-level SPU2 path
- [ ] streaming ring buffer
- [ ] explicit format/rate/channel API
- [ ] hardware latency tests

## P5 - Runtime and ports

- [ ] allocator
- [ ] libc strategy
- [ ] C++ runtime strategy
- [ ] zlib
- [ ] libpng
- [ ] FreeType
- [ ] SDL3 feasibility
