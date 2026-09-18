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
- [x] loader-aware argc/argv public ABI
- [x] main-return/KExit clean exit path
- [x] initial range-based EE D-cache writeback/invalidate helper
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
- [x] initial two-framebuffer VRAM reservation and explicit present API
- [x] bounded GS VSINT/VSync wait helper
- [x] initial VSync-synchronized double buffering
- [ ] configurable public framebuffer formats/layouts
- [x] initial GIF DMA normal-mode path with FIFO fallback
- [x] validate GIF DMA transport in NetherSX2 (2026-09-18)
- [ ] validate GIF DMA transport on real hardware
- [x] explicit emulator-visible GIF DMA/fallback diagnostic
- [x] initial solid rectangle/sprite and line primitive API
- [x] initial page-safe VRAM texture allocator
- [x] initial PSMCT32 host-to-local texture upload
- [x] initial textured sprite API
- [x] partial UV/crop textured sprite API
- [x] RGBA modulation and source-over alpha blending
- [x] validate UV crop + alpha blending in NetherSX2 (2026-09-18)
- [x] initial PSMT8 indexed texture + RGBA32 CSM1 CLUT support
- [x] validate PSMT8 + 256-color CLUT in NetherSX2 (2026-09-18)
- [x] initial PSMT4 indexed texture + 16-color RGBA32 CSM1 CLUT support
- [x] validate PSMT4 + 16-color CLUT in NetherSX2 (2026-09-18)

## P3 - Input and IOP

- [x] minimal EE SIFCMD/RPC client base
- [x] in-memory IOP module loading
- [x] initial controller API and private EF2Pad RPC protocol
- [x] direct SIO2 polling backend with timeout/reconnect handling
- [x] held/pressed/released helpers and normalized analog deadzone API
- [x] best-effort DualShock analog/pressure configuration
- [ ] validate EF2Pad on real fat/slim PS2 hardware
- [x] initial rumble/actuator API and direct SIO2 actuator alignment
- [ ] validate rumble on real DualShock 1/2 and common clones
- [x] initial direct-SIO2 Multitap topology and per-slot API
- [ ] validate Multitap slot switching on real hardware
- [ ] memory card access
- [ ] USB mass storage path

## P4 - Audio

- [x] source-rate-agnostic S16 mono/stereo stream description
- [x] freestanding Q32 fixed-point sample-rate converter
- [x] nearest and linear resampling modes
- [x] saturating S16 mixer helper
- [x] host-side audio regression tests in CI
- [x] first EF2Audio IOP service and private RPC protocol
- [x] autonomous IOP PCM streaming ring buffer
- [x] SPU2 block-transfer bootstrap through ROM LIBSD
- [x] embedded 32 kHz -> 48 kHz audible melody smoke test
- [x] direct SPU2 backend replacing the temporary LIBSD dependency
- [ ] per-stream and per-bus mixer
- [ ] cubic resampler
- [ ] band-limited/sinc resampler
- [x] underrun/overrun and latency diagnostics
- [ ] hardware latency tests

## P5 - Runtime and ports

- [x] initial freestanding 16-byte-aligned heap allocator
- [x] malloc/free/calloc/realloc equivalents with coalescing
- [x] host-side heap regression tests
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
