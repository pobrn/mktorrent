/*
This file is part of mktorrent
copy from main by pertershaw
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

#include "libmktorrent.h"

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

metafile_t *getDefaultMetaStruc(){
 	static metafile_t m = {
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
	return &m;
}


/*
 * a reimplementation of init.
 * this version does not pare the command line, 
 * it takes &m as given input.
 */
int initLib(metafile_t *m, char *annouceUrl, char *fileOrDirName)
{
	llist_t *announce_last = NULL;
	slist_t *web_seed_last = NULL;

	if (announce_last == NULL) {
		m->announce_list = announce_last = malloc(sizeof(llist_t));
	} else {
		announce_last->next = malloc(sizeof(llist_t));
		announce_last = announce_last->next;
	}
	if (announce_last == NULL) {
		fprintf(stderr, "Out of memory.\n");
		return EXIT_FAILURE;
	}
	announce_last->l = get_slist(annouceUrl);

	/* set the correct piece length.
	   default is 2^18 = 256kb. */
	if (m->piece_length < 15 || m->piece_length > 28) {
		fprintf(stderr,
			"The piece length must be a number between "
			"15 and 28.\n");
		return EXIT_FAILURE;
	}
	m->piece_length = 1 << m->piece_length;

	/* user must specify at least one announce URL as it wouldn't make
	 * any sense to have a default for this.
	 * it is ok not to have any unless torrent is private. */
	if (m->announce_list == NULL && m->private == 1) {
		fprintf(stderr, "Must specify an announce URL.\n");
		return EXIT_FAILURE;
	}
	if (announce_last != NULL){
		announce_last->next = NULL;
	}

#ifdef USE_PTHREADS
	/* check the number of threads */
	if (m->threads) {
		if (m->threads > 20) {
			fprintf(stderr, "The number of threads is limited to "
			                "at most 20\n");
			return EXIT_FAILURE;
		}
	} else {
#ifdef _SC_NPROCESSORS_ONLN
		m->threads = sysconf(_SC_NPROCESSORS_ONLN);
		if (m->threads == -1)
#endif
			m->threads = 2; /* some sane default */
	}
#endif

	/* strip ending DIRSEP's from target */
	strip_ending_dirseps(fileOrDirName);

	/* if the torrent name isn't set use the basename of the target */
	if (m->torrent_name == NULL){
		m->torrent_name = basename(fileOrDirName);
	}

	/* make sure m->metainfo_file_path is the absolute path to the file */
	set_absolute_file_path(m);

	/* if we should be verbose print out all the options
	   as we have set them */
	if (m->verbose){
		dump_options(m);
	}

	/* check if target is a directory or just a single file */
	m->target_is_directory = is_dir(m, fileOrDirName);
	if (m->target_is_directory) {
		/* change to the specified directory */
		if (chdir(fileOrDirName)) {
			fprintf(stderr, "Error changing directory to '%s': %s\n",
					fileOrDirName, strerror(errno));
			return EXIT_FAILURE;
		}

		if (file_tree_walk("." DIRSEP, MAX_OPENFD, process_node, m)){
			return EXIT_FAILURE;
		}
	}

	/* calculate the number of pieces
	   pieces = ceil( size / piece_length ) */
	m->pieces = (m->size + m->piece_length - 1) / m->piece_length;

	/* now print the size and piece count if we should be verbose */
	if (m->verbose) {
		printf("\n%" PRIoff " bytes in all.\n"
			"That's %u pieces of %u bytes each.\n\n",
			m->size, m->pieces, m->piece_length);
	}
	return EXIT_SUCCESS;
}

int mktorrent(char *fileOrDirName, char* annouceUrl, metafile_t *m){
	FILE *file;	/* stream for writing to the metainfo file */

 	/* process options */
 	int status = initLib(m, annouceUrl, fileOrDirName);
 	if(status == EXIT_FAILURE){
		return status;
	}

 	/* open the file stream now, so we don't have to abort
 	   _after_ we did all the hashing in case we fail */
 	file = open_file(m->metainfo_file_path);

 	/* calculate hash string and write the metainfo to file */
 	write_metainfo(file, m, make_hash(m));

 	/* close the file stream */
 	close_file(file);

	return EXIT_SUCCESS;
}


