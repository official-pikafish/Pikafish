#!/bin/sh
#
# Install Pikafish's git hooks into the current clone.
#
#     scripts/install-hooks.sh                  install
#     scripts/install-hooks.sh --use-repo-hooks also pin core.hooksPath to
#                                               .git/hooks for this clone only
#
# Installs one hook:
#   pre-commit -> scripts/hooks/pre-commit   (formats staged C++ sources)
#
# Safety: this script refuses to write anywhere outside the repository.
# core.hooksPath is frequently set in a global gitconfig, and that directory
# is shared by every repository on the machine -- writing a Pikafish hook
# there would silently replace unrelated projects' hooks. When that situation
# is detected you get instructions instead of a surprise.
#
# A symlink is used when the platform allows it, so hook updates are picked up
# without reinstalling. Copy is the fallback; re-run after pulling.

set -eu

HOOK_NAME=pre-commit
HOOK_SRC=scripts/hooks/$HOOK_NAME
MARKER='Pikafish pre-commit hook'

die() {
    printf '%s\n' "$*" >&2
    exit 1
}

use_repo_hooks=no
for arg in "$@"; do
    case "$arg" in
    --use-repo-hooks) use_repo_hooks=yes ;;
    *) die "unknown option: $arg" ;;
    esac
done

root=$(git rev-parse --show-toplevel)

if [ "$use_repo_hooks" = yes ]; then
    git config --local core.hooksPath .git/hooks
    printf 'set core.hooksPath=.git/hooks for this clone only\n'
fi

hooks=$(git rev-parse --git-path hooks)

# Normalise a Windows drive prefix to a form the case patterns can match.
case "$hooks" in
[A-Za-z]:[\\/]*) hooks=$(printf '%s' "$hooks" | sed 's|\\|/|g') ;;
esac

# git rev-parse --git-path may return a repo-relative path.
case "$hooks" in
/* | [A-Za-z]:/*) ;;
*) hooks="$root/$hooks" ;;
esac

# The safety gate: only ever touch something inside this clone.
case "$hooks" in
"$root"/*) ;;
*)
    printf 'refusing to install: git resolves hooks to\n' >&2
    printf '    %s\n' "$hooks" >&2
    printf 'which is outside this clone (%s).\n\n' "$root" >&2
    printf 'That usually means core.hooksPath is set globally:\n' >&2
    printf '    git config --show-origin --get core.hooksPath\n\n' >&2
    printf 'Options:\n' >&2
    printf '  1. Pin the hooks path for this clone only, then re-run:\n' >&2
    printf '         scripts/install-hooks.sh --use-repo-hooks\n' >&2
    printf '  2. Or add this line to the shared hook by hand:\n' >&2
    printf '         exec "%s/%s" --staged\n' "$root" "$HOOK_SRC" >&2
    exit 1
    ;;
esac

mkdir -p "$hooks"
chmod +x "$root/$HOOK_SRC"

target="$hooks/$HOOK_NAME"

if [ -e "$target" ] && ! grep -q "$MARKER" "$target" 2>/dev/null; then
    backup="$target.pikafish-backup"
    cp "$target" "$backup"
    printf 'existing %s is not ours; backed up to %s\n' "$HOOK_NAME" "$backup"
fi

rm -f "$target"

if ln -sf "$root/$HOOK_SRC" "$target" 2>/dev/null; then
    printf 'installed: %s -> %s (symlink)\n' "$target" "$HOOK_SRC"
else
    cp "$root/$HOOK_SRC" "$target"
    chmod +x "$target"
    printf 'installed: %s (copy -- re-run after pulling)\n' "$target"
fi
