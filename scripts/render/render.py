#!/usr/bin/env python3
"""Compila as telas reais para o host e grava PNGs ampliados em --out (padrao: /tmp/mp3-render).
Uso: scripts/render/render.py [filtro-de-nome]   (precisa de `pio run` antes, para baixar as libs)"""
import glob, os, shutil, subprocess, sys
from PIL import Image

here = os.path.dirname(os.path.abspath(__file__))
fw = os.path.abspath(f"{here}/../../firmware")
out = os.environ.get("OUT", "/tmp/mp3-render")
os.makedirs(out, exist_ok=True)
libs = f"{fw}/.pio/libdeps/mp3"
oled = glob.glob(f"{libs}/*OLED*/src")[0]
fmk = f"{libs}/marmota-fmk/src"
# copia so a UI: assim os stubs de Library.h e Audio.h valem no lugar dos reais
ui = f"{out}/src"
shutil.rmtree(ui, ignore_errors=True)
os.makedirs(ui)
for f in glob.glob(f"{fw}/src/Ui*") + glob.glob(f"{fw}/src/Marmot*") + [f"{fw}/src/{n}" for n in ("View.h", "Text.h", "Menus.h", "Device.h", "SizeFormat.h")]:
    shutil.copy(f, ui)
srcs = [f"{here}/render.cpp", f"{ui}/Ui.cpp", f"{ui}/UiDraw.cpp", f"{ui}/UiMenus.cpp", f"{ui}/Marmot.cpp",
        f"{oled}/OLEDDisplay.cpp", f"{fmk}/Gfx.cpp", f"{fmk}/Icons.cpp", f"{fmk}/Locale.cpp", f"{fmk}/ui/Draw.cpp", f"{fmk}/ui/Menu.cpp"] + [f for f in glob.glob(f"{fmk}/game/*.cpp") if not f.endswith("GameHost.cpp")] + glob.glob(f"{fmk}/game/twobtn/*.cpp")
exe = f"{out}/render"
cmd = ["g++", "-std=gnu++17", "-w", "-funsigned-char", "-DARDUINO=1", "-o", exe, "-I", f"{here}/stub", f"-I{oled}", f"-I{fmk}", f"-I{ui}"] + srcs
subprocess.run(cmd, check=True)
text = subprocess.run([exe], check=True, capture_output=True, text=True).stdout.split("# ")[1:]
S = 5
for block in text:
    head, *rows = block.strip().split("\n")
    name = head.replace(" ", "-")
    if len(sys.argv) > 1 and sys.argv[1] not in name:
        continue
    img = Image.new("RGB", (128 * S, 64 * S), (8, 12, 20))
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c == "#":
                for j in range(S - 1):
                    for i in range(S - 1):
                        img.putpixel((x * S + i, y * S + j), (120, 220, 255))
    img.save(f"{out}/{name}.png")
print(out)
