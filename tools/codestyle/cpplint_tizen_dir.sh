#!/bin/bash

SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  SCRIPT_DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$SCRIPT_DIR/$SOURCE"
  # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
SCRIPT_DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"

patch_cpp="$SCRIPT_DIR/cpplint_tizen_160919.py"

# check rule to cpp
find -- "$1" \( -name "*.cpp" -o name "*.cc" \) -exec python "$patch_cpp" {} \;
find -- "$1" -name "*.h" -exec python "$patch_cpp" {} \;
