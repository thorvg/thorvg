#!/bin/bash

if [ ! -d "./build_wasm" ]; then
    sed "s|EMSDK:|$1|g" wasm_cross.txt > /tmp/.wasm_cross.txt
    meson -Db_lto=true -Ddefault_library=static -Dstatic=true -Dloaders="all, lottie_beta" -Dsavers="all" -Dlog=true --cross-file /tmp/.wasm_cross.txt build_wasm
    cp ./test/wasm/wasm_test.html build_wasm/src/index.html
fi

ninja -C build_wasm/
echo "RESULT:"
echo " thorvg-wasm.wasm and thorvg-wasm.js can be found in build_wasm/src folder"
ls -lrt build_wasm/src/thorvg-wasm.*
