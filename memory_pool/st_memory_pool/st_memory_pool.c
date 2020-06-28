#include "st_memory_pool.h"
#include <stdlib.h>
#include <string.h>

memory_poll_t* g_memory_pool;

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

memory_pool_head_t* mp_format_head(char* pool, uint32_t size, uint8_t flag, memory_pool_head_t* mh_hrevptr, void* mh_end) {
	memory_pool_head_t* mph = pool;
	mph->mh_prevptr = mh_hrevptr;				//当前为内存池头，不存在上级连续指针
	mph->mh_flag = flag;
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
		prev_unused = !(mph->mh_prevptr->mh_flag & MEMORY_POOL_FLAG_INUSED);
		if (prev_unused) {
			remove_tailq(mph->mh_prevptr);
			opt_bit(&(mph->mh_prevptr->mh_flag), 1, MEMORY_POOL_FLAG_INUSED);
		}
	}

	if (prev_unused) {
		//合并
		mph = mp_format_head(mph->mh_prevptr, sizeof(memory_pool_head_t) * 2 + mph->mh_size + mph->mh_prevptr->mh_size, mph->mh_prevptr->mh_flag, mph->mh_prevptr->mh_prevptr, mph->mh_prevptr->mh_end);

		/*此处不加锁的原因: 无论下一个内存块是否处于未使用状态,next_block->mh_prevptr 都不会导致竞态条件的发生*/
		//若合并后下一个节点并未越界
		next_block = mph->mh_data + mph->mh_size;
		if (next_block < mph->mh_end) {
			next_block->mh_prevptr = mph;
		}
	}


	next_block = mph->mh_data + mph->mh_size;

	//若下一个节点未未越界，且不在使用中此处最多跑2次
	while (next_block < mph->mh_end && !(next_block->mh_flag & MEMORY_POOL_FLAG_INUSED)) {
		remove_tailq(next_block);
		opt_bit(&(next_block->mh_flag), 1, MEMORY_POOL_FLAG_INUSED);
		mph->mh_size += ((sizeof(memory_pool_head_t) + next_block->mh_size));
		next_block = mph->mh_data + mph->mh_size;
	}

	//若合并后下一个节点并未越界
	next_block = mph->mh_data + mph->mh_size;
	if (next_block < mph->mh_end) {
		next_block->mh_prevptr = mph;
	}

	//计算应该插入哪个队列
	int bit = max_bit(mph->mh_size);
	opt_bit(&(mph->mh_flag), 0, MEMORY_POOL_FLAG_INUSED);
	insert_tailq(&(mp->unused_head[bit]), mph);
}

int st_memory_pool_create() {
	g_memory_pool = malloc(sizeof(memory_poll_t));
	if (g_memory_pool) {
		memset(g_memory_pool, 0, sizeof(memory_poll_t));
		return 0;
	}
	return -1;
}

void st_memory_pool_destroy() {
	while (g_memory_pool->inused_tailq) {
		st_free(g_memory_pool->inused_tailq->mh_data);
	}

	for (int i = 0; i < 24; ++i) {
		memory_pool_head_t* walk = g_memory_pool->unused_head[i];
		while (walk) {
			remove_tailq(walk);
			printf("free: [%d]\n", walk->mh_size + sizeof(memory_pool_head_t));
			free(walk);
			walk = g_memory_pool->unused_head[i];
		}
	}
	free(g_memory_pool);
}

void* st_malloc(size_t size) {
	int bit = max_bit(size);
	memory_pool_head_t* block = 0;

	for (int i = bit; i < 24; ++i) {
		memory_pool_head_t* walk = g_memory_pool->unused_head[i];
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

	if (block) {
		//此处预先插入已使用队列待分割
		insert_tailq(&(g_memory_pool->inused_tailq), block);

		//是否能够分割为2个内存块
		if ((block->mh_size - size) > sizeof(memory_pool_head_t) + 16) {
			memory_pool_head_t* next_block_ptr = ((char*)block) + (size + sizeof(memory_pool_head_t));
			uint32_t next_block_len = block->mh_size - size;

			mp_format_head(block, sizeof(memory_pool_head_t) + size, block->mh_flag, block->mh_prevptr, block->mh_end);

			/*
			！！！！！！
			不在此时修改内存块使用标识，将引发竞态条件，移至临界区内修改
			*/
			//mp_format_head(next_block_ptr, next_block_len, block->mh_flag & (~MEMORY_POOL_FLAG_INUSED), block->mh_thread_id, block, block->mh_end);
			mp_format_head(next_block_ptr, next_block_len, block->mh_flag, block, block->mh_end);
			insert_unused_tailq(g_memory_pool, next_block_ptr);
		}
		return block->mh_data;
	}

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
		mp_format_head(new_block, sizeof(memory_pool_head_t) + size, MEMORY_POOL_FLAG_INUSED, 0, ((char*)new_block) + alloc_size);

		insert_tailq(&(g_memory_pool->inused_tailq), new_block);

		if ((new_block->mh_size - size - sizeof(memory_pool_head_t)) > sizeof(memory_pool_head_t) + 16) {
			memory_pool_head_t* test = mp_format_head(new_block->mh_data + new_block->mh_size, alloc_size - sizeof(memory_pool_head_t) - size, new_block->mh_flag, new_block, new_block->mh_end);
			insert_unused_tailq(g_memory_pool, test);
			//return new_block->mh_data;
		}
		printf("malloc: [%d]\n", alloc_size);
		return new_block->mh_data;
	}
	return 0;
}

void st_free(void* free_ptr) {
	memory_pool_head_t* head = ((char*)free_ptr) - sizeof(memory_pool_head_t);
	remove_tailq(head);
	insert_unused_tailq(g_memory_pool, head);
}