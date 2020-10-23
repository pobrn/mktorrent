#ifndef MKTORRENT_MKTORRENT_H
#define MKTORRENT_MKTORRENT_H

#ifdef _WIN32
#define DIRSEP      "\\"
#define DIRSEP_CHAR '\\'
#else
#define DIRSEP      "/"
#define DIRSEP_CHAR '/'
#endif

/* number of bytes in one MB */
#define ONEMEG 1048576

/* max torrent size in MB for a given piece length in bits */
/* where an X bit piece length equals a 2^X byte piece size */
#define BIT23MAX 12800
#define BIT22MAX 6400
#define BIT21MAX 3200
#define BIT20MAX 1600
#define BIT19MAX 800
#define BIT18MAX 400
#define BIT17MAX 200
#define BIT16MAX 100
#define BIT15MAX 50

#include <stdint.h>

#include "ll.h"

struct file_data {
	char *path;
	uintmax_t size;
};

struct metafile {
	/* options */
	unsigned int piece_length; /* piece length */
	struct ll *announce_list;  /* announce URLs */
	char *comment;             /* optional comment */
	const char *torrent_name;  /* name of torrent (name of directory) */
	char *metainfo_file_path;  /* absolute path to the metainfo file */
	struct ll *web_seed_list;  /* web seed URLs */
	int target_is_directory;   /* target is a directory */
	int no_creation_date;      /* don't write the creation date */
	int private;               /* set the private flag */
	char *source;              /* set source for private trackers */
	int cross_seed;            /* ensure info hash is unique for easier cross-seeding */
	int verbose;               /* be verbose */
	int force_overwrite;       /* overwrite existing output file */
#ifdef USE_PTHREADS
	long threads;              /* number of threads used for hashing */
#endif

	/* information calculated by read_dir() */
	uintmax_t size;              /* combined size of all files */
	struct ll *file_list;      /* list of files and their sizes */
	unsigned int pieces;       /* number of pieces */
};

#endif /* MKTORRENT_MKTORRENT_H */
