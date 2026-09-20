#!/bin/sh
#
# clang-format wrapper for Pikafish.
#
# Why this exists: the repository already runs a clang-format check in CI
# (.github/workflows/analyzer.yml), but the check only reports a problem --
# somebody then has to format by hand and push a "Fix format" commit.
# This script is the missing write-back half, usable both locally and in CI.
#
# Usage:
#   scripts/clang-format.sh              format source files staged for commit
#   scripts/clang-format.sh --staged     same as above (explicit)
#   scripts/clang-format.sh --all        format every tracked source file
#   scripts/clang-format.sh --check      report misformatted files, exit 1 if any
#   scripts/clang-format.sh --version    print the resolved clang-format version
#
# The version used in CI is pinned in analyzer.yml (CLANG_FORMAT_VERSION).
# This script prefers a binary with that exact version and falls back to the
# newest versioned binary found, then to plain "clang-format".
# Override the search entirely by setting the CLANG_FORMAT environment variable.

# -f (noglob) is load-bearing: $SOURCES is expanded unquoted so that git sees
# three separate pathspecs. Without -f the shell would glob them first, and
# "src/*.cpp" would match only the files directly under src/, silently
# skipping every source in src/nnue/, src/layers/ and src/external/.
set -efu

# Keep in sync with CLANG_FORMAT_VERSION in .github/workflows/analyzer.yml.
CLANG_FORMAT_VERSION=${CLANG_FORMAT_VERSION:-22}

# Interpreted as git pathspecs, not shell globs. A git pathspec "*" crosses
# directory separators, which is what we want here.
#
# src/external/ is vendored third-party code (zstd, (c) Meta Platforms) and is
# deliberately left alone: reformatting it would diverge from upstream and
# produce noise in every future sync. src/Makefile's "analyze" target excludes
# external/ for the same reason, so this matches the project's own convention.
SOURCES='src/*.cpp src/*.h src/*.S :(exclude)src/external/*'

die() {
    printf '%s\n' "$*" >&2
    exit 1
}

# True when a binary reports the major version CI pins.
matches_pin() {
    "$1" --version 2>/dev/null | grep -qE "version ${CLANG_FORMAT_VERSION}([. ]|$)"
}

find_clang_format() {
    if [ -n "${CLANG_FORMAT:-}" ]; then
        command -v "$CLANG_FORMAT" >/dev/null 2>&1 \
            || die "CLANG_FORMAT=$CLANG_FORMAT is not executable"
        printf '%s\n' "$CLANG_FORMAT"
        return 0
    fi

    # Order matters. "clang-format" comes before the versioned names on purpose:
    # a machine may ship an unrelated clang-format-<N> on PATH (CI runner images
    # do), and picking that one silently would mean using a style engine that
    # differs from the pinned version -- a newer clang-format drops options this
    # repository's .clang-format still sets, so every file then looks
    # misformatted. Prefer whichever candidate actually reports the pinned
    # version, and only fall back to a mismatched one with a warning.
    candidates="clang-format-$CLANG_FORMAT_VERSION clang-format"
    i=30
    while [ "$i" -ge 14 ]; do
        candidates="$candidates clang-format-$i"
        i=$((i - 1))
    done

    fallback=''
    for c in $candidates; do
        command -v "$c" >/dev/null 2>&1 || continue
        if matches_pin "$c"; then
            printf '%s\n' "$c"
            return 0
        fi
        [ -n "$fallback" ] || fallback=$c
    done

    if [ -n "$fallback" ]; then
        printf 'clang-format: %s does not match the version CI pins (%s);\n' \
            "$("$fallback" --version 2>/dev/null)" "$CLANG_FORMAT_VERSION" >&2
        printf 'clang-format: results may differ from CI. Set CLANG_FORMAT to override.\n' >&2
        printf '%s\n' "$fallback"
        return 0
    fi

    return 1
}

CF=$(find_clang_format) || die "clang-format not found. Install LLVM $CLANG_FORMAT_VERSION or set CLANG_FORMAT."

cd "$(git rev-parse --show-toplevel)"

# Tracked C++ sources, newline separated.
all_sources() {
    # shellcheck disable=SC2086
    git ls-files -- $SOURCES
}

# Tracked C++ sources staged for commit (added or modified).
staged_sources() {
    # shellcheck disable=SC2086
    git diff --cached --name-only --diff-filter=ACM -- $SOURCES
}

# Reads paths from stdin, formats each in place, prints how many were handled.
format_file_list() {
    tmp=$(mktemp)
    cat >"$tmp"
    count=0
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        [ -f "$f" ] || continue
        "$CF" -i "$f"
        count=$((count + 1))
    done <"$tmp"
    rm -f "$tmp"
    printf '%s\n' "$count"
}

# Reads paths from stdin, prints violations to stderr and "<total> <bad>" to
# stdout. Kept on separate streams so the summary can be captured while the
# detail still reaches the log.
check_file_list() {
    tmp=$(mktemp)
    cat >"$tmp"
    bad=0
    count=0
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        [ -f "$f" ] || continue
        count=$((count + 1))
        if ! "$CF" --dry-run --Werror "$f" >/dev/null 2>&1; then
            printf '  misformatted: %s\n' "$f" >&2
            bad=$((bad + 1))
        fi
    done <"$tmp"
    rm -f "$tmp"
    printf '%s %s\n' "$count" "$bad"
}

mode=${1:---staged}

case "$mode" in
--version)
    "$CF" --version
    ;;

--check)
    result=$(all_sources | check_file_list)
    total=${result% *}
    bad=${result#* }
    if [ "$bad" -ne 0 ]; then
        printf '\nclang-format: %s of %s files need formatting.\n' "$bad" "$total" >&2
        printf 'clang-format: using %s (%s)\n' "$CF" "$("$CF" --version)" >&2
        printf 'Fix with "scripts/clang-format.sh --all".\n' >&2
        exit 1
    fi
    printf 'clang-format: clean (%s files, %s)\n' "$total" "$("$CF" --version)"
    ;;

--all)
    n=$(all_sources | format_file_list)
    printf 'clang-format: formatted %s files with %s\n' "$n" "$("$CF" --version)"
    # shellcheck disable=SC2086
    git diff --stat -- $SOURCES
    ;;

--staged)
    list=$(staged_sources)
    if [ -z "$list" ]; then
        exit 0
    fi
    n=$(printf '%s\n' "$list" | format_file_list)
    # Re-stage only the files we actually touched, so unstaged edits elsewhere
    # are never swept into the commit by accident.
    printf '%s\n' "$list" | while IFS= read -r f; do
        [ -n "$f" ] && git add -- "$f"
    done
    printf 'clang-format: formatted %s staged file(s)\n' "$n"

    # git decides whether there is anything to commit *before* it runs this
    # hook. If formatting just removed the last staged difference, the commit
    # would land empty. Stop it and say why instead.
    if git diff --cached --quiet; then
        printf 'clang-format: the only staged change was a formatting violation.\n' >&2
        printf 'clang-format: the sources now match HEAD, so there is nothing to commit.\n' >&2
        exit 1
    fi
    ;;

*)
    die "unknown option: $mode (try --staged, --all, --check, --version)"
    ;;
esac
