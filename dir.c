/*
This file is part of mktorrent
Copyright (C) 2007  Emil Renner Berthing

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
#include <stdlib.h>		/* exit() */
#include <stdio.h>		/* printf() etc. */
#include <unistd.h>		/* getcwd() */
#include <string.h>		/* strlen() etc. */
#include <ftw.h>		/* ftw() */

#include "mktorrent.h"

#ifndef MAX_OPENFD
#define MAX_OPENFD 100		/* Maximum no. of file descriptors ftw will open */
#endif

/*
 * called by ftw() on every file and directory in the subtree
 * counts the number of (readable) files, their commulative size and adds
 * their names and individual sizes to the file list
 */
static int process_node(const char *fpath, const struct stat *sb,
			int typeflag)
{
	const char *path;	/* path to file */
	fl_node *p;		/* pointer to a node in the file list */
	fl_node new_node;	/* place to store a newly created node */

	/* skip directories */
	if (typeflag == FTW_D)
		return 0;

	/* ignore the leading "./" */
	path = fpath + 2;

	/* now path should be a normal file and readable
	   otherwise display a warning and skip it */
	if (typeflag != FTW_F || access(path, R_OK)) {
		fprintf(stderr, "warning: cannot read %s!\n", path);
		return 0;
	}

	if (verbose)
		printf("Adding %s\n", path);

	/* count the commulative size of the files */
	size += sb->st_size;

	/* create a new file list node for the file */
	new_node = malloc(sizeof(fl_node));
	new_node->path = strdup(path);
	new_node->size = sb->st_size;

	/* find where to insert the new node so that the file list
	   remains ordered by the path */
	p = &file_list;
	while (*p && strcmp(path, (*p)->path) > 0)
		p = &((*p)->next);

	/* now insert the node there */
	new_node->next = *p;
	*p = new_node;

	/* insertion sort is a really stupid way of sorting a list,
	   but usually a torrent doesn't contain too many files,
	   so we'll probably be alright ;) */
	return 0;
}

/*
 * read the specified directory to create the file_list, count the size
 * of all the files and calculate the number of pieces
 */
void read_dir(const char *dir)
{
#ifdef DEBUG
	fl_node p;
#endif

	/* change to the specified directory */
	if (chdir(dir)) {
		fprintf(stderr, "error: cannot change directory to %s.\n",
			dir);
		exit(1);
	}

	/* now process all the files in it
	   process_node() will take care of creating the file list
	   and counting the size of all the files */
	if (ftw("./", process_node, MAX_OPENFD)) {
		fprintf(stderr,
			"error: ftw() failed, this shouldn't happen.\n");
		exit(1);
	}
#ifdef DEBUG
	printf("File list consists of:\n");
	for (p = file_list; p; p = p->next) {
		printf("%s: %u\n", p->path, (unsigned) p->size);
	}
	printf("\n");
#endif

	/* calculate the number of pieces
	   pieces = ceiling( size / piece_length ) */
	pieces = (size + piece_length - 1) / piece_length;

	if (verbose) {
		printf("\n%llu bytes in all.\n", size);
		printf("That's %u pieces of %zu bytes each.\n\n", pieces,
		       piece_length);
	}
}
