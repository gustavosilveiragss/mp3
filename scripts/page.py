#!/usr/bin/env python3
"""Gera firmware/src/PortalPage.h a partir de firmware/web/portal.html (minificado de leve e gzip).

Uso: page.py [--check]   --check falha se o .h gerado estiver desatualizado.
"""
import gzip
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "firmware/web/portal.html"
OUT = ROOT / "firmware/src/PortalPage.h"
MAX_RAW = 32 * 1024
MAX_GZ = 12 * 1024
MAX_LINES = 480


WORD = re.compile(r"[\w$]")


def squeeze(code):
    """Tira espacos entre tokens do JS fora de strings e regex; mantem as quebras de linha (ASI)."""
    out, i, n = [], 0, len(code)
    while i < n:
        c = code[i]
        prev = "".join(out).rstrip(" ")[-8:]
        regex = c == "/" and code[i + 1] not in "/*" and (not prev or prev[-1] in "(,=:[!&|?{};\n" or prev.endswith("return"))
        if c in "'\"`" or regex:
            j = i + 1
            klass = False
            while code[j] != c or klass:
                if code[j] == "\\":
                    j += 1
                elif regex and code[j] in "[]":
                    klass = code[j] == "["
                j += 1
            out.append(code[i:j + 1])
            i = j + 1
        elif c == "\n" and (not out or out[-1][-1] in ";{,(" or code[i + 1:i + 2] == "}"):
            i += 1
        elif c == " ":
            j = i
            while j < n and code[j] == " ":
                j += 1
            if out and j < n and WORD.match(out[-1][-1]) and WORD.match(code[j]):
                out.append(" ")
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def minify(html):
    """Tira indentacao, linhas vazias e comentarios // de linha inteira; aperta o JS."""
    lines = [l.strip() for l in html.splitlines()]
    html = "\n".join(l for l in lines if l and not l.startswith("//")) + "\n"
    head, _, rest = html.partition("<script>")
    js, _, tail = rest.partition("</script>")
    return head.replace("\n", "") + "<script>" + squeeze(js) + "</script>" + tail


def header(gz):
    per = max(16, -(-len(gz) // MAX_LINES))
    rows = [", ".join(f"0x{b:02x}" for b in gz[i:i + per]) + "," for i in range(0, len(gz), per)]
    return (
        "// Gerado por scripts/page.py a partir de firmware/web/portal.html. Nao editar.\n"
        "// clang-format off\n"
        "#pragma once\n\n"
        "#include <Arduino.h>\n\n"
        "inline const uint8_t kPortalPageGz[] PROGMEM = {\n"
        + "\n".join(rows)
        + "\n};\n"
        f"inline constexpr size_t kPortalPageGzLen = {len(gz)};\n"
        "// clang-format on\n"
    )


def main():
    raw = minify(SRC.read_text(encoding="utf-8")).encode()
    gz = gzip.compress(raw, compresslevel=9, mtime=0)
    text = header(gz)
    print(f"portal: {len(raw)} B cru, {len(gz)} B gzip")
    if len(raw) > MAX_RAW or len(gz) > MAX_GZ:
        sys.exit(f"portal: passa do alvo ({MAX_RAW} cru, {MAX_GZ} gzip)")
    if "--check" in sys.argv:
        if not OUT.exists() or OUT.read_text() != text:
            sys.exit("portal: PortalPage.h desatualizado, rode python3 scripts/page.py")
        return
    OUT.write_text(text)


if __name__ == "__main__":
    main()
