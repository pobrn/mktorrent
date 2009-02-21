#ifdef ALLINONE
#include <stdlib.h>		/* exit() */
#include <stdio.h>		/* printf() etc. */
#include <unistd.h>		/* access(), read(), close(), getcwd() */
#include <string.h>		/* strlen() etc. */
#include <getopt.h>		/* getopt_long() */
#include <fcntl.h>		/* open() */
#include <ftw.h>		/* ftw() */
#include <time.h>		/* time() */
#include <openssl/sha.h>	/* SHA1(), SHA_DIGEST_LENGTH */
#ifndef NO_THREADS
#include <pthread.h>		/* pthread functions and data structures */
#endif
#endif

/* define the type of a file list node */
struct fl_node_s;
typedef struct fl_node_s *fl_node;
struct fl_node_s {
	char *path;
	off_t size;
	fl_node next;
};

/* global variables */
/* options */
extern size_t piece_length;	/* piece length */
extern char *announce_url;	/* announce URL */
extern char *comment;		/* optional comment to add to the metafile */
extern char *torrent_name;	/* name of the torrent (name of directory) */
extern char *metafile_path;	/* absolute path to the metafile we're creating */
extern int verbose;		/* be verbose */
extern int no_creation_date;	/* don't write the creation date */

/* information calculated by read_dir() */
extern unsigned long long size;	/* combined size of all files in the torrent */
extern fl_node file_list;	/* list of files and their individual sizes */
extern unsigned int pieces;	/* number of pieces */
