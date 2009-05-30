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
#include <stdlib.h>      /* exit(), malloc() */
#include <sys/types.h>   /* off_t */
#include <errno.h>       /* errno */
#include <string.h>      /* strerror() */
#include <stdio.h>       /* printf() etc. */
#include <fcntl.h>       /* open() */
#include <unistd.h>      /* access(), read(), close() */
#ifdef USE_OPENSSL
#include <openssl/sha.h> /* SHA1() */
#else
#include <inttypes.h>
#include "sha1.h"
#endif
#include <pthread.h>     /* pthread functions and data structures */

#include "mktorrent.h"

#define EXPORT
#endif /* ALLINONE */


#ifndef PROGRESS_PERIOD
#define PROGRESS_PERIOD 200000
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct piece_s;
typedef struct piece_s piece_t;
struct piece_s {
	piece_t *next;
	unsigned char *dest;
	unsigned long len;
	unsigned char data[1];
};

struct queue_s;
typedef struct queue_s queue_t;
struct queue_s {
	piece_t *free;
	piece_t *full;
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

static piece_t *get_free(queue_t *q, size_t piece_length)
{
	piece_t *r;

	pthread_mutex_lock(&q->mutex_free);
	if (q->free) {
		r = q->free;
		q->free = r->next;
	} else if (q->buffers < q->buffers_max) {
		r = malloc(sizeof(piece_t) - 1 + piece_length);
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

static piece_t *get_full(queue_t *q)
{
	piece_t *r;

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

static void put_free(queue_t *q, piece_t *p, unsigned int hashed)
{
	pthread_mutex_lock(&q->mutex_free);
	p->next = q->free;
	q->free = p;
	q->pieces_hashed += hashed;
	pthread_mutex_unlock(&q->mutex_free);
	pthread_cond_signal(&q->cond_full);
}

static void put_full(queue_t *q, piece_t *p)
{
	pthread_mutex_lock(&q->mutex_full);
	p->next = q->full;
	q->full = p;
	pthread_mutex_unlock(&q->mutex_full);
	pthread_cond_signal(&q->cond_empty);
}

static void set_done(queue_t *q)
{
	pthread_mutex_lock(&q->mutex_full);
	q->done = 1;
	pthread_mutex_unlock(&q->mutex_full);
	pthread_cond_broadcast(&q->cond_empty);
}

static void free_buffers(queue_t *q)
{
	piece_t *first = q->free;

	while (first) {
		piece_t *p = first;
		first = p->next;
		free(p);
	}

	q->free = NULL;
}

/*
 * print the progress in a thread of its own
 */
static void *print_progress(void *data)
{
	queue_t *q = data;
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

static void *worker(void *data)
{
	queue_t *q = data;
	piece_t *p;
	SHA_CTX c;

	while ((p = get_full(q))) {
		SHA1_Init(&c);
		SHA1_Update(&c, p->data, p->len);
		SHA1_Final(p->dest, &c);
		put_free(q, p, 1);
	}

	return NULL;
}

static void read_files(metafile_t *m, queue_t *q, unsigned char *pos)
{
	int fd;                         /* file descriptor */
	flist_t *f;                     /* pointer to a place in the file
	                                   list */
	ssize_t r = 0;                  /* number of bytes read from
	                                   file(s) into the read buffer */
#ifndef NO_HASH_CHECK
	off_t counter = 0;              /* number of bytes hashed
	                                   should match size when done */
#endif
	piece_t *p = get_free(q, m->piece_length);

	/* go through all the files in the file list */
	for (f = m->file_list; f; f = f->next) {

		/* open the current file for reading */
#if defined _LARGEFILE_SOURCE && defined O_LARGEFILE
		if ((fd = open(f->path, O_RDONLY | O_BINARY | O_LARGEFILE)) == -1) {
#else
		if ((fd = open(f->path, O_RDONLY | O_BINARY)) == -1) {
#endif
			fprintf(stderr, "Error opening '%s' for reading: %s\n",
					f->path, strerror(errno));
			exit(EXIT_FAILURE);
		}

		while (1) {
			ssize_t d = read(fd, p->data + r, m->piece_length - r);

			if (d < 0) {
				fprintf(stderr, "Error reading from '%s': %s\n",
						f->path, strerror(errno));
				exit(EXIT_FAILURE);
			}

			if (d == 0) /* end of file */
				break;

			r += d;

			if (r == m->piece_length) {
				p->dest = pos;
				p->len = m->piece_length;
				put_full(q, p);
				pos += SHA_DIGEST_LENGTH;
#ifndef NO_HASH_CHECK
				counter += r;
#endif
				r = 0;
				p = get_free(q, m->piece_length);
			}
		}

		/* now close the file */
		if (close(fd)) {
			fprintf(stderr, "Error closing '%s': %s\n",
					f->path, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	/* finally append the hash of the last irregular piece to the hash string */
	if (r) {
		p->dest = pos;
		p->len = r;
		put_full(q, p);
	} else
		put_free(q, p, 0);

#ifndef NO_HASH_CHECK
	counter += r;
	if (counter != m->size) {
		fprintf(stderr, "Counted %" PRIoff " bytes, "
				"but hashed %" PRIoff " bytes. "
				"Something is wrong...\n", m->size, counter);
		exit(EXIT_FAILURE);
	}
#endif
}

EXPORT unsigned char *make_hash(metafile_t *m)
{
	queue_t q = {
		NULL, NULL, 0, 0,
		PTHREAD_MUTEX_INITIALIZER,
		PTHREAD_MUTEX_INITIALIZER,
		PTHREAD_COND_INITIALIZER,
		PTHREAD_COND_INITIALIZER,
		0, 0, 0
	};
	pthread_t print_progress_thread;	/* progress printer thread */
	pthread_t *workers;
	unsigned char *hash_string;		/* the hash string */
	unsigned int i;
	int err;

	workers = malloc(m->threads * sizeof(pthread_t));
	hash_string = malloc(m->pieces * SHA_DIGEST_LENGTH);
	if (workers == NULL || hash_string == NULL)
		return NULL;

	q.pieces = m->pieces;
	q.buffers_max = 3*m->threads;

	/* create worker threads */
	for (i = 0; i < m->threads; i++) {
		err = pthread_create(&workers[i], NULL, worker, &q);
		if (err) {
			fprintf(stderr, "Error creating thread: %s\n",
					strerror(err));
			exit(EXIT_FAILURE);
		}
	}

	/* now set off the progress printer */
	err = pthread_create(&print_progress_thread, NULL, print_progress, &q);
	if (err) {
		fprintf(stderr, "Error creating thread: %s\n",
				strerror(err));
		exit(EXIT_FAILURE);
	}

	/* read files and feed pieces to the workers */
	read_files(m, &q, hash_string);

	/* we're done so stop printing our progress. */
	err = pthread_cancel(print_progress_thread);
	if (err) {
		fprintf(stderr, "Error cancelling thread: %s\n",
				strerror(err));
		exit(EXIT_FAILURE);
	}

	/* inform workers we're done */
	set_done(&q);

	/* wait for workers to finish */
	for (i = 0; i < m->threads; i++) {
		err = pthread_join(workers[i], NULL);
		if (err) {
			fprintf(stderr, "Error joining thread: %s\n",
					strerror(err));
			exit(EXIT_FAILURE);
		}
	}

	free(workers);

	/* the progress printer should be done by now too */
	err = pthread_join(print_progress_thread, NULL);
	if (err) {
		fprintf(stderr, "Error joining thread: %s\n",
				strerror(err));
		exit(EXIT_FAILURE);
	}

	/* destroy mutexes and condition variables */
	pthread_mutex_destroy(&q.mutex_full);
	pthread_mutex_destroy(&q.mutex_free);
	pthread_cond_destroy(&q.cond_empty);
	pthread_cond_destroy(&q.cond_full);

	/* free buffers */
	free_buffers(&q);

	/* ok, let the user know we're done too */
	printf("\rHashed %u of %u pieces.\n", q.pieces_hashed, q.pieces);

	return hash_string;
}
