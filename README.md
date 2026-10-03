root
====

`root` lets you run commands as the root user.

It is designed as a simpler version of `sudo` that doesn't require
complex configuration and doesn't mess with your environment variables.

Installation
------------
To install, just run `make install` as root. On BSD and macOS, where
group 0 is called `wheel`, run `make install INSTALL_GROUP=wheel`.

You will require GNU make and a stable Rust toolchain (cargo + rustc).

Note that cargo and rustc are only needed to *build* `root`. The resulting
binary has no Rust runtime dependency, so the usual approach for a machine
without a toolchain is to build `root` once elsewhere and copy the binary
(and `root.1`) into place.

### Legacy C build (fallback)

If you genuinely need to build from source on a machine that has no Rust
toolchain, a legacy C implementation is kept under `legacy/`. It builds the
same `root` command with only a C99 compiler and GNU make:

    cd legacy
    make install      # as root

The Rust version is the primary, supported implementation; the C version is
maintained only as a fallback.

Configuration
-------------
Any user in group 0 (usually called `wheel` or `root`) is allowed
to use `root`.

Use your system's tools such as `gpasswd` and `usermod` to make
any necessary changes, e.g. `usermod -a -G 0 USERNAME`. On macOS, use
`dseditgroup -o edit -a USERNAME -t user wheel`.

Usage
-----
Just write `root` before the command you want to run as root,
e.g. `root vi /etc/fstab`.

Logging
-------
Each command `root` runs is logged to syslog (the `authpriv` facility),
naming the calling user and the full path of the command.

Unusual characters in command names and paths are escaped in log and
error messages, so each entry stays on one line: a backslash is shown
as `\\`, a newline, carriage return or tab as `\n`, `\r` or `\t`, and
other control characters and bytes that are not valid UTF-8 as `\xNN`.
Ordinary text, including non-ASCII letters, is shown as is.

More Info
---------
See the root(1) man page for more details.
