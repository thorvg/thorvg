#!/bin/bash

#https://github.com/thorvg/thorvg/wiki/ThorVG-Viewer-Development-Guide

BACKEND="$1"
EMSDK="$2"

if [ -z "$2" ]; then
  BACKEND="all"
  EMSDK="$1"
fi

if [ ! -d "./build_wasm" ]; then
    if [[ "$BACKEND" == "wg" ]]; then
      sed "s|EMSDK:|$EMSDK|g" ./cross/wasm32_wg.txt > /tmp/.wasm_cross.txt
      meson setup -Db_lto=true -Ddefault_library=static -Dstatic=true -Dloaders="all" -Dsavers="all" -Dthreads=false -Dbindings="wasm_beta" -Dengines="wg" -Dfile=false -Dpartial=false --cross-file /tmp/.wasm_cross.txt build_wasm
    elif [[ "$BACKEND" == "sw" ]]; then
      sed "s|EMSDK:|$EMSDK|g" ./cross/wasm32_sw.txt > /tmp/.wasm_cross.txt
      meson setup -Db_lto=true -Ddefault_library=static -Dstatic=true -Dloaders="all" -Dsavers="all" -Dthreads=false -Dbindings="wasm_beta" -Dfile=false -Dpartial=false --cross-file /tmp/.wasm_cross.txt build_wasm
    elif [[ "$BACKEND" == "gl" ]]; then
      sed "s|EMSDK:|$EMSDK|g" ./cross/wasm32_gl.txt > /tmp/.wasm_cross.txt
      meson setup -Db_lto=true -Ddefault_library=static -Dstatic=true -Dloaders="all" -Dsavers="all" -Dthreads=false -Dbindings="wasm_beta" -Dengines="gl" -Dpartial=false -Dfile=false --cross-file /tmp/.wasm_cross.txt build_wasm
    else
      sed "s|EMSDK:|$EMSDK|g" ./cross/wasm32.txt > /tmp/.wasm_cross.txt
      meson setup -Db_lto=true -Ddefault_library=static -Dstatic=true -Dloaders="all" -Dsavers="all" -Dthreads=false -Dbindings="wasm_beta" -Dengines="all" -Dfile=false -Dpartial=false --cross-file /tmp/.wasm_cross.txt build_wasm
    fi
fi

ninja -C build_wasm/
wasm-opt build_wasm/src/bindings/wasm/thorvg-wasm.wasm -Oz --converge -all -o build_wasm/src/bindings/wasm/thorvg-wasm.wasm
ls -lrt build_wasm/src/bindings/wasm/*.{js,wasm}
