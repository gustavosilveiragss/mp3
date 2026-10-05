#!/usr/bin/env bash
# Compila e roda cada test_*.cpp no host, so com g++ e assert.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
fmk="${FMK:-$(python3 "$root/scripts/render/render.py" --print-fmk)}"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT

for source in "$here"/test_*.cpp; do
    name="$(basename "$source" .cpp)"
    g++ -std=gnu++17 -Wall -Wextra -Werror -I"$root/firmware/src" -I"$fmk" -I"$here/stub" "$source" -o "$build/$name"
    "$build/$name"
done
