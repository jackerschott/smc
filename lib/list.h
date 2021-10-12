#ifndef MTX_LIST_H
#define MTX_LIST_H

#include <stddef.h>

#include "lib/util.h"

/* circular doubly linked lists
   based on https://www.oreilly.com/library/view/linux-device-drivers/0596000081/ch10s05.html */

struct mtx_listentry_t {
	struct mtx_listentry_t *next;
	struct mtx_listentry_t *prev;
};
typedef struct mtx_listentry_t mtx_listentry_t;

void mtx_list_init(mtx_listentry_t *e);
void mtx_list_concat(mtx_listentry_t *e1, mtx_listentry_t *e2);

void mtx_list_add(mtx_listentry_t *head, mtx_listentry_t *e);
void mtx_list_add_head(mtx_listentry_t *head, mtx_listentry_t *e);
void mtx_list_del(mtx_listentry_t *e);
void mtx_list_replace(struct mtx_listentry_t *old, struct mtx_listentry_t *new);

size_t mtx_list_length(mtx_listentry_t *head);
void mtx_list_entry_at(const mtx_listentry_t *head, size_t idx, mtx_listentry_t **entry);

int mtx_list_empty(mtx_listentry_t *head);

#define mtx_list_entry_content(ptr, type, member) \
	CONTAINER(ptr, type, member)

#define mtx_list_entry_content_at(ptr, type, member, idx, content) 		\
	{ 									\
		mtx_listentry_t *e; 						\
		mtx_list_entry_at((ptr), (idx), &e); 				\
		*(content) = mtx_list_entry_content(e, type, member); 	\
	}

#define mtx_list_free(ptr, type, member, free_entry) 					\
	do { 										\
		if (!(ptr)->next && !(ptr)->prev) 					\
			break; 								\
											\
		mtx_listentry_t *e = (ptr)->next; 					\
		while (e != (ptr)) { 							\
			mtx_listentry_t *next = e->next; 				\
			type *entry = mtx_list_entry_content(e, type, member);		\
			free_entry(entry); 						\
			e = next; 							\
		} 									\
	} while (0)

#define mtx_list_dup(dest, src, type, member, dup_entry_content) 			\
	do { 										\
		mtx_list_init((dest)); 							\
		for (mtx_listentry_t *e = (src)->next; e != (src); e = e->next) { 	\
			type *content = mtx_list_entry_content(e, type, member); 	\
			type *newcontent = dup_entry_content(content); 			\
			if (!newcontent) { 						\
				(dest) = NULL; 						\
				break; 							\
			} 								\
			mtx_list_add((dest), &newcontent->entry); 			\
		} 									\
	}  while (0)

#define mtx_list_foreach(ptr, type, member, content) 					\
	type *content __attribute__((__unused__)) = NULL; 				\
	for (mtx_listentry_t *e_##content = (ptr)->next; ({ 				\
			content = mtx_list_entry_content(e_##content, type, member); 	\
			e_##content != (ptr); 						\
		}); e_##content = e_##content->next)

#endif /* MTX_LIST_H */
