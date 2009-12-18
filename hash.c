/*
This file is part of mktorrent
Copyright (C) 2007, 2009 Emil Renner Berthing

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
#include <stdlib.h>       /* malloc(), free() */
#include <sys/types.h>    /* off_t            */
#include <errno.h>        /* errno            */
#include <string.h>       /* strerror()       */
#include <stdio.h>        /* printf() etc.    */
#include <fcntl.h>        /* open()           */
#include <unistd.h>       /* read(), close()  */

#ifdef USE_OPENSSL
#include <openssl/sha.h>
#else
#include <inttypes.h>
#include "sha1.h"
#endif

#define EXPORT
#endif /* ALLINONE */

#include "benc.h"
#include "fiter.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#if defined _LARGEFILE_SOURCE && defined O_LARGEFILE
#define OPENFLAGS (O_RDONLY | O_BINARY | O_LARGEFILE)
#else
#define OPENFLAGS (O_RDONLY | O_BINARY)
#endif

static unsigned int
get_info(union be_t *meta, off_t *size, unsigned int *piece_length)
{
	union be_t *info = be_dict_get(meta, "info");
	union be_t *files;
	struct be_list_node *n;
	off_t total = 0;

	if (info == NULL || info->type != BE_TYPE_DICT) {
		fprintf(stderr, "No info section found\n");
		return 1;
	}

	files = be_dict_get(info, "piece length");
	if (files == NULL || files->type != BE_TYPE_OFF_T) {
		fprintf(stderr, "No piece length found\n");
		return 1;
	}
	*piece_length = files->off.n;

	files = be_dict_get(info, "files");
	if (files == NULL || files->type != BE_TYPE_LIST) {
		union be_t *length = be_dict_get(info, "length");

		if (length == NULL || length->type != BE_TYPE_OFF_T) {
			fprintf(stderr, "No length specified\n");
			return 1;
		}

		*size = length->off.n;
		return 0;
	}

	for (n = files->list.first; n; n = n->next) {
		union be_t *length = be_dict_get(n->v, "length");

		if (length == NULL || length->type != BE_TYPE_OFF_T) {
			fprintf(stderr, "Found file with no length specified\n");
			return 1;
		}

		total += length->off.n;
	}

	*size = total;
	return 0;
}

EXPORT union be_t *
make_hash(union be_t *meta, unsigned int verbosity)
{
	struct fiter it;
	const char *file;
	union be_t *hash_string;
	unsigned char *pos;
	unsigned char *read_buf;
	int fd;
	ssize_t r;
	SHA_CTX c;
#ifndef NO_HASH_CHECK
	off_t counter = 0;
#endif
	off_t size;
	unsigned int piece_length;
	unsigned int pieces;

	if (get_info(meta, &size, &piece_length))
		return NULL;

	/* calculate the number of pieces
	   pieces = ceil( size / piece_length ) */
	pieces = (size + piece_length - 1) / piece_length;

	hash_string = be_string_new(pieces * SHA_DIGEST_LENGTH);
	if (hash_string == NULL) {
		fprintf(stderr, "Out of memory.\n");
		return NULL;
	}

	read_buf = malloc(piece_length);
	if (read_buf == NULL) {
		be_destroy(hash_string);
		fprintf(stderr, "Out of memory.\n");
		return NULL;
	}

	pos = (unsigned char *)be_string_get(&hash_string->string);
	r = 0;
	for (file = fiter_first(&it, meta); file; file = fiter_next(&it)) {
		if ((fd = open(file, OPENFLAGS)) == -1) {
			fprintf(stderr, "Error opening '%s' for reading: %s\n",
					file, strerror(errno));
			goto error;
		}

		if (verbosity > 0) {
			printf("Hashing %s.\n", file);
			fflush(stdout);
		}

		while (1) {
			ssize_t d = read(fd, read_buf + r, piece_length - r);

			if (d < 0) {
				fprintf(stderr, "Error reading from '%s': %s\n",
						file, strerror(errno));
				goto error;
			}

			if (d == 0) /* end of file */
				break;

			r += d;

			if (r == piece_length) {
				SHA1_Init(&c);
				SHA1_Update(&c, read_buf, piece_length);
				SHA1_Final(pos, &c);
				pos += SHA_DIGEST_LENGTH;
#ifndef NO_HASH_CHECK
				counter += r;	/* r == piece_length */
#endif
				r = 0;
			}
		}

		/* now close the file */
		if (close(fd)) {
			fprintf(stderr, "Error closing '%s': %s\n",
					file, strerror(errno));
			goto error;
		}
	}


	/* finally append the hash of the last irregular piece to the hash string */
	if (r) {
		SHA1_Init(&c);
		SHA1_Update(&c, read_buf, r);
		SHA1_Final(pos, &c);
	}

#ifndef NO_HASH_CHECK
	counter += r;
	if (counter != size) {
		fprintf(stderr, "Counted %" PRIoff " bytes, "
				"but hashed %" PRIoff " bytes. "
				"Something is wrong...\n", size, counter);
		goto error;
	}
#endif

	/* free the read buffer before we return */
	free(read_buf);

	return hash_string;

error:
	be_destroy(hash_string);
	free(read_buf);
	return NULL;
}
