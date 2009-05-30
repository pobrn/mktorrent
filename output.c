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
#include <sys/types.h>    /* off_t */
#include <stdio.h>        /* printf() etc. */
#include <string.h>       /* strlen() etc. */
#include <time.h>         /* time() */
#ifdef USE_OPENSSL
#include <openssl/sha.h>  /* SHA_DIGEST_LENGTH */
#else
#include <inttypes.h>
#include "sha1.h"
#endif

#include "mktorrent.h"

#define EXPORT
#endif /* ALLINONE */

/*
 * write announce list
 */
static void write_announce_list(FILE *f, llist_t *list)
{
	/* the announce list is a list of lists of urls */
	fprintf(f, "13:announce-listl");
	/* go through them all.. */
	for (; list; list = list->next) {
		slist_t *l;

		/* .. and print the lists */
		fprintf(f, "l");
		for (l = list->l; l; l = l->next)
			fprintf(f, "%lu:%s",
					(unsigned long)strlen(l->s), l->s);
		fprintf(f, "e");
	}
	fprintf(f, "e");
}

/*
 * write file list
 */
static void write_file_list(FILE *f, flist_t *list)
{
	char *a, *b;

	fprintf(f, "5:filesl");

	/* go through all the files */
	for (; list; list = list->next) {
		/* the file list contains a dictionary for every file
		   with entries for the length and path
		   write the length first */
		fprintf(f, "d6:lengthi%" PRIoff "e4:pathl", list->size);
		/* the file path is written as a list of subdirectories
		   and the last entry is the filename
		   sorry this code is even uglier than the rest */
		a = list->path;
		/* while there are subdirectories before the filename.. */
		while ((b = strchr(a, DIRSEP_CHAR)) != NULL) {
			/* set the next DIRSEP_CHAR to '\0' so fprintf
			   will only write the first subdirectory name */
			*b = '\0';
			/* print it bencoded */
			fprintf(f, "%lu:%s", (unsigned long)strlen(a), a);
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
static void write_web_seed_list(FILE *f, slist_t *list)
{
	/* print the entry and start the list */
	fprintf(f, "8:url-listl");
	/* go through the list and write each URL */
	for (; list; list = list->next)
		fprintf(f, "%lu:%s", (unsigned long)strlen(list->s), list->s);
	/* end the list */
	fprintf(f, "e");
}

/*
 * write metainfo to the file stream using all the information
 * we've gathered so far and the hash string calculated
 */
EXPORT void write_metainfo(FILE *f, metafile_t *m, unsigned char *hash_string)
{
	/* let the user know we've started writing the metainfo file */
	printf("Writing metainfo file... ");
	fflush(stdout);

	/* every metainfo file is one big dictonary
	   and the first entry is the announce URL */
	fprintf(f, "d8:announce%lu:%s",
		(unsigned long)strlen(m->announce_list->l->s),
		m->announce_list->l->s);
	/* write the announce-list entry if we have
	   more than one announce URL */
	if (m->announce_list->next || m->announce_list->l->next)
		write_announce_list(f, m->announce_list);
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
		fprintf(f, "6:lengthi%" PRIoff "e", m->file_list->size);
	else
		write_file_list(f, m->file_list);

	/* the info section also contains the name of the torrent,
	   the piece length and the hash string */
	fprintf(f, "4:name%lu:%s12:piece lengthi%ue6:pieces%u:",
		(unsigned long)strlen(m->torrent_name), m->torrent_name,
		m->piece_length, m->pieces * SHA_DIGEST_LENGTH);
	fwrite(hash_string, 1, m->pieces * SHA_DIGEST_LENGTH, f);

	/* set the private flag */
	if (m->private)
		fprintf(f, "7:privatei1e");

	/* end the info section */
	fprintf(f, "e");

	/* add url-list if one is specified */
	if (m->web_seed_list != NULL) {
		if (m->web_seed_list->next == NULL)
			fprintf(f, "8:url-list%lu:%s",
					(unsigned long)strlen(m->web_seed_list->s),
					m->web_seed_list->s);
		else
			write_web_seed_list(f, m->web_seed_list);
	}

	/* end the root dictionary */
	fprintf(f, "e");

	/* let the user know we're done already */
	printf("done.\n");
	fflush(stdout);
}
