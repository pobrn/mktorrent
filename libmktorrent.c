/*
This file is part of mktorrent
copy from main by pertershaw
Copyright (C) 2007, 2009 Emil Renner Berthing

mktorrent is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

mktorrent is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#include <stdlib.h>      /* exit() */
#include <sys/types.h>   /* off_t */
#include <errno.h>       /* errno */
#include <string.h>      /* strerror() */
#include <stdio.h>       /* printf() etc. */
#include <sys/stat.h>    /* S_IRUSR, S_IWUSR, S_IRGRP, S_IROTH */
#include <fcntl.h>       /* open() */

#ifdef ALLINONE
#include <sys/stat.h>
#include <unistd.h>      /* access(), read(), close(), getcwd(), sysconf() */
#ifdef USE_LONG_OPTIONS
#include <getopt.h>      /* getopt_long() */
#endif
#include <time.h>        /* time() */
#include <dirent.h>      /* opendir(), closedir(), readdir() etc. */
#ifdef USE_OPENSSL
#include <openssl/sha.h> /* SHA1(), SHA_DIGEST_LENGTH */
#else
#include <inttypes.h>
#endif
#ifdef USE_PTHREADS
#include <pthread.h>     /* pthread functions and data structures */
#endif

#define EXPORT static
#else  /* ALLINONE */

#define EXPORT
#endif /* ALLINONE */

#include "libmktorrent.h"

#ifdef ALLINONE
#include "ftw.c"
#include "init.c"

#ifndef USE_OPENSSL
#include "sha1.c"
#endif

#ifdef USE_PTHREADS
#include "hash_pthreads.c"
#else
#include "hash.c"
#endif

#include "output.c"
#else /* ALLINONE */
/* init.c */
extern void init(metafile_t *m, int argc, char *argv[]);
/* hash.c */
extern unsigned char *make_hash(metafile_t *m);
/* output.c */
extern void write_metainfo(FILE *f, metafile_t *m, unsigned char *hash_string);
#endif /* ALLINONE */

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef S_IRGRP
#define S_IRGRP 0
#endif
#ifndef S_IROTH
#define S_IROTH 0
#endif

/*
 * create and open the metainfo file for writing and create a stream for it
 * we don't want to overwrite anything, so abort if the file is already there
 */
static FILE *open_file(const char *path)
{
	int fd;  /* file descriptor */
	FILE *f; /* file stream */

	/* open and create the file if it doesn't exist already */
	fd = open(path, O_WRONLY | O_BINARY | O_CREAT | O_EXCL,
		       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd < 0) {
		fprintf(stderr, "Error creating '%s': %s\n",
				path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* create the stream from this filedescriptor */
	f = fdopen(fd, "wb");
	if (f == NULL) {
		fprintf(stderr,	"Error creating stream for '%s': %s\n",
				path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	return f;
}

/*
 * close the metainfo file
 */
static void close_file(FILE *f)
{
	/* close the metainfo file */
	if (fclose(f)) {
		fprintf(stderr, "Error closing stream: %s\n",
				strerror(errno));
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char *argv[])
{};

