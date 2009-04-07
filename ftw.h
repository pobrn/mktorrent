#ifndef _FTW_H
#define _FTW_H

typedef int (*file_tree_walk_cb)(const char *name,
		const struct stat *sbuf, void *data);

#ifndef ALLINONE
int file_tree_walk(const char *dirname, unsigned int nfds,
		file_tree_walk_cb callback, void *data);

#endif /* ALLINONE */

#endif /* _FTW_H */
