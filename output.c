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
#include <stdlib.h>		/* off_t etc. */
#include <stdio.h>		/* printf() etc. */
#include <string.h>		/* strlen() etc. */
#include <time.h>		/* time() */
#include <openssl/sha.h>	/* SHA_DIGEST_LENGTH */

#include "mktorrent.h"

/*
 * write the file list if the torrent consists of a directory
 */
static void write_file_list(FILE * file)
{
	fl_node p;
	char *a, *b;

	fprintf(file, "5:filesl");

	/* go through all the files */
	for (p = file_list; p; p = p->next) {
#ifdef DEBUG
		printf("Writing %s\n", p->path);
#endif
		/* the file list contains a dictionary for every file
		   with entries for the length and path
		   write the length first */
		fprintf(file, "d6:lengthi%llue4:pathl",
			(unsigned long long) p->size);
		/* the file path is written as a list of subdirectories
		   and the last entry is the filename
		   sorry this code is even uglier than the rest */
		a = p->path;
		/* while there are subdirectories before the filename.. */
		while ((b = index(a, '/')) != NULL) {
			/* set the next '/' to '\0' so fprintf
			   will only write the first subdirectory name */
			*b = '\0';
			/* print it bencoded */
			fprintf(file, "%zu:%s", strlen(a), a);
			/* undo our alteration to the string */
			*b = '/';
			/* and move a to the beginning of the next
			   subdir or filename */
			a = b + 1;
		}
		/* now print the filename bencoded and end the
		   path name list and file dictionary */
		fprintf(file, "%zu:%see", strlen(a), a);
	}

	/* whew, now end the file list */
	fprintf(file, "e");
}

/*
 * write metainfo to the file stream using all the information
 * we've gathered so far and the hash string calculated
 */
void write_metainfo(FILE * file, unsigned char *hash_string)
{
	/* let the user know we've started writing the metainfo file */
	printf("Writing metainfo file... ");
	fflush(stdout);

#ifdef DEBUG
	printf("\n");
#endif

	/* every metainfo file is one big dictonary
	   and the first entry is the announce url */
	fprintf(file, "d8:announce%zu:%s",
		strlen(announce_url), announce_url);
	/* now add the comment if one is specified */
	if (comment != NULL)
		fprintf(file, "7:comment%zu:%s", strlen(comment), comment);
	/* I made this! */
	fprintf(file, "10:created by13:mktorrent " VERSION);
	/* add the creation date */
	if (!no_creation_date)
		fprintf(file, "13:creation datei%ue",
			(unsigned int) time(NULL));

	/* now here comes the info section
	   it is yet another dictionary */
	fprintf(file, "4:infod");
	/* first entry is either 'length', which specifies the length of a
	   single file torrent, or a list of files and their respective sizes */
	if (!target_is_directory)
		fprintf(file, "6:lengthi%llue",
			(unsigned long long) file_list->size);
	else
		write_file_list(file);

	/* the info section also contains the name of the torrent,
	   the piece length and the hash string */
	fprintf(file, "4:name%zu:%s12:piece lengthi%zue6:pieces%u:",
		strlen(torrent_name), torrent_name,
		piece_length, pieces * SHA_DIGEST_LENGTH);
	fwrite(hash_string, 1, pieces * SHA_DIGEST_LENGTH, file);

	/* set the private flag */
	if (private)
		fprintf(file, "7:privatei1e");

	/* now end the info section and the root dictionary */
	fprintf(file, "ee");

	/* let the user know we're done already */
	printf("done.\n");
	fflush(stdout);
}
