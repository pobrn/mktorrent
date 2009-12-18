#ifndef _FITER_H
#define _FITER_H

struct fiter {
	unsigned int size;
	char *buf;
	struct be_list_node *n;
};

#ifndef ALLINONE
const char *fiter_first(struct fiter *it, union be_t *meta);
const char *fiter_next(struct fiter *it);
#endif /* ALLINONE */
#endif /* _FITER_H */
