#!/usr/bin/env python3
"""Regras estaticas de memoria, bateria e legibilidade para firmware PlatformIO. Uso: lint.py <pasta src>

Silenciar uma linha: comentario `// lint: ok <motivo>` na mesma linha. O motivo e obrigatorio.
"""
import re
import sys
from pathlib import Path

MAX_FUNCTION_LINES = 60
MAX_NESTING = 4
MAX_PARAMS = 6
MAX_FILE_LINES = 500
MAX_DELAY_MS = 20

# (regra, regex, mensagem). Rodam sobre o codigo sem comentarios nem strings.
LINE_RULES = [
    ("memoria", r"\bString\b|std::(string|vector|map|unordered_map|unordered_set|set|list|deque|queue|stack|function|unique_ptr|shared_ptr|make_unique|make_shared)\b|using\s+namespace\s+std\b|\bnew\b|\bdelete\b|\b(malloc|calloc|realloc|strdup)\s*\(",
     "heap: use buffers fixos (char[N], arrays), sem String, std:: dinamico, new ou malloc"),
    ("memoria", r"\b(sprintf|vsprintf|strcpy|strcat|gets|strncpy|strncat|atoi|atof)\s*\(",
     "funcao sem limite: use snprintf, strlcpy, strlcat ou strtol"),
    ("bateria", r"\bwhile\s*\((?:[^()]|\([^()]*\))*\)\s*(;|\{\s*\})|\bfor\s*\(\s*;\s*;\s*\)\s*(;|\{\s*\})",
     "espera ocupada: durma, use evento ou timer"),
    ("pinos", r"\b(pinMode|digitalWrite|digitalRead|analogRead|attachInterrupt|ledcAttach)\s*\(\s*(\d|LED_BUILTIN|[AD]\d)|\bWire\.begin\s*\(\s*\d",
     "pino literal: declare em pins.h"),
]
WAIVER = re.compile(r"//\s*lint:\s*ok\s+\S")

problems = []


def report(path, line, rule, message):
    problems.append(f"{path}:{line}: [{rule}] {message}")


def strip(text):
    """Tira comentarios e conteudo de strings, mantendo as quebras de linha."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        elif c == "R" and text.startswith('R"', i):
            m = re.match(r'R"([^(\s]*)\(', text[i:])
            end = text.find(f"){m.group(1)}\"", i) if m else -1
            if end < 0:
                out.append(c)
                i += 1
            else:
                end += len(m.group(1)) + 2
                out.append('""' + "\n" * text.count("\n", i, end))
                i = end
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(c + c)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def check_delay(path, lineno, code):
    for m in re.finditer(r"(?<![\w:.])delay\s*\(\s*([^)]*)\)", code):
        if re.search(r"\b(void|int|unsigned|uint\d+_t)\s*$", code[:m.start()]):
            continue  # declaracao de delay, nao chamada
        arg = m.group(1).strip()
        literal = re.fullmatch(r"(\d+)[uUlL]*", arg)
        if not literal or int(literal.group(1)) > MAX_DELAY_MS:
            report(path, lineno, "bateria", f"delay({arg}) bloqueia o loop e segura a CPU: use millis ou light sleep (maximo {MAX_DELAY_MS} ms)")


def check_functions(path, code_lines):
    """Acha funcoes por chaves: tamanho, aninhamento e numero de parametros."""
    stack = []  # 'c' contêiner, 'f' funcao, 'b' bloco, 'o' outro
    header = ""
    fn_start = fn_name = None
    nesting = max_nesting = 0
    for no, line in enumerate(code_lines, 1):
        for ch in line:
            if ch == "{":
                h = header.strip()
                inside_fn = "f" in stack
                if inside_fn:
                    stack.append("b")
                    nesting += 1
                    max_nesting = max(max_nesting, nesting)
                elif re.search(r"\b(namespace|class|struct|enum|union)\b", h) and ")" not in h:
                    stack.append("c")
                elif re.search(r"\)\s*(const|noexcept|override|final|\s)*(->\s*[\w:<>&*\s]+)?$", h) or re.search(r"\)\s*:\s*[\w:,\s(){}.\-+*/<>&|!=\"']*$", h):
                    stack.append("f")
                    fn_start, nesting, max_nesting = no, 0, 0
                    fn_name = re.search(r"([~\w:]+)\s*\(", h)
                    fn_name = fn_name.group(1) if fn_name else "?"
                    params = h[h.find("(") + 1:h.rfind(")")] if "(" in h else ""
                    depth, count = 0, (1 if params.strip() else 0)
                    for p in params:
                        depth += p in "(<[{"
                        depth -= p in ")>]}"
                        count += p == "," and depth == 0
                    if count > MAX_PARAMS:
                        report(path, no, "legibilidade", f"{fn_name} tem {count} parametros (maximo {MAX_PARAMS}): agrupe em uma struct")
                else:
                    stack.append("o")
                header = ""
            elif ch == "}":
                kind = stack.pop() if stack else "o"
                if kind == "b":
                    nesting -= 1
                elif kind == "f":
                    length = no - fn_start + 1
                    if length > MAX_FUNCTION_LINES:
                        report(path, fn_start, "legibilidade", f"{fn_name} tem {length} linhas (maximo {MAX_FUNCTION_LINES}): divida")
                    if max_nesting > MAX_NESTING:
                        report(path, fn_start, "legibilidade", f"{fn_name} aninha {max_nesting} niveis (maximo {MAX_NESTING}): use retorno antecipado ou extraia")
                header = ""
            elif ch == ";":
                header = ""
            else:
                header += ch
        header += "\n"


def lint(path):
    raw = path.read_text(encoding="utf-8", errors="replace")
    raw_lines = raw.splitlines()
    if len(raw_lines) > MAX_FILE_LINES:
        report(path, 1, "legibilidade", f"{len(raw_lines)} linhas (maximo {MAX_FILE_LINES}): separe responsabilidades")
    code_lines = strip(raw).splitlines()
    waived = {i for i, l in enumerate(raw_lines, 1) if WAIVER.search(l)}
    before = len(problems)
    for no, code in enumerate(code_lines, 1):
        if code.lstrip().startswith("#"):
            continue  # diretivas (#include <new>) nao sao codigo
        code = re.sub(r"=\s*(delete|default)\b", "", code)  # "= delete" nao e heap
        for rule, regex, message in LINE_RULES:
            for m in re.finditer(regex, code):
                if rule == "bateria" and m.group(0).startswith("while") and code[:m.start()].rstrip().endswith("}"):
                    continue  # "} while (x);" fecha um do-while, nao e espera ocupada
                report(path, no, rule, message)
                break
        check_delay(path, no, code)
    check_functions(path, code_lines)
    problems[before:] = [p for p in problems[before:] if int(p.split(":")[1]) not in waived]


root = Path(sys.argv[1] if len(sys.argv) > 1 else "src")
for f in sorted(root.rglob("*")):
    if f.suffix in (".cpp", ".h", ".hpp", ".c", ".ino"):
        lint(f)

if problems:
    print("lint: " + str(len(problems)) + " problema(s)")
    print("\n".join("  " + p for p in problems))
    sys.exit(1)
print("lint ok")
