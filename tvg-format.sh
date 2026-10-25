#!/usr/bin/env bash

# ------------------------------------------------------------------
# ThorVG clang-format commit formatter
# ------------------------------------------------------------------
#
# Description:
#   Formats only the lines modified in a specific commit
#   using clang-format-diff.
#
# Usage:
#   ./tvg-format.sh <commit-hash>
#
# Example:
#   ./tvg-format.sh HEAD
#   ./tvg-format.sh 3f2a8b1
#
# Requirements:
#   - git
#   - clang-format
#   - clang-format-diff
#
# Notes:
#   - Must be run from repository root
#   - .clang-format must exist at repo root
#   - Works on macOS and Linux
# ------------------------------------------------------------------

set -euo pipefail

# ------------------------------
# Argument check
# ------------------------------

if [ $# -ne 1 ]; then
    echo "Usage: $0 <commit-hash>"
    exit 1
fi

COMMIT="$1"

# ------------------------------
# Tool existence checks
# ------------------------------

if ! command -v git >/dev/null 2>&1; then
    echo "Error: git is not installed or not in PATH."
    exit 1
fi

if ! command -v clang-format >/dev/null 2>&1; then
    echo "Error: clang-format is not installed or not in PATH."
    exit 1
fi

FORMAT_DIFF=(clang-format-diff)
if ! command -v clang-format-diff >/dev/null 2>&1; then
    # Homebrew installs the helper outside PATH.
    if command -v brew >/dev/null 2>&1 && command -v python3 >/dev/null 2>&1 &&
       prefix=$(brew --prefix clang-format 2>/dev/null); then
        helper="$prefix/share/clang/clang-format-diff.py"
        if [ -f "$helper" ]; then
            FORMAT_DIFF=(python3 "$helper")
        fi
    fi
    if [ "${FORMAT_DIFF[0]}" = clang-format-diff ]; then
        echo "Error: clang-format-diff is not installed or not in PATH."
        echo "Hint: On macOS, install via: brew install clang-format"
        exit 1
    fi
fi

# ------------------------------
# Repository root check
# ------------------------------

if [ ! -f ".clang-format" ]; then
    echo "Error: .clang-format not found in current directory."
    echo "Please run this script from the repository root."
    exit 1
fi

# ------------------------------
# Commit existence check
# ------------------------------

if ! git rev-parse --verify "$COMMIT" >/dev/null 2>&1; then
    echo "Error: Commit '$COMMIT' not found."
    exit 1
fi

# ------------------------------
# Run formatter
# ------------------------------

echo "Formatting changes in commit: $COMMIT"

git show -U0 "$COMMIT" | "${FORMAT_DIFF[@]}" -p1 -i

echo "Done."
