PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man/man1
CC="gcc"
CFLAGS="-std=gnu99"

all: root

root: root.c
	$(CC) $(CFLAGS) -o root root.c

install:
	install -o root -g root -m 4755 root $(BINDIR)
	install -o root -g root -m 644 root.1 $(MANDIR)

clean:
	-rm *.o

clobber: clean
	-rm root

