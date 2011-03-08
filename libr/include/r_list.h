#ifndef _INCLUDE_R_LIST_H_
#define _INCLUDE_R_LIST_H_

#include <r_flist.h>

// TODO: implement r_list_foreach_prev

typedef void (*RListFree)(void *ptr);

typedef struct r_list_iter_t {
	void *data;
	struct r_list_iter_t *n, *p;
} RListIter;

typedef struct r_list_t {
	struct r_list_iter_t *head;
	struct r_list_iter_t *tail;
	RListFree free;
} RList;

typedef int (*RListComparator)(void *a, void *b);

#define ROFList_Parent RList
typedef struct r_oflist_t {
	ROFList_Parent super; // super class
	RFList *array; // statical readonly cache of linked list as a pointer array
} ROFList;

#ifdef R_API
#define r_list_foreach(list, it, pos) \
	for (it = list->head; it && (pos = it->data); it = it->n)
#define r_list_foreach_prev(list, it, pos) \
	for (it = list->tail; it && (pos = it->data); it = it->p)
#define r_list_iterator(x) (x)?(x)->head:NULL
#define r_list_empty(x) (x==NULL || (x->head==NULL && x->tail==NULL))
#define r_list_head(x) x->head
#define r_list_tail(x) x->tail
#define r_list_unref(x) x
#define r_list_iter_get(x) x->data; x=x->n
#define r_list_iter_next(x) (x?1:0)
#define r_list_iter_cur(x) x->p
#define r_list_iter_unref(x) x
R_API RList *r_list_new();
R_API RListIter *r_list_append(RList *list, void *data);
R_API RListIter *r_list_prepend(RList *list, void *data);
R_API int r_list_length(RList *list);
R_API void r_list_add_sorted(RList *list, void *data, RListComparator cmp);
R_API void r_list_sort(RList *list, RListComparator cmp);

R_API void r_list_init(RList *list);
R_API void r_list_delete (RList *list, RListIter *iter);
R_API void r_list_iter_init (RListIter *iter, RList *list);
R_API void r_list_destroy (RList *list);
R_API void r_list_free (RList *list);
R_API RListIter *r_list_item_new (void *data);
R_API void r_list_unlink (RList *list, void *ptr);
R_API void r_list_split (RList *list, void *ptr);
R_API void r_list_split_iter (RList *list, RListIter *iter);
R_API void *r_list_get_n(RList *list, int n);

/* hashlike api */
R_API void *r_list_get_by_int(RList *list, int off, int n);
R_API void *r_list_get_by_int64(RList *list, int off, ut64 n);
R_API void *r_list_get_by_string(RList *list, int off, const char *str);

/* rlistflist */
// TODO: rename to init or so.. #define r_oflist_new() R_NEW(ROFList);memset
#define r_oflist_length(x,y) r_list_length(x,y)
#define r_oflist_destroy(x) r_oflist_deserialize(x), r_list_destroy(x)
#define r_oflist_free(x) r_oflist_deserialize(x), r_list_free(x)
#define r_oflist_append(x,y) r_oflist_deserialize(x), r_list_append(x,y)
#define r_oflist_prepend(x,y) r_oflist_deserialize(x), r_list_prepend(x,y)
#define r_oflist_delete(x,y) r_oflist_deserialize(x), r_list_delete(x,y)
#define r_oflist_array(x) x->array?x->array:(x->array=r_oflist_serialize(x)),x->array
#define r_oflist_deserialize(x) \
	free(x->array-1),x->array=0
#define r_oflist_serialize(x) \
	x->array = r_flist_new(r_list_length(x)), { \
		int idx = 0; \
		void *ptr; \
		RListIter *iter; \
		r_list_foreach (x, iter, ptr) \
			r_flist_set (x->array, idx++, ptr); \
	} x->array;
#endif

#endif
