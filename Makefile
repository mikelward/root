PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man/man1
INSTALL_GROUP ?= root

GO ?= go

all: test root

root: $(wildcard *.go) go.mod
	$(GO) build -trimpath -o root .

test:
	$(GO) test ./...

install: root
	install -d $(BINDIR)
	install -o root -g $(INSTALL_GROUP) -m 4755 root $(BINDIR)
	# Work around uutils install stripping setuid: https://github.com/uutils/coreutils/issues/9134
	chmod 4755 $(BINDIR)/root
	install -d $(MANDIR)
	install -o root -g $(INSTALL_GROUP) -m 644 root.1 $(MANDIR)

clean:
	-rm -f root

.PHONY: all test install clean
