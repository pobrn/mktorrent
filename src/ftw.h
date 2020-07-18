#ifndef MKTORRENT_FTW_H
#define MKTORRENT_FTW_H

#include "export.h"

typedef int (*file_tree_walk_cb)(const char *name,
		const struct stat *sbuf, void *data);

EXPORT int file_tree_walk(const char *dirname, unsigned int nfds,
		file_tree_walk_cb callback, void *data);


#endif /* MKTORRENT_FTW_H */
