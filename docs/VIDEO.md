# EF2 Video

EF2 Video is the freestanding EE-side GS layer.

## Current transport

Small packed GIF packets can still be written directly through the GIF FIFO,
but the preferred path is normal-mode EE DMAC channel 2.

`ef2_gif_dma_submit_qwords()` starts an asynchronous transfer and
`ef2_gif_dma_wait()` waits for channel completion.
`ef2_gif_dma_send_qwords()` combines both operations.

The video layer automatically falls back to the FIFO if GIF DMA initialization
or a later DMA transfer fails. `ef2_video_get_transport_stats()` exposes
whether DMA remains available and how many fallbacks have occurred.

## Primitive API

`ef2_video_draw_rect()` draws a clipped solid GS sprite using integer pixel
coordinates.

`ef2_video_draw_line()` draws a single solid GS line. The first version
clamps endpoints to the framebuffer; a full line-clipping algorithm can be
added later without changing the API.

`ef2_video_clear()` is now just a full-frame rectangle through the same
primitive path.

`ef2_video_get_size()` returns the active framebuffer dimensions.

## Next graphics steps

The next major step is textured sprites. That requires an explicit VRAM
allocation policy plus host-to-local transfers and texture-state helpers, so
it is kept separate from the small solid-primitive API.


## Alpha.24 texture baseline

Alpha.24 adds a conservative PSMCT32 texture heap behind the framebuffer.
The allocator is a resettable bump allocator and reserves texture storage in
GS page-sized chunks, avoiding overlap between small textures that share the
same swizzled GS page.

`ef2_video_upload_rgba32()` uploads 32-bit RGBA pixels through the existing
GIF transport. Host-to-local transfer setup uses BITBLTBUF/TRXPOS/TRXREG/
TRXDIR, followed by IMAGE-mode GIF payloads and TEXFLUSH. Large transfers are
split at the 15-bit GIF IMAGE NLOOP limit.

The initial upload API requires a 16-byte-aligned source and a total pixel
payload divisible by 16 bytes. This keeps the DMA path explicit and avoids
silently reading beyond caller memory; a staging path for arbitrary alignment
can be added later.

`ef2_video_draw_texture()` draws the complete texture as a nearest-filtered
sprite using TEX0/UV with decal texture function.

The boot smoke test generates a 32x32 checkerboard, uploads it to VRAM and
draws it enlarged to 96x96. It also shows a transport indicator:

- green square: GIF DMA remains active with zero fallbacks;
- orange square: GIF DMA failed or the video path fell back to FIFO.


## Alpha.25 PRIM attribute ownership fix

The alpha.24 emulator test confirmed that GIF DMA remained active with zero
fallbacks, but the textured sprite appeared as a flat 128/128/128 rectangle.
That color exactly matched the sprite RGBAQ value, indicating that the GS was
rendering the primitive while ignoring texture enable.

Alpha.25 explicitly writes PRMODECONT.AC=1 before the textured PRIM so TME/FST
and the other primitive attributes are sourced from PRIM. This removes the
post-reset ambiguity without relying on BIOS or previous GS state.


## Alpha.26 regions and blending

Alpha.26 adds `ef2_video_draw_texture_region()`, which selects an arbitrary
source rectangle with UV coordinates and maps it to an arbitrary destination
rectangle.

The same call accepts RGBA modulation. GS MODULATE mode treats component
0x80 as unity, so callers can tint a texture or scale its alpha without
rewriting texture memory.

Optional alpha blending uses the standard source-over equation:

`(source - destination) * source_alpha / 128 + destination`.

The alpha.26 smoke test draws the center 16x16 region of the validated
checkerboard over a yellow rectangle at approximately 50% opacity. The
original full-texture draw remains in place as a baseline.


## Alpha.27 PSMT8 + CLUT

Alpha.27 adds `ef2_video_upload_indexed8()` for 8-bit indexed textures with
a 256-entry RGBA32 palette.

The texture is stored as PSMT8 and the palette as PSMCT32. EF2SDK performs
the CSM1 palette index rearrangement internally by swapping address bits 3
and 4, so callers provide an ordinary linear palette indexed 0..255.

The indexed texture and its CLUT receive independent VRAM allocations.
Texture storage is page-safe for the PSMT8 128x64 page geometry, while the
CLUT is aligned to a 256-byte GS block.

The alpha.27 smoke test generates a 32x32 indexed gradient/pattern, uploads
its 256-color CLUT, and draws it enlarged next to the RGBA32 checkerboard.


## Alpha.28 PSMT4 + 16-color CLUT

Alpha.28 adds PSMT4 uploads with a 16-entry RGBA32 palette.

`ef2_video_pack_indices4()` packs ordinary 0..15 palette indices into the
GS byte layout: the even/left pixel occupies the low nibble and the odd/right
pixel occupies the high nibble.

`ef2_video_upload_indexed4()` uploads the packed image and a separate 8x2
PSMCT32 CSM1 CLUT. A 16-color CSM1 CLUT uses identity entry order, unlike the
256-color PSMT8 palette.

PSMT4 storage is reserved with the 128x128 GS page geometry. The alpha.28
smoke test draws a generated 16-color 32x32 pattern beside the PSMT8 test.


## NetherSX2 validation through alpha.28

The alpha.28 smoke screen was visually validated in NetherSX2 on 2026-09-18.

The validated screen simultaneously showed:

- the original solid rectangle and line primitives;
- the green GIF DMA/no-fallback indicator;
- the RGBA32 checkerboard texture;
- a cropped checkerboard region blended over a yellow destination;
- a PSMT8 indexed texture with its 256-entry RGBA32 CSM1 CLUT;
- a PSMT4 indexed texture with its 16-entry RGBA32 CSM1 CLUT.

This confirms the current emulator path from EE-side texture data through GIF
DMA, host-to-local transfer, VRAM allocation, TEX0/UV sampling, alpha blending
and both indexed CLUT formats.


## Alpha.29 VSync and double buffering

Alpha.29 adds an explicit frame-presentation API while preserving the
single-buffer behavior of existing callers.

EF2 Video now reserves two page-aligned 32-bit framebuffers during
initialization. Textures are allocated after both buffers, so enabling double
buffering later cannot invalidate existing VRAM layout assumptions.

New calls:

- `ef2_video_set_double_buffering()` selects compatibility single-buffer or
  back-buffer rendering;
- `ef2_video_wait_vsync()` waits for the next GS VSINT event with a bounded
  timeout;
- `ef2_video_present()` swaps display/draw buffers on VSync when double
  buffering is enabled;
- `ef2_video_get_frame_stats()` reports the active buffer indices, present
  count and VSync timeout count.

The boot diagnostic is rendered entirely into the hidden back buffer and is
made visible only by `ef2_video_present()`. Therefore seeing the alpha.29
diagnostic frame validates the first VSync-synchronized swap in addition to
the existing GIF DMA and texture tests.


## Alpha.29 texture regression isolation

The alpha.28 NetherSX2 test showed a useful split: solid primitives and the
green zero-fallback GIF DMA indicator remained correct, while every textured
sprite disappeared. Texture uploads returned success, so the failure was
isolated to texture sampling/draw state or a semantically wrong VRAM upload.

Alpha.29 restores the exact alpha.25 DECAL draw state for the ordinary
`ef2_video_draw_texture()` path, while leaving the newer MODULATE/alpha
region path isolated in `ef2_video_draw_texture_region()`. Indexed PSMT8 and
PSMT4 textures use the same DECAL diagnostic path with their CLUT state.

The region path also now targets the active draw framebuffer instead of
hard-coding FRAME_1 base zero, fixing an independent double-buffering bug.


## NetherSX2 validation: alpha.26-alpha.28

The alpha.28 visual smoke test was validated in NetherSX2 on 2026-09-18.

The captured frame simultaneously confirmed:

- GIF DMA remained active with zero FIFO fallbacks;
- the RGBA32 checkerboard still sampled correctly;
- partial UV selection and source-over alpha blending produced the expected
  composited region;
- the generated PSMT8 texture sampled through its 256-entry RGBA32 CSM1 CLUT;
- the generated PSMT4 texture sampled through its 16-entry RGBA32 CSM1 CLUT.

This closes emulator validation for the first RGBA32, PSMT8 and PSMT4 texture
paths. Real-hardware validation remains pending.


## Alpha.42 automatic console default

Alpha.42 adds `EF2_VIDEO_AUTO`. When requested, EF2 Video reads the console's
`rom:ROMVER` through EF2SDK's own SIFRPC/IOP-heap client and uses the ROMVER
region byte to select the default analog TV standard: region `E` selects PAL;
other standard retail regions select NTSC. This follows the console ROM region
rather than emulator host settings.

The ROMVER read does not link PS2SDK at runtime. EF2SDK asks the ROM IOP heap
service to load ROMVER into temporary IOP memory, copies the 14-byte identifier
through its existing EE-visible IOP window, then frees the temporary block.
If automatic detection is unavailable, video initialization falls back to NTSC
instead of failing boot.

`ef2_video_get_config()` exposes the resolved active configuration, so callers
never have to treat `EF2_VIDEO_AUTO` as the post-initialization mode. The boot
smoke now requests AUTO and logs the resolved standard, framebuffer size and
ROMVER string before running the existing validated drawing path.


## Alpha.42 PAL/NTSC auto-mode validation

Automatic ROMVER-based mode selection was validated in NetherSX2 on
2026-09-18 with two BIOSes. The PAL BIOS resolved standard 3 and initialized a
640x512 PAL field-mode framebuffer at 50 Hz. The NTSC BIOS resolved standard 2
and initialized a 640x448 NTSC field-mode framebuffer at approximately 60 Hz.
Both runs reported successful ROMVER reads and continued through GIF DMA,
VSync, audio, pad and the deliberate crash-handler test.

## Alpha.43 configurable framebuffer layouts

Alpha.43 extends `ef2_video_config` with a public framebuffer format and
optional framebuffer dimensions. Zero width/height preserve the region-native
640x448 NTSC or 640x512 PAL layout.

The first public framebuffer formats are:

- `EF2_VIDEO_FB_RGBA32` -> GS PSMCT32;
- `EF2_VIDEO_FB_RGB16` -> GS PSMCT16.

Custom widths must be multiples of 64 and divide the 2560-sample analog TV
scanout width exactly. Custom heights must divide the active NTSC/PAL height
exactly. The resulting integer horizontal/vertical magnification must fit the
GS DISPLAY register. This intentionally keeps the first layout API
deterministic instead of silently stretching unsupported dimensions.

Framebuffer VRAM reservation now follows GS page geometry rather than a raw
width*height byte estimate: PSMCT32 uses 64x32 pages and PSMCT16 uses 64x64
pages. `ef2_video_get_framebuffer_layout()` exposes the resolved format,
dimensions, GS PSM, per-buffer byte size, both buffer bases and the texture
heap start.

The Alpha.43 smoke selects RGB16 with region-native dimensions so the complete
existing primitive, RGBA32 texture, PSMT8, PSMT4, alpha, GIF DMA and crash
diagnostics exercise the alternate framebuffer format without changing their
screen positions.


## Alpha.43 RGB16 validation

The native-layout RGB16 smoke was visually validated in NetherSX2 on
2026-09-18. The NTSC run resolved a 640x448 framebuffer with format 1 /
PSMCT16 (PSM 2), reserved 573440 bytes per framebuffer and placed the texture
heap at byte 1146880. The established primitive, RGBA32 texture, PSMT8,
PSMT4, crop/alpha and green GIF DMA diagnostics remained visible, and the
run continued through audio, pad and the deliberate crash screen.

## Alpha.44 non-native scaled-layout smoke

Alpha.44 keeps the validated RGB16 framebuffer format but changes the smoke
layout to half the native vertical resolution while preserving the 640-pixel
width. Before video initialization the smoke reads the ROMVER-derived
standard and chooses 640x224 for NTSC or 640x256 for PAL.

Both layouts use GS vertical magnification 2x to fill the same analog TV
output. This isolates the non-native layout path from horizontal-coordinate
changes: all established drawing diagnostics keep their existing X positions
while the framebuffer page geometry, DISPLAY MAGV programming, double
buffering and crash renderer operate on the smaller logical height.

For PSMCT16 both half-height layouts reserve 327680 bytes per framebuffer
(10 GS pages across by 4 pages high), so the texture heap begins at byte
655360.


## Alpha.44 scaled-layout validation

The 640x224 NTSC RGB16 smoke was visually validated in NetherSX2 on
2026-09-18. The runtime reported PSMCT16, 327680 bytes per framebuffer and a
texture heap start at byte 655360. The GS expanded the logical 224-line
framebuffer to the NTSC field output while the established primitive,
RGBA32/PSMT8/PSMT4 texture, crop/alpha, GIF DMA, audio, pad and crash
diagnostics remained functional.
