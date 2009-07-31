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
#ifndef ALLINONE
#include <stdlib.h>       /* exit() */
#include <sys/types.h>    /* off_t */
#include <errno.h>        /* errno */
#include <string.h>       /* strerror() */
#include <stdio.h>        /* printf() etc. */
#include <sys/stat.h>     /* the stat structure */
#include <unistd.h>       /* getopt(), getcwd() */
#include <string.h>       /* strcmp(), strlen(), strncpy() */
#ifdef USE_LONG_OPTIONS
#include <getopt.h>       /* getopt_long() */
#endif

#include "mktorrent.h"
#include "ftw.h"

#define EXPORT
#endif /* ALLINONE */

#ifndef MAX_OPENFD
#define MAX_OPENFD 100	/* Maximum number of file descriptors
			   file_tree_walk() will open */
#endif

static void strip_ending_dirseps(char *s)
{
	char *end = s;

	while (*end)
		end++;

	while (end > s && *(--end) == DIRSEP_CHAR)
		*end = '\0';
}

static const char *basename(const char *s)
{
	const char *r = s;

	while (*s != '\0') {
		if (*s == DIRSEP_CHAR)
			r = ++s;
		else
			++s;
	}

	return r;
}

static void set_absolute_file_path(metafile_t *m)
{
	char *string;		/* string to return */
	size_t length = 32;	/* length of the string */

	/* if the file_path is already an absolute path just
	   return that */
	if (m->metainfo_file_path && *m->metainfo_file_path == DIRSEP_CHAR)
		return;

	/* first get the current working directory
	   using getcwd is a bit of a PITA */
	/* allocate initial string */
	string = malloc(length);
	if (string == NULL) {
		fprintf(stderr, "Out of memory.\n");
		exit(EXIT_FAILURE);
	}
	/* while our allocated memory for the working dir isn't big enough */
	while (getcwd(string, length) == NULL) {
		/* double the buffer size */
		length *= 2;
		/* free our current buffer */
		free(string);
		/* and allocate a new one twice as big muahaha */
		string = malloc(length);
		if (string == NULL) {
			fprintf(stderr, "Out of memory.\n");
			exit(EXIT_FAILURE);
		}
	}

	/* now set length to the proper length of the working dir */
	length = strlen(string);
	/* if the metainfo file path isn't set */
	if (m->metainfo_file_path == NULL) {
		/* append <torrent name>.torrent to the working dir */
		string =
		    realloc(string, length + strlen(m->torrent_name) + 10);
		if (string == NULL) {
			fprintf(stderr, "Out of memory.\n");
			exit(EXIT_FAILURE);
		}
		sprintf(string + length, DIRSEP "%s.torrent", m->torrent_name);
	} else {
		/* otherwise append the torrent path to the working dir */
		string =
		    realloc(string,
			    length + strlen(m->metainfo_file_path) + 2);
		if (string == NULL) {
			fprintf(stderr, "Out of memory.\n");
			exit(EXIT_FAILURE);
		}
		sprintf(string + length, DIRSEP "%s", m->metainfo_file_path);
	}

	m->metainfo_file_path = string;
}

/*
 * parse a comma separated list of strings <str>[,<str>]* and
 * return a string list containing the substrings
 */
static slist_t *get_slist(char *s)
{
	slist_t *list, *last;
	char *e;

	/* allocate memory for the first node in the list */
	list = last = malloc(sizeof(slist_t));
	if (list == NULL) {
		fprintf(stderr, "Out of memory.\n");
		exit(EXIT_FAILURE);
	}

	/* add URLs to the list while there are commas in the string */
	while ((e = strchr(s, ','))) {
		/* set the commas to \0 so the URLs appear as
		 * separate strings */
		*e = '\0';
		last->s = s;

		/* move s to point to the next URL */
		s = e + 1;

		/* append another node to the list */
		last->next = malloc(sizeof(slist_t));
		last = last->next;
		if (last == NULL) {
			fprintf(stderr, "Out of memory.\n");
			exit(EXIT_FAILURE);
		}
	}

	/* set the last string in the list */
	last->s = s;
	last->next = NULL;

	/* return the list */
	return list;
}

/*
 * checks if target is a directory
 * sets the file_list and size if it isn't
 */
static int is_dir(metafile_t *m, char *target)
{
	struct stat s;		/* stat structure for stat() to fill */

	/* stat the target */
	if (stat(target, &s)) {
		fprintf(stderr, "Error stat'ing '%s': %s\n",
				target, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* if it is a directory, just return 1 */
	if (S_ISDIR(s.st_mode))
		return 1;

	/* if it isn't a regular file either, something is wrong.. */
	if (!S_ISREG(s.st_mode)) {
		fprintf(stderr,
			"'%s' is neither a directory nor regular file.\n",
				target);
		exit(EXIT_FAILURE);
	}

	/* since we know the torrent is just a single file and we've
	   already stat'ed it, we might as well set the file list */
	m->file_list = malloc(sizeof(flist_t));
	if (m->file_list == NULL) {
		fprintf(stderr, "Out of memory.\n");
		exit(EXIT_FAILURE);
	}
	m->file_list->path = target;
	m->file_list->size = s.st_size;
	m->file_list->next = NULL;
	/* ..and size variable */
	m->size = s.st_size;

	/* now return 0 since it isn't a directory */
	return 0;
}

/*
 * called by file_tree_walk() on every file and directory in the subtree
 * counts the number of (readable) files, their commulative size and adds
 * their names and individual sizes to the file list
 */
static int process_node(const char *path, const struct stat *sb, void *data)
{
	flist_t **p;            /* pointer to a node in the file list */
	flist_t *new_node;      /* place to store a newly created node */
	metafile_t *m = data;

	/* skip non-regular files */
	if (!S_ISREG(sb->st_mode))
		return 0;

	/* ignore the leading "./" */
	path += 2;

	/* now path should be readable otherwise
	 * display a warning and skip it */
	if (access(path, R_OK)) {
		fprintf(stderr, "Warning: Cannot read '%s', skipping.\n", path);
		return 0;
	}

	if (m->verbose)
		printf("Adding %s\n", path);

	/* count the total size of the files */
	m->size += sb->st_size;

	/* find where to insert the new node so that the file list
	   remains ordered by the path */
	p = &m->file_list;
	while (*p && strcmp(path, (*p)->path) > 0)
		p = &((*p)->next);

	/* create a new file list node for the file */
	new_node = malloc(sizeof(flist_t));
	if (new_node == NULL ||
			(new_node->path = strdup(path)) == NULL) {
		fprintf(stderr, "Out of memory.\n");
		return -1;
	}
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
 * 'elp!
 */
static void print_help()
{
	printf(
	  "Usage: mktorrent [OPTIONS] <target directory or filename>\n\n"
	  "Options:\n"
#ifdef USE_LONG_OPTIONS
	  "-a, --announce=<url>[,<url>]* : specify the full announce URLs\n"
	  "                                at least one is required\n"
	  "                                additional -a adds backup trackers\n"
	  "-c, --comment=<comment>       : add a comment to the metainfo\n"
	  "-d, --no-date                 : don't write the creation date\n"
	  "-h, --help                    : show this help screen\n"
	  "-l, --piece-length=<n>        : set the piece length to 2^n bytes,\n"
	  "                                default is 18, that is 2^18 = 256kb\n"
	  "-n, --name=<name>             : set the name of the torrent\n"
	  "                                default is the basename of the target\n"
	  "-o, --output=<filename>       : set the path and filename of the created file\n"
	  "                                default is <name>.torrent\n"
	  "-p, --private                 : set the private flag\n"
#ifdef USE_PTHREADS
	  "-t, --threads=<n>             : use <n> threads for calculating hashes\n"
	  "                                default is 2\n"
#endif
	  "-v, --verbose                 : be verbose\n"
	  "-w, --web-seed=<url>[,<url>]* : add web seed URLs\n"
	  "                                additional -w adds more URLs\n"
#else
	  "-a <url>[,<url>]* : specify the full announce URLs\n"
	  "                    at least one is required\n"
	  "                    additional -a adds backup trackers\n"
	  "-c <comment>      : add a comment to the metainfo\n"
	  "-d                : don't write the creation date\n"
	  "-h                : show this help screen\n"
	  "-l <n>            : set the piece length to 2^n bytes,\n"
	  "                    default is 18, that is 2^18 = 256kb\n"
	  "-n <name>         : set the name of the torrent,\n"
	  "                    default is the basename of the target\n"
	  "-o <filename>     : set the path and filename of the created file\n"
	  "                    default is <name>.torrent\n"
	  "-p                : set the private flag\n"
#ifdef USE_PTHREADS
	  "-t <n>            : use <n> threads for calculating hashes\n"
	  "                    default is 2\n"
#endif
	  "-v                : be verbose\n"
	  "-w <url>[,<url>]* : add web seed URLs\n"
	  "                    additional -w adds more URLs\n"
#endif
	  "\nPlease send bug reports, patches, feature requests, praise and\n"
	  "general gossip about the program to: esmil@users.sourceforge.net\n");
}

/*
 * print the full announce list
 */
static void print_announce_list(llist_t *list)
{
	unsigned int n;

	for (n = 1; list; list = list->next, n++) {
		slist_t *l = list->l;

		printf("    %u : %s\n", n, l->s);
		for (l = l->next; l; l = l->next)
			printf("        %s\n", l->s);
	}
}

/*
 * print the list of web seed URLs
 */
static void print_web_seed_list(slist_t *list)
{
	printf("  Web Seed URL: ");

	if (list == NULL) {
		printf("none\n");
		return;
	}

	printf("%s\n", list->s);
	for (list = list->next; list; list = list->next)
		printf("                %s\n", list->s);
}

/*
 * print out all the options
 */
static void dump_options(metafile_t *m)
{
	printf("Options:\n"
	       "  Announce URLs:\n");

	print_announce_list(m->announce_list);

	printf("  Torrent name: %s\n"
	       "  Metafile:     %s\n"
	       "  Piece length: %u\n"
	       "  Be verbose:   yes\n",
	       m->torrent_name, m->metainfo_file_path, m->piece_length);

	printf("  Write date:   ");
	if (m->no_creation_date)
		printf("no\n");
	else
		printf("yes\n");

	print_web_seed_list(m->web_seed_list);

	printf("  Comment:      ");
	if (m->comment == NULL)
		printf("none\n\n");
	else
		printf("\"%s\"\n\n", m->comment);
}

/*
 * parse and check the command line options given
 * and fill out the appropriate fields of the
 * metafile structure
 */
EXPORT void init(metafile_t *m, int argc, char *argv[])
{
	int c;			/* return value of getopt() */
	llist_t *announce_last = NULL;
	slist_t *web_seed_last = NULL;
#ifdef USE_LONG_OPTIONS
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
#ifdef USE_PTHREADS
		{"threads", 1, NULL, 't'},
#endif
		{"verbose", 0, NULL, 'v'},
		{"web-seed", 1, NULL, 'w'},
		{NULL, 0, NULL, 0}
	};
#endif

	/* now parse the command line options given */
#ifdef USE_PTHREADS
#define OPT_STRING "a:c:dhl:n:o:pt:vw:"
#else
#define OPT_STRING "a:c:dhl:n:o:pvw:"
#endif
#ifdef USE_LONG_OPTIONS
	while ((c = getopt_long(argc, argv, OPT_STRING,
				long_options, NULL)) != -1) {
#else
	while ((c = getopt(argc, argv, OPT_STRING)) != -1) {
#endif
#undef OPT_STRING
		switch (c) {
		case 'a':
			if (announce_last == NULL) {
				m->announce_list = announce_last =
					malloc(sizeof(llist_t));
			} else {
				announce_last->next =
					malloc(sizeof(llist_t));
				announce_last = announce_last->next;

			}
			if (announce_last == NULL) {
				fprintf(stderr, "Out of memory.\n");
				exit(EXIT_FAILURE);
			}
			announce_last->l = get_slist(optarg);
			break;
		case 'c':
			m->comment = optarg;
			break;
		case 'd':
			m->no_creation_date = 1;
			break;
		case 'h':
			print_help();
			exit(EXIT_SUCCESS);
		case 'l':
			m->piece_length = atoi(optarg);
			break;
		case 'n':
			m->torrent_name = optarg;
			break;
		case 'o':
			m->metainfo_file_path = optarg;
			break;
		case 'p':
			m->private = 1;
			break;
#ifdef USE_PTHREADS
		case 't':
			m->threads = atoi(optarg);
			break;
#endif
		case 'v':
			m->verbose = 1;
			break;
		case 'w':
			if (web_seed_last == NULL) {
				m->web_seed_list = web_seed_last =
					get_slist(optarg);
			} else {
				web_seed_last->next =
					get_slist(optarg);
				web_seed_last = web_seed_last->next;
			}
			while (web_seed_last->next)
				web_seed_last = web_seed_last->next;
			break;
		case '?':
			fprintf(stderr, "Use -h for help.\n");
			exit(EXIT_FAILURE);
		}
	}

	/* set the correct piece length.
	   default is 2^18 = 256kb. */
	if (m->piece_length < 15 || m->piece_length > 28) {
		fprintf(stderr,
			"The piece length must be a number between "
			"15 and 28.\n");
		exit(EXIT_FAILURE);
	}
	m->piece_length = 1 << m->piece_length;

	/* user must specify at least one announce URL as it wouldn't make
	 * any sense to have a default for this */
	if (m->announce_list == NULL) {
		fprintf(stderr, "Must specify an announce URL. "
			"Use -h for help.\n");
		exit(EXIT_FAILURE);
	}
	announce_last->next = NULL;

	/* ..and a file or directory from which to create the torrent */
	if (optind >= argc) {
		fprintf(stderr, "Must specify the contents, "
			"use -h for help\n");
		exit(EXIT_FAILURE);
	}

#ifdef USE_PTHREADS
	/* check the number of threads */
	if (m->threads < 1 || m->threads > 20) {
		fprintf(stderr, "The number of threads must be a number"
				"between 1 and 20\n");
		exit(EXIT_FAILURE);
	}
#endif

	/* strip ending DIRSEP's from target */
	strip_ending_dirseps(argv[optind]);

	/* if the torrent name isn't set use the basename of the target */
	if (m->torrent_name == NULL)
		m->torrent_name = basename(argv[optind]);

	/* make sure m->metainfo_file_path is the absolute path to the file */
	set_absolute_file_path(m);

	/* if we should be verbose print out all the options
	   as we have set them */
	if (m->verbose)
		dump_options(m);

	/* check if target is a directory or just a single file */
	m->target_is_directory = is_dir(m, argv[optind]);
	if (m->target_is_directory) {
		/* change to the specified directory */
		if (chdir(argv[optind])) {
			fprintf(stderr, "Error changing directory to '%s': %s\n",
					argv[optind], strerror(errno));
			exit(EXIT_FAILURE);
		}

		if (file_tree_walk("." DIRSEP, MAX_OPENFD, process_node, m))
			exit(EXIT_FAILURE);
	}

	/* calculate the number of pieces
	   pieces = ceil( size / piece_length ) */
	m->pieces = (m->size + m->piece_length - 1) / m->piece_length;

	/* now print the size and piece count if we should be verbose */
	if (m->verbose)
		printf("\n%" PRIoff " bytes in all.\n"
			"That's %u pieces of %u bytes each.\n\n",
			m->size, m->pieces, m->piece_length);
}
