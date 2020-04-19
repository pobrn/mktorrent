#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "msg.h"


void fatal(const char *format, ...) {

	va_list args;
	va_start(args, format);
	
	fputs("fatal error: ", stderr);
	vfprintf(stderr, format, args);
	
	va_end(args);
	
	exit(EXIT_FAILURE);
}
