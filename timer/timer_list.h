#ifndef _TIMER_LIST_H_
#define _TIMER_LIST_H_

#define TIMER_LIST_COUNT (1024)

#define GET_MIN(x,y) (x)<(y)?(x):(y);


struct timer {
	unsigned int timer_id;	//定时器ID
	unsigned int interval;	//间隔
	unsigned long ring_time;	//响铃时间

	void* user_data;
	void (*on_timeout)(void* user_data);

	int repeat;

	struct timer* next;
};

struct timer_list {
	unsigned int unique_id;

	struct timer* running_timer;
	struct timer* timers[TIMER_LIST_COUNT];
};

#ifdef __cplusplus
extern "C"
{
#endif

unsigned long get_cur_ms();

struct timer_list* create_timer_list();

unsigned int add_timer(struct timer_list* tl, unsigned int interval, unsigned int repeat, void(*on_timeout)(void* p), void* user_data);

long update_timer(struct timer_list* tl);

void del_timer(struct timer_list* tl,unsigned int timer_id);

void destory_timer_list(struct timer_list* tl);

#ifdef __cplusplus
}
#endif

#endif//_TIMER_LIST_H_