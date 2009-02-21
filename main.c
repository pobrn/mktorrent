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

/* options.c */
extern char *process_options(int argc, char *argv[]);

/* dir.c */
extern void read_dir();

/* hash.c */
extern unsigned char *make_hash();

/* metafile.c */
extern void write_metafile(FILE * metafile, unsigned char *hash_string);

/* global variables */
/* options */
size_t piece_length = 256 * 1024;	/* 256kb by default */
char *announce_url = NULL;	/* announce url */
char *torrent_name = NULL;	/* name of the torrent (name of directory) */
char *metafile_path;		/* absolute path to the metafile we're creating */
char *comment = NULL;		/* optional comment to add to the metafile */
int verbose = 0;		/* be verbose */
int no_creation_date = 0;	/* don't write the creation date */

/* information calculated by read_dir() */
unsigned long long size = 0;	/* the combined size of all files in the torrent */
fl_node file_list = NULL;	/* linked list of files and their individual sizes */
unsigned int pieces;		/* number of pieces */


/*
 * create and open the metafile for writing and create a stream for it
 * we don't want to overwrite anything, so abort if the file is already there
 */
static FILE *open_metafile()
{
	int fd;			/* file descriptor */
	FILE *stream;		/* metafile stream */

	/* open and create the metafile if it doesn't exist already */
	if ((fd = open(metafile_path, O_WRONLY | O_CREAT | O_EXCL,
		       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
		fprintf(stderr, "error: couldn't create %s for writing, "
			"perhaps it is already there.\n", metafile_path);
		exit(1);
	}

	/* create the stream from this filedescriptor */
	if ((stream = fdopen(fd, "w")) == NULL) {
		fprintf(stderr,
			"error: couldn't create stream for file %s.",
			metafile_path);
		exit(1);
	}

	return stream;
}

/*
 * close the metafile
 */
static void close_metafile(FILE * metafile)
{
	/* close the metafile */
	if (fclose(metafile)) {
		fprintf(stderr, "error: couldn't close stream.");
		exit(1);
	}
}

/*
 * main().. it starts
 */
int main(int argc, char *argv[])
{
	char *dir;		/* directory for which we are creating a torrent */
	static FILE *metafile;	/* stream for writing to the metafile */

	/* print who we are */
	printf("mktorrent " VERSION " (c) 2007 Emil Renner Berthing\n\n");

	/* process the options and get the directory */
	dir = process_options(argc, argv);

	/* open the metafile stream now, so we don't have to abort
	 *after* we did all the hashing in case we fail */
	metafile = open_metafile();

	/* read the specified directory and initiate
	   size, file_list and pieces */
	read_dir(dir);

	/* calculate hash string and write the metafile */
	write_metafile(metafile, make_hash());

	/* close the metafile stream */
	close_metafile(metafile);

	/* yeih! everything seemed to go as planned */
	return 0;
}
