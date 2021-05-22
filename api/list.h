#ifndef LIST_H
#define LIST_H

/* circular doubly linked lists
   based on https://www.oreilly.com/library/view/linux-device-drivers/0596000081/ch10s05.html */

#define LIST_ENTRY(ptr, type, member) \
	CONTAINER(ptr, type, member)

struct listentry_t {
	struct listentry_t *next;
	struct listentry_t *prev;
};
typedef struct listentry_t listentry_t;

void list_init(listentry_t *e);
void list_concat(listentry_t *e1, listentry_t *e2);
void list_add(listentry_t *head, listentry_t *e);
void list_add_head(listentry_t *head, listentry_t *e);

int list_empty(listentry_t *head);

#endif /* LIST_H */
