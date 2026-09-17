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
- [x] retire failed background-only PCRTC smoke-test path
- [x] initial DISPFB/DISPLAY framebuffer scanout setup
- [x] direct GIF FIFO packet writer
- [x] initial GS drawing-environment register packers
- [x] framebuffer clear implemented as a GS sprite
- [x] validate visible framebuffer clear in NetherSX2 (2026-09-17)
- [ ] validate framebuffer path on real hardware
- [ ] automatic region/default-mode selection
- [ ] public framebuffer allocation/configuration API
- [ ] GIF DMA path
- [ ] primitive API beyond the smoke test

## P3 - Input and IOP

- [ ] SIF/RPC base
- [ ] IOP module loading
- [ ] controller API
- [ ] memory card access
- [ ] USB mass storage path

## P4 - Audio

- [x] source-rate-agnostic S16 mono/stereo stream description
- [x] freestanding Q32 fixed-point sample-rate converter
- [x] nearest and linear resampling modes
- [x] saturating S16 mixer helper
- [x] host-side audio regression tests in CI
- [ ] low-level SPU2 path
- [ ] autonomous IOP streaming ring buffer
- [ ] per-stream and per-bus mixer
- [ ] cubic resampler
- [ ] band-limited/sinc resampler
- [ ] underrun/overrun and latency diagnostics
- [ ] hardware latency tests

## P5 - Runtime and ports

- [ ] allocator
- [ ] libc strategy
- [ ] C++ runtime strategy
- [ ] zlib
- [ ] libpng
- [ ] FreeType
- [ ] SDL3 feasibility

## P6 - Community extensions

- [ ] unified input layer across pad and USB HID backends
- [ ] asynchronous VFS operations
- [ ] crash screen with exception/register diagnostics
- [ ] lightweight profiler and GS/DMA timing counters
- [ ] package/port workflow for modern third-party libraries
- [ ] compatibility adapters for selected legacy PS2 homebrew APIs
