#ifndef _MEMORY_POOL_H_
#define _MEMORY_POOL_H_

#ifdef _WIN32
#include <Windows.h>
#else
#define _GNU_SOURCE
#include <pthread.h>
#endif//_WIN32

//��ͷ�ļ���_GNU_SOURCE���Ӱ��
#include <stdio.h>
#include <stdint.h>



#ifdef __cplusplus
extern "C"{
#endif

enum {
	MEMORY_POOL_FLAG_INUSED = 0x01, 
};

//Ĭ�Ϸ�����С���ڴ��(���޸ĸ�ֵ)
#define DEFAULT_ALLOC_SIZE (8192)

typedef struct memory_pool_head {
	struct memory_pool_head*	mh_tail_next;				//����β���е�ʹ��
	struct memory_pool_head**	mh_tail_prev;	
	struct memory_pool_head*	mh_prevptr;					//��һ�������ڴ�صĵ�ַ����alloc��ʼ��

	void* mh_end;
	uint32_t mh_size;										//��ǰͷ�������������ĳ���

#ifdef _WIN32
	uint32_t mh_thread_id;
#else
	uint64_t mh_thread_id;									//�����߳�ID
#endif

	uint8_t mh_flag;										//��ǰͷ�ı�ʶ

	char*	mh_data;										//ʹ�õ��ڴ��
}memory_pool_head_t;

typedef struct memory_poll {
	memory_pool_head_t* inused_tailq;						//��ʹ�����ڴ���β����
	memory_pool_head_t* unused_head[24];					//��λ��ȷ��ĳ�����ȵ��ڴ��洢���Ǹ������� [0]128
	//uint64_t thread_id;										//�����߳�ID
	struct memory_poll* next;								//���ڶ��̼߳���ʱʹ��
#ifdef _WIN32
	CRITICAL_SECTION unused_lock;
	CRITICAL_SECTION inused_lock;
	uint32_t thread_id;
#else
	pthread_spinlock_t unused_lock;
	pthread_spinlock_t inused_lock;
	uint64_t thread_id;										//�����߳�ID
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