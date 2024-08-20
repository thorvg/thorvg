#!/bin/bash

#https://github.com/thorvg/thorvg/wiki/ThorVG-Viewer-Development-Guide

BACKEND="$1"
EMSDK="$2"

if [ -z "$2" ]; then
  BACKEND="all"
  EMSDK="$1"
fi

if [ ! -d "./build_wasm32_$BACKEND" ]; then
    if [[ "$BACKEND" == "wg" ]]; then
      sed "s|EMSDK:|$EMSDK|g" ./cross/wasm32_wg.txt > /tmp/.wasm_cross.txt
      meson -Db_lto=true -Ddefault_library=static -Dstatic=true -Dloaders="all" -Dsavers="all" -Dthreads=false -Dbindings="wasm_beta" -Dengines="wg_beta" --cross-file /tmp/.wasm_cross.txt build_wasm32_wg
    elif [[ "$BACKEND" == "sw" ]]; then
      sed "s|EMSDK:|$EMSDK|g" ./cross/wasm32_sw.txt > /tmp/.wasm_cross.txt
      meson -Db_lto=true -Ddefault_library=static -Dstatic=true -Dloaders="all" -Dsavers="all" -Dthreads=false -Dbindings="wasm_beta" --cross-file /tmp/.wasm_cross.txt build_wasm32_sw
    else
      sed "s|EMSDK:|$EMSDK|g" ./cross/wasm32_wg.txt > /tmp/.wasm_cross.txt
      meson -Db_lto=true -Ddefault_library=static -Dstatic=true -Dloaders="all" -Dsavers="all" -Dthreads=false -Dbindings="wasm_beta" -Dengines="wg_beta, sw" --cross-file /tmp/.wasm_cross.txt build_wasm32_all
    fi
fi

ninja -C build_wasm32_$BACKEND/
ls -lrt build_wasm32_$BACKEND/src/bindings/wasm/*.{js,wasm}
