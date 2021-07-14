#include "lib/list.h"

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
void list_concat(listentry_t *head, listentry_t *list)
{
	if (!list_empty(list))
		__list_concat(list, head->prev, head);
}
void list_concat_head(listentry_t *head, listentry_t *list)
{
	if (!list_empty(list))
		__list_concat(list, head, head->next);
}
void list_add(listentry_t *head, listentry_t *e)
{
	__list_add(head->prev, head, e);
}
void list_add_head(listentry_t *head, listentry_t *e)
{
	__list_add(head, head->next, e);
}
void list_replace(struct listentry_t *old, struct listentry_t *new)
{
        new->next = old->next;
        new->next->prev = new;
        new->prev = old->prev;
        new->prev->next = new;
}

size_t list_length(listentry_t *head)
{
	size_t l = 0;
	listentry_t *e = head;
	while ((e = e->next) != head) {
		++l;
	}
	return l;
}
void list_entry_at(listentry_t *head, size_t idx, listentry_t **entry)
{
	size_t i = 0;
	listentry_t *e = head->next;
	while (i < idx && e != head) {
		++i;
		e = e->next;
	}
	*entry = e;
}

int list_empty(listentry_t *head)
{
	return head->next == head;
}
