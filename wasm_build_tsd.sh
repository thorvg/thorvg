#!/bin/bash

#https://github.com/thorvg/thorvg/wiki/ThorVG-Viewer-Development-Guide

EMSDK="$1"
TMP_DIR="/tmp/.build_wasm_tsd"
OUT_DIR="build_wasm/src/bindings/wasm"

if [ ! -d "$OUT_DIR" ]; then
    echo "[ERROR] $OUT_DIR does not exist. Please run wasm_build.sh first."
    exit 1
fi

rm -rf "$TMP_DIR"

sed "s|EMSDK:|$EMSDK|g; s|'--bind'|'--bind', '--emit-tsd=thorvg-wasm.d.ts'|g" ./cross/wasm32.txt > /tmp/.wasm_cross.txt
meson setup -Db_lto=true -Ddefault_library=static -Dstatic=true -Dloaders="all" -Dsavers="all" -Dthreads=false -Dbindings="wasm_beta" -Dpartial=false -Dengines="all" --cross-file /tmp/.wasm_cross.txt "$TMP_DIR"

ninja -C "$TMP_DIR/"
mv "$TMP_DIR/src/bindings/wasm/thorvg-wasm.d.ts" "$OUT_DIR"

rm -rf "$TMP_DIR"

ls -lrt "$OUT_DIR"/*.d.ts
