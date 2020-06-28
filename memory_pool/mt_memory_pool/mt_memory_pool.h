#ifndef _MT_MEMORY_POOL_H_
#define _MT_MEMORY_POOL_H_

#include "memory_pool.h"

#define MP_THREAD_HASH (8)

#ifdef __cplusplus
extern "C" {
#endif

void mt_memory_pool_create();

void mt_memory_pool_destroy();

void* mt_malloc(size_t size);

void mt_free(void* free_ptr);

#ifdef __cplusplus
}
#endif

#endif//_MT_MEMORY_POOL_H_