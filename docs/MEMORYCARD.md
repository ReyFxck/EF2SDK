# EF2 Memory Card

Alpha.45 introduces the first EF2SDK memory-card client.

The EE side uses EF2SDK's existing synchronous SIFCMD/RPC implementation and
does not link PS2SDK's libmc. At runtime it binds the standard memory-card RPC
service (SID 0x80000400) exposed by the console ROM's MCSERV/XMCSERV modules.

## Initialization

`ef2_mc_init()` initializes EF2 SIFRPC, then prefers the newer ROM module set:

- `rom0:XSIO2MAN`
- `rom0:XMCMAN`
- `rom0:XMCSERV`

If that service cannot be bound it falls back to the classic
`SIO2MAN/MCMAN/MCSERV` ROM set. The memory-card commands used in Alpha.45 are
the classic-compatible MCSERV commands that are also accepted by XMCSERV.

## Card information

`ef2_mc_get_info()` currently exposes:

- card type (none / PS1 / PS2 / PDA);
- free clusters;
- formatted/unformatted state inferred compatibly with classic MCSERV;
- the raw card-detection result.

The first query after a card change commonly returns -1 for a formatted card
or -2 for an unformatted card. A following query normally returns 0 if the
same card remains inserted.

Alpha.45 intentionally stops at read-only card metadata. File and directory
operations are the next storage step; no save data is created or modified by
the smoke test.


## Alpha.45 first emulator result

The first NetherSX2 run reached the memory-card stage but reported
`init=-201`. The rest of the smoke continued, so the failure was isolated to
the MCSERV bring-up/bind path rather than the general SIFRPC transport.

## Alpha.46 bring-up hardening

Alpha.46 changes the module preference to the classic
`SIO2MAN/MCMAN/MCSERV` family first, matching the classic 0x70/0x78 command
ABI used by the initial EF2 client. If that service still cannot be bound, EF2
tries `XSIO2MAN/XMCMAN/XMCSERV`.

The client now preserves the raw `ef2_sif_bind()` result and records both the
LOADFILE return value and module start result for every ROM module. The smoke
prints these as:

`[EF2][MC] modules=... sio2=load/start mcman=load/start mcserv=load/start bind=...`

This makes the next emulator run sufficient to distinguish missing ROM files,
module-start dependency failures and a pure RPC bind failure.
