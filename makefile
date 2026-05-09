.POSIX: # POSIX Makefile, use make,gmake,pdpmake,bmake
.SUFFIXES:
.PHONY: all clean install check

PROJECT    =c-xtimeout
VERSION    =1.0.0
CC         =$(TARGET_PREFIX)cc
CFLAGS     =-Wall -g3 -std=c99
PREFIX     =/usr/local
TOOLCHAINS =noarch
DESTDIR    =
PROGS      =xtimeout

all: $(PROGS)
clean:
	rm -f $(PROGS)
install:
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m755 $(PROGS) $(DESTDIR)$(PREFIX)/bin
check:


xtimeout: xtimeout.c
	$(CC) -o $@ xtimeout.c $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LIBS)
