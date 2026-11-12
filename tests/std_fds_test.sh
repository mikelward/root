#!/bin/sh
#
# Checks that the root binary $1 runs its command with stdin, stdout and
# stderr open even when its caller closed them (SPEC.md, Execution Flow).
# Otherwise the next file or socket it opens takes that number, and its own
# messages, or the command it runs as root, write there.
#
# Running the command needs root (CI runs this with sudo); without it, it
# skips.

bin=${1:?usage: std_fds_test.sh path/to/root}

if test "$(id -u)" -ne 0; then
    echo "std_fds_test: skipping: needs root to run a command" >&2
    exit 0
fi

# Each redirection fails in sh if its descriptor is closed.
if ! "$bin" /bin/sh -c 'true <&0 && echo >&1 && echo >&2' <&- >&- 2>&-; then
    echo "std_fds_test ($bin): the command ran with a standard descriptor closed" >&2
    exit 1
fi
echo "std_fds_test ($bin): passed"
