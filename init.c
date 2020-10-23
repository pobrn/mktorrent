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


#include <stdlib.h>       /* exit() */
#include <sys/types.h>    /* off_t */
#include <errno.h>        /* errno */
#include <string.h>       /* strerror() */
#include <stdio.h>        /* perror(), printf() etc. */
#include <sys/stat.h>     /* the stat structure */
#include <unistd.h>       /* getopt(), getcwd(), sysconf() */
#include <string.h>       /* strcmp(), strlen(), strncpy() */
#include <strings.h>      /* strcasecmp() */
#include <inttypes.h>     /* PRId64 etc. */

#ifdef USE_LONG_OPTIONS
#include <getopt.h>       /* getopt_long() */
#endif

#include "export.h"
#include "mktorrent.h"
#include "ftw.h"
#include "msg.h"

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

static void set_absolute_file_path(struct metafile *m)
{
	char *string;		/* string to return */
	size_t length = 32;	/* length of the string */

	/* if the file_path is already an absolute path just
	   return that */
	if (m->metainfo_file_path && *m->metainfo_file_path == DIRSEP_CHAR) {
		/* we need to reallocate the string, because we want to be able to
		 * free() it in cleanup_metafile(), and that would not be possible
		 * if m->metainfo_file_path pointed to a string from argv[]
		 */
		m->metainfo_file_path = strdup(m->metainfo_file_path);
		FATAL_IF0(m->metainfo_file_path == NULL, "out of memory\n");
		return;
	}

	/* first get the current working directory
	   using getcwd is a bit of a PITA */
	/* allocate initial string */
	string = malloc(length);
	FATAL_IF0(string == NULL, "out of memory\n");

	/* while our allocated memory for the working dir isn't big enough */
	while (getcwd(string, length) == NULL) {
		/* double the buffer size */
		length *= 2;
		/* free our current buffer */
		free(string);
		/* and allocate a new one twice as big muahaha */
		string = malloc(length);
		FATAL_IF0(string == NULL, "out of memory\n");
	}

	/* now set length to the proper length of the working dir */
	length = strlen(string);
	/* if the metainfo file path isn't set */
	if (m->metainfo_file_path == NULL) {
		/* append <torrent name>.torrent to the working dir */
		string =
		    realloc(string, length + strlen(m->torrent_name) + 10);
		FATAL_IF0(string == NULL, "out of memory\n");
		sprintf(string + length, DIRSEP "%s.torrent", m->torrent_name);
	} else {
		/* otherwise append the torrent path to the working dir */
		string =
		    realloc(string,
			    length + strlen(m->metainfo_file_path) + 2);
		FATAL_IF0(string == NULL, "out of memory\n");
		sprintf(string + length, DIRSEP "%s", m->metainfo_file_path);
	}

	m->metainfo_file_path = string;
}

/*
 * parse a comma separated list of strings <str>[,<str>]* and
 * return a string list containing the substrings
 */
static struct ll *get_slist(char *s)
{
	char *e;

	/* allocate a new list */
	struct ll *list = ll_new();
	FATAL_IF0(list == NULL, "out of memory\n");

	/* add URLs to the list while there are commas in the string */
	while ((e = strchr(s, ','))) {
		/* set the commas to \0 so the URLs appear as
		 * separate strings */
		*e = '\0';

		FATAL_IF0(ll_append(list, s, 0) == NULL, "out of memory\n");

		/* move s to point to the next URL */
		s = e + 1;
	}

	/* set the last string in the list */
	FATAL_IF0(ll_append(list, s, 0) == NULL, "out of memory\n");

	/* return the list */
	return list;
}

/*
 * checks if target is a directory
 * sets the file_list and size if it isn't
 */
static int is_dir(struct metafile *m, char *target)
{
	struct stat s;		/* stat structure for stat() to fill */

	/* stat the target */
	FATAL_IF(stat(target, &s), "cannot stat '%s': %s\n",
		target, strerror(errno));

	/* if it is a directory, just return 1 */
	if (S_ISDIR(s.st_mode))
		return 1;

	/* if it isn't a regular file either, something is wrong.. */
	FATAL_IF(!S_ISREG(s.st_mode),
		"'%s' is neither a directory nor regular file\n", target);

	/* if it has negative size, something it wrong */
	FATAL_IF(s.st_size < 0, "'%s' has negative size\n", target);

	/* since we know the torrent is just a single file and we've
	   already stat'ed it, we might as well set the file list */
	struct file_data fd = {
		strdup(target),
		(uintmax_t) s.st_size
	};

	FATAL_IF0(
		fd.path == NULL || ll_append(m->file_list, &fd, sizeof(fd)) == NULL,
		"out of memory\n");

	/* ..and size variable */
	m->size = (uintmax_t) s.st_size;

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
	struct metafile *m = data;

	/* skip non-regular files */
	if (!S_ISREG(sb->st_mode))
		return 0;

	/* ignore the leading "./" */
	path += 2;

	/* now path should be readable otherwise
	 * display a warning and skip it */
	if (access(path, R_OK)) {
		fprintf(stderr, "warning: cannot read '%s', skipping\n", path);
		return 0;
	}

	if (sb->st_size < 0) {
		fprintf(stderr, "warning: '%s' has negative size, skipping\n", path);
		return 0;
	}

	if (m->verbose)
		printf("adding %s\n", path);

	/* count the total size of the files */
	m->size += (uintmax_t) sb->st_size;

	/* create a new file list node for the file */
	struct file_data fd = {
		strdup(path),
		(uintmax_t) sb->st_size
	};

	if (fd.path == NULL || ll_append(m->file_list, &fd, sizeof(fd)) == NULL) {
		fprintf(stderr, "fatal error: out of memory\n");
		return -1;
	}

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
	  "                                additional -a adds backup trackers\n"
	  "-c, --comment=<comment>       : add a comment to the metainfo\n"
	  "-d, --no-date                 : don't write the creation date\n"
	  "-f, --force                   : overwrite output file if it exists\n"
	  "-h, --help                    : show this help screen\n"
	  "-l, --piece-length=<n>        : set the piece length to 2^n bytes,\n"
	  "                                default is calculated from the total size\n"
	  "-n, --name=<name>             : set the name of the torrent\n"
	  "                                default is the basename of the target\n"
	  "-o, --output=<filename>       : set the path and filename of the created file\n"
	  "                                default is <name>.torrent\n"
	  "-p, --private                 : set the private flag\n"
	  "-s, --source=<source>         : add source string embedded in infohash\n"
#ifdef USE_PTHREADS
	  "-t, --threads=<n>             : use <n> threads for calculating hashes\n"
	  "                                default is the number of CPU cores\n"
#endif
	  "-v, --verbose                 : be verbose\n"
	  "-w, --web-seed=<url>[,<url>]* : add web seed URLs\n"
	  "                                additional -w adds more URLs\n"
	  "-x, --cross-seed              : ensure info hash is unique for easier cross-seeding\n"
#else
	  "-a <url>[,<url>]* : specify the full announce URLs\n"
	  "                    additional -a adds backup trackers\n"
	  "-c <comment>      : add a comment to the metainfo\n"
	  "-d                : don't write the creation date\n"
	  "-f                : overwrite output file if it exists\n"
	  "-h                : show this help screen\n"
	  "-l <n>            : set the piece length to 2^n bytes,\n"
	  "                    default is calculated from the total size\n"
	  "-n <name>         : set the name of the torrent,\n"
	  "                    default is the basename of the target\n"
	  "-o <filename>     : set the path and filename of the created file\n"
	  "                    default is <name>.torrent\n"
	  "-p                : set the private flag\n"
	  "-s                : add source string embedded in infohash\n"
#ifdef USE_PTHREADS
	  "-t <n>            : use <n> threads for calculating hashes\n"
	  "                    default is the number of CPU cores\n"
#endif
	  "-v                : be verbose\n"
	  "-w <url>[,<url>]* : add web seed URLs\n"
	  "                    additional -w adds more URLs\n"
	  "-x                : ensure info hash is unique for easier cross-seeding\n"
#endif
	  "\nPlease send bug reports, patches, feature requests, praise and\n"
	  "general gossip about the program to: mktorrent@rudde.org\n");
}

/*
 * print the full announce list
 */
static void print_announce_list(struct ll *list)
{
	unsigned int tier = 1;

	LL_FOR(node, list) {

		struct ll *inner_list = LL_DATA(node);

		printf("    %u : %s\n",
			tier, LL_DATA_AS(LL_HEAD(inner_list), const char*));

		LL_FOR_FROM(inner_node, LL_NEXT(LL_HEAD(inner_list))) {
			printf("        %s\n", LL_DATA_AS(inner_node, const char*));
		}

		tier += 1;
	}
}

/*
 * print the list of web seed URLs
 */
static void print_web_seed_list(struct ll *list)
{
	printf("  Web Seed URL: ");

	if (LL_IS_EMPTY(list)) {
		printf("none\n");
		return;
	}

	printf("%s\n", LL_DATA_AS(LL_HEAD(list), const char*));
	LL_FOR_FROM(node, LL_NEXT(LL_HEAD(list))) {
		printf("                %s\n", LL_DATA_AS(node, const char*));
	}
}

/*
 * print out all the options
 */
static void dump_options(struct metafile *m)
{
	printf("Options:\n"
	       "  Announce URLs:\n");

	print_announce_list(m->announce_list);

	printf("  Torrent name: %s\n"
	       "  Metafile:     %s\n"
	       "  Piece length: %u\n"
#ifdef USE_PTHREADS
	       "  Threads:      %ld\n"
#endif
	       "  Be verbose:   yes\n",
	       m->torrent_name, m->metainfo_file_path, m->piece_length
#ifdef USE_PTHREADS
	       ,m->threads
#endif
	       );

	printf("  Write date:   ");
	if (m->no_creation_date)
		printf("no\n");
	else
		printf("yes\n");

	print_web_seed_list(m->web_seed_list);

	/* Print source string only if set */
	if (m->source)
		printf("\n Source:      %s\n\n", m->source);

	printf("  Comment:      ");
	if (m->comment == NULL)
		printf("none\n\n");
	else
		printf("\"%s\"\n\n", m->comment);
}

static int file_data_cmp_by_name(const void *a, const void *b)
{
	const struct file_data *x = a, *y = b;
	return strcmp(x->path, y->path);
}

static void file_data_clear(void *data)
{
	struct file_data *fd = data;
	free(fd->path);
}

static void free_inner_list(void *data)
{
	struct ll *list = data;
	ll_free(list, NULL);
}

/*
 * parse and check the command line options given
 * and fill out the appropriate fields of the
 * metafile structure
 */
EXPORT void init(struct metafile *m, int argc, char *argv[])
{
	int c;			/* return value of getopt() */
	const uintmax_t piece_len_maxes[] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		(uintmax_t) BIT15MAX * ONEMEG, (uintmax_t) BIT16MAX * ONEMEG,
		(uintmax_t) BIT17MAX * ONEMEG, (uintmax_t) BIT18MAX * ONEMEG,
		(uintmax_t) BIT19MAX * ONEMEG, (uintmax_t) BIT20MAX * ONEMEG,
		(uintmax_t) BIT21MAX * ONEMEG, (uintmax_t) BIT22MAX * ONEMEG,
		(uintmax_t) BIT23MAX * ONEMEG
	};

	const int num_piece_len_maxes = sizeof(piece_len_maxes) /
	    sizeof(piece_len_maxes[0]);

#ifdef USE_LONG_OPTIONS
	/* the option structure to pass to getopt_long() */
	static struct option long_options[] = {
		{"announce", 1, NULL, 'a'},
		{"comment", 1, NULL, 'c'},
		{"no-date", 0, NULL, 'd'},
		{"force", 0, NULL, 'f'},
		{"help", 0, NULL, 'h'},
		{"piece-length", 1, NULL, 'l'},
		{"name", 1, NULL, 'n'},
		{"output", 1, NULL, 'o'},
		{"private", 0, NULL, 'p'},
		{"source", 1, NULL, 's'},
#ifdef USE_PTHREADS
		{"threads", 1, NULL, 't'},
#endif
		{"verbose", 0, NULL, 'v'},
		{"web-seed", 1, NULL, 'w'},
		{"cross-seed", 0, NULL, 'x'},
		{NULL, 0, NULL, 0}
	};
#endif

	m->announce_list = ll_new();
	FATAL_IF0(m->announce_list == NULL, "out of memory\n");

	m->web_seed_list = ll_new();
	FATAL_IF0(m->web_seed_list == NULL, "out of memory\n");

	m->file_list = ll_new();
	FATAL_IF0(m->file_list == NULL, "out of memory\n");

	/* now parse the command line options given */
#ifdef USE_PTHREADS
#define OPT_STRING "a:c:dfhl:n:o:ps:t:vw:x"
#else
#define OPT_STRING "a:c:dfhl:n:o:ps:vw:x"
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
			FATAL_IF0(
				ll_append(m->announce_list, get_slist(optarg), 0) == NULL,
				"out of memory\n");
			break;
		case 'c':
			m->comment = optarg;
			break;
		case 'd':
			m->no_creation_date = 1;
			break;
		case 'f':
			m->force_overwrite = 1;
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
		case 's':
			m->source = optarg;
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
			ll_extend(m->web_seed_list, get_slist(optarg));
			break;
		case 'x':
			m->cross_seed = 1;
			break;
		case '?':
			fatal("use -h for help.\n");
		}
	}

	/* check that the user provided a file or directory from which to create the torrent */
	FATAL_IF0(optind >= argc,
		"must specify the contents, use -h for help\n");

#ifdef USE_PTHREADS
	/* check the number of threads */
	if (m->threads) {
		FATAL_IF0(m->threads > 20,
			"the number of threads is limited to at most 20\n");
	} else {
#ifdef _SC_NPROCESSORS_ONLN
		m->threads = sysconf(_SC_NPROCESSORS_ONLN);
		if (m->threads <= 0)
#endif
			m->threads = 2; /* some sane default */
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
		FATAL_IF(chdir(argv[optind]), "cannot change directory to '%s': %s\n",
			argv[optind], strerror(errno));

		if (file_tree_walk("." DIRSEP, MAX_OPENFD, process_node, m))
			exit(EXIT_FAILURE);
	}

	ll_sort(m->file_list, file_data_cmp_by_name);

	/* determine the piece length based on the torrent size if
	   it was not user specified. */
	if (m->piece_length == 0) {
		int i;
		for (i = 15; i < num_piece_len_maxes &&
			m->piece_length == 0; i++)
			if (m->size <= piece_len_maxes[i])
				m->piece_length = i;
		if (m->piece_length == 0)
			m->piece_length = num_piece_len_maxes;
	} else {
		/* if user did specify a piece length, verify its validity */
		FATAL_IF0(m->piece_length < 15 || m->piece_length > 28,
			"the piece length must be a number between 15 and 28.\n");
	}

	/* convert the piece length from power of 2 to an integer. */
	m->piece_length = 1 << m->piece_length;

	/* calculate the number of pieces
	   pieces = ceil( size / piece_length ) */
	m->pieces = (m->size + m->piece_length - 1) / m->piece_length;

	/* now print the size and piece count if we should be verbose */
	if (m->verbose)
		printf("\n%" PRIuMAX " bytes in all\n"
			"that's %u pieces of %u bytes each\n\n",
			m->size, m->pieces, m->piece_length);
}

EXPORT void cleanup_metafile(struct metafile *m)
{
	ll_free(m->announce_list, free_inner_list);

	ll_free(m->file_list, file_data_clear);

	ll_free(m->web_seed_list, NULL);

	free(m->metainfo_file_path);
}
