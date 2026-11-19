#!/bin/sh
#
# Tests that `make root`, in the Makefile in directory $1, builds a binary
# that names neither the checkout it was built in nor the cargo registry
# under the builder's home directory, and that a change to the Makefile
# rebuilds it.
#
# It builds a copy of the checkout in a temporary directory, so the path it
# looks for is one nothing else could have put there. The directory's name
# has a space in it, which must stay one argument to rustc.

dir=${1:-.}
failed=0

fail() {
    echo "paths_test: $*" >&2
    failed=1
}

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT
# Its physical path, so ".." steps climb it exactly (macOS's /var is a
# link to /private/var).
tmp=$(cd "$tmp" && pwd -P) || exit 1

mkdir "$tmp/with space"
(cd "$dir" && tar cf - --exclude=./target --exclude=./root --exclude=./.git .) \
    | (cd "$tmp/with space" && tar xf -) || exit 1
# The copy builds into its own target/, as `make root` expects.
unset CARGO_TARGET_DIR
# Build with CARGO_HOME relative to the copy, the form cargo leaves
# unresolved: the registry's paths then start with the copy's own path,
# and must still be remapped away. The ".." steps climb to /.
# The caller's CARGO_HOME, resolved from where this test was started, so
# the copy reuses the same registry.
cargo_home=${CARGO_HOME:-$HOME/.cargo}
case $cargo_home in
    /*) ;;
    *) cargo_home=$(pwd)/$cargo_home ;;
esac
if test -d "$cargo_home"; then
    cargo_home=$(cd "$cargo_home" && pwd -P) || exit 1
fi
dots=$(printf '%s\n' "$tmp/with space" | sed 's|[^/][^/]*|..|g; s|^/||')
CARGO_HOME=$dots$cargo_home
export CARGO_HOME
if ! make -s -C "$tmp/with space" root >"$tmp/build.log" 2>&1; then
    cat "$tmp/build.log" >&2
    echo "paths_test: make root failed" >&2
    exit 1
fi
bin="$tmp/with space/root"

# Where cargo's config sets rustflags, cargo-remap leaves them alone and
# the paths stay (cargo_remap_test.sh covers that), so there's nothing to
# check here.
if grep -q "build's paths stay in the binary" "$tmp/build.log"; then
    echo "paths_test: skipping: cargo's config sets rustflags"
    exit 0
fi

if grep -q -F "$tmp" "$bin"; then
    fail "binary names its build directory"
fi
if grep -q -F "$cargo_home" "$bin"; then
    fail "binary names the cargo registry under $cargo_home"
fi

# A binary built before a change to the build flags must be rebuilt: with
# the sources older than the binary and the Makefile newer, make must
# consider it out of date.
find "$tmp/with space/src" "$tmp/with space/Cargo.toml" "$tmp/with space/tools" -exec touch -t 200001010000 {} +
touch -t 200101010000 "$bin"
if make -q -C "$tmp/with space" root >/dev/null 2>&1; then
    fail "root is not rebuilt when the Makefile changes"
fi

test "$failed" -eq 0 && echo "paths_test: passed"
exit "$failed"
