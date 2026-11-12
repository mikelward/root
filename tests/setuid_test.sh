#!/bin/sh
#
# End-to-end checks of the root binary $1 the way it is installed: a setuid
# copy, run by a caller who is not root (SPEC.md, Permission Model, Execution
# Flow and Environment Handling). The unit tests and tests/cli.rs never get
# that far, since only a setuid copy can become root for another user.
#
# It needs root to make the setuid copy, Linux's setpriv to run it as another
# user, and a place where setuid programs work; it skips without them. CI
# runs it with sudo.

bin=${1:?usage: setuid_test.sh path/to/root}
failed=0

skip() {
    echo "setuid_test: skipping: $*" >&2
    exit 0
}

fail() {
    echo "setuid_test ($bin): $*" >&2
    failed=1
}

test "$(id -u)" -eq 0 || skip "needs root to make a setuid copy"
command -v setpriv >/dev/null 2>&1 || skip "needs setpriv to run as another user"

dir=$(mktemp -d) || exit 1
trap 'rm -rf "$dir"' EXIT
root="$dir/root"
id=$(command -v id)
# Search-only for others: the caller runs the copies but cannot list them.
chmod 711 "$dir" || exit 1
cp "$bin" "$root" && chown 0:0 "$root" && chmod 4755 "$root" || exit 1
cp "$id" "$dir/id" && chown 0:0 "$dir/id" && chmod 4755 "$dir/id" || exit 1

# The caller: an unprivileged user, in group 0 or not.
as_member() {
    setpriv --reuid 65534 --regid 65534 --groups 0 "$@"
}
as_other() {
    setpriv --reuid 65534 --regid 65534 --clear-groups "$@"
}

# A setuid copy of id says whether setuid programs work here at all: they
# don't on a nosuid filesystem or under no_new_privs.
test "$(as_member "$dir/id" -u 2>/dev/null)" = 0 || skip "setuid programs don't work in $dir"

# A member of group 0 runs the command as root, real and effective.
out=$(as_member "$root" "$id" -u; as_member "$root" "$id" -ru; as_member "$root" "$id" -g)
test "$out" = "0
0
0" || fail "a member of group 0 ran id as $(echo $out), not 0 0 0"

# Anyone else is refused before the command runs.
out=$(as_other "$root" "$id" -u 2>/dev/null)
status=$?
if test "$status" -ne 123 || test -n "$out"; then
    fail "a caller outside group 0 got status $status and output \"$out\", not 123 and none"
fi

# HOME is root's, unless -H leaves it alone.
home=$(getent passwd 0 | cut -d: -f6)
out=$(HOME=/nonexistent as_member "$root" /bin/sh -c 'echo "$HOME"')
test "$out" = "$home" || fail "HOME was \"$out\", not \"$home\""
out=$(HOME=/nonexistent as_member "$root" -H /bin/sh -c 'echo "$HOME"')
test "$out" = /nonexistent || fail "with -H, HOME was \"$out\", not \"/nonexistent\""

# An environment can hold HOME twice. No shell or env passes one on, so
# Python calls execve() itself. The command must see root's HOME, once.
# The caller runs Python, and root's may sit where the caller cannot reach.
if as_member python3 -c 'import ctypes' 2>/dev/null; then
    out=$(as_member python3 -c '
import ctypes, sys

def strings(*items):
    return (ctypes.c_char_p * (len(items) + 1))(*[i.encode() for i in items], None)

ctypes.CDLL(None).execve(sys.argv[1].encode(),
                         strings("root", "/usr/bin/env"),
                         strings("HOME=/first", "HOME=/second"))
sys.exit("execve failed")
' "$root" | grep '^HOME=')
    test "$out" = "HOME=$home" || fail "with HOME set twice, the command saw \"$(echo $out)\", not \"HOME=$home\""
else
    echo "setuid_test: not checking a duplicate HOME: needs a python3 the caller can run" >&2
fi

# The command gets the default SIGPIPE action, so a closed pipe ends it. An
# ignored SIGPIPE is inherited, so only check where this script has the
# default itself.
sigpipe='kill -PIPE $$; echo survived'
if test -z "$(/bin/sh -c "$sigpipe")"; then
    out=$(as_member "$root" /bin/sh -c "$sigpipe")
    test -z "$out" || fail "the command ran with SIGPIPE ignored"
else
    echo "setuid_test: not checking SIGPIPE: it is ignored here" >&2
fi

if test "$failed" -ne 0; then
    exit 1
fi
echo "setuid_test ($bin): passed"
