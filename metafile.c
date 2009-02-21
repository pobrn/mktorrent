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
#include <stdlib.h>		/* exit() */
#include <stdio.h>		/* printf() etc. */
#include <string.h>		/* strlen() etc. */
#include <time.h>		/* time() */
#include <openssl/sha.h>	/* SHA_DIGEST_LENGTH */

#include "mktorrent.h"

/*
 * write the metafile to the metafile stream using all the information
 * we've gathered so far and the hash string calculated
 */
void write_metafile(FILE * metafile, unsigned char *hash_string)
{
	fl_node p;
	char *a, *b;

	/* let the user know we've started writing the metafile */
	printf("Writing torrent... ");
	fflush(stdout);

#ifdef DEBUG
	printf("\n");
#endif

	/* every metafile is one big dictonary
	   and the first entry is the announce url */
	fprintf(metafile, "d8:announce%zu:%s",
		strlen(announce_url), announce_url);
	/* now add the comment if one is specified */
	if (comment != NULL)
		fprintf(metafile, "7:comment%zu:%s",
			strlen(comment), comment);
	/* I made this! */
	fprintf(metafile, "10:created by13:mktorrent " VERSION);
	/* add the creation date */
	if (!no_creation_date)
		fprintf(metafile, "13:creation datei%ue",
			(unsigned) time(NULL));

	/* now here comes the info section.
	   it is yet another dictionary and the first entry
	   is the list of files and their respective sizes */
	fprintf(metafile, "4:infod5:filesl");

	/* go through all the files */
	for (p = file_list; p; p = p->next) {
#ifdef DEBUG
		printf("Writing %s\n", p->path);
#endif
		/* the file list contains a dictionary for every file
		   with entries for the length and path
		   write the length first */
		fprintf(metafile, "d6:lengthi%ue4:pathl",
			(unsigned) p->size);
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
			fprintf(metafile, "%zu:%s", strlen(a), a);
			/* undo our alteration to the string */
			*b = '/';
			/* and move a to the beginning of the next
			   subdir or filename */
			a = b + 1;
		}
		/* now print the filename bencoded and end the
		   path name list and file dictionary */
		fprintf(metafile, "%zu:%see", strlen(a), a);
	}

	/* whew, now end the file list */
	fprintf(metafile, "e");

	/* the info section also contains the name of the torrent */
	fprintf(metafile, "4:name%zu:%s", strlen(torrent_name),
		torrent_name);
	/* ..and the piece length */
	fprintf(metafile, "12:piece lengthi%zue", piece_length);
	/* ..and the number of pieces */
	fprintf(metafile, "6:pieces%u:", pieces * SHA_DIGEST_LENGTH);
	/* ..and finally the hash string of all the pieces */
	fwrite(hash_string, 1, pieces * SHA_DIGEST_LENGTH, metafile);

	/* now end the info section and the root dictionary */
	fprintf(metafile, "ee");

	/* let the user know how fast we were */
	printf("done.\n");
	fflush(stdout);
}
