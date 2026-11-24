#!/bin/sh
# Build the PC test harness: the Android front end plus the Neo Geo CD rules.
set -e
cd "$(dirname "$0")/../.."
B=${1:-build-pc}
mkdir -p "$B"
NG=neogeo-cd/src
gcc -std=gnu99 -D_GNU_SOURCE -Wall -Wextra -Werror -O2 -DCALM_TEST -Iandroid/core -I$NG \
    android/core/calm.c android/core/gfx.c android/core/scenery.c \
    $NG/board.c $NG/puzzle.c android/test/pc.c -o "$B/calm-pc"
echo "built $B/calm-pc"
