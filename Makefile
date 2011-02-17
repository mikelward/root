PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man/man1

all: root

root:
	gcc -o root root.c

install:
	install -o root -g root -m 2755 root $(BINDIR)
	install -o root -g root -m 644 root.1 $(MANDIR)

