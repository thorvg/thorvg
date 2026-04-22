#!/usr/bin/env sh

set -eu

BUILD_DIR="${1:-build-gcc-coverage}"
SHIM_DIR="$(mktemp -d "${TMPDIR:-/tmp}/thorvg-gcov.XXXXXX")"
LCOV_HOME="$(mktemp -d "${TMPDIR:-/tmp}/thorvg-lcov.XXXXXX")"

cleanup() {
  rm -rf "${SHIM_DIR}"
  rm -rf "${LCOV_HOME}"
}

trap cleanup EXIT INT TERM

find_versioned_tool() {
  prefix="$1"
  version=30

  while [ "${version}" -ge 10 ]; do
    if command -v "${prefix}-${version}" >/dev/null 2>&1; then
      printf '%s\n' "${prefix}-${version}"
      return 0
    fi
    version=$((version - 1))
  done

  return 1
}

if [ -n "${CC:-}" ]; then
  CC_BIN="${CC}"
else
  CC_BIN="$(find_versioned_tool gcc || true)"
fi

if [ -n "${CXX:-}" ]; then
  CXX_BIN="${CXX}"
else
  CXX_BIN="$(find_versioned_tool g++ || true)"
fi

if [ -z "${CC_BIN}" ] || [ -z "${CXX_BIN}" ]; then
  printf 'Homebrew GCC was not found. Install it first with: brew install gcc\n' >&2
  exit 1
fi

CC_BASE="$(basename "${CC_BIN}")"
GCC_VERSION="${CC_BASE#gcc-}"

if [ "${GCC_VERSION}" = "${CC_BASE}" ]; then
  GCOV_BIN="${GCOV:-gcov}"
else
  GCOV_BIN="${GCOV:-gcov-${GCC_VERSION}}"
fi

if ! command -v "${GCOV_BIN}" >/dev/null 2>&1; then
  printf 'Matching gcov tool was not found: %s\n' "${GCOV_BIN}" >&2
  exit 1
fi

rm -rf "${BUILD_DIR}"

cat > "${SHIM_DIR}/gcov" <<EOF
#!/usr/bin/env sh
exec "${GCOV_BIN}" "\$@"
EOF
chmod +x "${SHIM_DIR}/gcov"

cat > "${LCOV_HOME}/.lcovrc" <<EOF
geninfo_unexecuted_blocks = 1
check_data_consistency = 0
ignore_errors = gcov,inconsistent
EOF

printf 'Using CC=%s\n' "${CC_BIN}"
printf 'Using CXX=%s\n' "${CXX_BIN}"
printf 'Using GCOV=%s\n' "${GCOV_BIN}"

env \
  HOME="${LCOV_HOME}" \
  PATH="${SHIM_DIR}:${PATH}" \
  CC="${CC_BIN}" \
  CXX="${CXX_BIN}" \
  GCOV="${GCOV_BIN}" \
  meson setup "${BUILD_DIR}" \
    -Dtests=true \
    -Db_coverage=true \
    -Dengines=cpu,gl \
    -Dloaders=all \
    -Dsavers=all \
    -Dextra="" \
    --errorlogs

env HOME="${LCOV_HOME}" PATH="${SHIM_DIR}:${PATH}" GCOV="${GCOV_BIN}" ninja -C "${BUILD_DIR}" install test
env HOME="${LCOV_HOME}" PATH="${SHIM_DIR}:${PATH}" GCOV="${GCOV_BIN}" ninja -C "${BUILD_DIR}" install test coverage-html

printf 'Meson coverage log: %s\n' "${BUILD_DIR}/meson-logs"
