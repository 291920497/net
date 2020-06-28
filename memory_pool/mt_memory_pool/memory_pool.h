#ifndef _MEMORY_POOL_H_
#define _MEMORY_POOL_H_

#ifdef _WIN32
#include <Windows.h>
#else
#define _GNU_SOURCE
#include <pthread.h>
#endif//_WIN32

//此头文件受_GNU_SOURCE宏的影响
#include <stdio.h>
#include <stdint.h>



#ifdef __cplusplus
extern "C"{
#endif

enum {
	MEMORY_POOL_FLAG_INUSED = 0x01, 
};

//默认分配最小的内存块(可修改该值)
#define DEFAULT_ALLOC_SIZE (8192)

typedef struct memory_pool_head {
	struct memory_pool_head*	mh_tail_next;				//用于尾队列的使用
	struct memory_pool_head**	mh_tail_prev;	
	struct memory_pool_head*	mh_prevptr;					//上一个连续内存池的地址。由alloc初始化

	void* mh_end;
	uint32_t mh_size;										//当前头所包含缓冲区的长度

#ifdef _WIN32
	uint32_t mh_thread_id;
#else
	uint64_t mh_thread_id;									//所属线程ID
#endif

	uint8_t mh_flag;										//当前头的标识

	char*	mh_data;										//使用的内存块
}memory_pool_head_t;

typedef struct memory_poll {
	memory_pool_head_t* inused_tailq;						//在使用中内存块的尾队列
	memory_pool_head_t* unused_head[24];					//以位来确定某个长度的内存块存储在那个队列中 [0]128
	//uint64_t thread_id;										//所属线程ID
	struct memory_poll* next;								//用于多线程集成时使用
#ifdef _WIN32
	CRITICAL_SECTION unused_lock;
	CRITICAL_SECTION inused_lock;
	uint32_t thread_id;
#else
	pthread_spinlock_t unused_lock;
	pthread_spinlock_t inused_lock;
	uint64_t thread_id;										//所属线程ID
#endif//_WIN32
}memory_poll_t;

void opt_bit(uint8_t* opt_flag, uint8_t is_add, uint8_t flag);

uint8_t max_bit(uint32_t size);

uint64_t get_current_thread_id();

memory_pool_head_t* mp_format_head(char* pool, uint32_t size, uint8_t flag, uint64_t mh_thread_id, memory_pool_head_t* mh_hrevptr, void* mh_end);

void insert_tailq(memory_pool_head_t** head, memory_pool_head_t* mph);

void remove_tailq(memory_pool_head_t* mph);

void insert_unused_tailq(memory_poll_t* mp, memory_pool_head_t* mph);

void enter_unused_lock(memory_poll_t* mp);

void leave_unused_lock(memory_poll_t* mp);

void enter_inused_lock(memory_poll_t* mp);

void leave_inused_lock(memory_poll_t* mp);

memory_poll_t* mp_create();

void mp_destroy(memory_poll_t* mp);

void* mp_malloc(memory_poll_t* mp, size_t size);

void mp_free(memory_poll_t* mp, void* free_ptr);

#ifdef __cplusplus
}
#endif


#endif//_MEMORY_POOL_H_