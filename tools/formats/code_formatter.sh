#!/bin/bash

SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  SCRIPT_DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$SCRIPT_DIR/$SOURCE"
  # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
SCRIPT_DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"
echo "$SCRIPT_DIR"

dir2analyze="$SCRIPT_DIR/../../src"
analyzeC="false";
files_to_update="";

printHelp() {
  echo "Script for automatic fixing some coding style rules issues."
  echo "In case of any issues of analysed code, the script would adjust it to follow"
  echo "coding style. THIS COULD CHANGE YOUR FILES. Please commit your changes locally"
  echo "if you want to have meaningful changes divided from style-only changes"
  echo "Using it without specifying a path would analyse \"src\" from current directory."
  printf -- "\nUsage: %s [directory] [options ...]\n" "$(basename "$0")"
  printf -- "Options:\n"
  printf -- "-u\t\tcheck all C/C++ files that has been modified since last commit and fix issues\n"
  printf -- "-c\t\tcheck C/C++ files and fix issues\n"
  printf -- "-a [--all]\tcheck all C/C++ files and fix issues\n"
  printf -- "-h [--help]\tshow this help\n"
  echo "When using any of 'u' options, all not commited files will be backed up."
}

formatC++() {
  path="$(readlink -f "$1")"
  if [[ "$analyzeC" == "true" ]]; then
    echo "Reformatting C++ sources recursively for directory $path";
    "$SCRIPT_DIR/c++_clang_formatter.sh" "$path";
  else
    echo "no C++ sources reformatting - please use '-c' option to enable it";
  fi
}

backupUpdatedFiles() {
  files_to_update="$(git diff-index -z --name-only --diff-filter=AM HEAD --)";
  if [[ "$files_to_update" == "" ]]; then
    echo "There are no updated files to format";
    exit;
  fi
  time_stamp=$(date +%Y-%m-%d-%T);
  dir_name="backup_${time_stamp}";
  echo "Creating backup of not commited changes in directory: $dir_name";
  mkdir -p "${dir_name}";
  while IFS= read -r -d '' file; do
    cp -- "$file" "$dir_name";
  done <<< "$files_to_update"
}

formatUpdatedC++() {
  if [[ "$files_to_update" == "" ]]; then
    exit
  fi
  echo "Reformatting C++ sources from list:";
  git diff-index --name-only --diff-filter=AM HEAD --;
  "$SCRIPT_DIR/c++_clang_formatter.sh" -u;
}

for arg in "$@";
do
  if [[ "$arg" == "-u" ]]; then
    backupUpdatedFiles
    formatUpdatedC++
    exit
  elif [[ "$arg" == "-c" ]]; then
    analyzeC="true";
  elif [[ "$arg" == "--all" ]] || [[ "$arg" == "-a" ]]; then
    analyzeC="true";
  elif [[ "$arg" == "--help" ]] || [[ "$arg" == "-h" ]]; then
    printHelp
    exit
  else
    if [[ -d "$arg" ]]; then
      dir2analyze="$arg";
    else
      (>&2 printf "ERROR: directory %s does not exist\n\n" "$arg")
      printHelp
      exit 1
    fi
  fi
done

formatC++ "$dir2analyze"
