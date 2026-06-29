# Legacy C build of `root`.
#
# This is a fallback for machines that don't have a Rust toolchain (cargo +
# rustc). The Rust version in the repository root is the primary, supported
# implementation; build that with `make` from the top level when you can.
#
# From this directory:
#   make            # build and run the unit tests, then build ./root
#   make install    # install the C-built binary and the shared man page
#
# Only a C99 compiler and GNU make are required.

PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man/man1
CC=cc
CFLAGS=-std=c99 -Wall -Werror

# The man page is shared with the Rust build and lives at the repo root.
MANPAGE=../root.1

all: test root

test: loggingtest pathtest argstest

loggingtest: loggingtest.o logging.o
	$(CC) $(LDFLAGS) -o $@ loggingtest.o logging.o
	./$@

pathtest: pathtest.o path.o logging.o
	$(CC) $(LDFLAGS) -o $@ pathtest.o path.o logging.o
	./$@

argstest: argstest.o args.o
	$(CC) $(LDFLAGS) -o $@ argstest.o args.o
	./$@

root: root.o user.o path.o logging.o args.o
	$(CC) $(LDFLAGS) -o $@ root.o user.o path.o logging.o args.o

# Header dependencies
root.o: root.h logging.h path.h user.h args.h
user.o: user.h root.h logging.h
path.o: path.h root.h logging.h
logging.o: logging.h
args.o: args.h
loggingtest.o: logging.h
pathtest.o: path.h
argstest.o: args.h

INSTALL_GROUP?=root

install: root
	install -d $(BINDIR)
	install -o root -g $(INSTALL_GROUP) -m 4755 root $(BINDIR)
	# Work around uutils install stripping setuid: https://github.com/uutils/coreutils/issues/9134
	chmod 4755 $(BINDIR)/root
	install -d $(MANDIR)
	install -o root -g $(INSTALL_GROUP) -m 644 $(MANPAGE) $(MANDIR)

clean:
	-rm -f *.o

clobber: clean
	-rm -f root loggingtest pathtest argstest

.PHONY: all test install clean clobber
