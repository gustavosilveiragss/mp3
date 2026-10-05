#!/usr/bin/env python3
"""Regras estaticas de memoria, bateria e legibilidade para firmware PlatformIO. Uso: lint.py <pasta src>

Silenciar uma linha: comentario `// lint: ok <motivo>` na mesma linha. O motivo e obrigatorio.

Regras duras reprovam sempre. Regras de ratchet (nome curto, condicao longa, muitos returns) comparam a
contagem por arquivo com `lint-baseline.txt` (`arquivo|regra|contagem`): subir reprova, descer e livre.
`--update` reescreve o baseline na criacao ou quando nada subiu. Numero magico e so aviso.
Respiro visual (ratchet, so C/C++): `bloco-longo` (mais de 8 linhas seguidas sem linha em branco numa funcao),
`sem-respiro-apos-bloco` (statement colado no `}` de um if/for/while/switch) e `sem-respiro-antes-de-bloco`
(if/for/while/switch colado em guarda, `return` ou chamada solta anterior).
"""
import re
import sys
from collections import Counter
from pathlib import Path

MAX_FUNCTION_LINES = 60
MAX_NESTING = 4
MAX_PARAMS = 6
MAX_FILE_LINES = 500
MAX_DELAY_MS = 20
MAX_COLUMNS = 120
MAX_CONDITION_TERMS = 3
MAX_RETURNS = 6
MAX_BLOCK_LINES = 8
SHORT_NAME_WHITELIST = {"i", "j", "k", "fd", "id", "ms", "us", "ip", "rx", "tx", "cb", "fn", "ch", "x", "y"}
BASELINE_NAME = "lint-baseline.txt"

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
GENERATED = re.compile(r"gerado por|generated", re.IGNORECASE)
LINE_COMMENT = re.compile(r"""^(?:[^"'/]|"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'|/(?!/))*//(.*)$""")
PATCH_COMMENT = re.compile(r"\b(antes|agora)\s*:|\b(corrige|corrigido|fix|hack|workaround|TODO|FIXME|gambiarra)\b")
BLOCK_COMMENT_LINE = re.compile(r"^\s*(/\*|\*(\s|/|$))")
COMMENTED_CODE = re.compile(r"^\s*(?:(?:return|break|continue)\b.*;|[\w:.\->\[\]]+\s*(?:\(.*\)|[-+*/|&]?=.*);)\s*$")
DISABLED_BLOCK = re.compile(r"^\s*#\s*if\s+0\b")
SHORT_NAME = re.compile(r"\b(?:u?int\d*_t|int|long|short|auto|float|double|size_t|char|bool|byte)[\s*&]+(\w{1,2})\s*[=;,\[)]")
CONDITION_START = re.compile(r"\b(?:if|while)\s*\(")
MAGIC_NUMBER = re.compile(r"[<>=!+\-*/%(,]\s*(\d{2,}|[2-9])\b(?![.\w])")
CONTROL_START = re.compile(r"^\s*(?:\}\s*)?(if|else|for|while|switch|do)\b")
CONTROL_OPEN = re.compile(r"^\s*(if|for|while|switch)\s*\(")
BLOCK_CLOSE = re.compile(r"^\s*\}\s*(while\b.*;)?\s*$")
NOT_STATEMENT = re.compile(r"^\s*(else\b|\}|\)|;|case\b|default\b|#)")
JUMP = re.compile(r"^\s*(return|continue|break|goto)\b.*;\s*$")
LOOSE_CALL = re.compile(r"^\s*[\w:.\->\[\]]+\s*\(.*\)\s*;\s*$")
DATA_OPEN = re.compile(r"=\s*\{\s*$|\b(constexpr|enum|struct)\b.*\{\s*$")
CASE_LINE = re.compile(r"^\s*(case\b.*|default\s*):\s*(return\b.*;|break;)?\s*$")
CASE_ONLY = re.compile(r"^\s*(case\b.*|default\s*):\s*$")
COMMENT_QUOTED = re.compile(r"\"[^\"]*\"|'[^']*'|`[^`]*`")
COMMENT_PUNCTUATION = re.compile(r";|\u2014|\u2013|\S\s+-\s+\S")
RATCHET_HINTS = {
    "bloco-longo": f"mais de {MAX_BLOCK_LINES} linhas seguidas sem linha em branco: separe em blocos",
    "sem-respiro-apos-bloco": "sem linha em branco entre blocos: separe o que vem depois do } fechado",
    "sem-respiro-antes-de-bloco": "sem linha em branco entre blocos: separe o if/for/while do statement anterior",
}
MAGIC_SKIP = re.compile(r"\b(constexpr|enum|const|case|static)\b|\[[^\]]*\]\s*(=|;)")

problems = []
ratchet_hits = []  # (arquivo relativo, linha, regra)
warnings = []
root_dir = Path(".")


def report(path, line, rule, message):
    problems.append(f"{path}:{line}: [{rule}] {message}")


def flag(path, line, rule):
    ratchet_hits.append((path.relative_to(root_dir).as_posix(), line, rule))


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


def check_returns(path, start, body_lines):
    if len(re.findall(r"\breturn\b", "\n".join(body_lines))) > MAX_RETURNS:
        flag(path, start, "returns")


def check_conditions(path, code_lines):
    text = "\n".join(code_lines)
    for m in CONDITION_START.finditer(text):
        depth, end = 1, m.end()
        while end < len(text) and depth:
            depth += (text[end] == "(") - (text[end] == ")")
            end += 1
        if len(re.findall(r"&&|\|\|", text[m.end():end])) + 1 > MAX_CONDITION_TERMS:
            flag(path, text.count("\n", 0, m.start()) + 1, "condicao")


def comment_text(raw_line):
    if BLOCK_COMMENT_LINE.match(raw_line):
        return raw_line.lstrip(" /*")
    found = LINE_COMMENT.match(raw_line)
    return found.group(1) if found else None


def check_text_rules(path, raw_lines):
    generated = any(GENERATED.search(l) for l in raw_lines[:5])
    for no, line in enumerate(raw_lines, 1):
        if len(line) > MAX_COLUMNS and not generated:
            report(path, no, "legibilidade", f"linha com {len(line)} colunas (maximo {MAX_COLUMNS})")
        if DISABLED_BLOCK.match(line):
            report(path, no, "legibilidade", "#if 0: apague o codigo, o passado mora no git")
        comment = comment_text(line)
        if comment is None:
            continue
        if PATCH_COMMENT.search(comment):
            report(path, no, "legibilidade", "comentario de remendo ou historico: explique so o porque")
        if COMMENT_PUNCTUATION.search(COMMENT_QUOTED.sub("", comment)):
            report(path, no, "legibilidade", "comentario com ponto e virgula ou travessao: use ponto ou virgula")
        if COMMENTED_CODE.match(comment):
            report(path, no, "legibilidade", "codigo comentado: apague")


def check_line_style(path, no, code):
    for m in SHORT_NAME.finditer(code):
        if m.group(1) not in SHORT_NAME_WHITELIST:
            flag(path, no, "nome-curto")
    if MAGIC_NUMBER.search(code) and not MAGIC_SKIP.search(code):
        warnings.append(f"{path}:{no}: numero magico: de nome a constante")


def check_functions(path, code_lines):
    """Acha funcoes por chaves: tamanho, aninhamento e numero de parametros."""
    stack = []  # 'c' contêiner, 'f' funcao, 'b' bloco, 'o' outro
    functions = []  # (linha do "{", linha do "}") de cada funcao
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
                    functions.append((fn_start, no))
                    length = no - fn_start + 1
                    if length > MAX_FUNCTION_LINES:
                        report(path, fn_start, "legibilidade", f"{fn_name} tem {length} linhas (maximo {MAX_FUNCTION_LINES}): divida")
                    check_returns(path, fn_start, code_lines[fn_start - 1:no])
                    if max_nesting > MAX_NESTING:
                        report(path, fn_start, "legibilidade", f"{fn_name} aninha {max_nesting} niveis (maximo {MAX_NESTING}): use retorno antecipado ou extraia")
                header = ""
            elif ch == ";":
                header = ""
            else:
                header += ch
        header += "\n"
    return functions


def indent_of(line):
    return len(line) - len(line.lstrip())


def opener_of(code_lines, index):
    """Linha que abre o bloco fechado em `index`: a anterior mais a esquerda ou igual, sem case/default/#."""
    indent = indent_of(code_lines[index])
    for j in range(index - 1, -1, -1):
        line = code_lines[j]
        if line.strip() and indent_of(line) <= indent and not re.match(r"\s*(#|case\b|default\b)", line):
            return line
    return ""


def is_comment_only(raw_lines, code_lines, index):
    return bool(raw_lines[index].strip()) and not code_lines[index].strip()


def check_after_block(path, raw_lines, code_lines, body):
    for i in body:
        if not BLOCK_CLOSE.match(code_lines[i]) or not CONTROL_START.match(opener_of(code_lines, i)):
            continue
        k = i + 1
        while k < len(raw_lines) and is_comment_only(raw_lines, code_lines, k):
            k += 1
        touching = k == i + 1 or raw_lines[i + 1].strip()
        if touching and k < len(raw_lines) and code_lines[k].strip() and not NOT_STATEMENT.match(code_lines[k]):
            flag(path, i + 1, "sem-respiro-apos-bloco")


def check_before_block(path, raw_lines, code_lines, body):
    for i in body:
        if not CONTROL_OPEN.match(code_lines[i]) or not raw_lines[i - 1].strip():
            continue
        previous = code_lines[i - 1]
        opens_paragraph = i < 2 or not raw_lines[i - 2].strip() or code_lines[i - 2].rstrip().endswith("{")
        if JUMP.match(previous) or (LOOSE_CALL.match(previous) and not opens_paragraph):
            flag(path, i + 1, "sem-respiro-antes-de-bloco")


def data_line_indexes(code_lines, body):
    """Linhas de tabelas (inicializadores, enum, case curto) que nao contam como bloco de codigo."""
    data, table_indent, after_case = set(), None, False
    for i in body:
        line = code_lines[i]
        if table_indent is not None:
            data.add(i)
            if line.strip().startswith("}") and indent_of(line) <= table_indent:
                table_indent = None
        elif DATA_OPEN.search(line):
            data.add(i)
            table_indent = indent_of(line)
        elif CASE_LINE.match(line) or (after_case and JUMP.match(line)):
            data.add(i)
        after_case = bool(CASE_ONLY.match(line))
    return data


def check_long_runs(path, raw_lines, code_lines, body):
    data = data_line_indexes(code_lines, body)
    run = start = 0
    for i in body:
        if not raw_lines[i].strip():
            run = 0
        elif i not in data:
            start = start if run else i + 1
            run += 1
            if run == MAX_BLOCK_LINES + 1:
                flag(path, start, "bloco-longo")


def check_spacing(path, raw_lines, code_lines, functions):
    if any(GENERATED.search(l) for l in raw_lines[:5]):
        return
    for first, last in functions:
        body = range(first, last - 1)  # indices 0-based das linhas entre o "{" e o "}" da funcao
        check_after_block(path, raw_lines, code_lines, body)
        check_before_block(path, raw_lines, code_lines, body)
        check_long_runs(path, raw_lines, code_lines, body)


def lint(path):
    raw = path.read_text(encoding="utf-8", errors="replace")
    raw_lines = raw.splitlines()
    if len(raw_lines) > MAX_FILE_LINES:
        report(path, 1, "legibilidade", f"{len(raw_lines)} linhas (maximo {MAX_FILE_LINES}): separe responsabilidades")
    code_lines = strip(raw).splitlines()
    waived = {i for i, l in enumerate(raw_lines, 1) if WAIVER.search(l)}
    before, before_hits, before_warnings = len(problems), len(ratchet_hits), len(warnings)
    check_text_rules(path, raw_lines)
    check_conditions(path, code_lines)
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
        check_line_style(path, no, code)
    check_spacing(path, raw_lines, code_lines, check_functions(path, code_lines))
    problems[before:] = [p for p in problems[before:] if int(p.split(":")[1]) not in waived]
    ratchet_hits[before_hits:] = [h for h in ratchet_hits[before_hits:] if h[1] not in waived]
    warnings[before_warnings:] = [w for w in warnings[before_warnings:] if int(w.split(":")[1]) not in waived]


def load_baseline(baseline):
    counts = {}
    if baseline.exists():
        for line in baseline.read_text().splitlines():
            name, rule, count = line.rsplit("|", 2)
            counts[(name, rule)] = int(count)
    return counts


def find_regressions(current, previous):
    return sorted(key for key, count in current.items() if count > previous.get(key, 0))


def write_baseline(baseline, current):
    lines = [f"{name}|{rule}|{count}" for (name, rule), count in sorted(current.items())]
    baseline.write_text("\n".join(lines) + ("\n" if lines else ""))
    print(f"lint: baseline gravado em {baseline}")


def check_ratchet(baseline, update):
    current = Counter((name, rule) for name, _, rule in ratchet_hits)
    if update and not baseline.exists():
        return write_baseline(baseline, current)
    previous = load_baseline(baseline)
    regressions = find_regressions(current, previous)
    for name, rule in regressions:
        lines = [str(l) for n, l, r in ratchet_hits if (n, r) == (name, rule)]
        report(name, ",".join(lines[:8]), rule,
               f"{current[(name, rule)]} ocorrencias, baseline {previous.get((name, rule), 0)}: nao pode subir"
               + (f" ({RATCHET_HINTS[rule]})" if rule in RATCHET_HINTS else ""))
    if update and not regressions:
        write_baseline(baseline, current)


def main():
    global root_dir
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    root_dir = Path(args[0] if args else "src")
    option = next((a.split("=", 1)[1] for a in sys.argv if a.startswith("--baseline=")), "")
    baseline = Path(option) if option else Path(__file__).with_name(BASELINE_NAME)
    for f in sorted(root_dir.rglob("*")):
        if f.suffix in (".cpp", ".h", ".hpp", ".c", ".ino"):
            lint(f)
    check_ratchet(baseline, "--update" in sys.argv)
    if warnings:
        print(f"lint: {len(warnings)} aviso(s) (nao reprovam)")
        print("\n".join("  " + w for w in warnings[:10]))
    if problems:
        print("lint: " + str(len(problems)) + " problema(s)")
        print("\n".join("  " + p for p in problems))
        sys.exit(1)
    print("lint ok")


main()
