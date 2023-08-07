#!/bin/bash

#https://github.com/thorvg/thorvg/wiki/ThorVG-Viewer-Development-Guide

if [ ! -d "./build_wasm" ]; then
    sed "s|EMSDK:|$1|g" wasm_cross.txt > /tmp/.wasm_cross.txt
    meson -Db_lto=true -Ddefault_library=static -Dstatic=true -Dloaders="all, lottie" -Dsavers="all" -Dbindings="wasm_beta" -Dlog=true --cross-file /tmp/.wasm_cross.txt build_wasm
fi

ninja -C build_wasm/
ls -lrt build_wasm/src/bindings/wasm/thorvg-wasm.*
