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

# See format-cpp.sh: --others is what stops new, uncommitted code from
# being skipped while the run still reports success.
# Files copied verbatim from sibling house projects live under imported/.
# They keep their origin's naming and formatting on purpose, so a fix
# crosses between trees as a plain diff. Checking them would only report
# that they are not ours.
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

    # Our own headers are checked through the files that include them;
    # everything else (Qt, toml++, gtest) is somebody else's naming.
    #
    # imported/ has to be excluded here as well as from the file list.
    # Leaving it out of the list only stops it being a subject; the
    # moment one of our own files includes the header, its naming is
    # reported against whichever file pulled it in. The exemption has to
    # cover both or it only holds until someone uses the code.
    #
    # --exclude-header-filter, not a negative lookahead in the include
    # pattern: clang-tidy compiles these with LLVM's regex engine, which
    # has no lookahead. An unsupported pattern does not fail loudly, it
    # matches nothing, and the run then reports every file clean while
    # inspecting no header at all. Verified by putting a deliberately
    # wrong name in a header and watching it get caught.
    clang-tidy -p "$build_dir" --warnings-as-errors='*' \
        --header-filter='(src|tests)/.*' \
        --exclude-header-filter='.*/imported/.*' "${files[@]}"
    printf 'clang-tidy: clean across %s files\n' "${#files[@]}"
}

main "$@"
