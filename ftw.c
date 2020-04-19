/*
This file is part of mktorrent
Copyright (C) 2009 Emil Renner Berthing

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


#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "export.h"
#include "mktorrent.h" /* DIRSEP_CHAR */
#include "ftw.h"


struct dir_state {
	struct dir_state *next;
	struct dir_state *prev;
	size_t length;
	DIR *dir;
	off_t offset;
};

static struct dir_state *dir_state_new(struct dir_state *prev,
		struct dir_state *next)
{
	struct dir_state *ds = malloc(sizeof(struct dir_state));

	if (ds == NULL) {
		fprintf(stderr, "fatal error: out of memory\n");
		return NULL;
	}

	ds->prev = prev;
	ds->next = next;

	return ds;
}

static unsigned int dir_state_open(struct dir_state *ds, const char *name,
		size_t length)
{
	ds->dir = opendir(name);
	if (ds->dir == NULL) {
		fprintf(stderr, "fatal error: cannot open '%s': %s\n",
				name, strerror(errno));
		return 1;
	}

	ds->length = length;

	return 0;
}

static unsigned int dir_state_reopen(struct dir_state *ds, char *name)
{
	name[ds->length] = '\0';
	ds->dir = opendir(name);
	if (ds->dir == NULL) {
		fprintf(stderr, "fatal error: cannot open '%s': %s\n",
				name, strerror(errno));
		return 1;
	}

	name[ds->length] = DIRSEP_CHAR;

	seekdir(ds->dir, ds->offset);

	return 0;
}

static unsigned int dir_state_close(struct dir_state *ds)
{
	ds->offset = telldir(ds->dir);
	if (ds->offset < 0) {
		fprintf(stderr, "fatal error: cannot obtain dir offset: %s\n",
				strerror(errno));
		return 1;
	}

	if (closedir(ds->dir)) {
		fprintf(stderr, "fatal error: cannot close directory: %s\n",
				strerror(errno));
		return 1;
	}

	ds->dir = NULL;

	return 0;
}

static unsigned int cleanup(struct dir_state *ds, char *path, int ret)
{
	while (ds->prev)
		ds = ds->prev;

	do {
		struct dir_state *next = ds->next;
		free(ds);
		ds = next;
	} while (ds);

	if (path)
		free(path);

	return ret;
}

EXPORT int file_tree_walk(const char *dirname, unsigned int nfds,
		file_tree_walk_cb callback, void *data)
{
	size_t path_size = 256;
	char *path;
	char *path_max;
	char *end;
	struct dir_state *ds = dir_state_new(NULL, NULL);
	struct dir_state *first_open;
	unsigned int nopen;

	if (ds == NULL)
		return -1;

	path = malloc(path_size);
	if (path == NULL) {
		fprintf(stderr, "fatal error: out of memory\n");
		return cleanup(ds, NULL, -1);
	}
	path_max = path + path_size;

	/* copy dirname to path */
	end = path;
	while ((*end = *dirname)) {
		dirname++;
		end++;
		if (end == path_max) {
			char *new_path;

			new_path = realloc(path, 2*path_size);
			if (new_path == NULL) {
				fprintf(stderr, "fatal error: out of memory\n");
				return cleanup(ds, path, -1);
			}
			end = new_path + path_size;
			path_size *= 2;
			path = new_path;
			path_max = path + path_size;
		}
	}

	/* strip ending directory separators */
	while (end > path && *(end-1) == DIRSEP_CHAR) {
		end--;
		*end = '\0';
	}

	if (dir_state_open(ds, path, end - path))
		return cleanup(ds, path, -1);

	first_open = ds;
	nopen = 1;

	*end = DIRSEP_CHAR;

	while (1) {
		struct dirent *de = readdir(ds->dir);

		if (de) {
			struct stat sbuf;
			const char *p;
			int r;

			if (de->d_name[0] == '.'
					&& (de->d_name[1] == '\0'
					|| (de->d_name[1] == '.'
					&& de->d_name[2] == '\0'))) {
				continue;
			}

			end = path + ds->length + 1;
			p = de->d_name;
			while ((*end = *p)) {
				p++;
				end++;
				if (end == path_max) {
					char *new_path;

					new_path = realloc(path, 2*path_size);
					if (new_path == NULL) {
						fprintf(stderr, "fatal error: out of memory\n");
						return cleanup(ds, path, -1);
					}
					end = new_path + path_size;
					path_size *= 2;
					path = new_path;
					path_max = path + path_size;
				}
			}

			if (stat(path, &sbuf)) {
				fprintf(stderr, "fatal error: cannot stat '%s': %s\n",
						path, strerror(errno));
				return cleanup(ds, path, -1);
			}

			r = callback(path, &sbuf, data);
			if (r)
				return cleanup(ds, path, r);

			if (S_ISDIR(sbuf.st_mode)) {
				if (ds->next == NULL &&
					(ds->next = dir_state_new(ds, NULL)) == NULL)
					return cleanup(ds, path, -1);

				ds = ds->next;

				if (nopen == nfds) {
					if (dir_state_close(first_open))
						return cleanup(ds, path, -1);
					first_open = first_open->next;
					nopen--;
				}

				if (dir_state_open(ds, path, end - path))
					return cleanup(ds, path, -1);

				nopen++;

				*end = DIRSEP_CHAR;
			}
		} else {
			if (closedir(ds->dir)) {
				path[ds->length] = '\0';
				fprintf(stderr, "fatal error: cannot close '%s': %s\n",
					path, strerror(errno));
				return cleanup(ds, path, -1);
			}

			if (ds->prev == NULL)
				break;

			ds = ds->prev;
			nopen--;

			if (ds->dir == NULL) {
				if (dir_state_reopen(ds, path))
					return cleanup(ds, path, -1);
				first_open = ds;
				nopen++;
			}
		}
	}

	return cleanup(ds, path, 0);
}
