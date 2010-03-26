#ifndef _INCLUDE_R_LIST_H_
#define _INCLUDE_R_LIST_H_

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

#ifdef R_API
#define r_list_foreach(list, it, pos) \
	for (it = list->head; it && (pos = it->data); it = it->n)
#define r_list_iterator(x) x->head
#define r_list_empty(x) (x->head==NULL && x->tail==NULL)
#define r_list_head(x) x->head
#define r_list_tail(x) x->tail
#define r_list_unref(x) x
#define r_list_iter_get(x) x->data; x=x->n
#define r_list_iter_next(x) (x?1:0)
#define r_list_iter_cur(x) x->p
#define r_list_iter_unref(x) x
RList *r_list_new();
RListIter *r_list_append(RList *list, void *data);
RListIter *r_list_prepend(RList *list, void *data);

R_API void r_list_init(RList *list);
R_API void r_list_delete (RList *list, RListIter *iter);
R_API void r_list_iter_init (RListIter *iter, RList *list);
R_API void r_list_destroy (RList *list);
R_API void r_list_free (RList *list);
R_API RListIter *r_list_item_new (void *data);
R_API void r_list_unlink (RList *list, void *ptr);
R_API void r_list_split (RList *list, void *ptr);
R_API void r_list_split_iter (RList *list, RListIter *iter);
#endif

#endif
