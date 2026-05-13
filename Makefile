PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man/man1
CARGO=cargo

RUST_SOURCES = $(wildcard src/*.rs) Cargo.toml

all: test root

test:
	$(CARGO) test --release

root: $(RUST_SOURCES)
	$(CARGO) build --release
	cp target/release/root ./root

INSTALL_GROUP?=root

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
