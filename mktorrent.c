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
#include <unistd.h>      /* getopt(), access(), read(), getcwd(), sysconf().. */
#ifdef USE_LONG_OPTIONS
#include <getopt.h>      /* getopt_long() */
#endif
#include <time.h>        /* time() */

#ifdef _WIN32
#define DIRSEP      "\\"
#define DIRSEP_CHAR '\\'
#else
#define DIRSEP      "/"
#define DIRSEP_CHAR '/'
#endif

#ifdef ALLINONE
#include <sys/stat.h>
#include <dirent.h>

#ifdef USE_OPENSSL
#include <openssl/sha.h>
#else
#include <inttypes.h>
#endif

#ifdef USE_PTHREADS
#include <pthread.h>
#endif

#define EXPORT static

#include "ftw.c"
#include "benc.c"
#include "fiter.c"
#include "parse.c"

#ifndef USE_OPENSSL
#include "sha1.c"
#endif

#ifdef USE_PTHREADS
#include "hash_pthreads.c"
#else
#include "hash.c"
#endif

#else  /* ALLINONE */
#define EXPORT

#include "ftw.h"
#include "benc.h"
#include "fiter.h"
#include "parse.h"

#ifdef USE_PTHREADS
union be_t *make_hash(union be_t *meta, int threads, unsigned int verbosity);
#else
union be_t *make_hash(union be_t *meta, unsigned int verbosity);
#endif
#endif /* ALLINONE */

#ifndef MAX_OPENFD
#define MAX_OPENFD 100
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef S_IRGRP
#define S_IRGRP 0
#endif
#ifndef S_IROTH
#define S_IROTH 0
#endif

#ifdef USE_PTHREADS
static int
get_default_threads_count()
{
	int count = sysconf(_SC_NPROCESSORS_ONLN);

	if (count < 0)
		count = 2; /* some sane default */

	return count;
}
#endif

static void
strip_ending_dirseps(char *s)
{
	char *end = s;

	while (*end)
		end++;

	while (end > s && *(--end) == DIRSEP_CHAR)
		*end = '\0';
}

static char *
basename(char *s)
{
	char *r = s;

	while (*s != '\0') {
		if (*s == DIRSEP_CHAR)
			r = ++s;
		else
			++s;
	}

	return r;
}

static FILE *
fopen_safe(const char *path)
{
	int fd  = open(path, O_WRONLY | O_BINARY | O_CREAT | O_EXCL,
	               S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (fd < 0)
		return NULL;

	return fdopen(fd, "wb");
}

static int
pushdir(const char *path)
{
	int cwd = open(".", O_RDONLY);
	if (cwd < 0) {
		fprintf(stderr, "Error opening current directory: %s\n",
		        strerror(errno));
		return -1;
	}

	if (chdir(path)) {
		fprintf(stderr, "Error changing directory to '%s': %s\n",
		        path, strerror(errno));
		(void)close(cwd);
		return -1;
	}

	return cwd;
}

static unsigned int
popdir(int cwd)
{
	if (fchdir(cwd)) {
		(void)close(cwd);
		fprintf(stderr, "Error changing directory back: %s\n",
		        strerror(errno));
		return 1;
	}

	if (close(cwd)) {
		fprintf(stderr, "Error closing current directory\n");
		return 1;
	}

	return 0;
}

static unsigned int
parse_list_add(union be_t *list, const char *str)
{
	const char *e;

	do {
		for (e = str; *e && *e != ','; e++);

		if (be_list_add(list, be_string_cpy(str, e - str)))
			return 1;

		str = e + 1;
	} while (*e);

	return 0;
}

static union be_t *
parse_list(const char *str)
{
	union be_t *r = be_list_new();

	if (r == NULL)
		return NULL;

	if (parse_list_add(r, str)) {
		be_destroy(r);
		return NULL;
	}

	return r;
}

static union be_t *
filename_list(const char *name)
{
	union be_t *l = be_list_new();
	const char *e;

	if (l == NULL)
		return NULL;

	do {
		for (e = name; *e && *e != DIRSEP_CHAR; e++);

		if (be_list_add(l, be_string_cpy(name, e - name)))
			goto error;

		name = e + 1;
	} while (*e);

	return l;

error:
	be_destroy(l);
	return NULL;
}

static int
ftw_callback(const char *path, const struct stat *sbuf, void *data)
{
	union be_t *files = data;
	union be_t *d;

	if (!S_ISREG(sbuf->st_mode))
		return 0;

	/* ignore the leading "./" */
	path += 2;

	if (access(path, R_OK)) {
		fprintf(stderr, "WARNING: Cannot read '%s', skipping.\n", path);
		return 0;
	}

	d = be_dict_new();
	if (d == NULL
	    || be_dict_add(d, "length", be_off_new(sbuf->st_size))
	    || be_dict_add(d, "path", filename_list(path))
	    || be_list_add(files, d)) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}

	return 0;
}

static int
read_target(union be_t *info, const char *target)
{
	union be_t *files;
	struct stat sbuf;
	int cwd;

	if (stat(target, &sbuf)) {
		fprintf(stderr, "Error stat'ing '%s': %s\n",
				target, strerror(errno));
		return -1;
	}

	if (S_ISREG(sbuf.st_mode)) {
		if (be_dict_set(info, "length", be_off_new(sbuf.st_size))) {
			fprintf(stderr, "Out of memory\n");
			return -1;
		}
		return 0;
	}

	if (!S_ISDIR(sbuf.st_mode)) {
		fprintf(stderr,	"'%s' is neither a directory nor regular file.\n",
		        target);
		return -1;
	}

	files = be_list_new();
	if (files == NULL) {
		fprintf(stderr, "Out of memory\n");
		goto error;
	}

	cwd = pushdir(target);
	if (cwd < 0)
		goto error;

	if (file_tree_walk(".", MAX_OPENFD, ftw_callback, files)) {
		(void)popdir(cwd);
		goto error;
	}

	if (popdir(cwd))
		goto error;

	if (be_dict_set(info, "files", files)) {
		fprintf(stderr, "Out of memory\n");
		goto error;
	}

	return 1;

error:
	be_destroy(files);
	return -1;
}

static int
print_help()
{
	if (printf(
	  "Usage: mktorrent [OPTIONS] <target directory or filename>\n\n"
	  "Options:\n"
#ifdef USE_LONG_OPTIONS
	  "-a, --announce=<url>[,<url>]* : specify the full announce URLs\n"
	  "                                at least one is required\n"
	  "                                additional -a adds backup trackers\n"
	  "-C, --check                   : check a downloaded torrent\n"
	  "-c, --comment=<comment>       : add a comment to the metainfo\n"
	  "-D, --dump                    : dump the contents of a b-encoded file\n"
	  "                                in a format easier on the human eye\n"
	  "-d, --no-date                 : don't write the creation date\n"
	  "-h, --help                    : show this help screen\n"
	  "-l, --piece-length=<n>        : set the piece length to 2^n bytes,\n"
	  "                                default is 18, that is 2^18 = 256kb\n"
	  "-n, --name=<name>             : set the name of the torrent\n"
	  "                                default is the basename of the target\n"
	  "-o, --output=<filename>       : set the path and filename of the created file\n"
	  "                                default is <name>.torrent\n"
	  "-p, --private                 : set the private flag\n"
	  "-q, --quiet                   : be more quiet\n"
#ifdef USE_PTHREADS
	  "-t, --threads=<n>             : use <n> threads for calculating hashes\n"
	  "                                default is %d\n"
#endif
	  "-v, --verbose                 : be more verbose\n"
	  "-w, --web-seed=<url>[,<url>]* : add web seed URLs\n"
	  "                                additional -w adds more URLs\n"
#else
	  "-a <url>[,<url>]* : specify the full announce URLs\n"
	  "                    at least one is required\n"
	  "                    additional -a adds backup trackers\n"
	  "-C                : check a downloaded torrent\n"
	  "-c <comment>      : add a comment to the metainfo\n"
	  "-D                : dump the contents of a b-encoded file\n"
	  "                    in a format easier on the human eye\n"
	  "-d                : don't write the creation date\n"
	  "-h                : show this help screen\n"
	  "-l <n>            : set the piece length to 2^n bytes,\n"
	  "                    default is 18, that is 2^18 = 256kb\n"
	  "-n <name>         : set the name of the torrent,\n"
	  "                    default is the basename of the target\n"
	  "-o <filename>     : set the path and filename of the created file\n"
	  "                    default is <name>.torrent\n"
	  "-p                : set the private flag\n"
	  "-q                : be more quiet\n"
#ifdef USE_PTHREADS
	  "-t <n>            : use <n> threads for calculating hashes\n"
	  "                    default is %d\n"
#endif
	  "-v                : be more verbose\n"
	  "-w <url>[,<url>]* : add web seed URLs\n"
	  "                    additional -w adds more URLs\n"
#endif
	  "\nPlease send bug reports, patches, feature requests, praise and\n"
	  "general gossip about the program to: esmil@users.sourceforge.net\n"
#ifdef USE_PTHREADS
	  , get_default_threads_count()
#endif
	  ) < 0)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static int
out_of_memory()
{
	fprintf(stderr, "Out of memory\n");
	return EXIT_FAILURE;
}

static int
do_dump(const char *target)
{
	FILE *f;
	union be_t *meta;

	f = fopen(target, "rb");
	if (f == NULL) {
		fprintf(stderr, "Error opening '%s': %s\n",
		       target, strerror(errno));
		return EXIT_FAILURE;
	}

	meta = be_parse(f);
	if (meta == NULL || meta->type != BE_TYPE_DICT) {
		if (ferror(f))
			fprintf(stderr, "Error reading from '%s': %s\n",
			       target, strerror(errno));
		else
			fprintf(stderr, "Error parsing '%s'\n", target);
		(void)fclose(f);
		return EXIT_FAILURE;
	}

	if (fclose(f)) {
		fprintf(stderr, "Error closing '%s': %s\n",
		       target, strerror(errno));
		return EXIT_FAILURE;
	}

	if (be_pp(meta, stdout) < 0) {
		fprintf(stderr, "Error writing to standard output: %s\n",
		        strerror(errno));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

#ifdef USE_PTHREADS
static int
do_check(const char *target, int threads, unsigned int verbosity)
#else
static int
do_check(const char *target, unsigned int verbosity)
#endif
{
	FILE *f;
	union be_t *meta;
	union be_t *info;
	union be_t *pieces;
	union be_t *v;
	int cwd;

	f = fopen(target, "rb");
	if (f == NULL) {
		fprintf(stderr, "Error opening '%s': %s\n",
		       target, strerror(errno));
		return EXIT_FAILURE;
	}

	meta = be_parse(f);
	if (meta == NULL || meta->type != BE_TYPE_DICT) {
		if (ferror(f))
			fprintf(stderr, "Error reading from '%s': %s\n",
			       target, strerror(errno));
		else
			fprintf(stderr, "Error parsing '%s'\n", target);
		(void)fclose(f);
		return EXIT_FAILURE;
	}

	if (fclose(f)) {
		fprintf(stderr, "Error closing '%s': %s\n",
		       target, strerror(errno));
		return EXIT_FAILURE;
	}

	info = be_dict_get(meta, "info");
	if (info == NULL || info->type != BE_TYPE_DICT) {
		fprintf(stderr, "No info section found in metafile\n");
		return EXIT_FAILURE;
	}

	pieces = be_dict_get(info, "pieces");
	if (pieces == NULL || pieces->type != BE_TYPE_STRING) {
		fprintf(stderr, "No hash string found in metafile\n");
		return EXIT_FAILURE;
	}

	v = be_dict_get(info, "files");
	if (v == NULL || v->type != BE_TYPE_LIST)
		cwd = 0;
	else
		cwd = 1;

	v = be_dict_get(info, "name");
	if (v == NULL || v->type != BE_TYPE_STRING) {
		fprintf(stderr, "No name found in metafile\n");
		return EXIT_FAILURE;
	}

	if (cwd) {
		char *dir = malloc(v->string.len + 1);

		if (dir == NULL) {
			fprintf(stderr, "Out of memory\n");
			return EXIT_FAILURE;
		}

		memcpy(dir, be_string_get(&v->string), v->string.len);
		dir[v->string.len] = '\0';

		cwd = pushdir(dir);
		if (cwd < 0)
			return EXIT_FAILURE;

		free(dir);
	}

#ifdef USE_PTHREADS
	v = make_hash(meta, threads, verbosity);
#else
	v = make_hash(meta, verbosity);
#endif
	if (v == NULL) {
		if (cwd)
			(void)popdir(cwd);
		return EXIT_FAILURE;
	}

	if (cwd && popdir(cwd))
		return EXIT_FAILURE;

	if (be_cmp(pieces, v) != 0) {
		if (verbosity > 0)
			printf("Hash strings disagree!\n");
		return EXIT_FAILURE;
	}

	if (verbosity > 0)
		printf("Hash strings agree\n");
	return EXIT_SUCCESS;
}

enum action {
	DO_CREATE,
	DO_DUMP,
	DO_CHECK
};

int
main(int argc, char *argv[])
{
	int c;
#ifdef USE_LONG_OPTIONS
	/* the option structure to pass to getopt_long() */
	static struct option long_options[] = {
		{"announce", 1, NULL, 'a'},
		{"check", 0, NULL, 'C'},
		{"comment", 1, NULL, 'c'},
		{"dump", 0, NULL, 'D'},
		{"no-date", 0, NULL, 'd'},
		{"help", 0, NULL, 'h'},
		{"piece-length", 1, NULL, 'l'},
		{"name", 1, NULL, 'n'},
		{"output", 1, NULL, 'o'},
		{"private", 0, NULL, 'p'},
		{"quiet", 0, NULL, 'q'},
#ifdef USE_PTHREADS
		{"threads", 1, NULL, 't'},
#endif
		{"verbose", 0, NULL, 'v'},
		{"web-seed", 1, NULL, 'w'},
		{NULL, 0, NULL, 0}
	};
#endif
	union be_t *meta = be_dict_new();
	union be_t *info = be_dict_new();
	union be_t *announce_list = be_list_new();
	union be_t *web_seeds = NULL;
	char *output = NULL;
	const char *name = NULL;
	char *target = NULL;
	unsigned int piece_length = 18;
#ifdef USE_PTHREADS
	int threads = -1;
#endif
	unsigned int date = 1;
	unsigned int verbosity = 1;
	enum action action = DO_CREATE;
	int cwd;
	FILE *f;

	if (meta == NULL || info == NULL || announce_list == NULL)
		return out_of_memory();

#ifdef USE_PTHREADS
#define OPT_STRING "a:Cc:Ddhl:n:o:pqt:vw:"
#else
#define OPT_STRING "a:Cc:Ddhl:n:o:pqvw:"
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
			if (be_list_add(announce_list, parse_list(optarg)))
				return out_of_memory();
			break;
		case 'C':
			action = DO_CHECK;
			break;
		case 'c':
			if (be_dict_set(meta, "comment", be_string_dup(optarg)))
				return out_of_memory();
			break;
		case 'D':
			action = DO_DUMP;
			break;
		case 'd':
			date = 0;
			break;
		case 'h':
			return print_help();
		case 'l':
			piece_length = atoi(optarg);
			break;
		case 'n':
			name = optarg;
			break;
		case 'o':
			output = optarg;
			break;
		case 'p':
			if (be_dict_set(info, "private", be_off_new(1)))
				return out_of_memory();
			break;
		case 'q':
			if (verbosity > 0)
				verbosity--;
			break;
#ifdef USE_PTHREADS
		case 't':
			threads = atoi(optarg);
			break;
#endif
		case 'v':
			if (verbosity < 3)
				verbosity++;
			break;
		case 'w':
			if (web_seeds == NULL) {
				web_seeds = parse_list(optarg);
				if (web_seeds == NULL)
					return out_of_memory();
			} else {
				if (parse_list_add(web_seeds, optarg))
					return out_of_memory();
			}
			break;
		case '?':
			fprintf(stderr, "Invalid argument, use -h for help.\n");
			return EXIT_FAILURE;
		}
	}

	if (verbosity > 0)
		printf("mktorrent " VERSION
		       " (c) 2007, 2009 Emil Renner Berthing\n\n");

	if (optind >= argc) {
		fprintf(stderr, "Must specify the contents, "
			"use -h for help\n");
		return EXIT_FAILURE;
	}
	target = argv[optind];
	strip_ending_dirseps(target);

#ifdef USE_PTHREADS
	if (threads == -1)
		threads = get_default_threads_count();
	else if (threads < 1 || threads > 20) {
		fprintf(stderr,
		        "The number of threads must be between 1 and 20\n");
	        return EXIT_FAILURE;
	}
	if (verbosity > 1) {
		printf("Using %d threads for hashing\n", threads);
		fflush(stdout);
	}
#endif

	switch (action) {
	case DO_CREATE: break;
	case DO_DUMP:   return do_dump(target);
	case DO_CHECK:
#ifdef USE_PTHREADS
		return do_check(target, threads, verbosity);
#else
		return do_check(target, verbosity);
#endif
	}

	if (announce_list->list.first == NULL) {
		fprintf(stderr, "Must specify an announce URL, "
			"use -h for help.\n");
		return EXIT_FAILURE;
	}

	if (be_dict_set(meta, "announce",
	                announce_list->list.first->v->list.first->v))
		return out_of_memory();

	if (verbosity > 1) {
		printf("Trackers: ");
		be_pp(announce_list, stdout);
		fflush(stdout);
	}
	if ((announce_list->list.first->next
	    || announce_list->list.first->v->list.first->next)
	    && be_dict_set(meta, "announce-list", announce_list))
		return out_of_memory();

	if (be_dict_add(meta, "created by", be_string_dup("mktorrent " VERSION)))
		return out_of_memory();

	if (!date && verbosity > 0) {
		printf("Not adding creation date\n");
		fflush(stdout);
	}
	if (date && be_dict_add(meta, "creation date",
	                        be_off_new((off_t)time(NULL))))
		return out_of_memory();

	if (be_dict_add(meta, "info", info))
		return out_of_memory();

	if (web_seeds) {
		if (verbosity > 1) {
			printf("Web seed urls: ");
			be_pp(web_seeds, stdout);
			fflush(stdout);
		}
		if (web_seeds->list.first->next == NULL) {
			if (be_dict_add(meta, "url-list",
			                web_seeds->list.first->v))
				return out_of_memory();
		} else {
			if (be_dict_add(meta, "url-list", web_seeds))
				return out_of_memory();
		}
	}

	if (name == NULL)
		name = basename(target);
	if (verbosity > 1) {
		printf("Torrent name: \"%s\"\n", name);
		fflush(stdout);
	}
	if (be_dict_set(info, "name", be_string_dup(name)))
		return out_of_memory();

	switch (read_target(info, target)) {
	case 0: /* target is a file */
		{
			char *p = basename(target);

			while (p > target && *(--p) == DIRSEP_CHAR)
				*p = '\0';

			if (p == target)
				target = NULL;
		}
		break;
	case 1: /* target is a directory */
		break;
	default:
		return EXIT_FAILURE;
	}

	if (piece_length < 15 || piece_length > 28) {
		fprintf(stderr,
			"The piece length must be a number between 15 and 28.\n");
		return EXIT_FAILURE;
	}
	if (verbosity > 1) {
		printf("Using piece length of 2^%u bytes (%u kb)\n",
		       piece_length, 1 << (piece_length - 10));
		fflush(stdout);
	}
	piece_length = 1 << piece_length;
	if (be_dict_set(info, "piece length",
	                be_off_new(piece_length)))
		return out_of_memory();

	if (verbosity > 2) {
		be_pp(meta, stdout);
		fflush(stdout);
	}

	if (output == NULL) {
		output = malloc(strlen(name) + 9);
		if (output == NULL)
			return out_of_memory();
		sprintf(output, "%s.torrent", name);
	}

	if (verbosity > 1) {
		printf("Creating '%s'\n", output);
		fflush(stdout);
	}
	f = fopen_safe(output);
	if (f == NULL) {
		fprintf(stderr, "Error creating '%s': %s\n",
				output, strerror(errno));
		return EXIT_FAILURE;
	}

	if (target == NULL)
		cwd = 0;
	else {
		cwd = pushdir(target);
		if (cwd < 0)
			goto cleanup;
	}

	if (be_dict_set(info, "pieces",
#ifdef USE_PTHREADS
	                make_hash(meta, threads, verbosity)))
#else
	                make_hash(meta, verbosity)))
#endif
		goto cleanup;

	if (cwd && popdir(cwd))
		goto cleanup;

	if (verbosity > 1) {
		printf("Writing metainfo\n");
		fflush(stdout);
	}
	if (be_fput(meta, f) < 0) {
		fprintf(stderr, "Error writing to '%s': %s\n",
		        output, strerror(errno));
		goto cleanup;
	}

	if (fclose(f)) {
		fprintf(stderr, "Error closing stream: %s\n",
				strerror(errno));
		goto cleanup;
	}
	if (verbosity > 1) {
		printf("Done\n");
		fflush(stdout);
	}

	return EXIT_SUCCESS;

cleanup:
	(void)fclose(f);

	if (verbosity > 1)
		printf("Removing '%s'\n", output);
	(void)remove(output);

	return EXIT_FAILURE;
}
