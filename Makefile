PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man/man1
CC=gcc
CFLAGS=-std=gnu99 -Wall -Werror

all: tags test root

tags: *.c *.h
	ctags -f $@ *.c *.h

test: loggingtest

loggingtest: loggingtest.o logging.o
	$(CC) $(LDFLAGS) -o $@ loggingtest.o logging.o
	./$@

root: root.o user.o path.o logging.o
	$(CC) $(LDFLAGS) -o $@ root.o user.o path.o logging.o

install:
	install -d $(BINDIR)
	install -o root -g root -m 4755 root $(BINDIR)
	install -d $(MANDIR)
	install -o root -g root -m 644 root.1 $(MANDIR)

clean:
	-rm *.o

clobber: clean
	-rm root

