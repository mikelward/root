PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man/man1
CC=cc
CFLAGS=-std=c99 -Wall -Werror

all: tags test root

tags: *.c *.h
	ctags -f $@ *.c *.h

test: loggingtest pathtest

loggingtest: loggingtest.o logging.o
	$(CC) $(LDFLAGS) -o $@ loggingtest.o logging.o
	./$@

pathtest: pathtest.o path.o logging.o
	$(CC) $(LDFLAGS) -o $@ pathtest.o path.o logging.o
	./$@

root: root.o user.o path.o logging.o
	$(CC) $(LDFLAGS) -o $@ root.o user.o path.o logging.o

INSTALL_GROUP?=root

install:
	install -d $(BINDIR)
	install -o root -g $(INSTALL_GROUP) -m 4755 root $(BINDIR)
	install -d $(MANDIR)
	install -o root -g $(INSTALL_GROUP) -m 644 root.1 $(MANDIR)

clean:
	-rm *.o

clobber: clean
	-rm root loggingtest pathtest

