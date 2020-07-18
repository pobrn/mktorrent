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


#include <stdlib.h>       /* exit() */
#include <errno.h>        /* errno */
#include <string.h>       /* strerror() */
#include <stdio.h>        /* printf() etc. */
#include <fcntl.h>        /* open() */
#include <unistd.h>       /* read(), close() */
#include <inttypes.h>     /* PRId64 etc. */

#ifdef USE_OPENSSL
#include <openssl/sha.h>  /* SHA1() */
#else
#include "sha1.h"
#endif

#include "export.h"
#include "mktorrent.h"
#include "hash.h"
#include "msg.h"
#include "ll.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#if defined _LARGEFILE_SOURCE && defined O_LARGEFILE
#define OPENFLAGS (O_RDONLY | O_BINARY | O_LARGEFILE)
#else
#define OPENFLAGS (O_RDONLY | O_BINARY)
#endif


/*
 * go through the files in file_list, split their contents into pieces
 * of size piece_length and create the hash string, which is the
 * concatenation of the (20 byte) SHA1 hash of every piece
 * last piece may be shorter
 */
EXPORT unsigned char *make_hash(struct metafile *m)
{
	unsigned char *hash_string;     /* the hash string */
	unsigned char *pos;             /* position in the hash string */
	unsigned char *read_buf;        /* read buffer */
	int fd;                         /* file descriptor */
	size_t r;                       /* number of bytes read from file(s) into
	                                   the read buffer */
	SHA_CTX c;                      /* SHA1 hashing context */
#ifndef NO_HASH_CHECK
	uintmax_t counter = 0;          /* number of bytes hashed
	                                   should match size when done */
#endif

	/* allocate memory for the hash string
	   every SHA1 hash is SHA_DIGEST_LENGTH (20) bytes long */
	hash_string = malloc(m->pieces * SHA_DIGEST_LENGTH);
	/* allocate memory for the read buffer to store 1 piece */
	read_buf = malloc(m->piece_length);

	/* check if we've run out of memory */
	FATAL_IF0(hash_string == NULL || read_buf == NULL, "out of memory\n");

	/* initiate pos to point to the beginning of hash_string */
	pos = hash_string;
	/* and initiate r to 0 since we haven't read anything yet */
	r = 0;
	/* go through all the files in the file list */
	LL_FOR(file_node, m->file_list) {
		struct file_data *f = LL_DATA_AS(file_node, struct file_data*);

		/* open the current file for reading */
		FATAL_IF((fd = open(f->path, OPENFLAGS)) == -1,
			"cannot open '%s' for reading: %s\n", f->path, strerror(errno));
		printf("hashing %s\n", f->path);
		fflush(stdout);

		/* fill the read buffer with the contents of the file and append
		   the SHA1 hash of it to the hash string when the buffer is full.
		   repeat until we can't fill the read buffer and we've thus come
		   to the end of the file */
		while (1) {
			ssize_t d = read(fd, read_buf + r, m->piece_length - r);
			FATAL_IF(d < 0, "cannot read from '%s': %s\n",
				f->path, strerror(errno));

			if (d == 0) /* end of file */
				break;

			r += d;

			if (r == m->piece_length) {
				SHA1_Init(&c);
				SHA1_Update(&c, read_buf, m->piece_length);
				SHA1_Final(pos, &c);
				pos += SHA_DIGEST_LENGTH;
#ifndef NO_HASH_CHECK
				counter += r;	/* r == piece_length */
#endif
				r = 0;
			}
		}

		/* now close the file */
		FATAL_IF(close(fd), "cannot close '%s': %s\n",
			f->path, strerror(errno));
	}

	/* finally append the hash of the last irregular piece to the hash string */
	if (r) {
		SHA1_Init(&c);
		SHA1_Update(&c, read_buf, r);
		SHA1_Final(pos, &c);
	}

#ifndef NO_HASH_CHECK
	counter += r;
	FATAL_IF(counter != m->size,
		"counted %" PRIuMAX " bytes, but hashed %" PRIuMAX " bytes; "
		"something is wrong...\n",
			m->size, counter);
#endif

	/* free the read buffer before we return */
	free(read_buf);

	return hash_string;
}
