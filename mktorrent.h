#ifndef MKTORRENT_MKTORRENT_H
#define MKTORRENT_MKTORRENT_H

#ifdef _WIN32
#define DIRSEP      "\\"
#define DIRSEP_CHAR '\\'
#else
#define DIRSEP      "/"
#define DIRSEP_CHAR '/'
#endif

/* string list */
struct slist_s;
typedef struct slist_s slist_t;
struct slist_s {
	char *s;
	slist_t *next;
};

/* list of string lists */
struct llist_s;
typedef struct llist_s llist_t;
struct llist_s {
	slist_t *l;
	llist_t *next;
};

/* file list */
struct flist_s;
typedef struct flist_s flist_t;
struct flist_s {
	char *path;
	int64_t size;
	flist_t *next;
};

struct metafile {
	/* options */
	unsigned int piece_length; /* piece length */
	llist_t *announce_list;    /* announce URLs */
	char *comment;             /* optional comment */
	const char *torrent_name;  /* name of torrent (name of directory) */
	char *metainfo_file_path;  /* absolute path to the metainfo file */
	slist_t *web_seed_list;    /* web seed URLs */
	int target_is_directory;   /* target is a directory */
	int no_creation_date;      /* don't write the creation date */
	int private;               /* set the private flag */
	char *source;              /* set source for private trackers */
	int verbose;               /* be verbose */
#ifdef USE_PTHREADS
	unsigned int threads;      /* number of threads used for hashing */
#endif

	/* information calculated by read_dir() */
	int64_t size;                /* combined size of all files */
	flist_t *file_list;        /* list of files and their sizes */
	unsigned int pieces;       /* number of pieces */
};

#endif /* MKTORRENT_MKTORRENT_H */
