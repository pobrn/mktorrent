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
#include <stdbool.h>     /* bool: true / false */

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

#include "mktorrent.h"

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
extern void write_metainfo(FILE *f, metafile_t *m, unsigned char *hash_string, bool fast);
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

/* Checks if string str ends with string suffix */
int EndsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

char* concat(char *s1, char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the zero-terminator
    //in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}


/* This is suppose to be faster somehow */
/*char* concat(char *s1, char *s2)
{
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    char *result = malloc(len1+len2+1);//+1 for the zero-terminator
    //in real code you would check for errors in malloc here
    memcpy(result, s1, len1);
    memcpy(result+len1, s2, len2+1);//+1 to copy the null-terminator
    return result;
}*/

/*char *str_replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep
    int len_with; // length of with
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    if (!orig)
        return NULL;
    if (!rep)
        rep = "";
    len_rep = strlen(rep);
    if (!with)
        with = "";
    len_with = strlen(with);

    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}*/

/*
 * create and open the metainfo file for writing and create a stream for it
 * we don't want to overwrite anything, so abort if the file is already there
 */
static FILE *open_file(char *path)
{
	int fd;  /* file descriptor */
	FILE *f; /* file stream */

	/*if (fast == true) {
		if (strlen(path) >= 8) {
			char *tail;
			substr(tail, strlen(path)+1, path, -8);
			if (strcmp(tail, ".torrent") == 0) {
				path = str_replace(path, ".torrent", "");
				path = concat(path, "-fast.torrent");
			}
		} else {
			path = concat(path, "-fast");
		}
	}

	printf("\n\n Path: %s \n\n", path);*/

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

char* get_path_name(char *path, bool fast) {
	if (EndsWith(path, ".torrent")) {
		//path = str_replace(path, ".torrent", "");
		path[strlen(path)-8] = '\0';
	} 

	if (fast == true) {
		return concat(path, "-fast.torrent");
	} else {
		return concat(path, ".torrent");
	}
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
	FILE *file;	/* stream for writing to the metainfo file */
	metafile_t m = {
		/* options */
		18,   /* piece_length, 2^18 = 256kb by default */
		NULL, /* announce_list */
		NULL, /* torrent_name */
		NULL, /* metainfo_file_path */
		false,/* libtorrent fast resume */
		NULL, /* web_seed_url */
		NULL, /* comment */
		0,    /* target_is_directory  */
		0,    /* no_creation_date */
		0,    /* private */
		NULL, /* source string */
		0,    /* verbose */
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

	/* process options */
	init(&m, argc, argv);

	/* open the file stream now, so we don't have to abort
	   _after_ we did all the hashing in case we fail */
	file = open_file(get_path_name(m.metainfo_file_path, false));

	/* calculate hash string from metadata*/
	unsigned char* info_hash = make_hash(&m);

	/* write the metainfo to file */
	write_metainfo(file, &m, info_hash, false);

	/* close the file stream */
	close_file(file);

	if (m.fast_resume) {
		file = open_file(get_path_name(m.metainfo_file_path, true));

		write_metainfo(file, &m, info_hash, true);

		close_file(file);
	}

	/* yeih! everything seemed to go as planned */
	return EXIT_SUCCESS;
}
