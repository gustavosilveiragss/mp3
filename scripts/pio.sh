#!/usr/bin/env bash
# PlatformIO 6.2 em pasta propria: o pioarduino exige >= 6.2 e o pio do sistema e 6.1. Uso: scripts/pio.sh run -d firmware [-t upload]
export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-$HOME/.platformio-fast}"
exec "$PLATFORMIO_CORE_DIR/penv/bin/pio" "$@"
