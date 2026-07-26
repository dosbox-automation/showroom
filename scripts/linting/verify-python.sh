#!/bin/bash
# This file is part of the dosbox-automation-showroom Project.
# License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
#
# Runs the house Python linters over the project's Python.
#
#   ./scripts/linting/verify-python.sh          ruff and bandit
#   ./scripts/linting/verify-python.sh --tests  also run pytest
#
# The engine's equivalent uses pylint, which is dosbox-staging's choice.
# The house standard is ruff for style and correctness plus bandit for
# security, so that is what runs here.

set -Eeuo pipefail

# The showroom has no venv of its own; it shares the one at the root of
# the project tree, which is where pytest, ruff and bandit are installed.
readonly VENV="${SHOWROOM_VENV:-$HOME/Projects/augrudottir/.venv}"

die() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

require_tool() {
    local -r tool=$1
    [[ -x "$VENV/bin/$tool" ]] \
        || die "$tool not found in $VENV. Install it there, or point SHOWROOM_VENV elsewhere."
}

list_python_files() {
    git ls-files -z -- '*.py'
}

main() {
    local run_tests=0
    case "${1:-}" in
        "") ;;
        --tests) run_tests=1 ;;
        -h | --help)
            sed -n '5,12p' "$0"
            return 0
            ;;
        *) die "unknown option: $1 (try --help)" ;;
    esac

    cd "$(git rev-parse --show-toplevel)" || die "not inside a git repository"
    require_tool ruff
    require_tool bandit

    local -a files=()
    while IFS= read -r -d '' file; do
        files+=("$file")
    done < <(list_python_files)
    (( ${#files[@]} > 0 )) || die "no Python files found - wrong directory?"

    printf '== ruff ==\n'
    "$VENV/bin/ruff" check "${files[@]}"

    # bandit flags every assert as a finding, so test files are scanned at
    # low confidence only: an assert is the point of a test, not a risk.
    local -a sources=() tests=()
    local file
    for file in "${files[@]}"; do
        if [[ "$file" == tests/* ]]; then
            tests+=("$file")
        else
            sources+=("$file")
        fi
    done

    printf '== bandit ==\n'
    if (( ${#sources[@]} > 0 )); then
        "$VENV/bin/bandit" -q "${sources[@]}" || die "bandit reported findings in sources"
    fi
    if (( ${#tests[@]} > 0 )); then
        "$VENV/bin/bandit" -q --skip B101 "${tests[@]}" \
            || die "bandit reported findings in tests"
    fi
    printf 'bandit: clean\n'

    if (( run_tests )); then
        require_tool pytest
        printf '== pytest ==\n'
        "$VENV/bin/pytest" -q
    fi
}

main "$@"
