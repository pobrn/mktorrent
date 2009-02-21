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
#include <stdlib.h>		/* exit(), malloc() */
#include <stdio.h>		/* printf() etc. */
#include <fcntl.h>		/* open() */
#include <unistd.h>		/* access(), read(), close() */
#include <openssl/sha.h>	/* SHA1() - remember to compile with -lssl */
#include <pthread.h>		/* pthread functions and data structures */

#include "mktorrent.h"

#ifndef PROGRESS_PERIOD
#define PROGRESS_PERIOD 200000
#endif

static unsigned int pieces_done = 0;	/* pieces processed so far */
/* piece to be transferred between threads */
static unsigned char *transfer_piece;
/* mutex only unlocked when transfer_piece contains a newly read piece */
static pthread_mutex_t data_ready_mutex = PTHREAD_MUTEX_INITIALIZER;
/* only unlocked when transfer_piece contains a piece already hashed */
static pthread_mutex_t free_space_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * deliver a free piece buffer and return a new piece to be hashed
 * thread safe
 */
static unsigned char *get_piece(unsigned char *free_buffer)
{
	unsigned char *buf;

	pthread_mutex_lock(&data_ready_mutex);
	buf = transfer_piece;
	transfer_piece = free_buffer;
	pthread_mutex_unlock(&free_space_mutex);
	return buf;
}

/*
 * deliver a newly read piece to be hashed and return a free piece buffer
 * thread safe
 */
static unsigned char *deliver_piece(unsigned char *piece)
{
	unsigned char *buf;

	pthread_mutex_lock(&free_space_mutex);
	buf = transfer_piece;
	transfer_piece = piece;
	pthread_mutex_unlock(&data_ready_mutex);
	return buf;
}

/*
 * print the progress in a thread of its own
 */
static void *print_progress(void *data)
{
	while (1) {
		/* print progress and flush the buffer immediately */
		printf("\rHashed %u/%u pieces.", pieces_done, pieces);
		fflush(stdout);
		/* now sleep for PROGRESS_PERIOD micro seconds */
		usleep(PROGRESS_PERIOD);
	}
	return NULL;
}

/*
 * this thread goes through all the files in the torrent, reads their
 * content in pieces of piece_length and deliver them to the hashing thread
 */
static void *file_reader(void *data)
{
	/* current piece buffer to fill with data from files */
	unsigned char *piece = (unsigned char *) data;
	fl_node p;		/* pointer to a place in the file list */
	int fd;			/* file descriptor */
	ssize_t r = 0;		/* number of bytes read from file(s) into
				   the current piece buffer */
#ifndef NO_HASH_CHECK
	unsigned long long counter = 0;	/* number of bytes hashed
					   should match size when done */
#endif

	/* go through all the files in the file list */
	for (p = file_list; p; p = p->next) {

		/* open the current file for reading */
		if ((fd = open(p->path, O_RDONLY)) == -1) {
			fprintf(stderr,
				"error: couldn't open %s for reading.\n",
				p->path);
			exit(EXIT_FAILURE);
		}

		/* fill the buffer with the contents of the file and deliver
		   it to the hashing thread.
		   repeat until we can't fill the buffer and we've thus come
		   to the end of the file */
		while ((r += read(fd, piece + r, piece_length - r))
		       == piece_length) {
			/* deliver the piece and get a new empty buffer */
			piece = deliver_piece(piece);
#ifndef NO_HASH_CHECK
			/* count the number of bytes read from files */
			counter += r;	/* r == piece_length */
#endif
			/* buffer is now empty and we can start
			   filling it again */
			r = 0;
		}

		/* now close the file */
		if (close(fd)) {
			fprintf(stderr, "error: failed to close %s.",
				p->path);
			exit(EXIT_FAILURE);
		}
	}

	/* deliver the last irregular sized piece, if there is one */
	if (r != 0)
		deliver_piece(piece);

#ifndef NO_HASH_CHECK
	/* now add the last number of read bytes and check if the
	   number of bytes read from files matches size */
	counter += r;
	if (counter != size) {
		fprintf(stderr, "error: counted %llu bytes, "
			"but hashed %llu bytes.\n", size, counter);
		exit(EXIT_FAILURE);
	}
#endif

	return NULL;
}

/*
 * allocate memory for the hash string and buffers,
 * initiate the progress printer and file reader threads then
 * then start hashing the pieces delivered by the file reader thread.
 * the SHA1 hash of every piece is concatenated into the hash string.
 * last piece may be shorter
 */
unsigned char *make_hash()
{

	unsigned char *hash_string,	/* the hash string we're producing */
	*pos,			/* where in hash_string to put the
				   hash of the next piece */
	*piece,			/* the current piece we're hashing */
	*piece1, *piece2, *piece3;	/* allocated piece buffers */
	pthread_t print_progress_thread;	/* progress printer thread */
	pthread_t file_reader_thread;	/* file reader thread */
	unsigned long last_piece_length;	/* length of last piece */


	/* allocate memory for the hash string and set pos to point
	   to its beginning.
	   every SHA1 hash is SHA_DIGEST_LENGTH (20) bytes long */
	pos = hash_string = malloc(pieces * SHA_DIGEST_LENGTH);
	/* allocate memory for 3 pieces */
	piece1 = malloc(piece_length);
	piece2 = malloc(piece_length);
	piece3 = malloc(piece_length);

	/* the data_ready_mutex should be locked initially as there are
	   no new pieces read yet */
	pthread_mutex_lock(&data_ready_mutex);
	/* let the first piece buffer be in transfer initially */
	transfer_piece = piece1;
	/* give the second piece buffer to the file reader thread and
	   set it to work */
	pthread_create(&file_reader_thread, NULL, file_reader, piece2);
	/* we set piece to the third piece for the while loop to begin */
	piece = piece3;

	/* now set off the progress printer */
	pthread_create(&print_progress_thread, NULL, print_progress, NULL);

	/* repeat hashing until only the last piece remains */
	while (pieces_done < pieces - 1) {
		/* deliver the already hashed piece and get a newly read one */
		piece = get_piece(piece);
		/* calculate the SHA1 hash of the piece and write it
		   the right place in the hash string */
		SHA1(piece, piece_length, pos);
		/* next hash should be written 20 bytes further ahead */
		pos += SHA_DIGEST_LENGTH;
		/* yeih! one piece done */
		pieces_done++;
	}

	/* get the last piece */
	piece = get_piece(piece);
	/* calculate the size of the last piece */
	last_piece_length = size % piece_length;
	if (last_piece_length == 0)
		last_piece_length = piece_length;

	/* now write its hash to the hash string */
	SHA1(piece, last_piece_length, pos);
	/* yeih! we're done */
	pieces_done++;
	/* ..so stop printing our progress. */
	pthread_cancel(print_progress_thread);
	/* ok, let the user know we're done too */
	printf("\rHashed %u/%u pieces.\n", pieces_done, pieces);

	/* the file reader thread stops itself when it's done. */

	/* free the piece buffers before we return */
	free(piece1);
	free(piece2);
	free(piece3);

	/* return our shiny new hash string */
	return hash_string;
}
