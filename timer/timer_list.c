#include "timer_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>



#define my_malloc malloc
#define my_free	free

#ifdef WIN32

#include <Windows.h>
unsigned long get_cur_ms() {
	return GetTickCount();
}
#else

unsigned long get_cur_ms() {
	struct timeval tv;
	gettimeofday(&tv, 0);
	//tv.tv_sec Seconds
	//tv.tv_usec Microseconds
	return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

#endif//WIN32


struct timer_list* create_timer_list() {
	struct timer_list* tl = (struct timer_list*)my_malloc(sizeof(struct timer_list));
	memset(tl, 0, sizeof(struct timer_list));
	return tl;
}

unsigned int add_timer(struct timer_list* tl, unsigned int interval, unsigned int repeat, void(*on_timeout)(void* p), void* user_data) {
	unsigned int timer_index = (tl->unique_id++) % TIMER_LIST_COUNT;

	struct timer* t = (struct timer*)my_malloc(sizeof(struct timer));
	t->timer_id = tl->unique_id - 1;
	t->interval = interval;
	t->ring_time = interval + get_cur_ms();
	t->repeat = repeat;
	t->on_timeout = on_timeout;
	t->user_data = user_data;
	t->next = tl->timers[timer_index];
	tl->timers[timer_index] = t;	//此处完成链表的插入

	return t->timer_id;
}

long update_timer(struct timer_list* tl) {
	unsigned long min_timer = -1;

	for (int i = 0; i < TIMER_LIST_COUNT; ++i) {
		//每一个链表重新获取一次当前ms 以避免在用户回调中可能存在的运行时间损耗
		unsigned long cur_ms = get_cur_ms();
		struct timer* t = tl->timers[i];
		
		while (t) {
			if (t->ring_time <= cur_ms) {				//!!!! 记住此处！记住此处！记住此处！
				tl->running_timer = t;
				if (t->on_timeout) {
					t->on_timeout(t->user_data);		//需要考虑使用者在回调中删除timer的情况
				}

				if (--t->repeat == 0) {
					tl->running_timer = 0;				//提前还原以为del放行

					struct timer* del = t;
					t = t->next;				

					del_timer(tl, del->timer_id);
					continue;
				}

				t->ring_time = get_cur_ms() + t->interval;
				tl->running_timer = 0;					//还原运行标识
			}

			min_timer = GET_MIN(t->ring_time, min_timer);
			t = t->next;
		}
	}

	if (min_timer == -1) { 	return -1; }

	long ms = min_timer - get_cur_ms();
	if (ms > -1) { return ms; }
	
	return 0;
}

void del_timer(struct timer_list* tl, unsigned int timer_id) {
	if (tl->running_timer && timer_id == tl->running_timer->timer_id) {
		tl->running_timer->repeat = 1;
		return;
	}

	struct timer** walk = &(tl->timers[timer_id % TIMER_LIST_COUNT]);

	while (*walk) {
		struct timer* t = *walk;
		if (t->timer_id == timer_id) {
			*walk = t->next;

			my_free(t);
		}
		else {
			walk = &(t->next);
		}
	}
}

void destory_timer_list(struct timer_list* tl) {
	if (tl) {
		for (int i = 0; i < TIMER_LIST_COUNT; ++i) {
			struct timer** walk = &(tl->timers[i]);
			while (*walk) {
				struct timer* t = *walk;
				walk = &(t->next);
				my_free(t);
			}
		}

		my_free(tl);
	}
}