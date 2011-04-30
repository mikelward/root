root
====

`root` lets you run commands as the root user.

It is designed as a simpler version of `sudo` that doesn't require
complex configuration and doesn't mess with your environment variables.

Installation
------------
To install, just run `make install` as root.

You will require GNU make and a C99 compiler such as GCC.

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
