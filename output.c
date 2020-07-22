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


#include <sys/types.h>    /* off_t */
#include <stdio.h>        /* fprintf() etc. */
#include <string.h>       /* strlen() etc. */
#include <time.h>         /* time() */
#include <inttypes.h>     /* PRIuMAX */
#include <stdlib.h>       /* random() */

#ifdef USE_OPENSSL
#include <openssl/sha.h>  /* SHA_DIGEST_LENGTH */
#else
#include "sha1.h"
#endif

#include "export.h"       /* EXPORT */
#include "mktorrent.h"    /* struct metafile */
#include "output.h"


/*
 * write announce list
 */
static void write_announce_list(FILE *f, struct ll *list)
{
	/* the announce list is a list of lists of urls */
	fprintf(f, "13:announce-listl");
	/* go through them all.. */
	LL_FOR(tier_node, list) {

		/* .. and print the lists */
		fprintf(f, "l");

		LL_FOR(announce_url_node, LL_DATA_AS(tier_node, struct ll*)) {

			const char *announce_url =
				LL_DATA_AS(announce_url_node, const char*);

			fprintf(f, "%lu:%s",
					(unsigned long) strlen(announce_url), announce_url);
		}

		fprintf(f, "e");
	}
	fprintf(f, "e");
}

/*
 * write file list
 */
static void write_file_list(FILE *f, struct ll *list)
{
	char *a, *b;

	fprintf(f, "5:filesl");

	/* go through all the files */
	LL_FOR(file_node, list) {
		struct file_data *fd = LL_DATA_AS(file_node, struct file_data*);

		/* the file list contains a dictionary for every file
		   with entries for the length and path
		   write the length first */
		fprintf(f, "d6:lengthi%" PRIuMAX "e4:pathl", fd->size);
		/* the file path is written as a list of subdirectories
		   and the last entry is the filename
		   sorry this code is even uglier than the rest */
		a = fd->path;
		/* while there are subdirectories before the filename.. */
		while ((b = strchr(a, DIRSEP_CHAR)) != NULL) {
			/* set the next DIRSEP_CHAR to '\0' so fprintf
			   will only write the first subdirectory name */
			*b = '\0';
			/* print it bencoded */
			fprintf(f, "%lu:%s", b - a, a);
			/* undo our alteration to the string */
			*b = DIRSEP_CHAR;
			/* and move a to the beginning of the next
			   subdir or filename */
			a = b + 1;
		}
		/* now print the filename bencoded and end the
		   path name list and file dictionary */
		fprintf(f, "%lu:%see", (unsigned long)strlen(a), a);
	}

	/* whew, now end the file list */
	fprintf(f, "e");
}

/*
 * write web seed list
 */
static void write_web_seed_list(FILE *f, struct ll *list)
{
	/* print the entry and start the list */
	fprintf(f, "8:url-listl");
	/* go through the list and write each URL */
	LL_FOR(node, list) {
		const char *web_seed_url = LL_DATA_AS(node, const char*);
		fprintf(f, "%lu:%s",
			(unsigned long) strlen(web_seed_url), web_seed_url);
	}
	/* end the list */
	fprintf(f, "e");
}

/*
 * write metainfo to the file stream using all the information
 * we've gathered so far and the hash string calculated
 */
EXPORT void write_metainfo(FILE *f, struct metafile *m, unsigned char *hash_string)
{
	/* let the user know we've started writing the metainfo file */
	printf("writing metainfo file... ");
	fflush(stdout);

	/* every metainfo file is one big dictonary */
	fprintf(f, "d");

	if (!LL_IS_EMPTY(m->announce_list)) {

		struct ll *first_tier =
			LL_DATA_AS(LL_HEAD(m->announce_list), struct ll*);

		/* write the announce URL */
		const char *first_announce_url
			= LL_DATA_AS(LL_HEAD(first_tier), const char*);

		fprintf(f, "8:announce%lu:%s",
			(unsigned long) strlen(first_announce_url), first_announce_url);

		/* write the announce-list entry if we have
		 * more than one announce URL, namely
		 * a) there are at least two tiers, or      (first part of OR)
		 * b) there are at least two URLs in tier 1 (second part of OR)
		 */
		if (LL_NEXT(LL_HEAD(m->announce_list)) || LL_NEXT(LL_HEAD(first_tier)))
			write_announce_list(f, m->announce_list);
	}

	/* add the comment if one is specified */
	if (m->comment != NULL)
		fprintf(f, "7:comment%lu:%s",
				(unsigned long)strlen(m->comment),
				m->comment);
	/* I made this! */
	fprintf(f, "10:created by13:mktorrent " VERSION);
	/* add the creation date */
	if (!m->no_creation_date)
		fprintf(f, "13:creation datei%lde",
			(long)time(NULL));

	/* now here comes the info section
	   it is yet another dictionary */
	fprintf(f, "4:infod");
	/* first entry is either 'length', which specifies the length of a
	   single file torrent, or a list of files and their respective sizes */
	if (!m->target_is_directory)
		fprintf(f, "6:lengthi%" PRIuMAX "e",
			LL_DATA_AS(LL_HEAD(m->file_list), struct file_data*)->size);
	else
		write_file_list(f, m->file_list);

	if (m->cross_seed) {
		fprintf(f, "12:x_cross_seed%u:mktorrent-", CROSS_SEED_RAND_LENGTH * 2 + 10);
		for (int i = 0; i < CROSS_SEED_RAND_LENGTH; i++) {
			unsigned char rand_byte = random();
			fputc("0123456789ABCDEF"[rand_byte >> 4], f);
			fputc("0123456789ABCDEF"[rand_byte & 0x0F], f);
		}
	}

	/* the info section also contains the name of the torrent,
	   the piece length and the hash string */
	fprintf(f, "4:name%lu:%s12:piece lengthi%ue6:pieces%u:",
		(unsigned long)strlen(m->torrent_name), m->torrent_name,
		m->piece_length, m->pieces * SHA_DIGEST_LENGTH);
	fwrite(hash_string, 1, m->pieces * SHA_DIGEST_LENGTH, f);

	/* set the private flag */
	if (m->private)
		fprintf(f, "7:privatei1e");

	if (m->source)
		fprintf(f, "6:source%lu:%s",
			(unsigned long) strlen(m->source), m->source);

	/* end the info section */
	fprintf(f, "e");

	/* add url-list if one is specified */
	if (!LL_IS_EMPTY(m->web_seed_list)) {
		if (LL_IS_SINGLETON(m->web_seed_list)) {
			const char *first_web_seed =
				LL_DATA_AS(LL_HEAD(m->web_seed_list), const char*);

			fprintf(f, "8:url-list%lu:%s",
					(unsigned long) strlen(first_web_seed), first_web_seed);
		} else
			write_web_seed_list(f, m->web_seed_list);
	}

	/* end the root dictionary */
	fprintf(f, "e");

	/* let the user know we're done already */
	printf("done\n");
	fflush(stdout);
}
