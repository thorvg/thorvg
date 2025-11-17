#!/bin/bash

# ThorVG Canvas Kit WASM Build Script
# Usage: ./wasm_canvaskit_build.sh [backend] [emsdk_path]
# backend: sw, gl, wg, or all (default: all)
# emsdk_path: path to emsdk directory

BACKEND="$1"
EMSDK="$2"

if [ -z "$2" ]; then
  BACKEND="all"
  EMSDK="$1"
fi

if [ -z "$EMSDK" ]; then
  echo "Usage: $0 [backend] <emsdk_path>"
  echo "  backend: sw, gl, wg, or all (default: all)"
  echo "  emsdk_path: path to emsdk directory"
  exit 1
fi

echo "========================================"
echo "ThorVG Canvas Kit WASM Build"
echo "Backend: $BACKEND"
echo "EMSDK: $EMSDK"
echo "========================================"

BUILD_DIR="build_wasm_canvaskit"

# Clean previous build if exists
if [ -d "./$BUILD_DIR" ]; then
    echo "Cleaning previous build directory..."
    rm -rf "./$BUILD_DIR"
fi

# Select cross-compilation file based on backend
if [[ "$BACKEND" == "wg" ]]; then
    echo "Building with WebGPU backend..."
    CROSS_FILE="wasm32_canvaskit.txt"
    ENGINES="wg"
elif [[ "$BACKEND" == "gl" ]]; then
    echo "Building with WebGL backend..."
    CROSS_FILE="wasm32_canvaskit_sw.txt"  # GL uses same as SW (no webgpu dependency)
    ENGINES="gl"
elif [[ "$BACKEND" == "sw" ]]; then
    echo "Building with Software backend..."
    CROSS_FILE="wasm32_canvaskit_sw.txt"
    ENGINES="sw"
else
    echo "Building with all backends (SW + GL + WebGPU)..."
    CROSS_FILE="wasm32_canvaskit.txt"
    ENGINES="all"
fi

# Substitute EMSDK path in cross-compilation file
sed "s|EMSDK:|$EMSDK|g" ./cross/$CROSS_FILE > /tmp/.wasm_canvaskit_cross.txt

echo "Setting up meson build..."
meson setup \
    -Db_lto=true \
    -Ddefault_library=static \
    -Dstatic=true \
    -Dloaders="all" \
    -Dsavers="all" \
    -Dthreads=false \
    -Dbindings="capi,wasm_canvaskit" \
    -Dpartial=false \
    -Dengines="$ENGINES" \
    --cross-file /tmp/.wasm_canvaskit_cross.txt \
    $BUILD_DIR

if [ $? -ne 0 ]; then
    echo "Error: Meson setup failed"
    exit 1
fi

echo "Building with ninja..."
ninja -C $BUILD_DIR/

if [ $? -ne 0 ]; then
    echo "Error: Ninja build failed"
    exit 1
fi

echo "========================================"
echo "Build complete!"
echo "Output files:"
ls -lh $BUILD_DIR/src/bindings/wasm/*.{js,wasm} 2>/dev/null || echo "No output files found"
echo "========================================"
