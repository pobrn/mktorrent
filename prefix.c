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

#ifndef TYPE
#define TYPE off_t
#endif

int main(int argc, char *argv[])
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
	printf("_LARGEFILE_SOURCE is set\n");
#endif

#ifdef _FILE_OFFSET_BITS
	printf("_FILE_OFFSET_BITS = %lu\n", _FILE_OFFSET_BITS);
#endif

	printf("sizeof(" xstr(TYPE) ") = %lu, %lu bits\n",
			sizeof(TYPE), 8*sizeof(TYPE));
#endif /* DEBUG */

	printf("%s\n", prefix[sizeof(TYPE)]);

	return EXIT_SUCCESS;
}
