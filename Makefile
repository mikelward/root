PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man/man1
CARGO=cargo

# tools/cargo-remap keeps the build machine's paths out of the binary.
CARGO_REMAP = sh tools/cargo-remap $(CARGO)

RUST_SOURCES = $(wildcard src/*.rs) Cargo.toml tools/cargo-remap

all: test root

test:
	$(CARGO_REMAP) test --release
	sh tests/install_test.sh .
	sh tests/std_fds_test.sh target/release/root
	sh tests/setuid_test.sh target/release/root
	sh tests/cargo_remap_test.sh .
	sh tests/paths_test.sh .

# Depends on the Makefile too, so a change to the build flags rebuilds it.
root: $(RUST_SOURCES) Makefile
	$(CARGO_REMAP) build --release
	cp target/release/root ./root

# Group 0 by number: it's root on Linux, wheel on macOS and the BSDs, and
# root's own check is for group 0 (SPEC.md), whatever it's called.
INSTALL_GROUP?=0

install: root
	install -d $(BINDIR)
	install -o root -g $(INSTALL_GROUP) -m 4755 root $(BINDIR)
	# Work around uutils install stripping setuid: https://github.com/uutils/coreutils/issues/9134
	chmod 4755 $(BINDIR)/root
	install -d $(MANDIR)
	install -o root -g $(INSTALL_GROUP) -m 644 root.1 $(MANDIR)

clean:
	$(CARGO) clean
	-rm -f root

clobber: clean
	-rm -rf target

.PHONY: all test install clean clobber
