#!/usr/bin/env python3
"""Flash usada pelo firmware.bin contra a particao app0 (o PlatformIO mede contra os 4 MB do chip e nao avisa).
Uso: flash_budget.py <pasta do firmware> <maximo em %>. Estourar app0 sobrescreve o LittleFS e o aparelho nao boota."""
import csv
import pathlib
import sys

root, limit = pathlib.Path(sys.argv[1]), float(sys.argv[2])


def size_of(text):
    text = text.strip()
    return int(text, 16) if text.lower().startswith("0x") else int(text[:-1]) * {"K": 1024, "M": 1 << 20}[text[-1].upper()]


app0 = 0
for row in csv.reader(l for l in (root / "partitions.csv").read_text().splitlines() if l.strip() and not l.startswith("#")):
    if row[0].strip() == "app0":
        app0 = size_of(row[4])
image = max(root.glob(".pio/build/*/firmware.bin"), key=lambda p: p.stat().st_mtime).stat().st_size
pct = 100.0 * image / app0
print(f"Flash: {pct:.1f}% (firmware.bin {image} de app0 {app0} bytes)")
if pct > limit:
    print(f"orcamento: Flash {pct:.1f}% passa de {limit:.0f}% da particao app0")
    sys.exit(1)
