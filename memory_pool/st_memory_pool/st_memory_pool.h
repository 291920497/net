#ifndef _ST_MEMORY_POOL_H_
#define _ST_MEMORY_POOL_H_

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	MEMORY_POOL_FLAG_INUSED = 0x01,
};

//默认分配最小的内存块(可修改该值)
#define DEFAULT_ALLOC_SIZE (8192)

typedef struct memory_pool_head {
	struct memory_pool_head*	mh_tail_next;				//用于尾队列的使用,固定mh_tail_next在结构体的前4字节(x86)
	struct memory_pool_head**	mh_tail_prev;
	struct memory_pool_head*	mh_prevptr;					//上一个连续内存池的地址。由alloc初始化

	void* mh_end;
	uint32_t mh_size;										//当前头所包含缓冲区的长度
	uint8_t mh_flag;										//当前头的标识

	char*	mh_data;										//使用的内存块
}memory_pool_head_t;

typedef struct memory_poll {
	memory_pool_head_t* inused_tailq;						//在使用中内存块的尾队列
	memory_pool_head_t* unused_head[24];					//以位来确定某个长度的内存块存储在那个队列中 [0]128
}memory_poll_t;

void opt_bit(uint8_t* opt_flag, uint8_t is_add, uint8_t flag);

uint8_t max_bit(uint32_t size);

memory_pool_head_t* mp_format_head(char* pool, uint32_t size, uint8_t flag, memory_pool_head_t* mh_hrevptr, void* mh_end);

void insert_tailq(memory_pool_head_t** head, memory_pool_head_t* mph);

void remove_tailq(memory_pool_head_t* mph);

void insert_unused_tailq(memory_poll_t* mp, memory_pool_head_t* mph);


int st_memory_pool_create();

void st_memory_pool_destroy();

void* st_malloc(size_t size);

void st_free(void* free_ptr);


#ifdef __cplusplus
}
#endif

#endif//_ST_MEMORY_POOL_H_