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


#include <stdlib.h>      /* exit(), srandom() */
#include <errno.h>       /* errno */
#include <string.h>      /* strerror() */
#include <stdio.h>       /* printf() etc. */
#include <sys/stat.h>    /* S_IRUSR, S_IWUSR, S_IRGRP, S_IROTH */
#include <fcntl.h>       /* open() */
#include <time.h>        /* clock_gettime() */

#include "export.h"
#include "mktorrent.h"
#include "init.h"
#include "hash.h"
#include "output.h"
#include "msg.h"
#include "ll.h"

#ifdef ALLINONE
/* include all .c files in alphabetical order */

#include "ftw.c"

#ifdef USE_PTHREADS
#include "hash_pthreads.c"
#else
#include "hash.c"
#endif

#include "init.c"
#include "ll.c"
#include "msg.c"
#include "output.c"

#ifndef USE_OPENSSL
#include "sha1.c"
#endif

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
 * and force is false
 */
static FILE *open_file(const char *path, int force)
{
	int fd;  /* file descriptor */
	FILE *f; /* file stream */

	int flags = O_WRONLY | O_BINARY | O_CREAT;
	if (!force)
		flags |= O_EXCL;

	/* open and create the file if it doesn't exist already */
	fd = open(path, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	FATAL_IF(fd < 0, "cannot create '%s': %s\n", path, strerror(errno));

	/* create the stream from this filedescriptor */
	f = fdopen(fd, "wb");
	FATAL_IF(f == NULL, "cannot create stream for '%s': %s\n",
		path, strerror(errno));

	return f;
}

/*
 * close the metainfo file
 */
static void close_file(FILE *f)
{
	/* close the metainfo file */
	FATAL_IF(fclose(f), "cannot close stream: %s\n", strerror(errno));
}

/*
 * main().. it starts
 */
int main(int argc, char *argv[])
{
	FILE *file;	/* stream for writing to the metainfo file */
	struct metafile m = {
		/* options */
		0,    /* piece_length, 0 by default indicates length should be calculated automatically */
		NULL, /* announce_list */
		NULL, /* torrent_name */
		NULL, /* metainfo_file_path */
		NULL, /* web_seed_url */
		NULL, /* comment */
		0,    /* target_is_directory  */
		0,    /* no_creation_date */
		0,    /* private */
		NULL, /* source string */
		0,    /* cross_seed */
		0,    /* verbose */
		0,    /* force_overwrite */
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

	/* seed PRNG with current time */
	struct timespec ts;
	FATAL_IF(clock_gettime(CLOCK_REALTIME, &ts) == -1,
		"failed to get time: %s\n", strerror(errno));
	srandom(ts.tv_nsec ^ ts.tv_sec);

	/* process options */
	init(&m, argc, argv);

	/* open the file stream now, so we don't have to abort
	   _after_ we did all the hashing in case we fail */
	file = open_file(m.metainfo_file_path, m.force_overwrite);

	/* calculate hash string... */
	unsigned char *hash = make_hash(&m);

	/* and write the metainfo to file */
	write_metainfo(file, &m, hash);

	/* close the file stream */
	close_file(file);

	/* free allocated memory */
	cleanup_metafile(&m);
	free(hash);

	/* yeih! everything seemed to go as planned */
	return EXIT_SUCCESS;
}
