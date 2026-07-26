#!/bin/bash
# This file is part of the dosbox-automation-showroom Project.
# License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
#
# Runs clang-tidy over the project's C++ using .clang-tidy at the root.
#
#   ./scripts/linting/verify-cpp.sh                 uses build/debug-linux
#   ./scripts/linting/verify-cpp.sh <build-dir>     uses another build
#
# Needs a configured build directory: clang-tidy reads the compile flags
# from its compile_commands.json, which CMakeLists generates.

set -Eeuo pipefail

readonly DEFAULT_BUILD_DIR="build/debug-linux"

die() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

# See format-cpp.sh: --others stops new code being skipped, and imported/
# keeps its origin's naming on purpose.
list_cpp_files() {
    git ls-files -z --cached --others --exclude-standard \
        -- 'src/*.cpp' 'tests/*.cpp' \
           ':(exclude)*/imported/*'
}

main() {
    case "${1:-}" in
        -h | --help)
            sed -n '5,12p' "$0"
            return 0
            ;;
    esac

    cd "$(git rev-parse --show-toplevel)" || die "not inside a git repository"
    [[ -f .clang-tidy ]] || die "no .clang-tidy at the repository root"

    local -r build_dir=${1:-$DEFAULT_BUILD_DIR}
    [[ -f "$build_dir/compile_commands.json" ]] \
        || die "no compile_commands.json in $build_dir - configure it first:
  cmake --preset debug-linux"

    command -v clang-tidy > /dev/null 2>&1 || die "clang-tidy not found"
    clang-tidy --version | sed -n '2p'

    local -a files=()
    while IFS= read -r -d '' file; do
        files+=("$file")
    done < <(list_cpp_files)
    (( ${#files[@]} > 0 )) || die "no C++ files found - wrong directory?"

    # imported/ must be excluded here as well as from the file list: a
    # header left out of the list is still reported against whoever
    # includes it.
    #
    # --exclude-header-filter rather than a negative lookahead: LLVM's
    # regex engine has none, and an unsupported pattern matches nothing
    # instead of failing, so every header goes uninspected while the run
    # reports clean.
    clang-tidy -p "$build_dir" --warnings-as-errors='*' \
        --header-filter='(src|tests)/.*' \
        --exclude-header-filter='.*/imported/.*' "${files[@]}"
    printf 'clang-tidy: clean across %s files\n' "${#files[@]}"
}

main "$@"
