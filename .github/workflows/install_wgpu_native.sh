#!/bin/bash

set -euo pipefail

version="${WGPU_NATIVE_VERSION:-v29.0.1.1}"
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

curl -fL --retry 5 --retry-delay 5 --retry-all-errors "${url}" -o "${tmpdir}/${archive}"
unzip -q "${tmpdir}/${archive}" -d "${tmpdir}/wgpu-native"

cat > "${tmpdir}/wgpu_native.pc" <<EOF
prefix=${prefix}
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: wgpu-native
Description: wgpu-native
Version: 0
Libs: -L\${libdir} -lwgpu_native
Cflags: -I\${includedir}
EOF

sudo mkdir -p "${prefix}/lib" "${prefix}/include/webgpu" "${prefix}/lib/pkgconfig"
sudo cp "${tmpdir}/wgpu-native/lib/libwgpu_native.so" "${prefix}/lib/"
sudo cp "${tmpdir}/wgpu-native/include/webgpu/webgpu.h" "${prefix}/include/webgpu/"
sudo cp "${tmpdir}/wgpu-native/include/webgpu/wgpu.h" "${prefix}/include/webgpu/"
sudo cp "${tmpdir}/wgpu_native.pc" "${prefix}/lib/pkgconfig/"

sudo ldconfig

if [[ -n "${GITHUB_ENV:-}" ]]; then
    echo "PKG_CONFIG_PATH=${prefix}/lib/pkgconfig:${PKG_CONFIG_PATH:-}" >> "${GITHUB_ENV}"
    echo "LD_LIBRARY_PATH=${prefix}/lib:${LD_LIBRARY_PATH:-}" >> "${GITHUB_ENV}"
fi

PKG_CONFIG_PATH="${prefix}/lib/pkgconfig:${PKG_CONFIG_PATH:-}" pkg-config --modversion wgpu_native
