/*
This file is part of mktorrent

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


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "export.h"
#include "msg.h"


EXPORT void fatal(const char *format, ...)
{

	va_list args;
	va_start(args, format);

	fputs("fatal error: ", stderr);
	vfprintf(stderr, format, args);

	va_end(args);

	exit(EXIT_FAILURE);
}
