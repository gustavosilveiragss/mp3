#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# Orcamento de memoria do chip: passar disso reprova (ajuste por projeto com as variaveis).
max_ram=${MAX_RAM_PCT:-60}
max_flash=${MAX_FLASH_PCT:-85}

dir=$(dirname "$(find . -name platformio.ini -not -path '*/.pio/*' | head -1)")
[ -f "$dir/platformio.ini" ] || { echo "platformio.ini nao encontrado"; exit 1; }
files=$(find "$dir/src" \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.ino' \))

clang-format --dry-run -Werror $files
python3 scripts/lint.py "$dir/src"
python3 scripts/page.py --check
log=$(scripts/pio.sh run -d "$dir" 2>&1) || { echo "$log" | grep -E 'error|warning|FAILED' | head -20 || true; echo "$log" | tail -5; exit 1; }
# Sem -Werror (headers do Arduino 3 avisam): aviso em codigo do projeto (src/) reprova.
if echo "$log" | grep -E '^src/[^ ]*: warning:' | head -10 | grep .; then echo "aviso do compilador em src/"; exit 1; fi
usage=$(echo "$log" | grep -E 'RAM:')
echo "$usage"
echo "$usage" | awk -v ram="$max_ram" '
    { match($0, /[0-9.]+%/); pct = substr($0, RSTART, RLENGTH - 1) + 0 }
    pct > ram { printf "orcamento: RAM %.1f%% passa de %d%%\n", pct, ram; bad = 1 }
    END { exit bad }'
python3 scripts/flash_budget.py "$dir" "$max_flash"
python3 scripts/layout.py
echo "check ok"
