# Contributing

EF2SDK is experimental and hardware-first.

Before opening a large pull request, open an issue describing the subsystem or compatibility change so implementation choices can be discussed early.

## Pull requests

- keep changes focused;
- explain any hardware assumptions;
- include or update a smoke test when changing runtime behavior;
- keep third-party patches minimal;
- update documentation when public behavior changes;
- do not introduce PS2SDK link dependencies into the bootstrap PoC.

## Commit style

Short imperative subjects are preferred, for example:

```text
Add EE BSS initialization
Fix stack alignment in startup
Document GS video mode fallback
```
