#!/usr/bin/env python3
"""Compila as telas reais para o host e grava PNGs ampliados em --out (padrao: /tmp/mp3-render).
Uso: scripts/render/render.py [filtro-de-nome]   (precisa de `pio run` antes, para baixar as libs)
     scripts/render/render.py --check    compara as cenas em ASCII com golden/ (sem PIL)
     scripts/render/render.py --update   regrava golden/"""
import glob, os, re, shutil, subprocess, sys

here = os.path.dirname(os.path.abspath(__file__))
fw = os.path.abspath(f"{here}/../../firmware")
out = os.environ.get("OUT", "/tmp/mp3-render")
os.makedirs(out, exist_ok=True)
libs = f"{fw}/.pio/libdeps/mp3"
oled_dirs = glob.glob(f"{libs}/*OLED*/src")
if "--print-fmk" not in sys.argv and not oled_dirs:
    sys.exit("libs ausentes: rode scripts/pio.sh run -d firmware antes")
oled = oled_dirs[0] if oled_dirs else ""


def fmk_source_dir():
    ini = open(f"{fw}/platformio.ini").read()
    linked = re.search(r"symlink://(\S+)", ini)
    root = linked.group(1) if linked else f"{libs}/marmota-fmk"
    return f"{root}/src"


fmk = os.environ.get("FMK", fmk_source_dir())
if "--print-fmk" in sys.argv:
    print(fmk)
    sys.exit(0)
# copia so a UI: assim os stubs de Library.h e Audio.h valem no lugar dos reais
ui = f"{out}/src"
shutil.rmtree(ui, ignore_errors=True)
os.makedirs(ui)
for f in glob.glob(f"{fw}/src/Ui*") + glob.glob(f"{fw}/src/Marmot*") + [f"{fw}/src/{n}" for n in ("View.h", "Text.h", "Menus.h", "Device.h", "SizeFormat.h")]:
    shutil.copy(f, ui)
srcs = [f"{here}/render.cpp", f"{ui}/Ui.cpp", f"{ui}/UiDraw.cpp", f"{ui}/UiMenus.cpp", f"{ui}/Marmot.cpp",
        f"{oled}/OLEDDisplay.cpp", f"{fmk}/Gfx.cpp", f"{fmk}/Locale.cpp", f"{fmk}/ui/Draw.cpp", f"{fmk}/ui/Menu.cpp"] + [f for f in glob.glob(f"{fmk}/game/*.cpp") if not f.endswith("GameHost.cpp")] + glob.glob(f"{fmk}/game/twobtn/*.cpp")
exe = f"{out}/render"
cmd = ["g++", "-std=gnu++17", "-w", "-funsigned-char", "-DARDUINO=1", "-o", exe, "-I", f"{here}/stub", f"-I{oled}", f"-I{fmk}", f"-I{ui}"] + srcs
subprocess.run(cmd, check=True)
text = subprocess.run([exe], check=True, capture_output=True, text=True).stdout.split("# ")[1:]
names = [b.strip().split("\n")[0].replace(" ", "-") for b in text]
golden = f"{here}/golden"


def scene_text(block):
    return block.strip() + "\n"


def check_goldens(update):
    os.makedirs(golden, exist_ok=True)
    failed = []
    for name, block in zip(names, text):
        path = f"{golden}/{name}.txt"
        if update:
            open(path, "w").write(scene_text(block))
        elif not os.path.exists(path) or open(path).read() != scene_text(block):
            failed.append(name)
    if update:
        keep = {f"{n}.txt" for n in names}
        for stale in set(os.listdir(golden)) - keep:
            os.remove(f"{golden}/{stale}")
        print(f"golden: {len(names)} cenas gravadas")
        return 0
    for name in failed:
        print(f"golden diferente: {name}")
    extra = set(os.listdir(golden)) - {f"{n}.txt" for n in names}
    for stale in sorted(extra):
        print(f"golden sem cena: {stale}")
    if failed or extra:
        print("render: use --update se a mudanca de tela for intencional")
        return 1
    print(f"golden ok: {len(names)} cenas")
    return 0


if "--check" in sys.argv or "--update" in sys.argv:
    sys.exit(check_goldens("--update" in sys.argv))

from PIL import Image

filters = [a for a in sys.argv[1:] if not a.startswith("--")]
S = 5
for block in text:
    head, *rows = block.strip().split("\n")
    name = head.replace(" ", "-")
    if filters and filters[0] not in name:
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
