#!/bin/bash

#https://github.com/thorvg/thorvg/wiki/ThorVG-Viewer-Development-Guide

BACKEND="$1"
EMSDK="$2"

if [ -z "$2" ]; then
  BACKEND="all"
  EMSDK="$1"
fi

if [ ! -d "./build_wasm" ]; then
      sed "s|EMSDK:|$EMSDK|g" ./cross/wasm32_sw.txt > /tmp/.wasm_cross.txt
      meson setup -Db_lto=true -Ddefault_library=static -Dstatic=true -Dloaders="jpg, png" -Dthreads=false -Dfile=false -Dbindings="wasm_beta" --cross-file /tmp/.wasm_cross.txt build_wasm
fi

ninja -C build_wasm/
ls -lrt build_wasm/src/bindings/wasm/*.{js,wasm}
