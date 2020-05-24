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

#-------------Interesting variables for you to override :)--------------------

# Default settings shown
#CC      = cc
#CFLAGS  = -O2 -Wall -Wextra -Wpedantic
#LDFLAGS =
#INSTALL = install
#PREFIX  = /usr/local
#DESTDIR =

# Use multiple POSIX threads for calculating hashes. This should be slightly
# faster. Much faster on systems with multiple CPUs and fast harddrives.
#USE_PTHREADS = 1

# Use the SHA1 implementation in the OpenSSL library instead of compiling our
# own.
#USE_OPENSSL = 1

# Enable long options, started with two dashes.
#USE_LONG_OPTIONS = 1

# This is needed on certain 32bit OSes (notably 32bit Linux) to support
# files and torrents > 2Gb.
#USE_LARGE_FILES = 1

# Disable a redundant check to see if the amount of bytes read from files while
# hashing matches the sum of reported file sizes. I've never seen this fail. It
# will fail if you change files yet to be hashed while mktorrent is running,
# but who'd want to do that?
#NO_HASH_CHECK = 1

# Set the number of microseconds mktorrent will wait between every progress
# report when hashing multithreaded. Default is 200000, that is 0.2 seconds
# between every update.
#PROGRESS_PERIOD = 200000

# Maximum number of file descriptors mktorrent will open when scanning the
# directory. Default is 100, but I have no idea what a sane default for this
# value is, so your number is probably better.
#MAX_OPENFD = 100

# Enable leftover debugging code.  Usually just spams you with lots of useless
# information.
#DEBUG = 1

#-------------Nothing interesting below this line-----------------------------

program = mktorrent
version = 1.1

HEADERS  = mktorrent.h ll.h
SRCS     = ftw.c init.c sha1.c hash.c output.c main.c msg.c ll.c
