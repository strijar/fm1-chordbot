#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
PY=${PYTHON:-python3}
mkdir -p build/gen build/host
"$PY" tools/gen_catalog.py
"$PY" tools/gen_font.py build/gen/felucca_font.h >/dev/null
CC=${CC:-gcc}
EAS_SRC=$(find firmware/src/dsp/eas -maxdepth 1 -name '*.c' ! -name 'wt_22khz.c' ! -name 'wt_200k_samples.c')
OUT=build/host/chordbot_host
SAN_FLAGS=
if [ "${SANITIZE:-0}" = 1 ]; then
    # EAS deliberately uses signed shifts in its fixed-point arithmetic. Check its
    # memory accesses; apply both ASan and UBSan to the new app and sequencer.
    mkdir -p build/host/eas-asan
    for src in $EAS_SRC; do
        "$CC" -O1 -g -DGM_HOST -fsanitize=address -Ifirmware/src/dsp/eas -c "$src" -o "build/host/eas-asan/$(basename "$src" .c).o"
    done
    EAS_SRC='build/host/eas-asan/*.o'
    SAN_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
    OUT=build/host/chordbot_host_asan
fi
"$CC" -O2 -g -std=gnu11 -ffp-contract=off -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
    $SAN_FLAGS \
    -DGM_HOST -Ifirmware/src -Ifirmware/src/dsp -Ifirmware/src/dsp/eas -Ibuild/gen \
    host/chordbot_host.c firmware/src/dsp/arr.c firmware/src/dsp/catalog.c firmware/src/dsp/gm.c \
    $EAS_SRC -lX11 -ldl -lm -o "$OUT"
echo "host: $OUT"
