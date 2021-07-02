#ifndef MKTORRENT_OUTPUT_H
#define MKTORRENT_OUTPUT_H

#include <stdio.h>      /* FILE */

#include "export.h"     /* EXPORT */
#include "mktorrent.h"  /* struct metafile */

#define CROSS_SEED_RAND_LENGTH 16

EXPORT void write_metainfo(FILE *f, struct metafile *m,
			unsigned char *hash_string);

#endif /* MKTORRENT_OUTPUT_H */
