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

//Ĭ�Ϸ�����С���ڴ��(���޸ĸ�ֵ)
#define DEFAULT_ALLOC_SIZE (8192)

typedef struct memory_pool_head {
	struct memory_pool_head*	mh_tail_next;				//����β���е�ʹ��,�̶�mh_tail_next�ڽṹ���ǰ4�ֽ�(x86)
	struct memory_pool_head**	mh_tail_prev;
	struct memory_pool_head*	mh_prevptr;					//��һ�������ڴ�صĵ�ַ����alloc��ʼ��

	void* mh_end;
	uint32_t mh_size;										//��ǰͷ�������������ĳ���
	uint8_t mh_flag;										//��ǰͷ�ı�ʶ

	char*	mh_data;										//ʹ�õ��ڴ��
}memory_pool_head_t;

typedef struct memory_poll {
	memory_pool_head_t* inused_tailq;						//��ʹ�����ڴ���β����
	memory_pool_head_t* unused_head[24];					//��λ��ȷ��ĳ�����ȵ��ڴ��洢���Ǹ������� [0]128
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