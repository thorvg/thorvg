#!/bin/bash

#https://github.com/thorvg/thorvg/wiki/ThorVG-Viewer-Development-Guide

if [ ! -d "./build_wasm" ]; then
    sed "s|EMSDK:|$1|g" ./cross/wasm_x86_i686.txt > /tmp/.wasm_cross.txt
    meson -Db_lto=true -Ddefault_library=static -Dstatic=true -Dloaders="all" -Dsavers="all" -Dthreads=false -Dbindings="wasm_beta" --cross-file /tmp/.wasm_cross.txt build_wasm
fi

ninja -C build_wasm/
ls -lrt build_wasm/src/bindings/wasm/thorvg-wasm.*
