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
