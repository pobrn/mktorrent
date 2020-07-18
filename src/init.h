#ifndef MKTORRENT_INIT_H
#define MKTORRENT_INIT_H

#include "export.h"    /* EXPORT */
#include "mktorrent.h" /* struct metafile */

EXPORT void init(struct metafile *m, int argc, char *argv[]);
EXPORT void cleanup_metafile(struct metafile *m);

#endif /* MKTORRENT_INIT_H */
