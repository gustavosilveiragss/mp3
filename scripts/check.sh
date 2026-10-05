#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# Orcamento de memoria do chip: passar disso reprova (ajuste por projeto com as variaveis).
max_ram=${MAX_RAM_PCT:-60}
max_flash=${MAX_FLASH_PCT:-85}

update_size=${UPDATE_SIZE:-0}
[ "${1:-}" = "--update-size" ] && update_size=1

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
python3 scripts/render/render.py --check
scripts/hosttest/run.sh
echo "$usage"
echo "$usage" | awk -v ram="$max_ram" '
    { match($0, /[0-9.]+%/); pct = substr($0, RSTART, RLENGTH - 1) + 0 }
    pct > ram { printf "orcamento: RAM %.1f%% passa de %d%%\n", pct, ram; bad = 1 }
    END { exit bad }'
python3 scripts/flash_budget.py "$dir" "$max_flash"
python3 scripts/layout.py

# Baseline exato em bytes: qualquer aumento reprova; reducao pede --update-size para travar o ganho.
ram_bytes=$(echo "$usage" | sed -E 's/.*used ([0-9]+) bytes.*/\1/')
flash_bytes=$(stat -c %s "$(ls -t "$dir"/.pio/build/*/firmware.bin | head -1)")
baseline=scripts/size-baseline.txt
if [ "$update_size" = 1 ] || [ ! -f "$baseline" ]; then
    printf 'ram_bytes=%s\nflash_bytes=%s\n' "$ram_bytes" "$flash_bytes" >"$baseline"
    echo "baseline de tamanho gravado: RAM $ram_bytes B, Flash $flash_bytes B"
else
    base_ram=$(sed -n 's/^ram_bytes=//p' "$baseline")
    base_flash=$(sed -n 's/^flash_bytes=//p' "$baseline")
    [ "$ram_bytes" -le "$base_ram" ] || { echo "RAM subiu: $ram_bytes B contra baseline $base_ram B (+$((ram_bytes - base_ram)))"; exit 1; }
    [ "$flash_bytes" -le "$base_flash" ] || { echo "Flash subiu: $flash_bytes B contra baseline $base_flash B (+$((flash_bytes - base_flash)))"; exit 1; }
    echo "tamanho: RAM $ram_bytes/$base_ram B, Flash $flash_bytes/$base_flash B"
fi
echo "check ok"
