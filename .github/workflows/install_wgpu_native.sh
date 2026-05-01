#!/bin/bash

set -euo pipefail

version="${WGPU_NATIVE_VERSION:-v27.0.4.0}"
pc_url="${WGPU_NATIVE_PC_URL:-https://github.com/user-attachments/files/20096376/wgpu_native.pc.zip}"
prefix="${WGPU_NATIVE_PREFIX:-/usr/local}"

case "$(uname -m)" in
    x86_64|amd64)
        arch="x86_64"
        ;;
    aarch64|arm64)
        arch="aarch64"
        ;;
    *)
        echo "Unsupported wgpu-native Linux architecture: $(uname -m)" >&2
        exit 1
        ;;
esac

archive="wgpu-linux-${arch}-release.zip"
url="https://github.com/gfx-rs/wgpu-native/releases/download/${version}/${archive}"
tmpdir="$(mktemp -d)"

cleanup() {
    rm -rf "${tmpdir}"
}
trap cleanup EXIT

curl -fL "${url}" -o "${tmpdir}/${archive}"
unzip -q "${tmpdir}/${archive}" -d "${tmpdir}/wgpu-native"
curl -fL "${pc_url}" -o "${tmpdir}/wgpu_native.pc.zip"
unzip -q "${tmpdir}/wgpu_native.pc.zip" -d "${tmpdir}/pkgconfig"

sudo mkdir -p "${prefix}/lib" "${prefix}/include/webgpu" "${prefix}/lib/pkgconfig"
sudo cp "${tmpdir}/wgpu-native/lib/libwgpu_native.so" "${prefix}/lib/"
sudo cp "${tmpdir}/wgpu-native/include/webgpu/webgpu.h" "${prefix}/include/webgpu/"
sudo cp "${tmpdir}/wgpu-native/include/webgpu/wgpu.h" "${prefix}/include/webgpu/"
sudo cp "$(find "${tmpdir}/pkgconfig" -type f -name 'wgpu_native.pc' -print -quit)" "${prefix}/lib/pkgconfig/"

sudo ldconfig

if [[ -n "${GITHUB_ENV:-}" ]]; then
    echo "PKG_CONFIG_PATH=${prefix}/lib/pkgconfig:${PKG_CONFIG_PATH:-}" >> "${GITHUB_ENV}"
    echo "LD_LIBRARY_PATH=${prefix}/lib:${LD_LIBRARY_PATH:-}" >> "${GITHUB_ENV}"
fi

PKG_CONFIG_PATH="${prefix}/lib/pkgconfig:${PKG_CONFIG_PATH:-}" pkg-config --modversion wgpu_native
