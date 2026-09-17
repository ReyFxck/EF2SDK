# Roadmap

## P0 - Repository and boot PoC

- [x] repository structure
- [x] independent linker script
- [x] independent EE entry point
- [x] freestanding build
- [x] CI artifacts
- [x] tag-driven releases
- [x] boot confirmation in NetherSX2
- [ ] boot confirmation in ARMSX2 (currently reaches OSDSYS instead of direct ELF boot)
- [ ] boot confirmation on real hardware

## P1 - EE runtime

- [x] BSS initialization
- [ ] argc/argv public ABI
- [ ] clean exit path
- [ ] cache helpers
- [x] first raw kernel syscall wrapper (`SetGsCrt`, syscall 0x02)
- [ ] general kernel syscall wrappers
- [ ] interrupt primitives
- [ ] timers

## P2 - Video

- [x] initial GS privileged register definitions
- [x] explicit NTSC/PAL mode selection
- [x] background-only PCRTC smoke-test path
- [ ] validate visible background smoke test in emulator/hardware
- [ ] automatic region/default-mode selection
- [ ] framebuffer API
- [ ] DMA/GIF primitives
- [ ] GS drawing-environment registers
- [ ] first framebuffer clear / primitive draw

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
