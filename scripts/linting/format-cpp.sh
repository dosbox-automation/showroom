#!/bin/bash
# This file is part of the dosbox-automation-showroom Project.
# License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
#
# Formats the project's C++ against .clang-format, or checks it.
#
#   ./scripts/linting/format-cpp.sh            format in place
#   ./scripts/linting/format-cpp.sh --check    report and exit 1 if unformatted
#   ./scripts/linting/format-cpp.sh --diff     show what would change
#
# The engine formats only the lines a commit touched, because it carries
# thousands of upstream files that must not be reformatted. Nothing here
# is inherited, so whole files are formatted and the line-range machinery
# is not needed.

set -Eeuo pipefail

readonly MIN_CLANG_FORMAT_MAJOR=15

die() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

# Distributions ship clang-format both unversioned and versioned, and the
# unversioned one is not always the newest. Prefer a known-new binary.
find_clang_format() {
    local candidate
    for candidate in clang-format-19 clang-format-18 clang-format-17 \
                     clang-format-16 clang-format-15 clang-format; do
        if command -v "$candidate" > /dev/null 2>&1; then
            printf '%s' "$candidate"
            return 0
        fi
    done
    die "clang-format not found. Install clang-format ${MIN_CLANG_FORMAT_MAJOR} or newer."
}

assert_version() {
    local -r binary=$1
    local version major
    version="$("$binary" --version)"
    # "Debian clang-format version 19.1.7 (3+b1)" -> 19
    major="$(printf '%s' "$version" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1 \
             | cut -d. -f1)"
    [[ -n "$major" ]] || die "cannot read a version from: $version"
    (( major >= MIN_CLANG_FORMAT_MAJOR )) \
        || die "$version is too old, need ${MIN_CLANG_FORMAT_MAJOR} or newer"
    printf 'Using %s\n' "$version"
}

# --others matters: files written this session are untracked, and a check
# that skips them still reports clean. imported/ keeps its origin's
# formatting so fixes cross between trees as a plain diff.
list_cpp_files() {
    git ls-files -z --cached --others --exclude-standard \
        -- 'src/*.cpp' 'src/*.h' 'tests/*.cpp' 'tests/*.h' \
           ':(exclude)*/imported/*'
}

main() {
    local -r mode=${1:-format}
    case "$mode" in
        format | --check | --diff | -h | --help) ;;
        *) die "unknown option: $mode (try --help)" ;;
    esac
    if [[ "$mode" == "-h" || "$mode" == "--help" ]]; then
        sed -n '5,15p' "$0"
        return 0
    fi

    cd "$(git rev-parse --show-toplevel)" || die "not inside a git repository"
    [[ -f .clang-format ]] || die "no .clang-format at the repository root"

    local clang_format
    clang_format="$(find_clang_format)"
    assert_version "$clang_format"

    local -a files=()
    while IFS= read -r -d '' file; do
        files+=("$file")
    done < <(list_cpp_files)
    (( ${#files[@]} > 0 )) || die "no C++ files found - wrong directory?"

    case "$mode" in
        --check)
            # --dry-run reports; -Werror turns a report into a failure.
            "$clang_format" --dry-run -Werror "${files[@]}"
            printf '%s files are formatted correctly\n' "${#files[@]}"
            ;;
        --diff)
            local file status=0
            for file in "${files[@]}"; do
                "$clang_format" "$file" | diff -u --label "$file" --label "$file (formatted)" \
                    "$file" - || status=1
            done
            return "$status"
            ;;
        *)
            "$clang_format" -i "${files[@]}"
            printf 'formatted %s files\n' "${#files[@]}"
            ;;
    esac
}

main "$@"
