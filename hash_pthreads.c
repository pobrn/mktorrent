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
#include <stdlib.h>      /* malloc(), free  */
#include <sys/types.h>   /* off_t           */
#include <errno.h>       /* errno           */
#include <string.h>      /* strerror()      */
#include <stdio.h>       /* printf() etc.   */
#include <fcntl.h>       /* open()          */
#include <unistd.h>      /* read(), close() */
#ifdef USE_OPENSSL
#include <openssl/sha.h> /* SHA1() */
#else
#include <inttypes.h>
#include "sha1.h"
#endif
#include <pthread.h>

#define EXPORT
#endif /* ALLINONE */

#include "benc.h"
#include "fiter.h"

#ifndef PROGRESS_PERIOD
#define PROGRESS_PERIOD 200000
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#if defined _LARGEFILE_SOURCE && defined O_LARGEFILE
#define OPENFLAGS (O_RDONLY | O_BINARY | O_LARGEFILE)
#else
#define OPENFLAGS (O_RDONLY | O_BINARY)
#endif

struct piece {
	struct piece *next;
	unsigned char *dest;
	unsigned long len;
	unsigned char data[1];
};

struct queue {
	struct piece *free;
	struct piece *full;
	unsigned int buffers_max;
	unsigned int buffers;
	pthread_mutex_t mutex_free;
	pthread_mutex_t mutex_full;
	pthread_cond_t cond_empty;
	pthread_cond_t cond_full;
	unsigned int done;
	unsigned int pieces;
	unsigned int pieces_hashed;
};

static struct piece *
get_free(struct queue *q, size_t piece_length)
{
	struct piece *r;

	pthread_mutex_lock(&q->mutex_free);
	if (q->free) {
		r = q->free;
		q->free = r->next;
	} else if (q->buffers < q->buffers_max) {
		r = malloc(sizeof(struct piece) - 1 + piece_length);
		if (r == NULL) {
			fprintf(stderr, "Out of memory.\n");
			exit(EXIT_FAILURE);
		}

		q->buffers++;
	} else {
		while (q->free == NULL) {
			pthread_cond_wait(&q->cond_full, &q->mutex_free);
		}

		r = q->free;
		q->free = r->next;
	}
	pthread_mutex_unlock(&q->mutex_free);

	return r;
}

static struct piece *
get_full(struct queue *q)
{
	struct piece *r;

	pthread_mutex_lock(&q->mutex_full);
again:
	if (q->full) {
		r = q->full;
		q->full = r->next;
	} else if (q->done) {
		r = NULL;
	} else {
		pthread_cond_wait(&q->cond_empty, &q->mutex_full);
		goto again;
	}
	pthread_mutex_unlock(&q->mutex_full);

	return r;
}

static void
put_free(struct queue *q, struct piece *p, unsigned int hashed)
{
	pthread_mutex_lock(&q->mutex_free);
	p->next = q->free;
	q->free = p;
	q->pieces_hashed += hashed;
	pthread_mutex_unlock(&q->mutex_free);
	pthread_cond_signal(&q->cond_full);
}

static void
put_full(struct queue *q, struct piece *p)
{
	pthread_mutex_lock(&q->mutex_full);
	p->next = q->full;
	q->full = p;
	pthread_mutex_unlock(&q->mutex_full);
	pthread_cond_signal(&q->cond_empty);
}

static void
set_done(struct queue *q)
{
	pthread_mutex_lock(&q->mutex_full);
	q->done = 1;
	pthread_mutex_unlock(&q->mutex_full);
	pthread_cond_broadcast(&q->cond_empty);
}

static void
free_buffers(struct queue *q)
{
	struct piece *first = q->free;

	while (first) {
		struct piece *p = first;

		first = p->next;
		free(p);
	}

	q->free = NULL;
}

static void *
print_progress(void *data)
{
	struct queue *q = data;
	int err;

	err = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	if (err) {
		fprintf(stderr, "Error setting thread cancel type: %s\n",
				strerror(err));
		exit(EXIT_FAILURE);
	}

	while (1) {
		/* print progress and flush the buffer immediately */
		printf("\rHashed %u of %u pieces.", q->pieces_hashed, q->pieces);
		fflush(stdout);
		/* now sleep for PROGRESS_PERIOD microseconds */
		usleep(PROGRESS_PERIOD);
	}

	return NULL;
}

static void *
worker(void *data)
{
	struct queue *q = data;
	struct piece *p;
	SHA_CTX c;

	while ((p = get_full(q))) {
		SHA1_Init(&c);
		SHA1_Update(&c, p->data, p->len);
		SHA1_Final(p->dest, &c);
		put_free(q, p, 1);
	}

	return NULL;
}

static unsigned int
read_files(union be_t *meta, struct queue *q, off_t size,
           unsigned int piece_length, unsigned char *pos)
{
	struct fiter it;
	const char *file;
	int fd;
	ssize_t r = 0;
#ifndef NO_HASH_CHECK
	off_t counter = 0;
#endif
	struct piece *p = get_free(q, piece_length);

	for (file = fiter_first(&it, meta); file; file = fiter_next(&it)) {
		if ((fd = open(file, OPENFLAGS)) == -1) {
			fprintf(stderr, "Error opening '%s' for reading: %s\n",
					file, strerror(errno));
			return 1;
		}

		while (1) {
			ssize_t d = read(fd, p->data + r, piece_length - r);

			if (d < 0) {
				fprintf(stderr, "Error reading from '%s': %s\n",
						file, strerror(errno));
				exit(EXIT_FAILURE);
			}

			if (d == 0) /* end of file */
				break;

			r += d;

			if (r == piece_length) {
				p->dest = pos;
				p->len = piece_length;
				put_full(q, p);
				pos += SHA_DIGEST_LENGTH;
#ifndef NO_HASH_CHECK
				counter += r;
#endif
				r = 0;
				p = get_free(q, piece_length);
			}
		}

		if (close(fd)) {
			fprintf(stderr, "Error closing '%s': %s\n",
					file, strerror(errno));
			return 1;
		}
	}

	if (r) {
		p->dest = pos;
		p->len = r;
		put_full(q, p);
	} else
		put_free(q, p, 0);

#ifndef NO_HASH_CHECK
	counter += r;
	if (counter != size) {
		fprintf(stderr, "Counted %" PRIoff " bytes, "
				"but hashed %" PRIoff " bytes. "
				"Something is wrong...\n", size, counter);
		return 1;
	}
#endif
	return 0;
}

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
make_hash(union be_t *meta, unsigned int threads, unsigned int verbosity)
{
	struct queue q = {
		NULL, NULL, 0, 0,
		PTHREAD_MUTEX_INITIALIZER,
		PTHREAD_MUTEX_INITIALIZER,
		PTHREAD_COND_INITIALIZER,
		PTHREAD_COND_INITIALIZER,
		0, 0, 0
	};
	pthread_t print_progress_thread;
	pthread_t *workers = NULL;
	union be_t *hash_string = NULL;
	unsigned int i;
	int err;
	off_t size;
	unsigned int piece_length;
	unsigned int pieces;

	if (get_info(meta, &size, &piece_length))
		return NULL;

	/* calculate the number of pieces
	   pieces = ceil( size / piece_length ) */
	pieces = (size + piece_length - 1) / piece_length;

	workers = malloc(threads * sizeof(pthread_t));
	hash_string = be_string_new(pieces * SHA_DIGEST_LENGTH);
	if (workers == NULL || hash_string == NULL) {
		fprintf(stderr, "Out of memory.\n");
		goto error;
	}

	q.pieces = pieces;
	q.buffers_max = 3*threads;

	for (i = 0; i < threads; i++) {
		err = pthread_create(&workers[i], NULL, worker, &q);
		if (err) {
			fprintf(stderr, "Error creating thread: %s\n",
					strerror(err));
			goto error;
		}
	}

	if (verbosity > 0) {
		err = pthread_create(&print_progress_thread, NULL,
		                     print_progress, &q);
		if (err) {
			fprintf(stderr, "Error creating thread: %s\n",
					strerror(err));
			goto error;
		}
	}

	if (read_files(meta, &q, size, piece_length,
	               (unsigned char *)be_string_get(hash_string)))
		goto error;

	if (verbosity > 0) {
		err = pthread_cancel(print_progress_thread);
		if (err) {
			fprintf(stderr, "Error cancelling thread: %s\n",
					strerror(err));
			goto error;
		}
	}

	set_done(&q);

	for (i = 0; i < threads; i++) {
		err = pthread_join(workers[i], NULL);
		if (err) {
			fprintf(stderr, "Error joining thread: %s\n",
					strerror(err));
			goto error;
		}
	}

	free(workers);
	workers = NULL;

	if (verbosity > 0) {
		err = pthread_join(print_progress_thread, NULL);
		if (err) {
			fprintf(stderr, "Error joining thread: %s\n",
					strerror(err));
			goto error;
		}
	}

	pthread_mutex_destroy(&q.mutex_full);
	pthread_mutex_destroy(&q.mutex_free);
	pthread_cond_destroy(&q.cond_empty);
	pthread_cond_destroy(&q.cond_full);

	free_buffers(&q);

	if (verbosity > 0) {
		printf("\rHashed %u of %u pieces.\n", q.pieces_hashed, q.pieces);
		fflush(stdout);
	}

	return hash_string;

error:
	set_done(&q);

	(void)pthread_mutex_destroy(&q.mutex_full);
	(void)pthread_mutex_destroy(&q.mutex_free);
	(void)pthread_cond_destroy(&q.cond_empty);
	(void)pthread_cond_destroy(&q.cond_full);

	if (workers) {
		for (i = 0; i < threads; i++) {
		}
		free(workers);
	}

	if (hash_string)
		be_destroy(hash_string);

	return NULL;
}
