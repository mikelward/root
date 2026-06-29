root
====

`root` lets you run commands as the root user.

It is designed as a simpler version of `sudo` that doesn't require
complex configuration and doesn't mess with your environment variables.

Installation
------------
To install, just run `make install` as root.

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
any necessary changes, e.g. `usermod -a -G 0 USERNAME`.

Usage
-----
Just write `root` before the command you want to run as root,
e.g. `root vi /etc/fstab`.

More Info
---------
See the root(1) man page for more details.
