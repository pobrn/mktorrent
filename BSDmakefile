# This file is part of mktorrent
# Copyright (C) 2007, 2009 Emil Renner Berthing
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

.include "Makefile"

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -Wpedantic
INSTALL ?= install
PREFIX  ?= /usr/local

.ifdef USE_PTHREADS
DEFINES += -DUSE_PTHREADS
SRCS := $(SRCS:hash.c=hash_pthreads.c)
LIBS += -lpthread
.endif

.ifdef USE_OPENSSL
DEFINES += -DUSE_OPENSSL
SRCS := $(SRCS:sha1.c=)
LIBS += -lcrypto
.endif

.ifdef USE_LONG_OPTIONS
DEFINES += -DUSE_LONG_OPTIONS
.endif

.ifdef USE_LARGE_FILES
DEFINES += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
.endif

.ifdef NO_HASH_CHECK
DEFINES += -DNO_HASH_CHECK
.endif

.ifdef MAX_OPENFD
DEFINES += -DMAX_OPENFD="$(MAX_OPENFD)"
.endif

.ifdef DEBUG
DEFINES += -DDEBUG
.endif

OBJS = $(SRCS:.c=.o)

all: $(program)

.SUFFIXES: .o .c
.c.o:
	$(CC) $(CFLAGS) $(DEFINES) -DVERSION="\"$(version)\"" -c $(.IMPSRC)

$(OBJS): $(HEADERS)

.include "rules.mk"
