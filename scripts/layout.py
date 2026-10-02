#!/usr/bin/env python3
"""Mede os textos da tela com a tabela da propria fonte do OLED, nos dois idiomas, contra a largura de 128 px."""
import glob, re, sys

root = __file__.rsplit("/scripts/", 1)[0]
fonts_h = glob.glob(f"{root}/firmware/.pio/libdeps/*/*/src/OLEDDisplayFonts.h")
if not fonts_h:
    print("layout: fontes do OLED ainda nao baixadas (rode pio run)")
    sys.exit(0)
src = open(fonts_h[0]).read()


def font(size):
    body = re.search(rf"const uint8_t ArialMT_Plain_{size}\[\] PROGMEM = \{{(.*?)\}};", src, re.S).group(1)
    return [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]{2}", re.sub(r"//[^\n]*", "", body))]


FONTS = {s: font(s) for s in (10, 16, 24)}


def width(text, size):
    f = FONTS[size]
    return sum(f[4 + ((ord(c) if 32 <= ord(c) <= 255 else 63) - f[2]) * 4 + 3] for c in text)


def table(path, enum):
    text = open(path).read()
    names = [n.strip() for n in re.search(rf"enum class {enum} : uint16_t \{{(.*?)Count", text, re.S).group(1).split(",") if n.strip()]
    pairs = re.findall(r'\{"((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)"\}', text)
    return dict(zip(names, pairs))


# Os textos dos jogos moram no fmk (tabelas da base e dos jogos de 2 botoes).
fmk = next(iter(glob.glob(f"{root}/firmware/.pio/libdeps/*/marmota-fmk/src/game/..")), f"{root}/../marmota-fmk/src")
T = table(f"{root}/firmware/src/Text.h", "Txt")
G = table(f"{fmk}/game/GameText.h", "Str")
G.update(table(f"{fmk}/game/twobtn/Text.h", "Str"))

W = 128
failures = []


def check(label, used, limit):
    if used > limit:
        failures.append(f"{label}: {used} px, cabem {limit}")


# Valores medidos em Ui.cpp: pad da tecla 1, folga entre dicas 4, margem 2 de cada lado.
def hint_bar(items):
    return sum(width(k, 10) + 2 + 3 + width(l, 10) for k, l in items) + (len(items) - 1) * 4


for i, lang in enumerate(("pt-BR", "en")):
    g = lambda n: T[n][i]
    gg = lambda n: G[n][i]
    check(f"{lang} barra de menu", hint_bar([("A", g("HintDown")), ("B", g("HintOk")), ("A+B", g("HintBack"))]), W - 4)
    check(f"{lang} barra do Wi-Fi", hint_bar([("B", g("HintOff")), ("A+B", g("HintBack"))]), W - 4)
    check(f"{lang} barra de conexao", hint_bar([("A+B", g("HintCancel"))]), W - 4)
    for n in ("Settings", "Bluetooth", "Language", "Devices", "Wifi", "Library"):
        check(f"{lang} titulo {n}", width(g(n), 10), W - 8)
    for n in ("NoMusic", "NoDeviceTitle", "StorageError", "BtOffTitle", "Connected", "Received"):
        check(f"{lang} 16pt {n}", width(g(n), 16), W - 4)
    for n in ("BtOffBody", "HoldSettings", "HoldSend", "HoldConnect", "RestartOr", "ReplaceCard", "ConnectingTo", "AutoOpens", "OrGoTo", "PickOnPhone", "SendMore"):
        check(f"{lang} 10pt {n}", width(g(n), 10), W - 4)
    for n in ("Wifi", "Bluetooth", "Language", "Exit", "UploadMusic", "PickDevice", "Back"):
        check(f"{lang} rotulo {n}", width(g(n), 10), W - 4 - 24)  # sobra a direita para interruptor ou valor
    for n in ("On", "Off", "NoDevice"):
        check(f"{lang} valor {n}", width(g(n), 10), W // 2 - 4)
    check(f"{lang} recebendo 128/128 e 100%", width(g("Receiving") + " 128/128", 10) + width("100%", 10), W - 8 - 4)
    check(f"{lang} total 100%", width(g("Total") + " 100%", 10), W - 8)
    check(f"{lang} 'buscando...'", width(g("Scanning") + "...", 10), W - 28)
    check(f"{lang} 'buscar de novo'", width(g("ScanAgain"), 10), W - 28)
    number = 6 + width("00", 24) + 6
    check(f"{lang} contagem de faixas", width("128 " + g("Tracks"), 10), W - 4 - number)
    for n in ("GoalSnake", "GoalImpact", "GoalBreakout", "GoalStacker", "GoalRhythm", "GoalOrbit"):
        check(f"{lang} objetivo {n}", width(gg(n), 10), W - 4)
    for n in ("KeyLeft", "KeyRight", "KeyUp", "KeyDown", "KeyDrop", "KeyBrake"):
        check(f"{lang} tecla {n}", width(gg(n), 10), W // 2 - 16)  # legenda ao lado do quadrado da tecla
    check(f"{lang} barra do armazenamento", hint_bar([("B", g("HintBack")), ("A+B", g("HintBack"))]), W - 4)
    for n in ("Storage", "SdCard", "NoSd", "Internal"):
        check(f"{lang} armazenamento {n}", width(g(n), 10), W - 8)
    check(f"{lang} rotulo Storage", width(g("Storage"), 10), W - 4 - 24)
    sep = "," if i == 0 else "."
    check(f"{lang} livres de total (cartao comum)", width(f"9{sep}9 GB {g('FreeOf')} 99{sep}9 GB", 10), W - 8)
    check(f"{lang} livres / total (pior caso)", width(f"999{sep}9 GB / 999{sep}9 GB", 10), W - 8)
    check(f"{lang} SdCard com 999 faixas", width(g("SdCard"), 10) + 6 + width("999 " + g("Tracks"), 10), W - 8)
    check(f"{lang} Internal com 9 faixas (some se nao couber)", width(g("Internal"), 10) + 6 + width("9 " + g("Tracks"), 10), W - 8)
    for n in ("Games", "Library"):
        check(f"{lang} rotulo {n}", width(g(n), 10), W - 4 - 24)
    for n in ("Resume", "HowTo", "PlayAgain", "GameSnake", "GameImpact", "GameBreakout", "GameStacker", "GameRhythm", "GameOrbit"):
        check(f"{lang} rotulo {n}", width(gg(n), 10), W - 4 - 24)
    check(f"{lang} barra da pausa e do fim", hint_bar([("A", gg("HintDown")), ("B", gg("HintOk")), ("A+B", gg("HintBack"))]), W - 4)
    check(f"{lang} titulo da pausa", width(gg("Paused"), 10), W - 8)
    check(f"{lang} fim de jogo", width("65535  (" + gg("Best") + " 65535)", 10), W - 8)
    check(f"{lang} fim de jogo com recorde", width("65535  " + gg("NewRecord"), 10), W - 8)
    check(f"{lang} dica jogar", hint_bar([("B", gg("HintPlay")), ("A+B", gg("HintBack"))]), W - 4)
    check(f"{lang} rotulo ExtraFeatures", width(g("ExtraFeatures"), 10), W - 8)
    for n in ("ScreenOff", "ButtonLock"):
        check(f"{lang} rotulo {n}", width(g(n), 10), W - 28)
    for n in ("ScreenOffHelp", "ButtonLockHelp", "ExtraFeatures"):
        check(f"{lang} titulo/explicacao {n}", width(g(n), 10), W - 8)
    check(f"{lang} aviso sem dispositivo", width(g("NoDeviceTitle"), 10) + 12, W - 4)
    for n in ("LockedToast", "UnlockedToast", "UnlockHint"):
        check(f"{lang} aviso {n}", width(g(n), 10) + 28, W - 4)
    for g_ in ("Snake", "Impact", "Breakout", "Stacker", "Rhythm", "Orbit"):
        for line in range(1, 4):
            check(f"{lang} tutorial {g_} R{line}", width(gg(f"{g_}R{line}"), 10), W - 4)
    nxt, ext, play, back, ok = (gg(n) for n in ("HintNext", "HintExit", "HintPlay", "HintBack", "HintOk"))
    check(f"{lang} barra do tutorial (pagina 1)", hint_bar([("B", nxt), ("A+B", ext)]), W - 4)
    check(f"{lang} barra do tutorial (pagina 1, da pausa)", hint_bar([("B", nxt), ("A+B", back)]), W - 4)
    check(f"{lang} barra do tutorial (pagina 2)", hint_bar([("A", back), ("B", play)]), W - 4)
    check(f"{lang} barra do tutorial (pagina 2, da pausa)", hint_bar([("A", back), ("B", ok)]), W - 4)
check("nome nativo do idioma", max(width("Português", 10), width("English", 10)), W // 2 - 4)
check("nome no Wi-Fi", width("marmota mp3 WWWW", 10), W - 4)

if failures:
    print("layout: texto nao cabe na tela")
    print("\n".join("  " + f for f in failures))
    sys.exit(1)
print("layout ok")
