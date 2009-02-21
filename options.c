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
#include <unistd.h>		/* getopt(), getcwd() */
#include <string.h>		/* strlen(), strncpy() */
#include <getopt.h>		/* getopt_long() */
#include <libgen.h>		/* basename() */

#include "mktorrent.h"

/*
 * returns the absolute path to the metafile
 * if metafile_path is NULL use <torrent name>.torrent as file name
 */
static char *get_absolute_metafile_path(char *metafile_path,
					char *torrent_name)
{
	char *string;		/* string to return */
	size_t length = 32;	/* length of the string */

	/* if the metafile_path is already an absolute path just
	   return that */
	if (metafile_path && *metafile_path == '/')
		return metafile_path;

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
	/* if the metafile path isn't set */
	if (metafile_path == NULL) {
		/* append <torrent name>.torrent to the working dir */
		string =
		    realloc(string, length + strlen(torrent_name) + 10);
		sprintf(string + length, "/%s.torrent", torrent_name);
	} else {
		/* otherwise append the torrent path to the working dir */
		string =
		    realloc(string, length + strlen(metafile_path) + 2);
		sprintf(string + length, "/%s", metafile_path);
	}

	/* return the string */
	return string;
}

/*
 * 'elp!
 */
static void print_help()
{
	printf("Usage: mktorrent [OPTIONS] directory\n\n"
	       "Options:\n"
	       "-a, --announce=<url>        : specify the announce url\n"
	       "-c, --comment=<comment>     : add an optional comment to the metafile\n"
	       "-d, --no-date               : don't write the creation date\n"
	       "-h, --help                  : show this help screen\n"
	       "-n, --name=<name>           : set the name of the torrent,\n"
	       "                              default is the directory (base-)name\n"
	       "-o, --output=<filename>     : set the filename of the created metafile,\n"
	       "                              default: <name>.torrent\n"
	       "-p, --piece-length=<length> : set the piece length in bytes,\n"
	       "                              default: 262144\n"
	       "-v, --verbose               : be verbose\n"
	       "\nPlease send bug reports, patches, feature requests, praise and\n"
	       "general gossip about the program to: esmil@imf.au.dk\n");
}

/*
 * parse and check the command line options given
 */
char *process_options(int argc, char *argv[])
{
	int c;			/* return value of getopt() */
	/* the option structure to pass to getopt_long() */
	static struct option long_options[] = {
		{"announce", 1, NULL, 'a'},
		{"comment", 1, NULL, 'c'},
		{"no-date", 0, NULL, 'd'},
		{"help", 1, NULL, 'h'},
		{"name", 1, NULL, 'n'},
		{"output", 1, NULL, 'o'},
		{"piece-length", 1, NULL, 'p'},
		{"verbose", 0, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};

	/* now parse the command line options given */
	while ((c = getopt_long(argc, argv, "a:c:dhn:o:p:v",
				long_options, NULL)) != -1) {
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
			exit(0);
		case 'n':
			torrent_name = optarg;
			break;
		case 'o':
			metafile_path = optarg;
			break;
		case 'p':
			piece_length = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
			fprintf(stderr, "error: wrong arguments given, "
				"use -h for help.\n");
			exit(1);
		}
	}

	/* user must specify an announce url as it wouldn't make any sense
	   to have a default for this */
	if (announce_url == NULL) {
		fprintf(stderr, "error: must specify an announce url, "
			"use -h for help.\n");
		exit(1);
	}

	/* ..and a directory from which to create the torrent */
	if (optind >= argc) {
		fprintf(stderr, "error: must specify a directory, "
			"use -h for help\n");
		exit(1);
	}

	/* if the torrent name isn't set use the basename of the directory */
	if (torrent_name == NULL)
		torrent_name = basename(argv[optind]);

	/* make sure metafile_path is the absolute path to the metafile
	   if metafile_path is not set use <torrent name>.torrent as
	   file name */
	metafile_path =
	    get_absolute_metafile_path(metafile_path, torrent_name);

	/* if we should be verbose print out all the options
	   as we have set them */
	if (verbose) {
		printf("Options:\n"
		       "  Announce URL: %s\n"
		       "  Torrent name: %s\n"
		       "  Metafile:     %s\n"
		       "  Piece length: %zu\n"
		       "  Be verbose:   yes\n",
		       announce_url, torrent_name, metafile_path,
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

	/* now return the directory name */
	return argv[optind];
}
