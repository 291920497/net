#include "mylist.h"

#include <stdlib.h>
#include <string.h>

#define my_malloc malloc
#define my_free free

#define DEFAULT_SIZE (2)

struct tlist* tl_create() {
	struct tlist* tl = my_malloc(sizeof(struct tlist));
	if (!tl) {
		goto create_tlist_failed;
	}

	memset(tl, 0, sizeof(struct tlist));

	tl->cache = my_malloc(sizeof(struct tlist_element) * DEFAULT_SIZE);
	if (!tl->cache) {
		goto create_tlist_failed;
	}
	tl->cache_size = DEFAULT_SIZE;
	memset(tl->cache, 0, sizeof(struct tlist_element) * DEFAULT_SIZE);

	for (int i = 0; i < tl->cache_size; ++i) {
		(tl->cache + i)->next = tl->free;
		tl->free = (tl->cache + i);
	}

	return tl;

create_tlist_failed:
	if (tl->cache) { my_free(tl->cache); }
	if (tl) { my_free(tl); }
	return 0;
}

void tl_delete(struct tlist* tl) {
	if (tl) {
		my_free(tl->cache);
		my_free(tl);
	}
	
}

static struct tlist_element* resize_cache(struct tlist* tl) {
	struct tlist_element* old_cache = tl->cache;
	struct tlist_element* old_tail = tl->tail;
	struct tlist_element* te;
	struct tlist_element* new_cache = my_malloc(sizeof(struct tlist_element) * tl->cache_size * 2);
	if (new_cache) {
		memset(new_cache, 0, sizeof(struct tlist_element) * tl->cache_size * 2);
		for (int i = 0; i < tl->cache_size * 2; ++i) {
			(new_cache + i)->next = tl->free;
			tl->free = (new_cache + i);
		}

		tl->head = 0;
		tl->tail = 0;

		while (old_tail) {
			te = tl->free;
			tl->free = tl->free->next;

			te->value = old_tail->value;
			if (te->next = tl->head) {
				tl->head->prev = te;
				tl->head = te;
			}
			else {
				te->prev = 0;
				tl->head = tl->tail = te;
			}
			old_tail = old_tail->prev;
		}
		tl->cache = new_cache;
		tl->cache_size = tl->cache_size * 2;
		my_free(old_cache);
		return tl->cache;
	}
	return 0;
}

static struct tlist_element* cache_elem(struct tlist* tl) {
	struct tlist_element* te;
	if (tl->free) {
		te = tl->free;
		tl->free = tl->free->next;
		return te;
	}
	else {
		if (resize_cache(tl)) {
			te = cache_elem(tl);
			return te;
		}
	}
	return 0;
}

static void free_elem(struct tlist* tl, struct tlist_element* te) {
	if (te >= tl->cache && te < (tl->cache + tl->cache_size)) {
		te->next = tl->free;
		tl->free = te;
	}
}

void tl_insert_head(struct tlist* tl, void* var) {
	struct tlist_element* te = cache_elem(tl);
	if (te) {
		te->prev = 0;
		te->value = var;
		if (te->next = tl->head) {
			te->next->prev = te;
			tl->head = te;
		}
		else {
			tl->tail = tl->head = te;
		}
		tl->elem_size += 1;
	}
}

void tl_insert_tail(struct tlist* tl, void* var) {
	struct tlist_element* te = cache_elem(tl);
	if (te) {

		te->next = 0;
		te->value = var;
		
		if (te->prev = tl->tail) {
			te->prev->next = te;
			tl->tail = te;
		}
		else {
			tl->tail = tl->head = te;
		}
		tl->elem_size += 1;
	}
}

void tl_remove_head(struct tlist* tl) {
	if (tl->head) {
		if (tl->head->next) {
			tl->head = tl->head->next;
			free_elem(tl, tl->head->prev);
			tl->head->prev = 0;
		}
		else {
			free_elem(tl, tl->head);
			tl->head = tl->tail = 0;
		}	
		tl->elem_size -= 1;
	}
}

void tl_remove_tail(struct tlist* tl) {
	if (tl->tail) {
		if (tl->tail->prev) {
			tl->tail = tl->tail->prev;
			free_elem(tl, tl->tail->next);
			tl->tail->next = 0;
		}
		else {
			free_elem(tl, tl->tail);
			tl->head = tl->tail = 0;
		}
		tl->elem_size -= 1;
	}
}

struct tlist_element* tl_remove_piter(struct tlist* tl, struct tlist_element* te) {
	if (te) {
		if (te == tl->head) {
			tl_remove_head(tl);
			return tl->head;
		}
		else if (te == tl->tail) {
			tl_remove_tail(tl);
			return tl->tail;
		}
		else {
			struct tlist_element* rt = te->prev;

			te->prev->next = te->next;
			te->next->prev = te->prev;
			free_elem(tl, te);
			tl->elem_size -= 1;

			return rt;
		}
	}
	return 0;
}

struct tlist_element* tl_remove_riter(struct tlist* tl, struct tlist_element* te) {
	if (te) {
		if (te == tl->head) {
			tl_remove_head(tl);
			return tl->head;
		}
		else if (te == tl->tail) {
			tl_remove_tail(tl);
			return tl->tail;
		}
		else {
			struct tlist_element* rt = te->next;

			te->prev->next = te->next;
			te->next->prev = te->prev;
			free_elem(tl, te);
			tl->elem_size -= 1;

			return rt;
		}
	}
	return 0;
}

struct tlist_element* tl_find_value(struct tlist* tl, void* var) {
	for (struct tlist_element* te = tl->head; te != 0; te = te->next) {
		if (var == te->value) {
			return te;
		}
	}
	return 0;
}

void tl_remove_value(struct tlist* tl, void* var, uint8_t all) {
	for (struct tlist_element* te = tl->head; te != 0;) {
		if (var == te->value) {
			te = tl_remove_piter(tl, te);
			if (all) {
				continue;
			}
			else {
				return;
			}
		}
		else {
			te = te->next;
		}
	}
}

uint32_t tl_get_size(struct tlist* tl) {
	return tl->elem_size;
}

void* tl_get_value(struct tlist_element* te) {
	return te->value;
}