# EF2 Storage

Alpha.48 hardens the direct-card handshake after Alpha.47 successfully loaded the storage service in NetherSX2 but reported zero devices. Alpha.47 was checking for a `0x5A` card terminator without first issuing the native `0x27` SET_TERMINATOR command. Alpha.48 negotiates `0x5A` first, accepts a standard `0x55` reset terminator as a fallback, exposes the card flags byte and publishes per-backend scan diagnostics. The storage architecture itself remains the EF2-owned embedded `ef2storage.irx` introduced in Alpha.47.

The public EE API is `<ef2/storage.h>`. It exposes a device model instead of
pretending that every accessory in a memory-card slot speaks the same
protocol.

## Direct backends

- **Native PS2 memory card** — direct SIO2 `0x81/0x27` terminator negotiation followed by the `0x81/0x26` geometry query for
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

## Scan diagnostics

`ef2_storage_get_scan_diag()` returns the last scan result for MMCE, native
card terminator negotiation and native geometry on both memory-card SIO2
ports, plus the MX4SIO probe result. A result of `0` means that stage
succeeded; `-127` means the stage was not attempted.

The boot smoke logs these values even when the final device count is zero.
This is intentionally read-only diagnostics: SET_TERMINATOR only changes the
current protocol terminator byte and does not write card media.
