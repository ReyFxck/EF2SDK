#!/usr/bin/env python3
import pathlib
import sys

if len(sys.argv) != 4:
    raise SystemExit("usage: bin2c.py INPUT OUTPUT SYMBOL")

src = pathlib.Path(sys.argv[1])
dst = pathlib.Path(sys.argv[2])
symbol = sys.argv[3]
data = src.read_bytes()

lines = [
    "#include <ef2/base.h>",
    "",
    f"const ef2_u8 {symbol}[] EF2_ALIGN(64) = {{",
]

for offset in range(0, len(data), 16):
    chunk = data[offset:offset + 16]
    lines.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")

lines += [
    "};",
    f"const ef2_u32 {symbol}_size = sizeof({symbol});",
    "",
]

dst.parent.mkdir(parents=True, exist_ok=True)
dst.write_text("\n".join(lines), encoding="utf-8")
