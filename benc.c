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
#include <stdio.h>     /* printf(), putc().. */

#define EXPORT
#endif /* ALLINONE */

#include "benc.h"

static int
pp_newline(unsigned int indent, FILE *f)
{
	unsigned int i;

	if (putc('\n', f) < 0)
		return EOF;

	for (i = indent; i; i--) {
		if (putc(' ', f) < 0)
			return EOF;
	}

	return indent + 1;
}

static int pp(union be_t *t, unsigned int indent, FILE *f);


/*
 * Off_t
 */

EXPORT union be_t *
be_off_new(off_t n)
{
	struct be_off *t = malloc(sizeof(struct be_off));

	if (t == NULL)
		return NULL;

	t->type = BE_TYPE_OFF_T;
	t->n = n;

	return (union be_t *)t;
}

static int
be_off_cmp(struct be_off *a, struct be_off *b)
{
	return (a->n > b->n) - (a->n < b->n);
}

static int
be_off_fput(struct be_off *t, FILE *f)
{
	int r = fprintf(f, "i%" PRIoff "e", t->n);

	if (r < 0)
		return EOF;

	return r;
}

static int
be_off_pp(struct be_off *t, FILE *f)
{
	int r = fprintf(f, "%" PRIoff, t->n);

	if (r < 0)
		return EOF;

	return r;
}


/*
 * String
 */

EXPORT union be_t *
be_string_new(size_t len)
{
	struct be_string *s = malloc(be_string_size(len));

	if (s == NULL)
		return NULL;

	s->type = BE_TYPE_STRING;
	s->len = len;

	return (union be_t *)s;
}

EXPORT union be_t *
be_string_cpy(const char *str, size_t len)
{
	union be_t *s = be_string_new(len);

	if (s == NULL)
		return NULL;

	memcpy(be_string_get(&s->string), str, len);

	return s;
}

EXPORT union be_t *
be_string_dup(const char *str)
{
	return be_string_cpy(str, strlen(str));
}

static int
be_string_cmp(struct be_string *a, struct be_string *b)
{
	size_t min;
	const char *p = be_string_get(a);
	const char *q = be_string_get(b);

	for (min = (a->len < b->len) ? a->len : b->len; min; min--) {
		int r = (*p > *q) - (*p < *q);
		if (r)
			return r;

		p++;
		q++;
	}

	return (a->len > b->len) - (a->len < b->len);
}

static int
be_strcmp(const char *p, struct be_string *b)
{
	size_t min;
	size_t alen = strlen(p);
	const char *q = be_string_get(b);

	for (min = (alen < b->len) ? alen : b->len; min; min--) {
		int r = (*p > *q) - (*p < *q);
		if (r)
			return r;

		p++;
		q++;
	}

	return (alen > b->len) - (alen < b->len);
}

static int
be_string_fput(struct be_string *s, FILE *f)
{
	int r = fprintf(f, "%u:", s->len);
	size_t d;

	if (r < 0)
		return EOF;

	d = fwrite(be_string_get(s), 1, s->len, f);
	if (ferror(f))
		return EOF;

	return r + d;
}

static int
be_string_pp(struct be_string *s, FILE *f)
{
	size_t d;

	if (putc('"', f) < 0)
		return EOF;

	d = fwrite(be_string_get(s), 1, s->len, f);
	if (ferror(f))
		return EOF;

	if (putc('"', f) < 0)
		return EOF;

	return d + 2;
}


/*
 * List
 */

EXPORT union be_t *
be_list_new()
{
	struct be_list *l = malloc(sizeof(struct be_list));

	if (l == NULL)
		return NULL;

	l->type = BE_TYPE_LIST;
	l->first = NULL;
	l->last = &l->first;

	return (union be_t *)l;
}

static void
be_list_destroy(struct be_list *l)
{
	struct be_list_node *n = l->first;

	while (n) {
	       struct be_list_node *next = n->next;

	       be_destroy(n->v);
	       free(n);

	       n = next;
	}

	free(l);
}

EXPORT enum be_return
be_list_add(union be_t *t, union be_t *v)
{
	struct be_list *l;
	struct be_list_node *n;

	if (t->type != BE_TYPE_LIST || v == NULL)
		return BE_INVALID_ARGS;

	l = &t->list;

	n = malloc(sizeof(struct be_list_node));
	if (n == NULL)
		return BE_MEMORY;

	n->v = v;
	n->next = NULL;

	*l->last = n;
	l->last = &n->next;

	return BE_OK;
}

static int
be_list_cmp(struct be_list *a, struct be_list *b)
{
	struct be_list_node *p;
	struct be_list_node *q;

	for (p = a->first, q = b->first; p && q; p = p->next, q = q->next) {
		int r = be_cmp(p->v, q->v);
		if (r)
			return r;
	}

	return (q == NULL) - (p == NULL);
}

static int
be_list_fput(struct be_list *l, FILE *f)
{
	struct be_list_node *n;
	int r = 2;

	if (putc('l', f) < 0)
		return EOF;

	for (n = l->first; n; n = n->next) {
		int s = be_fput(n->v, f);
		if (s < 0)
			return EOF;

		r += s;
	}

	if (putc('e', f) < 0)
		return EOF;

	return r;
}

static int
be_list_pp(struct be_list *l, unsigned int indent, FILE *f)
{
	struct be_list_node *n;
	int r = 2;
	int s;

	if (putc('[', f) < 0)
		return EOF;

	n = l->first;
	if (n == NULL) {
		if (putc(']', f) < 0)
			return EOF;

		return 2;
	}

	s = pp_newline(indent + 2, f);
	if (s < 0)
		return EOF;
	r += s;

	s = pp(n->v, indent + 2, f);
	if (s < 0)
		return EOF;

	r += s;

	for (n = n->next; n; n = n->next) {
		if (putc(',', f) < 0)
			return EOF;
		r++;

		s = pp_newline(indent + 2, f);
		if (s < 0)
			return EOF;
		r += s;

		s = pp(n->v, indent + 2, f);
		if (s < 0)
			return EOF;

		r += s;
	}

	s = pp_newline(indent, f);
	if (s < 0)
		return EOF;
	r += s;

	if (putc(']', f) < 0)
		return EOF;

	return r;
}


/*
 * Dictionary
 */

EXPORT union be_t *
be_dict_new()
{
	struct be_dict *d = malloc(sizeof(struct be_dict));

	if (d == NULL)
		return NULL;

	d->type = BE_TYPE_DICT;
	d->first = NULL;
	d->last = NULL;

	return (union be_t *)d;
}

static void
be_dict_destroy(struct be_dict *d)
{
	struct be_dict_node *n = d->first;

	while (n) {
		struct be_dict_node *next = n->next;

		free(n->key);
		be_destroy(n->value);
		free(n);

		n = next;
	}
}

static enum be_return
be_dict__add(union be_t *t, struct be_string *key, union be_t *value)
{
	struct be_dict *d;
	struct be_dict_node *n;

	if (t->type != BE_TYPE_DICT || value == NULL)
		return BE_INVALID_ARGS;

	d = &t->dict;

	if (d->last && be_string_cmp(key, d->last->key) <= 0) {
		/* printf("%s <= %s!\n" */
		fwrite(be_string_get(key), 1, key->len, stdout);
		fputs(" <= ", stdout);
		fwrite(be_string_get(d->last->key), 1, d->last->key->len, stdout);
		putc('\n', stdout);
		return BE_INVALID_ARGS;
	}

	n = malloc(sizeof(struct be_dict_node));
	if (n == NULL)
		return BE_MEMORY;

	n->key = key;
	n->value = value;
	n->next = NULL;

	if (d->last == NULL)
		d->first = d->last = n;
	else {
		d->last->next = n;
		d->last = n;
	}

	return BE_OK;
}

EXPORT enum be_return
be_dict_rawadd(union be_t *d, union be_t *key, union be_t *value)
{
	if (key->type != BE_TYPE_STRING)
		return BE_INVALID_ARGS;

	return be_dict__add(d, &key->string, value);
}

EXPORT enum be_return
be_dict_add(union be_t *d, const char *key, union be_t *value)
{
	union be_t *s = be_string_dup(key);

	if (s == NULL)
		return BE_MEMORY;

	return be_dict__add(d, &s->string, value);
}

EXPORT enum be_return
be_dict_set(union be_t *t, const char *key, union be_t *value)
{
	struct be_dict *d;
	struct be_dict_node *n;
	struct be_dict_node **p;
	int r;

	if (t->type != BE_TYPE_DICT || value == NULL)
		return BE_INVALID_ARGS;

	d = &t->dict;
	p = &d->first;
	while (1) {
		if (*p == NULL)
			goto insert;

		r = be_strcmp(key, (*p)->key);
		if (r <= 0)
			break;

		p = &((*p)->next);
	}

	if (r == 0) {
		n = *p;
		be_destroy(n->value);
		n->value = value;
		return BE_OK;
	}

insert:
	n = malloc(sizeof(struct be_dict_node));
	if (n == NULL)
		return BE_MEMORY;

	n->key = (struct be_string *)be_string_dup(key);
	if (n->key == NULL) {
		free(n);
		return BE_MEMORY;
	}
	n->value = value;
	n->next = *p;
	*p = n;

	if (n->next == NULL)
		d->last = n;

	return BE_OK;
}

EXPORT union be_t *
be_dict_get(union be_t *d, const char *key)
{
	struct be_dict_node *n;

	if (d->type != BE_TYPE_DICT)
		return NULL;

	for (n = d->dict.first; n; n = n->next) {
		if (be_strcmp(key, n->key) == 0)
			return n->value;
	}

	return NULL;
}

static int
be_dict_cmp(struct be_dict *a, struct be_dict *b)
{
	return (a > b) - (a < b);
}

static int
be_dict_fput(struct be_dict *d, FILE *f)
{
	struct be_dict_node *n;
	int r = 2;

	if (putc('d', f) < 0)
		return EOF;

	for (n = d->first; n; n = n->next) {
		int s = be_string_fput(n->key, f);
		if (s < 0)
			return EOF;

		r += s;

		s = be_fput(n->value, f);
		if (s < 0)
			return EOF;

		r += s;
	}

	if (putc('e', f) < 0)
		return EOF;

	return r;
}

static int
be_dict_pp(struct be_dict *d, unsigned int indent, FILE *f)
{
	struct be_dict_node *n = d->first;
	int r = 2;
	int s;
	size_t z;

	if (putc('{', f) < 0)
		return EOF;

	if (n == NULL) {
		if (putc('}', f) < 0)
			return EOF;
		return 2;
	}

	s = pp_newline(indent + 2, f);
	if (s < 0)
		return EOF;
	r += s;

	z = fwrite(be_string_get(n->key), 1, n->key->len, f);
	if (z < n->key->len)
		return EOF;
	r += z;

	if (fputs(" : ", f) < 0)
		return EOF;
	r += 3;

	s = pp(n->value, indent + 2, f);
	if (s < 0)
		return EOF;
	r += s;

	for (n = n->next; n; n = n->next) {
		if (putc(',', f) < 0)
			return EOF;
		r++;

		s = pp_newline(indent + 2, f);
		if (s < 0)
			return EOF;
		r += s;

		z = fwrite(be_string_get(n->key), 1, n->key->len, f);
		if (z < n->key->len)
			return EOF;
		r += z;

		if (fputs(" : ", f) < 0)
			return EOF;
		r += 3;

		s = pp(n->value, indent + 2, f);
		if (s < 0)
			return EOF;
		r += s;
	}

	s = pp_newline(indent, f);
	if (s < 0)
		return EOF;
	r += s;

	if (putc('}', f) < 0)
		return EOF;

	return r;
}


/*
 * General
 */

EXPORT void
be_destroy(union be_t *t)
{
	switch (t->type) {
	case BE_TYPE_OFF_T:
	case BE_TYPE_STRING:
		free(t);
		break;

	case BE_TYPE_LIST:
		be_list_destroy(&t->list);
		break;

	case BE_TYPE_DICT:
		be_dict_destroy(&t->dict);
		break;
	}
}

#define two_types(a, b) a | (b * (BE_TYPE_DICT + 1))
EXPORT int
be_cmp(union be_t *a, union be_t *b)
{
	switch (two_types(a->type, b->type)) {
	case two_types(BE_TYPE_OFF_T, BE_TYPE_OFF_T):
		return be_off_cmp(&a->off, &b->off);
	case two_types(BE_TYPE_STRING, BE_TYPE_STRING):
		return be_string_cmp(&a->string, &b->string);
	case two_types(BE_TYPE_LIST, BE_TYPE_LIST):
		return be_list_cmp(&a->list, &b->list);
	case two_types(BE_TYPE_DICT, BE_TYPE_DICT):
		return be_dict_cmp(&a->dict, &b->dict);
	default:
		return (a->type > b->type) - (a->type < b->type);
	}
}
#undef two_types

EXPORT int
be_fput(union be_t *t, FILE *f)
{
	switch (t->type) {
	case BE_TYPE_OFF_T:
		return be_off_fput(&t->off, f);
	case BE_TYPE_STRING:
		return be_string_fput(&t->string, f);
	case BE_TYPE_LIST:
		return be_list_fput(&t->list, f);
	case BE_TYPE_DICT:
		return be_dict_fput(&t->dict, f);
	}

	return EOF;
}

static int
pp(union be_t *t, unsigned int indent, FILE *f)
{
	switch (t->type) {
	case BE_TYPE_OFF_T:
		return be_off_pp(&t->off, f);
	case BE_TYPE_STRING:
		return be_string_pp(&t->string, f);
	case BE_TYPE_LIST:
		return be_list_pp(&t->list, indent, f);
	case BE_TYPE_DICT:
		return be_dict_pp(&t->dict, indent, f);
	}

	return EOF;
}

EXPORT int
be_pp(union be_t *t, FILE *f)
{
	int r = pp(t, 0, f);

	if (r < 0)
		return EOF;

	if (putc('\n', f) < 0)
		return EOF;

	return r + 1;
}
