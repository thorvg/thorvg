#!/bin/bash

if [ -z "$1" ]; then
    echo "Emscripten SDK PATH is not provided"
    echo "Usage: wasm_build EMSDK_PATH"
    exit 1;
fi

if [ ! -d "./builddir_wasm" ]; then
    sed "s|EMSDK:|$1|g" wasm_cross.txt > /tmp/.wasm_cross.txt
    meson -Db_lto=true -Ddefault_library=static -Dstatic=true -Dloaders="all, lottie_beta" -Dsavers="all" -Dlog=true --cross-file /tmp/.wasm_cross.txt builddir_wasm
    cp ./test/wasm/wasm_test.html builddir_wasm/src/index.html
fi

ninja -C builddir_wasm/
echo "RESULT:"
echo " thorvg-wasm.wasm and thorvg-wasm.js can be found in builddir_wasm/src folder"
ls -lrt builddir_wasm/src/thorvg-wasm.*
