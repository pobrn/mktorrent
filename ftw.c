#ifndef ALLINONE
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#ifdef _WIN32
#define DIRSEP_CHAR '\\'
#else
#define DIRSEP_CHAR '/'
#endif /* _WIN32 */

#define EXPORT
#endif /* ALLINONE */

#include "ftw.h"

struct dir_state {
	struct dir_state *next;
	struct dir_state *prev;
	char *end;
	DIR *dir;
	off_t offset;
};

static struct dir_state *dir_state_new(struct dir_state *prev,
		struct dir_state *next)
{
	struct dir_state *ds = malloc(sizeof(struct dir_state));

	if (ds == NULL) {
		fprintf(stderr, "Out of memory\n");
		return NULL;
	}

	ds->prev = prev;
	ds->next = next;

	return ds;
}

static unsigned int dir_state_open(struct dir_state *ds, const char *name,
		char *end)
{
	ds->dir = opendir(name);
	if (ds->dir == NULL) {
		fprintf(stderr, "Error opening '%s': %s\n",
				name, strerror(errno));
		return 1;
	}

	ds->end = end;

	return 0;
}

static unsigned int dir_state_reopen(struct dir_state *ds, const char *name)
{
	*ds->end = '\0';
	ds->dir = opendir(name);
	if (ds->dir == NULL) {
		fprintf(stderr, "Error opening '%s': %s\n",
				name, strerror(errno));
		return 1;
	}

	*ds->end = DIRSEP_CHAR;

	seekdir(ds->dir, ds->offset);

	return 0;
}

static unsigned int dir_state_close(struct dir_state *ds)
{
	ds->offset = telldir(ds->dir);
	if (ds->offset < 0) {
		fprintf(stderr, "Error getting dir offset: %s\n",
				strerror(errno));
		return 1;
	}

	if (closedir(ds->dir)) {
		fprintf(stderr, "Error closing directory: %s\n",
				strerror(errno));
		return 1;
	}

	ds->dir = NULL;

	return 0;
}

static unsigned int dir_state_destroyall(struct dir_state *ds, int ret)
{
	while (ds->prev)
		ds = ds->prev;

	do {
		struct dir_state *next = ds->next;
		free(ds);
		ds = next;
	} while (ds);

	return ret;
}

EXPORT int file_tree_walk(const char *dirname, unsigned int nfds,
		file_tree_walk_cb callback, void *data)
{
	char path[PATH_MAX];
	char *end;
	struct dir_state *ds = dir_state_new(NULL, NULL);
	struct dir_state *first_open;
	unsigned int nopen;

	if (ds == NULL)
		return -1;

	/* copy dirname to path */
	end = path;
	while ((*end = *dirname)) {
		dirname++;
		end++;
	}

	/* strip ending directory separators */
	while (end > path && *(end-1) == DIRSEP_CHAR) {
		end--;
		*end = '\0';
	}

	if (dir_state_open(ds, path, end))
		return dir_state_destroyall(ds, -1);

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

			end = ds->end + 1;
			p = de->d_name;
			while ((*end = *p)) {
				p++;
				end++;
			}

			if (stat(path, &sbuf)) {
				fprintf(stderr, "Error stat'ing '%s': %s\n",
						path, strerror(errno));
				return dir_state_destroyall(ds, -1);
			}

			r = callback(path, &sbuf, data);
			if (r)
				return dir_state_destroyall(ds, r);

			if (S_ISDIR(sbuf.st_mode)) {
				if (ds->next == NULL &&
					(ds->next = dir_state_new(ds, NULL)) == NULL)
					return dir_state_destroyall(ds, -1);

				ds = ds->next;

				if (nopen == nfds) {
					if (dir_state_close(first_open))
						return dir_state_destroyall(ds, -1);
					first_open = first_open->next;
					nopen--;
				}

				if (dir_state_open(ds, path, end))
					return dir_state_destroyall(ds, -1);

				nopen++;

				*ds->end = DIRSEP_CHAR;
			}
		} else {
			if (closedir(ds->dir)) {
				*ds->end = '\0';
				fprintf(stderr, "Error closing '%s': %s\n",
					path, strerror(errno));
				return dir_state_destroyall(ds, -1);
			}

			if (ds->prev == NULL)
				break;

			ds = ds->prev;
			nopen--;

			if (ds->dir == NULL) {
				if (dir_state_reopen(ds, path))
					return dir_state_destroyall(ds, -1);
				first_open = ds;
				nopen++;
			}
		}
	}

	return dir_state_destroyall(ds, 0);
}
