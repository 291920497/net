#include "memory_pool.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void opt_bit(uint8_t* opt_flag, uint8_t is_add, uint8_t flag) {
	if (is_add) {
		*opt_flag = *opt_flag | flag;
	}
	else {
		*opt_flag = *opt_flag & (~flag);
	}
}

uint8_t max_bit(uint32_t size) {
	for (int i = 31; i > 6; --i) {
		if (size & (1 << i)) {
			return i + 1 - 7;
		}
	}
	return 0;
}

uint64_t get_current_thread_id() {
#ifdef _WIN32
	return GetCurrentThreadId();
#else
	//��unixΪ��׼����ֵ64λ
	return pthread_self();
#endif//_WIN32
}

memory_pool_head_t* mp_format_head(char* pool, uint32_t size, uint8_t flag, uint64_t mh_thread_id, memory_pool_head_t* mh_hrevptr, void* mh_end) {
	memory_pool_head_t* mph = pool;
	mph->mh_prevptr = mh_hrevptr;				//��ǰΪ�ڴ��ͷ���������ϼ�����ָ��
	mph->mh_flag = flag;
	mph->mh_thread_id = mh_thread_id;
	mph->mh_end = mh_end;
	mph->mh_size = size - sizeof(memory_pool_head_t);
	mph->mh_data = pool + sizeof(memory_pool_head_t);
	return mph;
}


void insert_tailq(memory_pool_head_t** head, memory_pool_head_t* mph) {
	mph->mh_tail_next = *head;
	if (*head) {
		(*head)->mh_tail_prev = mph;
	}
	*head = mph;
	mph->mh_tail_prev = head;
}

void remove_tailq(memory_pool_head_t* mph) {
	*(mph->mh_tail_prev) = mph->mh_tail_next;
	if (mph->mh_tail_next) {
		mph->mh_tail_next->mh_tail_prev = mph->mh_tail_prev;
	}
}

void insert_unused_tailq(memory_poll_t* mp, memory_pool_head_t* mph) {
	memory_pool_head_t* next_block;
	uint8_t prev_unused = 0;

	if (mph->mh_prevptr) {
		enter_unused_lock(mp);
		prev_unused = !(mph->mh_prevptr->mh_flag & MEMORY_POOL_FLAG_INUSED);
		if (prev_unused) {
			remove_tailq(mph->mh_prevptr);
			opt_bit(&(mph->mh_prevptr->mh_flag), 1, MEMORY_POOL_FLAG_INUSED);
		}
		leave_unused_lock(mp);
	}

	if (prev_unused) {
		//�ϲ�
		//mph = mp_format_head(mph->mh_prevptr, sizeof(memory_pool_head_t) * 2 + mph->mh_size + mph->mh_prevptr->mh_size, mph->mh_prevptr->mh_flag & (~MEMORY_POOL_FLAG_INUSED), mph->mh_prevptr->mh_thread_id, mph->mh_prevptr->mh_prevptr, mph->mh_prevptr->mh_end);
		mph = mp_format_head(mph->mh_prevptr, sizeof(memory_pool_head_t) * 2 + mph->mh_size + mph->mh_prevptr->mh_size, mph->mh_prevptr->mh_flag, mph->mh_prevptr->mh_thread_id, mph->mh_prevptr->mh_prevptr, mph->mh_prevptr->mh_end);

		/*�˴���������ԭ��: ������һ���ڴ���Ƿ���δʹ��״̬,next_block->mh_prevptr �����ᵼ�¾�̬�����ķ���*/
		//���ϲ�����һ���ڵ㲢δԽ��
		next_block = mph->mh_data + mph->mh_size;
		if (next_block < mph->mh_end) {
			next_block->mh_prevptr = mph;
		}
	}


	next_block = mph->mh_data + mph->mh_size;
	enter_unused_lock(mp);

	//����һ���ڵ�δδԽ�磬�Ҳ���ʹ���д˴������2��
	while (next_block < mph->mh_end && !(next_block->mh_flag & MEMORY_POOL_FLAG_INUSED)) {
		remove_tailq(next_block);
		opt_bit(&(next_block->mh_flag), 1, MEMORY_POOL_FLAG_INUSED);
		mph->mh_size += ((sizeof(memory_pool_head_t) + next_block->mh_size));
		next_block = mph->mh_data + mph->mh_size;
	}
	leave_unused_lock(mp);

	//���ϲ�����һ���ڵ㲢δԽ��
	next_block = mph->mh_data + mph->mh_size;
	if (next_block < mph->mh_end) {
		next_block->mh_prevptr = mph;
	}

	//����Ӧ�ò����ĸ�����
	int bit = max_bit(mph->mh_size);
	enter_unused_lock(mp);
	opt_bit(&(mph->mh_flag), 0, MEMORY_POOL_FLAG_INUSED);
	insert_tailq(&(mp->unused_head[bit]), mph);
	leave_unused_lock(mp);
}


void enter_unused_lock(memory_poll_t* mp) {
#ifdef _WIN32
	EnterCriticalSection(&(mp->unused_lock));
#else
	pthread_spin_lock(&(mp->unused_lock));
#endif
}

void leave_unused_lock(memory_poll_t* mp) {
#ifdef _WIN32
	LeaveCriticalSection(&(mp->unused_lock));
#else
	pthread_spin_unlock(&(mp->unused_lock));
#endif
}

void enter_inused_lock(memory_poll_t* mp) {
#ifdef _WIN32
	EnterCriticalSection(&(mp->inused_lock));
#else
	pthread_spin_lock(&(mp->inused_lock));
#endif
}

void leave_inused_lock(memory_poll_t* mp) {
#ifdef _WIN32
	LeaveCriticalSection(&(mp->inused_lock));
#else
	pthread_spin_unlock(&(mp->inused_lock));
#endif
}

memory_poll_t* mp_create() {
	uint8_t lock_success[2] = { 0 };
	memory_poll_t* mp = malloc(sizeof(memory_poll_t));
	if (mp) {
		memset(mp, 0, sizeof(memory_poll_t));
		mp->thread_id = get_current_thread_id();

#ifdef _WIN32
		InitializeCriticalSection(&(mp->unused_lock));
		InitializeCriticalSection(&(mp->inused_lock));
#else
		
		lock_success[0] = pthread_spin_init(&(mp->unused_lock), PTHREAD_PROCESS_PRIVATE);
		lock_success[1] = pthread_spin_init(&(mp->inused_lock), PTHREAD_PROCESS_PRIVATE);
		if ((lock_success[0] | lock_success[1]) != 0) {
			goto spin_lock_failed;
		}
#endif
	}
	return mp;

#ifndef _WIN32
spin_lock_failed:
	if (lock_success[0]) {
		pthread_spin_destroy(&(mp->unused_lock));
	}
	if (lock_success[1]) {
		pthread_spin_destroy(&(mp->inused_lock));
	}
#endif
	free(mp);
	return 0;
}

void mp_destroy(memory_poll_t* mp) {
	while (mp->inused_tailq) {
		mp_free(mp, mp->inused_tailq->mh_data);
	}

	enter_unused_lock(mp);
	for (int i = 0; i < 24; ++i) {
		memory_pool_head_t* walk = mp->unused_head[i];
		while (walk) {
			remove_tailq(walk);
			//printf("���ں��ͷ���: [%d]\n", walk->mh_size + sizeof(memory_pool_head_t));
			printf("free: [%d], thread: [%uld]\n", walk->mh_size + sizeof(memory_pool_head_t), mp->thread_id);
			free(walk);
			walk = mp->unused_head[i];
		}
	}
	leave_unused_lock(mp);


#ifdef _WIN32
	DeleteCriticalSection(&(mp->unused_lock));
	DeleteCriticalSection(&(mp->inused_lock));
#else
	//�˴���û�п���EBUSY�Ĵ���
	pthread_spin_destroy(&(mp->unused_lock));	
	pthread_spin_destroy(&(mp->inused_lock));
#endif//_WIN32

	free(mp);
}




void* mp_malloc(memory_poll_t* mp, size_t size) {
	int bit = max_bit(size);
	memory_pool_head_t* block = 0;

	//����δʹ���ڴ���ٽ���
	enter_unused_lock(mp);

	for (int i = bit; i < 24; ++i) {
		memory_pool_head_t* walk = mp->unused_head[i];
		while (walk) {
			if (walk->mh_size >= size) {
				remove_tailq(walk);
				block = walk;
				opt_bit(&(block->mh_flag), 1, MEMORY_POOL_FLAG_INUSED);
				goto alloc_complate;
			}
			walk = walk->mh_tail_next;
		}
	}
//goto
alloc_complate:

	leave_unused_lock(mp);

	if (block) {
		//�˴�Ԥ�Ȳ�����ʹ�ö��д��ָ�
		enter_inused_lock(mp);
		insert_tailq(&(mp->inused_tailq), block);
		leave_inused_lock(mp);

		//�Ƿ��ܹ��ָ�Ϊ2���ڴ��
		if ((block->mh_size - size) > sizeof(memory_pool_head_t) + 16) {
			memory_pool_head_t* next_block_ptr = ((char*)block) + (size + sizeof(memory_pool_head_t));
			uint32_t next_block_len = block->mh_size - size;

			mp_format_head(block, sizeof(memory_pool_head_t) + size, block->mh_flag, block->mh_thread_id, block->mh_prevptr, block->mh_end);

			/*
			������������
			���ڴ�ʱ�޸��ڴ��ʹ�ñ�ʶ����������̬�����������ٽ������޸�
			*/
			//mp_format_head(next_block_ptr, next_block_len, block->mh_flag & (~MEMORY_POOL_FLAG_INUSED), block->mh_thread_id, block, block->mh_end);
			mp_format_head(next_block_ptr, next_block_len, block->mh_flag, block->mh_thread_id, block, block->mh_end);
			insert_unused_tailq(mp, next_block_ptr);
		}
		return block->mh_data;
	}

	//if (block) {
	//	//�Ƿ��ܹ��ָ�Ϊ2���ڴ��
	//	if ((block->mh_size - size) > sizeof(memory_pool_head_t) + 16) {
	//		//memory_pool_head_t* next_block_ptr = block->mh_data + size;
	//		memory_pool_head_t* next_block_ptr = ((char*)block) + (size + sizeof(memory_pool_head_t));
	//		uint32_t next_block_len = block->mh_size - size;

	//		mp_format_head(block, sizeof(memory_pool_head_t) + size, block->mh_flag, block->mh_thread_id, block->mh_prevptr, block->mh_end);

	//		/*
	//		������������
	//		���ڴ�ʱ�޸�ʹ�ñ�ʶ����̬���������ٽ���
	//		*/
	//		//mp_format_head(next_block_ptr, next_block_len, block->mh_flag & (~MEMORY_POOL_FLAG_INUSED), block->mh_thread_id, block, block->mh_end);
	//		mp_format_head(next_block_ptr, next_block_len, block->mh_flag, block->mh_thread_id, block, block->mh_end);
	//		insert_unused_tailq(mp, next_block_ptr);

	//		//������ʹ���ڴ���ٽ���
	//		enter_inused_lock(mp);
	//		insert_tailq(&(mp->inused_tailq), block);
	//		leave_inused_lock(mp);
	//		return block;
	//	}

	//	enter_inused_lock(mp);
	//	insert_tailq(&(mp->inused_tailq), block);
	//	leave_inused_lock(mp);
	//	return block;
	//}

	uint32_t alloc_size = DEFAULT_ALLOC_SIZE;
	if (alloc_size - sizeof(memory_pool_head_t) < size) {
		for (int i = 1; i < 17; ++i) {
			if (DEFAULT_ALLOC_SIZE << i > size + sizeof(memory_pool_head_t)) {
				alloc_size = DEFAULT_ALLOC_SIZE << i;
				break;
			}
		}
	}

	memory_pool_head_t* new_block = malloc(alloc_size);
	if (new_block) {
		memset(new_block, 0, alloc_size);
		mp_format_head(new_block, sizeof(memory_pool_head_t) + size, MEMORY_POOL_FLAG_INUSED, mp->thread_id, 0, ((char*)new_block) + alloc_size);


		enter_inused_lock(mp);
		insert_tailq(&(mp->inused_tailq), new_block);
		leave_inused_lock(mp);

		if ((new_block->mh_size - size - sizeof(memory_pool_head_t)) > sizeof(memory_pool_head_t) + 16) {
			memory_pool_head_t* test = mp_format_head(new_block->mh_data + new_block->mh_size, alloc_size - sizeof(memory_pool_head_t) - size, new_block->mh_flag, mp->thread_id, new_block, new_block->mh_end);
			insert_unused_tailq(mp, test);
			//return new_block->mh_data;
		}
		//printf("���ں�������: [%d]\n", alloc_size);
		printf("malloc: [%d], thread: [%uld]\n", alloc_size, mp->thread_id);
		return new_block->mh_data;


		//if (alloc_size - size > 2 * sizeof(memory_pool_head_t)) {
		//	//����ʼ����Ҫ��ʶ���ڴ˴����
		//	mp_format_head(new_block, sizeof(memory_pool_head_t) + size, MEMORY_POOL_FLAG_INUSED, mp->thread_id, 0, ((char*)new_block) + alloc_size);
		//	//������ʹ���ڴ���ٽ���
		//	enter_inused_lock(mp);
		//	insert_tailq(&(mp->inused_tailq), new_block);
		//	leave_inused_lock(mp);

		//	memory_pool_head_t* test = mp_format_head(new_block->mh_data + new_block->mh_size, alloc_size - sizeof(memory_pool_head_t) - size, new_block->mh_flag, mp->thread_id, new_block, new_block->mh_end);
		//	//test
		//	if (test->mh_size > 8196) {
		//		int i = 0;
		//		++i;
		//	}

		//	insert_unused_tailq(mp, test);


		//	//printf("���߳�IDΪ: [%d],�ڴ��IDΪ: [%d], ���ں�����: [%d] ���ȵ��ڴ��\n", get_current_thread_id(), mp->thread_id, alloc_size);
		//	char fmt[1024];
		//	sprintf(fmt, "���߳�IDΪ: [%d],�ڴ��IDΪ: [%d], ���ں�����: [%d] ���ȵ��ڴ��\n", get_current_thread_id(), mp->thread_id, alloc_size);
		//	thread_log(fmt);
		//}
	}
	return 0;
}

void mp_free(memory_poll_t* mp, void* free_ptr) {

	memory_pool_head_t* head = ((char*)free_ptr) - sizeof(memory_pool_head_t);
	enter_inused_lock(mp);
	remove_tailq(head);
	leave_inused_lock(mp);
	insert_unused_tailq(mp, head);
}