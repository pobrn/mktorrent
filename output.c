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

#include <stdlib.h>		/* off_t etc. */
#include <stdio.h>		/* printf() etc. */
#include <string.h>		/* strlen() etc. */
#include <time.h>		/* time() */
#include <openssl/sha.h>	/* SHA_DIGEST_LENGTH */

#include "mktorrent.h"

#define EXPORT
#endif /* ALLINONE */

/*
 * write announce list
 */
static void write_announce_list(FILE *f, al_node list)
{
	/* the announce list is a list of lists of urls */
	fprintf(f, "13:announce-listl");
	/* go through them all.. */
	for (; list; list = list->next) {
		sl_node l;

		/* .. and print the lists */
		fprintf(f, "l");
		for (l = list->l; l; l = l->next)
			fprintf(f, "%zu:%s", strlen(l->s), l->s);
		fprintf(f, "e");
	}
	fprintf(f, "e");
}

/*
 * write file list
 */
static void write_file_list(FILE *f, fl_node list)
{
	char *a, *b;

	fprintf(f, "5:filesl");

	/* go through all the files */
	for (; list; list = list->next) {
		/* the file list contains a dictionary for every file
		   with entries for the length and path
		   write the length first */
		fprintf(f, "d6:lengthi%llue4:pathl",
			(unsigned long long) list->size);
		/* the file path is written as a list of subdirectories
		   and the last entry is the filename
		   sorry this code is even uglier than the rest */
		a = list->path;
		/* while there are subdirectories before the filename.. */
		while ((b = index(a, DIRSEP_CHAR)) != NULL) {
			/* set the next DIRSEP_CHAR to '\0' so fprintf
			   will only write the first subdirectory name */
			*b = '\0';
			/* print it bencoded */
			fprintf(f, "%zu:%s", strlen(a), a);
			/* undo our alteration to the string */
			*b = DIRSEP_CHAR;
			/* and move a to the beginning of the next
			   subdir or filename */
			a = b + 1;
		}
		/* now print the filename bencoded and end the
		   path name list and file dictionary */
		fprintf(f, "%zu:%see", strlen(a), a);
	}

	/* whew, now end the file list */
	fprintf(f, "e");
}

/*
 * write metainfo to the file stream using all the information
 * we've gathered so far and the hash string calculated
 */
EXPORT void write_metainfo(FILE *f, unsigned char *hash_string)
{
	/* let the user know we've started writing the metainfo file */
	printf("Writing metainfo file... ");
	fflush(stdout);

	/* every metainfo file is one big dictonary
	   and the first entry is the announce url */
	fprintf(f, "d8:announce%zu:%s",
		strlen(announce_list->l->s), announce_list->l->s);
	/* now write the announce list */
	write_announce_list(f, announce_list);
	/* add the comment if one is specified */
	if (comment != NULL)
		fprintf(f, "7:comment%zu:%s", strlen(comment), comment);
	/* I made this! */
	fprintf(f, "10:created by13:mktorrent " VERSION);
	/* add the creation date */
	if (!no_creation_date)
		fprintf(f, "13:creation datei%ue",
			(unsigned int) time(NULL));

	/* now here comes the info section
	   it is yet another dictionary */
	fprintf(f, "4:infod");
	/* first entry is either 'length', which specifies the length of a
	   single file torrent, or a list of files and their respective sizes */
	if (!target_is_directory)
		fprintf(f, "6:lengthi%llue",
			(unsigned long long) file_list->size);
	else
		write_file_list(f, file_list);

	/* the info section also contains the name of the torrent,
	   the piece length and the hash string */
	fprintf(f, "4:name%zu:%s12:piece lengthi%zue6:pieces%u:",
		strlen(torrent_name), torrent_name,
		piece_length, pieces * SHA_DIGEST_LENGTH);
	fwrite(hash_string, 1, pieces * SHA_DIGEST_LENGTH, f);

	/* set the private flag */
	if (private)
		fprintf(f, "7:privatei1e");

	/* end the info section */
	fprintf(f, "e");

	/* add url-list if one is specified */
	if (web_seed_url != NULL)
		fprintf(f, "8:url-list%zu:%s",
				strlen(web_seed_url), web_seed_url);

	/* end the root dictionary */
	fprintf(f, "e");

	/* let the user know we're done already */
	printf("done.\n");
	fflush(stdout);
}
