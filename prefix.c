/*
This file is part of mktorrent
Copyright (C) 2009 Emil Renner Berthing

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

#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <inttypes.h>

#ifndef TYPE
#define TYPE off_t
#endif

#if __WORDSIZE == 32 && _FILE_OFFSET_BITS == 64
#  define PRIdOFF_T PRId64
#else
#  define PRIdOFF_T PRIdPTR
#endif

int main()
{
	unsigned int i;
	char *prefix[9];

	for (i = 0; i < 9; i++)
		prefix[i] = NULL;

	prefix[1] = "hh";
	prefix[2] = "h";

	prefix[8] = "ll";
	prefix[sizeof(int)] = "";
	prefix[sizeof(long int)] = "l";

#ifdef DEBUG
#define xstr(s) str(s)
#define str(s) #s

#ifdef _LARGEFILE_SOURCE
	fprintf(stderr, "_LARGEFILE_SOURCE is set\n");
#endif

#ifdef _FILE_OFFSET_BITS
	fprintf(stderr, "_FILE_OFFSET_BITS = %u\n", _FILE_OFFSET_BITS);
#endif

	fprintf(stderr, "__WORDSIZE is %d\n", __WORDSIZE);
	fprintf(stderr, "PRIdOFF_T is " PRIdOFF_T "\n");
	fprintf(stderr, "sizeof(int) = %u, %u bits\n",
		(u_int) sizeof(int), 8 * (u_int) sizeof(int));
	fprintf(stderr, "sizeof(long int) = %u, %u bits\n",
		(u_int) sizeof(long int), 8 * (u_int) sizeof(long int));
	fprintf(stderr, "sizeof(" xstr(TYPE) ") = %u, %u bits\n",
		(u_int) sizeof(TYPE), 8 * (u_int) sizeof(TYPE));
#endif				/* DEBUG */

	printf("%s\n", prefix[sizeof(TYPE)]);

	return EXIT_SUCCESS;
}
