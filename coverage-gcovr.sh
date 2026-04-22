#!/usr/bin/env sh

set -eu

BUILD_DIR="${1:-build-coverage}"
ROOT_DIR="$(pwd)"
case "${BUILD_DIR}" in
  /*) BUILD_DIR_ABS="${BUILD_DIR}" ;;
  *) BUILD_DIR_ABS="${ROOT_DIR}/${BUILD_DIR}" ;;
esac
SOURCE_ROOT="$(dirname "${ROOT_DIR}")"
PROFILE_DIR="${BUILD_DIR_ABS}/coverage"
PROFDATA="${PROFILE_DIR}/thorvg.profdata"
SUMMARY_TXT="${BUILD_DIR_ABS}/meson-logs/coverage-summary.txt"
HTML_DIR="${BUILD_DIR_ABS}/meson-logs/coverage-html"
INDEX_HTML="${HTML_DIR}/index.html"
IGNORE_REGEX='(.*/)?test/|(.*/)?subprojects/'
LLVM_FLAGS='-fprofile-instr-generate -fcoverage-mapping'

if command -v xcrun >/dev/null 2>&1; then
  LLVM_COV="$(xcrun --find llvm-cov)"
  LLVM_PROFDATA="$(xcrun --find llvm-profdata)"
else
  LLVM_COV="$(command -v llvm-cov)"
  LLVM_PROFDATA="$(command -v llvm-profdata)"
fi

rm -rf "${BUILD_DIR_ABS}"
mkdir -p "${PROFILE_DIR}" "${HTML_DIR}"

env \
  CFLAGS="${LLVM_FLAGS}" \
  CXXFLAGS="${LLVM_FLAGS}" \
  LDFLAGS='-fprofile-instr-generate' \
  meson setup "${BUILD_DIR_ABS}" \
    -Db_coverage=false \
    -Dbuildtype=debug \
    -Doptimization=0 \
    -Dlog=true \
    -Dloaders=all \
    -Dsavers=all \
    -Dengines=cpu,gl,wg \
    -Dtests=true \
    -Dstatic=true \
    --errorlogs

meson compile -C "${BUILD_DIR_ABS}"

PROFILE_PATTERN="${PROFILE_DIR}/%p-%m.profraw"
if command -v xvfb-run >/dev/null 2>&1; then
  xvfb-run -a env LIBGL_ALWAYS_SOFTWARE=1 LLVM_PROFILE_FILE="${PROFILE_PATTERN}" meson test -C "${BUILD_DIR_ABS}" --print-errorlogs
else
  env LLVM_PROFILE_FILE="${PROFILE_PATTERN}" meson test -C "${BUILD_DIR_ABS}" --print-errorlogs
fi

set -- "${PROFILE_DIR}"/*.profraw
if [ ! -e "$1" ]; then
  printf 'No LLVM profile data was generated in %s\n' "${PROFILE_DIR}" >&2
  exit 1
fi

"${LLVM_PROFDATA}" merge -sparse "${PROFILE_DIR}"/*.profraw -o "${PROFDATA}"

TEST_BINARY="${BUILD_DIR_ABS}/test/tvgUnitTests"
LIBTHORVG="$(find "${BUILD_DIR_ABS}/src" -maxdepth 1 -type f \( -name 'libthorvg-1.1.so' -o -name 'libthorvg-1.1.dylib' -o -name 'libthorvg-1.1.dll' \) | head -n 1)"

if [ -n "${LIBTHORVG}" ]; then
  "${LLVM_COV}" report "${TEST_BINARY}" \
    --object "${LIBTHORVG}" \
    --instr-profile "${PROFDATA}" \
    --compilation-dir "${ROOT_DIR}" \
    --path-equivalence "${SOURCE_ROOT},${ROOT_DIR}" \
    --ignore-filename-regex "${IGNORE_REGEX}" \
    --show-branch-summary \
    --show-region-summary \
    > "${SUMMARY_TXT}"

  "${LLVM_COV}" show "${TEST_BINARY}" \
    --object "${LIBTHORVG}" \
    --instr-profile "${PROFDATA}" \
    --compilation-dir "${ROOT_DIR}" \
    --path-equivalence "${SOURCE_ROOT},${ROOT_DIR}" \
    --ignore-filename-regex "${IGNORE_REGEX}" \
    --format=html \
    --output-dir "${HTML_DIR}" \
    --project-title 'ThorVG LLVM Coverage' \
    --show-line-counts-or-regions \
    --show-directory-coverage \
    --show-branches=count \
    --show-region-summary
else
  "${LLVM_COV}" report "${TEST_BINARY}" \
    --instr-profile "${PROFDATA}" \
    --compilation-dir "${ROOT_DIR}" \
    --path-equivalence "${SOURCE_ROOT},${ROOT_DIR}" \
    --ignore-filename-regex "${IGNORE_REGEX}" \
    --show-branch-summary \
    --show-region-summary \
    > "${SUMMARY_TXT}"

  "${LLVM_COV}" show "${TEST_BINARY}" \
    --instr-profile "${PROFDATA}" \
    --compilation-dir "${ROOT_DIR}" \
    --path-equivalence "${SOURCE_ROOT},${ROOT_DIR}" \
    --ignore-filename-regex "${IGNORE_REGEX}" \
    --format=html \
    --output-dir "${HTML_DIR}" \
    --project-title 'ThorVG LLVM Coverage' \
    --show-line-counts-or-regions \
    --show-directory-coverage \
    --show-branches=count \
    --show-region-summary
fi

cat "${SUMMARY_TXT}"
printf 'Coverage summary: %s\n' "${SUMMARY_TXT}"
printf 'Coverage report: %s\n' "${INDEX_HTML}"
