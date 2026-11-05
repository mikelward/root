#!/bin/sh
#
# Tests for `make install`, in the Makefile in directory $1 (the Rust build's
# or legacy/'s). Its files go to group 0, by number: root on Linux, wheel on
# macOS and the BSDs, and the group root's own check is for (SPEC.md). A
# group named "root" doesn't exist on a Mac, so install failed there.
#
# Without root, it checks what `make -n install` would run. As root (CI runs
# it with sudo, once the binary is built), it installs into a temporary
# prefix and checks the files themselves.

dir=${1:-.}
failed=0

fail() {
    echo "install_test: $*" >&2
    failed=1
}

# Every install that sets an owner sets group 0 too.
commands=$(make -s -n -C "$dir" install PREFIX=/nonexistent) || fail "make -n install failed"
owned=$(printf '%s\n' "$commands" | grep -c 'install .*-o root') || :
if test "$owned" -lt 2; then
    fail "expected two installs owned by root, found $owned"
fi
if printf '%s\n' "$commands" | grep 'install .*-o root' | grep -v -- '-g 0 ' >/dev/null; then
    fail "an install doesn't use group 0:"
    printf '%s\n' "$commands" | grep 'install .*-o root' | grep -v -- '-g 0 ' >&2
fi

if test "$(id -u)" -eq 0; then
    prefix=$(mktemp -d) || exit 1
    trap 'rm -rf "$prefix"' EXIT
    if ! make -s -C "$dir" install PREFIX="$prefix" >/dev/null; then
        fail "make install failed as root"
    else
        for f in "$prefix/bin/root" "$prefix/share/man/man1/root.1"; do
            # ls -n: owner and group by number, the same on Linux and BSD.
            set -- $(ls -ln "$f")
            if test "$3:$4" != "0:0"; then
                fail "$f is owned by $3:$4, not 0:0"
            fi
        done
        case "$(ls -l "$prefix/bin/root")" in
            -rwsr-xr-x*) ;;
            *) fail "$prefix/bin/root isn't setuid 4755: $(ls -l "$prefix/bin/root")" ;;
        esac
    fi
fi

if test "$failed" -ne 0; then
    exit 1
fi
echo "install_test ($dir): passed"
