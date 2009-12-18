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
#ifndef ALLINONE
#include <stdlib.h>    /* malloc(), free()   */
#include <sys/types.h> /* off_t              */
#include <string.h>    /* strlen(), memcpy() */
#include <stdio.h>     /* FILE               */

#ifdef _WIN32
#define DIRSEP_CHAR '\\'
#else
#define DIRSEP_CHAR '/'
#endif /* _WIN32 */

#define EXPORT
#endif /* ALLINONE */

#include "benc.h"
#include "fiter.h"

static const char *
fiter_destruct(struct fiter *it)
{
	if (it->buf)
		free(it->buf);
	return NULL;
}

static unsigned int
construct_filename(struct fiter *it, union be_t *file)
{
	union be_t *path;
	struct be_list_node *n;
	char *p;
	size_t len = 0;

	if (file->type != BE_TYPE_DICT)
		return 1;

	path = be_dict_get(file, "path");
	if (path == NULL
	    || path->type != BE_TYPE_LIST
	    || path->list.first == NULL)
		return 1;

	for (n = path->list.first; n; n = n->next) {
		if (n->v->type != BE_TYPE_STRING)
			return 1;

		len += n->v->string.len + 1;
	}

	if (len > it->size) {
		free(it->buf);
		it->size = 2 * len;
		it->buf = malloc(it->size);
		if (it->buf == NULL)
			return 1;
	}

	p = it->buf;
	for (n = path->list.first; n; n = n->next) {
		size_t len = n->v->string.len;

		memcpy(p, be_string_get(&(n->v->string)), len);
		p += len;

		*p = DIRSEP_CHAR;
		p++;
	}

	p--;
	*p = '\0';

	return 0;
}

EXPORT const char *
fiter_first(struct fiter *it, union be_t *meta)
{
	union be_t *info;
	union be_t *files;

	info = be_dict_get(meta, "info");
	if (info == NULL)
		return NULL;

	files = be_dict_get(info, "files");
	if (files == NULL) {
		union be_t *name = be_dict_get(info, "name");

		if (name == NULL)
			return NULL;

		it->n = NULL;
		it->size = name->string.len + 1;
		it->buf = malloc(it->size);
		if (it->buf == NULL)
			return NULL;

		memcpy(it->buf, be_string_get(&name->string), name->string.len);
		it->buf[name->string.len] = '\0';
		return it->buf;
	}

	it->size = 256;
	it->buf = malloc(256);
	if (it->buf == NULL)
		return NULL;

	if (files->type != BE_TYPE_LIST)
		return fiter_destruct(it);

	it->n = files->list.first;

	if (construct_filename(it, it->n->v))
		return fiter_destruct(it);

	it->n = it->n->next;
	return it->buf;
}

EXPORT const char *
fiter_next(struct fiter *it)
{
	if (it->n == NULL)
		return fiter_destruct(it);

	if (construct_filename(it, it->n->v))
		return fiter_destruct(it);

	it->n = it->n->next;
	return it->buf;
}
