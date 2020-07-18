#ifndef MKTORRENT_LL_H
#define MKTORRENT_LL_H

struct ll_node {
    struct ll_node *prev, *next;

    size_t data_size;
    void *data;
};

struct ll {
    struct ll_node head, *tail;
};

typedef void (*ll_node_data_destructor)(void *);
typedef int  (*ll_node_data_cmp)(const void *, const void *);


#define LL_DATA(node) ((node)->data)
#define LL_DATA_AS(node, type) ((type) LL_DATA(node))
#define LL_DATASIZE(node) ((node)->data_size)
#define LL_PREV(node) ((node)->prev)
#define LL_NEXT(node) ((node)->next)
#define LL_STEP(node) ((node) = LL_NEXT(node))
#define LL_STEP_PREV(node) ((node) = LL_PREV(node))
#define LL_CLEAR_LINKS(node) do { LL_NEXT(node) = NULL; LL_PREV(node) = NULL; } while(0)

#define LL_HEAD(list) ((list)->head.next)
#define LL_TAIL(list) ((list)->tail)
#define LL_IS_EMPTY(list) (LL_HEAD(list) == NULL)
#define LL_IS_SINGLETON(list) (!LL_IS_EMPTY(list) && LL_NEXT(LL_HEAD(list)) == NULL)
#define LL_FOR_FROM_TO_STEP(node, from, to, step) for (struct ll_node *(node) = from; node != to; step(node))
#define LL_FOR(node, list) LL_FOR_FROM_TO_STEP(node, LL_HEAD(list), NULL, LL_STEP)
#define LL_FOR_FROM(node, from) LL_FOR_FROM_TO_STEP(node, from, NULL, LL_STEP)


/* creates a new linked list instance */
EXPORT struct ll *ll_new(void);


/* frees the given list, calls a "destructor" function
 * on the data pointers if provided
 */
EXPORT void ll_free(struct ll *, ll_node_data_destructor);


/* appends a new node with data to the end of the given list,
 * if the provided size is zero, then the data pointer is set to
 * the pointer provided in the arguments, otherwise size number of
 * "bytes" is allocated, and that amount of bytes is copied into
 * this newly allocated node from the given pointer
 */
EXPORT struct ll_node *ll_append(struct ll *, void *, const size_t);


/* concatenates the second list to the first one
 * while destroying the second one,
 * returns the first one
 */
EXPORT struct ll *ll_extend(struct ll *, struct ll *);


/* sorts the given list using recursive merge sort based on
 * the provieded comparator function, fails if either of the arguments is
 * NULL, returns the given list
 */
EXPORT struct ll *ll_sort(struct ll *, ll_node_data_cmp);

#endif /* MKTORRENT_LL_H */
