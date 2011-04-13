PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man/man1
CC=gcc
CFLAGS=-std=gnu99 -Wall -Werror

all: root

root: root.c user.o path.o logging.o
	$(CC) $(CFLAGS) -o $@ $^

install:
	install -d $(BINDIR)
	install -o root -g root -m 4755 root $(BINDIR)
	install -d $(MANDIR)
	install -o root -g root -m 644 root.1 $(MANDIR)

clean:
	-rm *.o

clobber: clean
	-rm root

