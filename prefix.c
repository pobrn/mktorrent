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
