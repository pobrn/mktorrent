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
#include <sys/stat.h>		/* the stat structure */
#include <unistd.h>		/* getopt(), getcwd() */
#include <string.h>		/* strcmp(), strlen(), strncpy() */
#include <ftw.h>		/* ftw() */
#include <libgen.h>		/* basename() */
#ifndef NO_LONG_OPTIONS
#include <getopt.h>		/* getopt_long() */
#endif

#include "mktorrent.h"

#ifndef MAX_OPENFD
#define MAX_OPENFD 100		/* Maximum no. of file descriptors ftw will open */
#endif

/*
 * returns the absolute path to the metainfo file
 * if file_path is NULL use <torrent name>.torrent as file name
 */
static char *get_absolute_file_path(char *file_path, char *torrent_name)
{
	char *string;		/* string to return */
	size_t length = 32;	/* length of the string */

	/* if the file_path is already an absolute path just
	   return that */
	if (file_path && *file_path == '/')
		return file_path;

	/* first get the current working directory
	   using getcwd is a bit of a PITA */
	/* allocate initial string */
	string = malloc(length);
	/* while our allocated memory for the working dir isn't big enough */
	while (getcwd(string, length) == NULL) {
		/* double the buffer size */
		length *= 2;
		/* free our current buffer */
		free(string);
		/* and allocate a new one twice as big muahaha */
		string = malloc(length);
	}

	/* now set length to the proper length of the working dir */
	length = strlen(string);
	/* if the metainfo file path isn't set */
	if (metainfo_file_path == NULL) {
		/* append <torrent name>.torrent to the working dir */
		string =
		    realloc(string, length + strlen(torrent_name) + 10);
		sprintf(string + length, "/%s.torrent", torrent_name);
	} else {
		/* otherwise append the torrent path to the working dir */
		string =
		    realloc(string,
			    length + strlen(metainfo_file_path) + 2);
		sprintf(string + length, "/%s", metainfo_file_path);
	}

	/* return the string */
	return string;
}

/*
 * checks if target is a directory
 * sets the file_list and size if it isn't
 */
static int is_dir(char *target)
{
	struct stat s;		/* stat structure for stat() to fill */

	/* stat the target */
	if (stat(target, &s)) {
		fprintf(stderr, "error: cannot stat %s.\n", target);
		exit(EXIT_FAILURE);
	}

	/* if it is a directory, just return 1 */
	if (S_ISDIR(s.st_mode))
		return 1;

	/* if it isn't a regular file either, something is wrong.. */
	if (!S_ISREG(s.st_mode)) {
		fprintf(stderr,
			"error: %s isn't a directory og regular file.\n",
			target);
		exit(EXIT_FAILURE);
	}

	/* since we know the torrent is just a single file and we've
	   already stat'ed it, we might as well set the file list */
	file_list = malloc(sizeof(struct fl_node_s));
	file_list->path = target;
	file_list->size = s.st_size;
	file_list->next = NULL;
	/* ..and size global variables */
	size = s.st_size;

	/* now return 0 since it isn't a directory */
	return 0;
}

/*
 * called by ftw() on every file and directory in the subtree
 * counts the number of (readable) files, their commulative size and adds
 * their names and individual sizes to the file list
 */
static int process_node(const char *fpath, const struct stat *sb,
			int typeflag)
{
	const char *path;	/* path to file */
	fl_node *p;		/* pointer to a node in the file list */
	fl_node new_node;	/* place to store a newly created node */

	/* skip directories */
	if (typeflag == FTW_D)
		return 0;

	/* ignore the leading "./" */
	path = fpath + 2;

	/* now path should be a normal file and readable
	   otherwise display a warning and skip it */
	if (typeflag != FTW_F || access(path, R_OK)) {
		fprintf(stderr, "warning: cannot read %s!\n", path);
		return 0;
	}

	if (verbose)
		printf("Adding %s\n", path);

	/* count the total size of the files */
	size += sb->st_size;

	/* find where to insert the new node so that the file list
	   remains ordered by the path */
	p = &file_list;
	while (*p && strcmp(path, (*p)->path) > 0)
		p = &((*p)->next);

	/* create a new file list node for the file */
	new_node = malloc(sizeof(struct fl_node_s));
	new_node->path = strdup(path);
	new_node->size = sb->st_size;

	/* now insert the node there */
	new_node->next = *p;
	*p = new_node;

	/* insertion sort is a really stupid way of sorting a list,
	   but usually a torrent doesn't contain too many files,
	   so we'll probably be alright ;) */
	return 0;
}

/*
 * read the specified directory to create the file_list, count the size
 * of all the files and calculate the number of pieces
 */
static void read_dir(const char *dir)
{
	/* change to the specified directory */
	if (chdir(dir)) {
		fprintf(stderr, "error: cannot change directory to %s.\n",
			dir);
		exit(EXIT_FAILURE);
	}

	/* now process all the files in it
	   process_node() will take care of creating the file list
	   and counting the size of all the files */
	if (ftw("./", process_node, MAX_OPENFD)) {
		fprintf(stderr,
			"error: ftw() failed, this shouldn't happen.\n");
		exit(EXIT_FAILURE);
	}
}

/*
 * 'elp!
 */
static void print_help()
{
#ifndef NO_LONG_OPTIONS
	printf
	    ("Usage: mktorrent [OPTIONS] <target directory or filename>\n\n"
	     "Options:\n"
	     "-a, --announce=<url>    : specify the full announce url, required.\n"
	     "-c, --comment=<comment> : add an optional comment to the metainfo\n"
	     "-d, --no-date           : don't write the creation date\n"
	     "-h, --help              : show this help screen\n"
	     "-l, --piece-length=<n>  : set the piece length to 2^n bytes,\n"
	     "                          default is 18, that is 2^18 = 256kb.\n"
	     "-n, --name=<name>       : set the name of the torrent,\n"
	     "                          default is the basename of the target\n"
	     "-o, --output=<filename> : set the path and filename of the created file\n"
	     "                          default is <name>.torrent\n"
	     "-p, --private           : set the private flag\n"
	     "-v, --verbose           : be verbose\n"
	     "\nPlease send bug reports, patches, feature requests, praise and\n"
	     "general gossip about the program to: esmil@imf.au.dk\n");
#else
	printf
	    ("Usage: mktorrent [OPTIONS] <target directory or filename>\n\n"
	     "Options:\n"
	     "-a <url>       : specify the full announce url, required.\n"
	     "-c <comment>   : add an optional comment to the metainfo\n"
	     "-d             : don't write the creation date\n"
	     "-h             : show this help screen\n"
	     "-l <n>         : set the piece length to 2^n bytes,\n"
	     "                 default is 18, that is 2^18 = 256kb.\n"
	     "-n <name>      : set the name of the torrent,\n"
	     "                 default is the basename of the target\n"
	     "-o <filename>  : set the path and filename of the created file\n"
	     "                 default is <name>.torrent\n"
	     "-p             : set the private flag\n"
	     "-v             : be verbose\n"
	     "\nPlease send bug reports, patches, feature requests, praise and\n"
	     "general gossip about the program to: esmil@imf.au.dk\n");
#endif
}

/*
 * parse and check the command line options given
 * and initiate all the global variables
 */
void init(int argc, char *argv[])
{
	int c;			/* return value of getopt() */
#ifndef NO_LONG_OPTIONS
	/* the option structure to pass to getopt_long() */
	static struct option long_options[] = {
		{"announce", 1, NULL, 'a'},
		{"comment", 1, NULL, 'c'},
		{"no-date", 0, NULL, 'd'},
		{"help", 0, NULL, 'h'},
		{"piece-length", 1, NULL, 'l'},
		{"name", 1, NULL, 'n'},
		{"output", 1, NULL, 'o'},
		{"private", 0, NULL, 'p'},
		{"verbose", 0, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};
#endif

	/* now parse the command line options given */
#ifndef NO_LONG_OPTIONS
	while ((c = getopt_long(argc, argv, "a:c:dhl:n:o:pv",
				long_options, NULL)) != -1) {
#else
	while ((c = getopt(argc, argv, "a:c:dhl:n:o:pv")) != -1) {
#endif
		switch (c) {
		case 'a':
			announce_url = optarg;
			break;
		case 'c':
			comment = optarg;
			break;
		case 'd':
			no_creation_date = 1;
			break;
		case 'h':
			print_help();
			exit(EXIT_SUCCESS);
		case 'l':
			piece_length = atoi(optarg);
			break;
		case 'n':
			torrent_name = optarg;
			break;
		case 'o':
			metainfo_file_path = optarg;
			break;
		case 'p':
			private = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
			fprintf(stderr, "error: wrong arguments given, "
				"use -h for help.\n");
			exit(EXIT_FAILURE);
		}
	}

	/* set the correct piece length.
	   default is 2^18 = 256kb. */
	if (piece_length < 15 || piece_length > 21) {
		fprintf(stderr,
			"error: piece length must be a number between "
			"15 and 21.\n");
		exit(EXIT_FAILURE);
	}
	piece_length = 1 << piece_length;

	/* user must specify an announce url as it wouldn't make any sense
	   to have a default for this */
	if (announce_url == NULL) {
		fprintf(stderr, "error: must specify an announce url, "
			"use -h for help.\n");
		exit(EXIT_FAILURE);
	}

	/* ..and a file or directory from which to create the torrent */
	if (optind >= argc) {
		fprintf(stderr, "error: must specify the contents, "
			"use -h for help\n");
		exit(EXIT_FAILURE);
	}

	/* if the torrent name isn't set use the basename of the target */
	if (torrent_name == NULL)
		torrent_name = basename(argv[optind]);

	/* make sure metainfo_file_path is the absolute path to the file
	   if metainfo_file_path is not set use <torrent name>.torrent as
	   file name */
	metainfo_file_path =
	    get_absolute_file_path(metainfo_file_path, torrent_name);

	/* if we should be verbose print out all the options
	   as we have set them */
	if (verbose) {
		printf("Options:\n"
		       "  Announce URL: %s\n"
		       "  Torrent name: %s\n"
		       "  Metafile:     %s\n"
		       "  Piece length: %zu\n"
		       "  Be verbose:   yes\n",
		       announce_url, torrent_name, metainfo_file_path,
		       piece_length);

		printf("  Write date:   ");
		if (no_creation_date)
			printf("no\n");
		else
			printf("yes\n");

		printf("  Comment:      ");
		if (comment == NULL)
			printf("none\n\n");
		else
			printf("\"%s\"\n\n", comment);
	}


	/* check if target is a directory or just a single file */
	target_is_directory = is_dir(argv[optind]);
	if (target_is_directory)
		read_dir(argv[optind]);

	/* calculate the number of pieces
	   pieces = ceiling( size / piece_length ) */
	pieces = (size + piece_length - 1) / piece_length;

	/* now print the size and piece count if we should be verbose */
	if (verbose)
		printf("\n%llu bytes in all.\n"
		       "That's %u pieces of %zu bytes each.\n\n",
		       size, pieces, piece_length);
}
