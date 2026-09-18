# EF2 Storage

Alpha.47 pivots storage to an EF2-owned embedded service, `ef2storage.irx`.

The public EE API is `<ef2/storage.h>`. It exposes a device model instead of
pretending that every accessory in a memory-card slot speaks the same
protocol.

## Direct backends

- **Native PS2 memory card** — direct SIO2 `0x81/0x26` geometry query for
  page size, erase-block pages and total page count. This path does not use
  MCMAN/MCSERV.
- **MMCE** — direct MMCE v1 ping/product detection, status, virtual-card and
  channel selection, Game ID, and open/close/read/write/lseek filesystem
  operations.
- **MX4SIO** — direct SD-over-SIO2 SPI init for SD v1/v2, SDHC/SDXC and MMC,
  CSD capacity discovery, and 512-byte single-sector reads/writes.

The old `ef2_mc_*` ROM MCMAN/MCSERV client remains only as a compatibility
and diagnostic path.

## Device capabilities

`ef2_storage_scan()` returns `ef2_storage_device_info` entries with a
kind and capability bitmap.

MMCE is probed before the native-card geometry command because an MMCE device
can also emulate a normal PS2 memory card. When the native geometry query also
works, that geometry is attached to the MMCE entry.

MX4SIO is intentionally exposed as a block device. It is not mislabeled as a
normal memory card.

## MMCE interoperability

The packet implementation follows the public Multi-purpose Memory Card
Emulator protocol. The MMCE protocol/reference implementation is MIT licensed.
Known product IDs include SD2PSX, MemCard PRO2, PicoMemcard+ and
PicoMemcardZero; unknown future IDs are not rejected.

The EE MMCE API includes virtual-card/channel control and a filesystem subset,
so software can use the SD-backed filesystem without going through ROM
MCMAN/MCSERV.

## MX4SIO

The first MX4SIO backend is deliberately PIO-first. It implements SD SPI
initialization, bit ordering for SIO2, CSD capacity parsing, single-block read
(CMD17) and single-block write (CMD24). The public API is already sector based,
so a later DMA/multi-block accelerator will not require an EE API redesign.

The boot smoke performs discovery only and never writes storage.

## Native PS2 memory cards

Native geometry discovery is independent now. Full native-card authentication,
raw page I/O, ECC/bad-block management and the PS2 card filesystem remain
follow-up work; those are materially different from MMCE and MX4SIO and are
kept behind the same storage device model rather than forcing one protocol onto
all hardware.

## SIO2 ownership

`ef2storage.irx` uses synchronous SIO2 transactions and does not install a
persistent SIO2 interrupt handler. This allows the existing EF2Pad smoke to
load afterwards when calls are serialized. A shared SIO2 broker remains a
future requirement for applications issuing pad and storage RPCs concurrently
from multiple EE threads.
