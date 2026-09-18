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
