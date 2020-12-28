#!/bin/bash

SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  SCRIPT_DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$SCRIPT_DIR/$SOURCE"
  # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
SCRIPT_DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"

# https://clangformat.com/
if [ ! "$(command -v clang-format)" ]; then
    echo >&2 "clang-format is required, but it's not installed";
    echo "-------------------------------------------------------------------------------------";
    echo "To install clang-format on Debian/Ubuntu, execute the following commands."
    echo " sudo apt-get install clang-format-3.9"
    echo " sudo ln -s /usr/bin/clang-format-3.9 /usr/bin/clang-format"
    echo "-------------------------------------------------------------------------------------";
    exit 1;
fi

config='{
BasedOnStyle: Google,
Language: Cpp,
Standard: Cpp11,
UseTab: Never,
IndentWidth: 2,
ColumnLimit: 100,
PointerBindsToType: true,
AllowShortFunctionsOnASingleLine: false,
BreakBeforeBraces: GNU,
}'

formatC++() {
  printf "."
  find -- "$1" -type f \( -name '*.cc' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) | \
    grep -v 'src/google' | xargs -n1 --no-run-if-empty clang-format -i -style="$config"
}

checkUpdatedFiles() {
  while IFS= read -r -d '' f; do
  if [ "${f: -3}" == ".cc" ] || [ "${f: -4}" == ".cpp" ] || [ "${f: -2}" == ".h" ] || \
    [ "${f: -4}" == ".hpp" ]; then
    printf '.'
    clang-format -i -style="$config" "$f";
  fi
  done < <(git diff-index -z --name-only --diff-filter=AM HEAD --)
}

printf "C/C++ reformatting: ";
if [[ $# -eq 0 ]]; then
  formatC++ "$SCRIPT_DIR/../../src/"
elif [[ "$1" == "-u"  ]]; then
  checkUpdatedFiles
else
  formatC++ "$1"
fi
printf " DONE C/C++ reformatting\n"
