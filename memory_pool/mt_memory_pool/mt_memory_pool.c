#include "mt_memory_pool.h"

#include <string.h>


struct {
	memory_poll_t* mt_pool_hash[MP_THREAD_HASH];
#ifdef _WIN32
	CRITICAL_SECTION mt_pool_lock;
#else
	pthread_spinlock_t mt_pool_lock;
#endif//_WIN32
}g_mt_pool;

static void enter_lock() {
#ifdef _WIN32
	EnterCriticalSection(&(g_mt_pool.mt_pool_lock));
#else
	pthread_spin_lock(&(g_mt_pool.mt_pool_lock));
#endif//_WIN32
}

static void leave_lock() {
#ifdef _WIN32
	LeaveCriticalSection(&(g_mt_pool.mt_pool_lock));
#else
	pthread_spin_unlock(&(g_mt_pool.mt_pool_lock));
#endif//_WIN32
}

void mt_memory_pool_create() {
	memset(&g_mt_pool, 0, sizeof(g_mt_pool));
#ifdef _WIN32
	InitializeCriticalSection(&(g_mt_pool.mt_pool_lock));
#else
	pthread_spin_init(&(g_mt_pool.mt_pool_lock), 0);
#endif//_WIN32
}

void mt_memory_pool_destroy() {
	enter_lock();
	for (int i = 0; i < MP_THREAD_HASH; ++i) {
		memory_poll_t** walk = &(g_mt_pool.mt_pool_hash[i]);
		while (*walk) {
			memory_poll_t* tmp = *walk;
			*walk = tmp->next;
			mp_destroy(tmp);
		}
	}
	leave_lock();

#ifdef _WIN32
	DeleteCriticalSection(&(g_mt_pool.mt_pool_lock));
#else
	pthread_spin_destroy(&(g_mt_pool.mt_pool_lock));
#endif//_WIN32	
}

void* mt_malloc(size_t size) {
	uint64_t thread_id = get_current_thread_id();
	memory_poll_t* pool = 0;

	/*此处参与线程所属内存池竞态*/
	enter_lock();

	memory_poll_t* walk = g_mt_pool.mt_pool_hash[thread_id & (MP_THREAD_HASH - 1)];
	while (walk) {
		if (walk->thread_id == thread_id) {
			pool = walk;
			break;
		}
		walk = walk->next;
	}

	leave_lock();

	if (pool) {
		return mp_malloc(pool, size);
	}

	pool = mp_create();
	if (pool) {
		enter_lock();
		pool->next = g_mt_pool.mt_pool_hash[thread_id & (MP_THREAD_HASH - 1)];
		g_mt_pool.mt_pool_hash[thread_id & (MP_THREAD_HASH - 1)] = pool;
		leave_lock();
		return mp_malloc(pool, size);
	}
	return 0;
}

void mt_free(void* free_ptr) {
	memory_pool_head_t* head = ((memory_pool_head_t*)free_ptr) - 1;
	//memory_pool_head_t* head = (memory_pool_head_t*)free_ptr;
	uint64_t thread_id = head->mh_thread_id;
	
	enter_lock();
	memory_poll_t* walk = g_mt_pool.mt_pool_hash[thread_id & (MP_THREAD_HASH - 1)];
	while (walk) {
		if (walk->thread_id == thread_id) {
			mp_free(walk, free_ptr);
			break;
		}
		walk = walk->next;
	}
	leave_lock();
}