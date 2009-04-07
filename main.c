/*
This file is part of mktorrent
Copyright (C) 2007  Emil Renner Berthing

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

#include <stdlib.h>		/* exit() */
#include <errno.h>		/* errno */
#include <string.h>		/* strerror() */
#include <stdio.h>		/* printf() etc. */
#include <sys/stat.h>		/* S_IRUSR, S_IWUSR, S_IRGRP, S_IROTH */
#include <fcntl.h>		/* open() */

#ifdef ALLINONE

#include <unistd.h>		/* access(), read(), close(), getcwd() */
#include <getopt.h>		/* getopt_long() */
#include <libgen.h>		/* basename() */
#include <fcntl.h>		/* open() */
#include <ftw.h>		/* ftw() */
#include <time.h>		/* time() */
#include <openssl/sha.h>	/* SHA1(), SHA_DIGEST_LENGTH */
#ifndef NO_THREADS
#include <pthread.h>		/* pthread functions and data structures */
#endif /* NO_THREADS */

#define EXPORT static
#else
#define EXPORT
#endif /* ALLINONE */

#include "mktorrent.h"

/* global variables */
/* options */
EXPORT size_t piece_length = 18;        /* 2^18 = 256kb by default */
EXPORT al_node announce_list = NULL;    /* announce URLs */
EXPORT const char *torrent_name = NULL; /* name of the torrent (name of directory) */
EXPORT char *metainfo_file_path;        /* absolute path to the metainfo file */
EXPORT char *web_seed_url = NULL;       /* web seed URL */
EXPORT char *comment = NULL;            /* optional comment to add to the metainfo */
EXPORT int target_is_directory = 0;     /* target is a directory
                                           not just a single file */
EXPORT int no_creation_date = 0;        /* don't write the creation date */
EXPORT int private = 0;                 /* set the private flag */
EXPORT int verbose = 0;                 /* be verbose */

/* information calculated by read_dir() */
EXPORT unsigned long long size = 0;	/* the combined size of all files
					   in the torrent */
EXPORT fl_node file_list = NULL;	/* linked list of files and
					   their individual sizes */
EXPORT unsigned int pieces;		/* number of pieces */

#ifdef ALLINONE
#include "init.c"

#ifdef NO_THREADS
#include "simple_hash.c"
#else
#include "hash.c"
#endif /* NO_THREADS */

#include "output.c"
#else
/* init.c */
extern void init(int argc, char *argv[]);
/* hash.c */
extern unsigned char *make_hash();
/* output.c */
extern void write_metainfo(FILE * file, unsigned char *hash_string);
#endif /* ALLINONE */

/*
 * create and open the metainfo file for writing and create a stream for it
 * we don't want to overwrite anything, so abort if the file is already there
 */
static FILE *open_file()
{
	int fd;			/* file descriptor */
	FILE *f;		/* file stream */

	/* open and create the file if it doesn't exist already */
	fd = open(metainfo_file_path, O_WRONLY | O_CREAT | O_EXCL,
		       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd < 0) {
		fprintf(stderr, "Error creating '%s': %s\n",
				metainfo_file_path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* create the stream from this filedescriptor */
	f = fdopen(fd, "w");
	if (f == NULL) {
		fprintf(stderr,	"Error creating stream for '%s': %s\n",
				metainfo_file_path, strerror(errno));
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
	static FILE *file;	/* stream for writing to the metainfo file */

	/* print who we are */
	printf("mktorrent " VERSION " (c) 2007, 2009 Emil Renner Berthing\n\n");

	/* process options and initiate global variables */
	init(argc, argv);

	/* open the file stream now, so we don't have to abort
	   _after_ we did all the hashing in case we fail */
	file = open_file();

	/* calculate hash string and write the metainfo to file */
	write_metainfo(file, make_hash());

	/* close the file stream */
	close_file(file);

	/* yeih! everything seemed to go as planned */
	return EXIT_SUCCESS;
}
