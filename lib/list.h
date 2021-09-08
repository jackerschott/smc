#ifndef LIST_H
#define LIST_H

#include <stddef.h>

#include "lib/util.h"

/* circular doubly linked lists
   based on https://www.oreilly.com/library/view/linux-device-drivers/0596000081/ch10s05.html */

struct listentry_t {
	struct listentry_t *next;
	struct listentry_t *prev;
};
typedef struct listentry_t listentry_t;

void list_init(listentry_t *e);
void list_concat(listentry_t *e1, listentry_t *e2);

void list_add(listentry_t *head, listentry_t *e);
void list_add_head(listentry_t *head, listentry_t *e);
void list_del(listentry_t *e);
void list_replace(struct listentry_t *old, struct listentry_t *new);

size_t list_length(listentry_t *head);
void list_entry_at(listentry_t *head, size_t idx, listentry_t **entry);

int list_empty(listentry_t *head);

#define list_entry_content(ptr, type, member) \
	CONTAINER(ptr, type, member)

#define list_free(ptr, type, member, free_entry) 				\
	do { 									\
		listentry_t *e = (ptr)->next; 					\
		while (e != (ptr)) { 						\
			listentry_t *next = e->next; 				\
			type *entry = list_entry_content(e, type, member);		\
			free_entry(entry); 					\
			e = next; 						\
		} 								\
	} while (0)

#endif /* LIST_H */
