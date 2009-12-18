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
#include <stdio.h>     /* getc()             */

#define EXPORT
#endif /* ALLINONE */

#include "benc.h"

enum classes {
	C_____,
	C_NUMB,
	C_DIGT,
	C_DICT,
	C_LIST,
	C_COLN,
	C_ENDM
};


static const unsigned char ascii_class[128] = {
	C_____, C_____, C_____, C_____, C_____, C_____, C_____, C_____,
	C_____, C_____, C_____, C_____, C_____, C_____, C_____, C_____,
	C_____, C_____, C_____, C_____, C_____, C_____, C_____, C_____,
	C_____, C_____, C_____, C_____, C_____, C_____, C_____, C_____,

	C_____, C_____, C_____, C_____, C_____, C_____, C_____, C_____,
	C_____, C_____, C_____, C_____, C_____, C_____, C_____, C_____,
	C_DIGT, C_DIGT, C_DIGT, C_DIGT, C_DIGT, C_DIGT, C_DIGT, C_DIGT,
	C_DIGT, C_DIGT, C_COLN, C_____, C_____, C_____, C_____, C_____,

	C_____, C_____, C_____, C_____, C_____, C_____, C_____, C_____,
	C_____, C_____, C_____, C_____, C_____, C_____, C_____, C_____,
	C_____, C_____, C_____, C_____, C_____, C_____, C_____, C_____,
	C_____, C_____, C_____, C_____, C_____, C_____, C_____, C_____,

	C_____, C_____, C_____, C_____, C_DICT, C_ENDM, C_____, C_____,
	C_____, C_NUMB, C_____, C_____, C_LIST, C_____, C_____, C_____,
	C_____, C_____, C_____, C_____, C_____, C_____, C_____, C_____,
	C_____, C_____, C_____, C_____, C_____, C_____, C_____, C_____
};

static union be_t *
be_parse_number(FILE *f)
{
	int c;
	off_t n = 0;

again:
	c = getc(f);
	if (c < 0 || c >= 128)
		return NULL;

	switch (ascii_class[c]) {
	case C_DIGT:
		n *= 10;
		n += c - '0';
		goto again;
	case C_ENDM:
		return be_off_new(n);
	}

	return NULL;
}

static union be_t *
be_parse_string(FILE *f, int c)
{
	union be_t *s;
	size_t len = c - '0';

again:
	c = getc(f);
	if (c < 0 || c >= 128)
		return NULL;

	switch (ascii_class[c]) {
	case C_DIGT:
		len *= 10;
		len += c - '0';
		goto again;
	case C_COLN:
		break;
	default:
		return NULL;
	}

	s = be_string_new(len);
	if (s == NULL)
		return NULL;

	if (fread(be_string_get(&s->string), 1, len, f) < len) {
		be_destroy(s);
		return NULL;
	}

	return s;
}

static union be_t *be_parse_dict(FILE *f);

static union be_t *
be_parse_list(FILE *f)
{
	union be_t *l = be_list_new();

	if (l == NULL)
		return NULL;

	while (1) {
		union be_t *v;
		int c = getc(f);

		if (c < 0 || c >= 128)
			goto error;

		switch (ascii_class[c]) {
		case C_NUMB: v = be_parse_number(f);    break;
		case C_DIGT: v = be_parse_string(f, c); break;
		case C_LIST: v = be_parse_list(f);      break;
		case C_DICT: v = be_parse_dict(f);      break;
		case C_ENDM: return l;
		default:     goto error;
		}

		if (be_list_add(l, v)) {
			if (v)
				be_destroy(v);
			goto error;
		}
	}
error:
	be_destroy(l);
	return NULL;
}

static union be_t *
be_parse_dict(FILE *f)
{
	union be_t *d = be_dict_new();

	if (d == NULL)
		return NULL;

	while (1) {
		union be_t *key;
		union be_t *value;
		int c = getc(f);

		if (c < 0 || c >= 128)
			goto error;

		switch (ascii_class[c]) {
		case C_DIGT: break;
		case C_ENDM: return d;
		default:     goto error;
		}

		key = be_parse_string(f, c);
		if (key == NULL)
			goto error;

		c = getc(f);
		if (c < 0 || c >= 128) {
			be_destroy(key);
			goto error;
		}

		switch (ascii_class[c]) {
		case C_NUMB: value = be_parse_number(f);    break;
		case C_DIGT: value = be_parse_string(f, c); break;
		case C_LIST: value = be_parse_list(f);      break;
		case C_DICT: value = be_parse_dict(f);      break;
		default:
			     be_destroy(key);
			     goto error;
		}

		if (be_dict_rawadd(d, key, value)) {
			be_destroy(key);
			if (value)
				be_destroy(value);
			goto error;
		}
	}
error:
	be_destroy(d);
	return NULL;
}

EXPORT union be_t *
be_parse(FILE *f)
{
	int c = getc(f);

	if (c < 0 || c >= 128)
		return NULL;

	switch (ascii_class[c]) {
	case C_NUMB: return be_parse_number(f);
	case C_DIGT: return be_parse_string(f, c);
	case C_LIST: return be_parse_list(f);
	case C_DICT: return be_parse_dict(f);
	default:     return NULL;
	}
}
