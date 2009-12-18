#ifndef _BENC_H
#define _BENC_H

enum be_return {
	BE_OK,
	BE_MEMORY,
	BE_INVALID_ARGS
};

union be_t;

enum be_type {
	BE_TYPE_OFF_T,
	BE_TYPE_STRING,
	BE_TYPE_LIST,
	BE_TYPE_DICT
};

struct be_off {
	enum be_type type;
	off_t        n;
};

struct be_string {
	enum be_type type;
	unsigned int len;
};

struct be_list_node {
	union be_t          *v;
	struct be_list_node *next;
};

struct be_list {
	enum be_type        type;
	struct be_list_node *first;
	struct be_list_node **last;
};

struct be_dict_node {
	struct be_string    *key;
	union be_t          *value;
	struct be_dict_node *next;
};

struct be_dict {
	enum be_type        type;
	struct be_dict_node *first;
	struct be_dict_node *last;
};

union be_t {
	enum be_type     type;
	struct be_off    off;
	struct be_string string;
	struct be_list   list;
	struct be_dict   dict;
};

#define be_string_size(n) sizeof(struct be_string) + n
#define be_string_get(s) ((char *)(((struct be_string *)s) + 1))

#ifndef ALLINONE
union be_t *be_off_new(off_t n);

union be_t *be_string_new(size_t len);
union be_t *be_string_cpy(const char *str, size_t len);
union be_t *be_string_dup(const char *str);

union be_t *be_list_new();
enum be_return be_list_add(union be_t *t, union be_t *v);

union be_t *be_dict_new();
enum be_return be_dict_rawadd(union be_t *d, union be_t *key, union be_t *value);
enum be_return be_dict_add(union be_t *d, const char *key, union be_t *value);
enum be_return be_dict_set(union be_t *d, const char *key, union be_t *value);
union be_t *be_dict_get(union be_t *d, const char *key);
#endif /* ALLINONE */

EXPORT void be_destroy(union be_t *t);
EXPORT int be_cmp(union be_t *a, union be_t *b);
EXPORT int be_fput(union be_t *t, FILE *f);
EXPORT int be_pp(union be_t *t, FILE *f);

#endif /* _BENC_H */
