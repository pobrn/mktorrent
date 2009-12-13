/*
This file is part of mktorrent
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

#include "mktorrent.h"

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

/*
 * main().. it starts
 */
int main(int argc, char *argv[])
{
	FILE *file;	/* stream for writing to the metainfo file */
	metafile_t m = {
		/* options */
		18,   /* piece_length, 2^18 = 256kb by default */
		NULL, /* announce_list */
		NULL, /* torrent_name */
		NULL, /* metainfo_file_path */
		NULL, /* web_seed_url */
		NULL, /* comment */
		0,    /* target_is_directory  */
		0,    /* no_creation_date */
		0,    /* private */
		0,    /* verbose */
#ifdef USE_PTHREADS
		0,    /* threads, initialised by init() */
#endif

		/* information calculated by read_dir() */
		0,    /* size */
		NULL, /* file_list */
		0     /* pieces */
	};

	/* print who we are */
	printf("mktorrent " VERSION " (c) 2007, 2009 Emil Renner Berthing\n\n");

	/* process options */
	init(&m, argc, argv);

	/* open the file stream now, so we don't have to abort
	   _after_ we did all the hashing in case we fail */
	file = open_file(m.metainfo_file_path);

	/* calculate hash string and write the metainfo to file */
	write_metainfo(file, &m, make_hash(&m));

	/* close the file stream */
	close_file(file);

	/* yeih! everything seemed to go as planned */
	return EXIT_SUCCESS;
}
