#!/bin/sh
#
# Tests tools/cargo-remap, in directory $1: it adds the path remappings to
# the flags cargo passes to rustc, keeps the caller's own flags wherever
# they come from, and keeps a path with a space in it as one argument.

dir=$(cd "${1:-.}" && pwd) || exit 1
failed=0
sep=$(printf '\037')

fail() {
    echo "cargo_remap_test: $*" >&2
    failed=1
}

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/a b"

# Prints the encoded flags cargo-remap would hand to cargo, run from "a b".
encoded() {
    (cd "$tmp/a b" && env "$@" sh "$dir/tools/cargo-remap" \
        sh -c 'printf %s "$CARGO_ENCODED_RUSTFLAGS"')
}

want_tail="--remap-path-prefix=/c h/.cargo=/cargo$sep--remap-path-prefix=$tmp/a b=."

got=$(encoded -u CARGO_ENCODED_RUSTFLAGS RUSTFLAGS="-C  debuginfo=0 -W unused" CARGO_HOME="/c h/.cargo")
test "$got" = "-C${sep}debuginfo=0$sep-W${sep}unused$sep$want_tail" \
    || fail "RUSTFLAGS not kept: got $(printf %s "$got" | tr '\037' '|')"

got=$(encoded RUSTFLAGS=-Wignored CARGO_ENCODED_RUSTFLAGS="-C${sep}x=a b" CARGO_HOME="/c h/.cargo")
test "$got" = "-C${sep}x=a b$sep$want_tail" \
    || fail "CARGO_ENCODED_RUSTFLAGS not kept: got $(printf %s "$got" | tr '\037' '|')"

got=$(encoded -u CARGO_ENCODED_RUSTFLAGS RUSTFLAGS= CARGO_HOME="/c h/.cargo")
test "$got" = "$want_tail" \
    || fail "empty RUSTFLAGS: got $(printf %s "$got" | tr '\037' '|')"

# A relative CARGO_HOME is joined to the directory cargo runs in, both as
# joined and with its ".." steps resolved, since cargo versions differ.
# Shortest prefix first, so the longest wins.
got=$(encoded -u RUSTFLAGS CARGO_ENCODED_RUSTFLAGS= CARGO_HOME="../h/.cargo")
want="--remap-path-prefix=$tmp/a b=.$sep--remap-path-prefix=$tmp/h/.cargo=/cargo$sep--remap-path-prefix=$tmp/a b/../h/.cargo=/cargo"
test "$got" = "$want" \
    || fail "relative CARGO_HOME: got $(printf %s "$got" | tr '\037' '|')"

# With neither variable set but a config file setting rustflags, as a
# list or as a string, the build must keep them, so cargo-remap leaves it
# alone and says the paths stay. Built with real cargo, with -v so cargo
# shows the flags it hands rustc.
#
# The builds run in a clean environment, with an empty CARGO_HOME (the
# crate has no dependencies), so rustflags from the caller's environment
# or cargo config can't leak in.
mkdir "$tmp/cargo-home"
isolated() {
    env -i PATH="$PATH" HOME="$HOME" RUSTUP_HOME="${RUSTUP_HOME:-$HOME/.rustup}" \
        ${RUSTUP_TOOLCHAIN:+RUSTUP_TOOLCHAIN="$RUSTUP_TOOLCHAIN"} \
        CARGO_HOME="$tmp/cargo-home" CARGO_NET_OFFLINE=true "$@"
}
mkdir -p "$tmp/a b/proj/src" "$tmp/a b/proj/.cargo"
printf '[package]\nname = "p"\nversion = "0.1.0"\nedition = "2021"\n' >"$tmp/a b/proj/Cargo.toml"
printf 'fn main() {}\n' >"$tmp/a b/proj/src/main.rs"
for value in '["--cfg", "from_config"]' '"--cfg from_config"'; do
    printf '[build]\nrustflags = %s\n' "$value" >"$tmp/a b/proj/.cargo/config.toml"
    rm -rf "$tmp/a b/proj/target"
    log=$(cd "$tmp/a b/proj" && isolated sh "$dir/tools/cargo-remap" cargo build -v 2>&1) || fail "cargo build failed with rustflags = $value: $log"
    case $log in
        *"--cfg from_config"*) ;;
        *) fail "rustflags = $value from the config file were dropped: $log" ;;
    esac
    case $log in
        *"build's paths stay in the binary"*) ;;
        *) fail "no warning that the paths stay, with rustflags = $value: $log" ;;
    esac
done

# Without any config rustflags, cargo hands rustc the remappings.
rm "$tmp/a b/proj/.cargo/config.toml"
rm -rf "$tmp/a b/proj/target"
log=$(cd "$tmp/a b/proj" && isolated sh "$dir/tools/cargo-remap" cargo build -v 2>&1) || fail "cargo build failed: $log"
case $log in
    *"--remap-path-prefix=$tmp/a b/proj=."*) ;;
    *) fail "remapping missing: $log" ;;
esac

test "$failed" -eq 0 && echo "cargo_remap_test: passed"
exit "$failed"
