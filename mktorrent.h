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

#ifndef ALLINONE

/* global variables */
/* options */
extern size_t piece_length;	/* piece length */
extern al_node announce_list;	/* announce URLs */
extern char *comment;		/* optional comment to add to the metainfo */
extern const char *torrent_name;	/* name of the torrent (name of directory) */
extern char *metainfo_file_path;	/* absolute path to the metainfo file */
extern char *web_seed_url;	/* web seed URL */
extern int target_is_directory;	/* target is a directory not just a single file */
extern int no_creation_date;	/* don't write the creation date */
extern int private;		/* set the private flag */
extern int verbose;		/* be verbose */

/* information calculated by read_dir() */
extern unsigned long long size;	/* combined size of all files in the torrent */
extern fl_node file_list;	/* list of files and their individual sizes */
extern unsigned int pieces;	/* number of pieces */

#endif /* ALLINONE */

#endif /* _MKTORRENT_H */
