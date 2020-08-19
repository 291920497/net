#ifndef _MYLIST_H_
#define _MYLIST_H_

#include <stdint.h>

struct tlist_element {
	struct tlist_element* next;
	struct tlist_element* prev;
	void* value;
};

struct tlist {
	struct tlist_element* head;
	struct tlist_element* tail;
	uint32_t elem_size;
	
	struct tlist_element* cache;
	struct tlist_element* free;
	uint32_t cache_size;
};

#ifdef __cplusplus
extern "C"
{
#endif

struct tlist* tl_create();

void tl_delete(struct tlist* tl);

void tl_insert_head(struct tlist* tl, void* var);

void tl_insert_tail(struct tlist* tl, void* var);

void tl_remove_head(struct tlist* tl);

void tl_remove_tail(struct tlist* tl);

struct tlist_element* tl_remove_piter(struct tlist* tl, struct tlist_element* te);

struct tlist_element* tl_remove_riter(struct tlist* tl, struct tlist_element* te);

struct tlist_element* tl_find_value(struct tlist* tl, void* var);

void tl_remove_value(struct tlist* tl, void* var, uint8_t all);

uint32_t tl_get_size(struct tlist* tl);

void* tl_get_value(struct tlist_element* te);

struct tlist_element* tl_head(struct tlist* tl);

struct tlist_element* tl_tail(struct tlist* tl);


#ifdef __cplusplus
}
#endif

#endif//_MYLIST_H_