#!/bin/sh
# Build and run the plugin's logic tests on a PC. Needs a C compiler (gcc or
# clang, set CC to pick one). Run from anywhere: ./test/run.sh
set -e

cd "$(dirname "$0")/.."

CC=${CC:-gcc}
OUT=${TMPDIR:-/tmp}/tft_plugin_tests
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) OUT="$OUT.exe" ;;
esac

$CC -std=gnu11 -O1 -g -Wall -Wextra -Werror \
    -o "$OUT" \
    test/test_main.c test/test_gcode.c test/test_logic.c \
    tft_gcode.c tft_logic.c \
    -lm

"$OUT"
