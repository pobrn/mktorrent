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


#include <stdlib.h>
#include <string.h>

#include "export.h"
#include "ll.h"


EXPORT struct ll *ll_new(void)
{
	struct ll *list = calloc(1, sizeof(*list));

	if (list)
		LL_TAIL(list) = &list->head;

	return list;
}

EXPORT void ll_free(struct ll *list, ll_node_data_destructor destructor)
{
	if (list) {

		struct ll_node *head = LL_HEAD(list), *next;

		while (head) {

			if (destructor)
				destructor(LL_DATA(head));

			if (head->data_size)
				free(LL_DATA(head));

			next = LL_NEXT(head);

			free(head);

			head = next;
		}

		free(list);
	}
}

static struct ll_node *ll_node_new(
	void *data,
	const size_t data_size,
	struct ll_node *prev,
	struct ll_node *next)
{
	struct ll_node *node = calloc(1, sizeof(*node));

	if (!node)
		return NULL;

	if (data_size) {
		LL_DATA(node) = calloc(1, data_size);

		if (!LL_DATA(node))
			goto oom_node_data;

		memcpy(LL_DATA(node), data, data_size);
	}
	else
		LL_DATA(node) = data;

	LL_DATASIZE(node) = data_size;
	LL_PREV(node) = prev;
	LL_NEXT(node) = next;

	return node;

oom_node_data:
	free(node);

	return NULL;
}

/* appends a new node with 'data' to the end of 'list' */
EXPORT struct ll_node *ll_append(struct ll *list, void *data, const size_t data_size)
{
	if (!list)
		return NULL;

	struct ll_node *node = ll_node_new(data, data_size, LL_TAIL(list), NULL);

	if (node) {
		LL_NEXT(LL_TAIL(list)) = node;
		LL_TAIL(list) = node;
	}

	return node;
}

/* concatenates two lists while destroying the second one */
EXPORT struct ll *ll_extend(struct ll *list, struct ll *other)
{
	if (!list)
		return NULL;

	if (!other)
		return list;

	if (!LL_IS_EMPTY(other)) {
		LL_NEXT(LL_TAIL(list)) = LL_HEAD(other);
		LL_PREV(LL_HEAD(other)) = LL_TAIL(list);
		LL_TAIL(list) = LL_TAIL(other);
	}


	free(other);

	return list;
}

/* sort the given range using recursive merge sort in a stable way;
 * sets 'first' to the new head, 'last' to the new tail;
 * the new head will have ->prev = NULL,
 * and the new tail will have ->next = NULL;
 */
static void ll_sort_node_range(
	struct ll_node **first,
	struct ll_node **last,
	ll_node_data_cmp cmp)
{
#define APPEND_AND_STEP(t, x) do { \
	LL_NEXT(t) = (x); \
	LL_PREV(x) = (t); \
	                  \
	LL_STEP(x); \
	LL_STEP(t); \
} while(0)

	if (first == NULL || *first == NULL || last == NULL || *last == NULL)
		return;

	/* sorting a one element range is trivial */
	if (*first == *last) {
		LL_CLEAR_LINKS(*first);
		return;
	}

	struct ll_node *middle = *first, *middle2 = *last;

	while (middle != middle2 && LL_NEXT(middle) != middle2) {
		middle  = LL_NEXT(middle);
		middle2 = LL_PREV(middle2);
	}

	/* middle is now the midpoint of the list */

	/* 'tail' is the tail of the new, sorted list */
	struct ll_node dummy, *tail = &dummy;

	struct ll_node *a = *first;
	struct ll_node *b = LL_NEXT(middle);

	/* the values of middle and *last are not used anymore in this function,
	 * so they can be safely overwritten by the recursive calls
	 */
	ll_sort_node_range(&a, &middle, cmp);
	ll_sort_node_range(&b,    last, cmp);

	while (a && b) {
		int r = cmp(LL_DATA(a), LL_DATA(b));

		if (r <= 0) APPEND_AND_STEP(tail, a); /* if a.val <= b.val, append a */
		else        APPEND_AND_STEP(tail, b); /* otherwise,         append b */
	}

	/* at this point only one of a or b might be non-NULL,
	 * so only one of the next two loops will run
	 */

	/* append remaining nodes from the first half */
	while (a) APPEND_AND_STEP(tail, a);

	/* append remaining nodes from the second half */
	while (b) APPEND_AND_STEP(tail, b);

	/* the prev ptr of the first "real" node points to dummy, clear that */
	LL_PREV(LL_NEXT(&dummy)) = NULL;

	/* set the new head and tail */
	*first = LL_NEXT(&dummy);
	*last = tail;

#undef APPEND_AND_STEP
}

EXPORT struct ll *ll_sort(struct ll *list, ll_node_data_cmp cmp)
{
	if (list == NULL || cmp == NULL)
		return NULL;

	ll_sort_node_range(&LL_HEAD(list), &LL_TAIL(list), cmp);

	if (LL_HEAD(list))
		LL_PREV(LL_HEAD(list)) = &list->head;

	return list;
}
