#!/bin/bash

#https://github.com/thorvg/thorvg/wiki/WebGPU-Raster-Engine-Development

if [ ! -d "./build_wasm" ]; then
    sed "s|EMSDK:|$1|g" ./cross/wasm_webgpu.txt > /tmp/.wasm_cross.txt
    meson -Db_lto=true -Ddefault_library=static -Dstatic=true -Dloaders="all" -Dsavers="all" -Dthreads=false -Dbindings="wasm_beta" -Dengines="wg_beta" --cross-file /tmp/.wasm_cross.txt build_wasm
fi

ninja -C build_wasm/
ls -lrt build_wasm/src/bindings/wasm/thorvg-wasm.*
