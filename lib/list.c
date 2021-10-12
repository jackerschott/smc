#include "lib/list.h"

void __list_del(mtx_listentry_t *prev, mtx_listentry_t *next)
{
	prev->next = next;
	next->prev = prev;
}
void __list_concat(const mtx_listentry_t *list, mtx_listentry_t *prev, mtx_listentry_t *next)
{
	mtx_listentry_t *first = list->next;
	mtx_listentry_t *last = list->prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

void mtx_list_init(mtx_listentry_t *e)
{
	e->next = e;
	e->prev = e;
}
void mtx_list_concat(mtx_listentry_t *head, mtx_listentry_t *list)
{
	if (!mtx_list_empty(list))
		__list_concat(list, head->prev, head);
}
void list_concat_head(mtx_listentry_t *head, mtx_listentry_t *list)
{
	if (!mtx_list_empty(list))
		__list_concat(list, head, head->next);
}

void __list_add(mtx_listentry_t *prev, mtx_listentry_t *next, mtx_listentry_t *e)
{
	prev->next = e;
	e->prev = prev;

	e->next = next;
	next->prev = e;
}
void mtx_list_add(mtx_listentry_t *head, mtx_listentry_t *e)
{
	__list_add(head->prev, head, e);
}
void mtx_list_add_head(mtx_listentry_t *head, mtx_listentry_t *e)
{
	__list_add(head, head->next, e);
}

void mtx_list_del(mtx_listentry_t *e)
{
	__list_del(e->prev, e->next);
}
void mtx_list_replace(struct mtx_listentry_t *old, struct mtx_listentry_t *new)
{
        new->next = old->next;
        new->next->prev = new;
        new->prev = old->prev;
        new->prev->next = new;
}

size_t mtx_list_length(mtx_listentry_t *head)
{
	size_t l = 0;
	mtx_listentry_t *e = head;
	while ((e = e->next) != head) {
		++l;
	}
	return l;
}
void mtx_list_entry_at(const mtx_listentry_t *head, size_t idx, mtx_listentry_t **entry)
{
	size_t i = 0;
	mtx_listentry_t *e = head->next;
	while (i < idx && e != head) {
		++i;
		e = e->next;
	}
	*entry = e;
}

int mtx_list_empty(mtx_listentry_t *head)
{
	return head->next == head;
}
