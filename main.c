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
#include <stdio.h>		/* printf() etc. */
#include <unistd.h>		/* unlink() */
#include <sys/stat.h>		/* S_IRUSR, S_IWUSR, S_IRGRP, S_IROTH */
#include <fcntl.h>		/* open() */

#include "mktorrent.h"

/* global variables */
/* options */
size_t piece_length = 18;	/* 2^18 = 256kb by default */
char *announce_url = NULL;	/* announce url */
char *torrent_name = NULL;	/* name of the torrent (name of directory) */
char *metainfo_file_path;	/* absolute path to the metainfo file */
char *comment = NULL;		/* optional comment to add to the metainfo */
int target_is_directory = 0;	/* target is a directory not just a single file */
int no_creation_date = 0;	/* don't write the creation date */
int private = 0;		/* set the private flag */
int verbose = 0;		/* be verbose */

/* information calculated by read_dir() */
unsigned long long size = 0;	/* the combined size of all files in the torrent */
fl_node file_list = NULL;	/* linked list of files and their individual sizes */
unsigned int pieces;		/* number of pieces */

/* init.c */
extern void init(int argc, char *argv[]);

/* hash.c */
extern unsigned char *make_hash();

/* output.c */
extern void write_metainfo(FILE * file, unsigned char *hash_string);

/*
 * create and open the metainfo file for writing and create a stream for it
 * we don't want to overwrite anything, so abort if the file is already there
 */
static FILE *open_file()
{
	int fd;			/* file descriptor */
	FILE *stream;		/* file stream */

	/* open and create the file if it doesn't exist already */
	if ((fd = open(metainfo_file_path, O_WRONLY | O_CREAT | O_EXCL,
		       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
		fprintf(stderr, "error: couldn't create %s for writing, "
			"perhaps it is already there.\n",
			metainfo_file_path);
		exit(EXIT_FAILURE);
	}

	/* create the stream from this filedescriptor */
	if ((stream = fdopen(fd, "w")) == NULL) {
		fprintf(stderr,
			"error: couldn't create stream for file %s.",
			metainfo_file_path);
		exit(EXIT_FAILURE);
	}

	return stream;
}

/*
 * close the metainfo file
 */
static void close_file(FILE * file)
{
	/* close the metainfo file */
	if (fclose(file)) {
		fprintf(stderr, "error: couldn't close stream.");
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
	printf("mktorrent " VERSION " (c) 2007 Emil Renner Berthing\n\n");

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
