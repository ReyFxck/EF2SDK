# EF2 Storage

Alpha.49 adds read-only native-card page access and a page-0 superblock smoke check. Alpha.48 hardens the direct-card handshake after Alpha.47 successfully loaded the storage service in NetherSX2 but reported zero devices. Alpha.47 was checking for a `0x5A` card terminator without first issuing the native `0x27` SET_TERMINATOR command. Alpha.48 negotiates `0x5A` first, accepts a standard `0x55` reset terminator as a fallback, exposes the card flags byte and publishes per-backend scan diagnostics. The storage architecture itself remains the EF2-owned embedded `ef2storage.irx` introduced in Alpha.47.

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

## Alpha.48 validation

NetherSX2 validated the direct geometry path on 2026-09-19. Two virtual native
cards were discovered, one on each memory-card SIO2 port. Both reported
512-byte pages, 16 pages per erase block, 16384 pages (8 MiB) and card flags
`0x2B`. MMCE correctly did not identify the ordinary virtual cards; the
MX4SIO probe also remained absent, as expected without those accessories.

The SET_TERMINATOR diagnostic still reports a mismatch on NetherSX2, but the
geometry transaction succeeds with the reset terminator. The backend therefore
continues to accept both standard ready terminators and treats the validated
geometry packet/EDC as authoritative.

## Native page reads

Alpha.49 exposes `ef2_storage_read_page()` for geometry-capable native-card
devices. The initial implementation is deliberately read-only:

1. select a page with native command `0x23`;
2. fetch four 128-byte chunks with `0x43`;
3. validate each chunk EDC;
4. finish the transfer with `0x81`.

The boot smoke reads only page zero and checks the 28-byte PS2 memory-card
superblock signature. No erase or write command is issued.


## Alpha.49 black-screen regression and Alpha.50 fix

The Alpha.49 NetherSX2 run black-screened as soon as the smoke attempted the
new page-zero read. The geometry-only Alpha.48 path remained known-good.

The failing implementation copied MCMAN's 128-byte data command shape
(`0x86` / 134-byte SIO2 packet) onto EF2's simple PIO FIFO path. MCMAN uses
DMA buffers for those long packets. Alpha.50 keeps the same native protocol
semantics but limits each PIO transaction to 32 data bytes: a page read is
sixteen `0x43` packets of 38 bytes, each independently EDC checked. This
avoids pushing the DMA-sized packet through the small synchronous PIO path.

### Format state

Native-card discovery now sets `ef2_storage_device_info.formatted`:

- `EF2_STORAGE_FORMAT_UNKNOWN (-1)`: page zero could not be read safely;
- `EF2_STORAGE_FORMAT_UNFORMATTED (0)`: page zero was readable but did not
  contain the PS2 filesystem superblock magic;
- `EF2_STORAGE_FORMAT_FORMATTED (1)`: page zero begins with
  `Sony PS2 Memory Card Format `.

The scan diagnostic also exposes the page-zero format probe result for both
ports. No media write is required to distinguish formatted from unformatted
cards.


## Alpha.50 NetherSX2 validation

Alpha.50 was validated in NetherSX2 on 2026-09-19 with two intentionally
unformatted 8 MiB virtual PS2 memory cards. Both cards remained directly
detectable with 512-byte pages, 16-page erase blocks and 16384 pages, and the
bounded 32-byte-chunk page reader no longer reproduced Alpha.49's black
screen.

The page-zero format probe completed on both ports and correctly reported
`formatted=0` / `fmt=0`. The cards were deliberately unformatted, so this
confirms the negative-format state rather than a false negative. The smoke
continued through pad/audio and reached the deliberate crash-handler trap.

Formatted-card detection (`formatted=1`) remains to be explicitly validated
with a formatted virtual card.
