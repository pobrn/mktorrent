#ifndef MKTORRENT_HASH_H
#define MKTORRENT_HASH_H

#include "export.h"    /* EXPORT */
#include "mktorrent.h" /* struct metafile */

EXPORT unsigned char *make_hash(struct metafile *m);

#endif /* MKTORRENT_HASH_H */
