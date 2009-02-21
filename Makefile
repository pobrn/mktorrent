# This file is part of mktorrent
# Copyright (C) 2007  Emil Renner Berthing
#
# mktorrent is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# mktorrent is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

#-------------Interesting variables for you to override :)--------------------

CC	?= cc
CFLAGS  ?= -O2 -Wall
INSTALL ?= install
PREFIX  ?= /usr/local

# Disable multi threading. This will make the program smaller and a tiny
# bit easier on the system. It will make it slower in real time though,
# as we can't read from the harddisk and calculate hashes simultaneously.
#NO_THREADS = 1

# Disable support for long options. Will make the program smaller
# and perhaps even more portable.
#NO_LONG_OPTIONS = 1

# Disable a redundent check to see if the amount of bytes read from files
# when calculating their hash strings matches the sum of reported file sizes.
# I've never seen this fail. It will fail if you start deleting or altering
# files yet to be hashed while mktorrent is running, but who'd want to do that?
#NO_HASH_CHECK = 1

# Set the number of micro seconds mktorrent will wait between every progress
# report when hashing multi threaded. Default is 200000 that is 0.2 seconds
# between every update.
#PROGRESS_PERIOD = 200000

# Maximum number of file descriptors mktorrent will open when scanning the
# directory. Default is 100, but I have no idea what a sane default
# for this value is, so your number is probably better ;)
#MAX_OPENFD = 100

# Don't strip the binary after its compiled
#DONT_STRIP = 1

# Enable leftover debugging code.
# Usually just spams you with lots of useless information.
#DEBUG = 1


#-------------Nothing interesting below this line-----------------------------

program := mktorrent
version := 0.4

HEADERS  = mktorrent.h
SRCS    := init.c hash.c output.c main.c
OBJS     = $(SRCS:.c=.o)
LIBS    := -lssl

ifdef NO_THREADS
DEFINES += -DNO_THREADS
SRCS := $(SRCS:hash.c=simple_hash.c)
else
LIBS += -lpthread
endif

ifdef NO_LONG_OPTIONS
DEFINES += -DNO_LONG_OPTIONS
endif

ifdef NO_HASH_CHECK
DEFINES += -DNO_HASH_CHECK
endif

ifdef DEBUG
DEFINES += -DDEBUG
endif

override DEFINES += -DVERSION="\"$(version)\""
override CFLAGS  += $(DEFINES)
override LDFLAGS += $(LIBS)

.PHONY: all indent clean install uninstall

all: $(program)

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

$(program): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $(program) $(LDFLAGS)
ifndef DONT_STRIP
	strip $(program)
endif

allinone: $(SRCS) $(HEADERS)
	cat $(SRCS) | sed "s/#include .*//" | cat $(HEADERS) - | sed "s/^extern //;s/^static //" > allinone.c
	$(CC) $(CFLAGS) -DALLINONE allinone.c -o $(program) $(LDFLAGS)
	rm -f allinone.c
ifndef DONT_STRIP
	strip $(program)
endif

indent:
	indent -kr -i8 *.c *.h

clean:
	rm -f $(program) *.o *.c~ *.h~

install: $(program)
	$(INSTALL) -m755 -D $(program) $(DESTDIR)$(PREFIX)/bin/$(program)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(program)
