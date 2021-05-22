#include "list.h"

void __list_add(listentry_t *prev, listentry_t *next, listentry_t *e)
{
	prev->next = e;
	e->prev = prev;

	e->next = next;
	next->prev = e;
}
void __list_concat(const listentry_t *list, listentry_t *prev, listentry_t *next)
{
	listentry_t *first = list->next;
	listentry_t *last = list->prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

void list_init(listentry_t *e)
{
	e->next = e;
	e->prev = e;
}
void list_concat(listentry_t *list1, listentry_t *list2)
{
	if (!list_empty(list1))
		__list_concat(list1, list2->prev, list2);
}
void list_concat_head(listentry_t *list1, listentry_t *list2)
{
	if (!list_empty(list1))
		__list_concat(list1, list2, list2->next);
}
void list_add(listentry_t *head, listentry_t *e)
{
	__list_add(head->prev, head, e);
}
void list_add_head(listentry_t *head, listentry_t *e)
{
	__list_add(head, head->next, e);
}

int list_empty(listentry_t *head)
{
	return head->next == head;
}
