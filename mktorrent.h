#ifndef _MKTORRENT_H
#define _MKTORRENT_H

#ifdef _WIN32
#define DIRSEP      "\\"
#define DIRSEP_CHAR '\\'
#else
#define DIRSEP      "/"
#define DIRSEP_CHAR '/'
#endif

/* string list */
struct sl_node_s;
typedef struct sl_node_s *sl_node;
struct sl_node_s {
	char *s;
	sl_node next;
};

/* announce list */
struct al_node_s;
typedef struct al_node_s *al_node;
struct al_node_s {
	sl_node l;
	al_node next;
};

/* file list node */
struct fl_node_s;
typedef struct fl_node_s *fl_node;
struct fl_node_s {
	char *path;
	off_t size;
	fl_node next;
};

typedef struct {
	/* options */
	size_t piece_length;		/* piece length */
	al_node announce_list;		/* announce URLs */
	char *comment;			/* optional comment */
	const char *torrent_name;	/* name of torrent (name of directory) */
	char *metainfo_file_path;	/* absolute path to the metainfo file */
	char *web_seed_url;		/* web seed URL */
	int target_is_directory;	/* target is a directory */
	int no_creation_date;		/* don't write the creation date */
	int private;			/* set the private flag */
	int verbose;			/* be verbose */
#ifdef USE_PTHREADS
	unsigned int threads;		/* number of threads used for hashing */
#endif

	/* information calculated by read_dir() */
	unsigned long long size;	/* combined size of all files */
	fl_node file_list;		/* list of files and their sizes */
	unsigned int pieces;		/* number of pieces */
} metafile_t;

#endif /* _MKTORRENT_H */
