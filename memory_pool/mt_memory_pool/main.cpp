#include "mt_memory_pool.h"

#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <list>

pthread_spinlock_t g_lock;
std::list<void*> g_list;
uint8_t thread_run = 1;


void insert_list(void* p) {
	pthread_spin_lock(&g_lock);
	g_list.push_back(p);
	pthread_spin_unlock(&g_lock);
}

void* remove_list() {
	void* ret = 0;
	pthread_spin_lock(&g_lock);
	if (!g_list.empty()) {
		ret = g_list.front();
		g_list.pop_front();
	}
	pthread_spin_unlock(&g_lock);
	return ret;
}


void* insert_thread(void* p) {
	while (thread_run) {
		srand(time(0));
		uint32_t size = (rand() & 2047) + 1;

		uint64_t thread_id = pthread_self();

		void* head = mt_malloc(size);
		if (head) {
			insert_list(head);
		}
	}
	return 0;
}

void* remove_thread(void* p) {
	while (1) {
		pthread_spin_lock(&g_lock);
		if (thread_run == 0 && g_list.empty()) {
			pthread_spin_unlock(&g_lock);
			break;
		}
		pthread_spin_unlock(&g_lock);

		memory_pool_head_t* ptr = (memory_pool_head_t*)remove_list();
		if (ptr) {
			mt_free(ptr);
		}
		else {
		}

	}
	return 0;
}

int main() {
	int zz = sizeof(long int);


	pthread_spin_init(&g_lock, 0);


	mt_memory_pool_create();

	pthread_t tid_arr[15];

	for (int i = 0; i < 10; ++i) {
		pthread_t tid;
		pthread_create(&tid, 0, insert_thread, 0);
		pthread_detach(tid);
		tid_arr[i] = tid;
	}

	for (int i = 0; i < 0; ++i) {
		pthread_t tid;
		int ret = pthread_create(&tid, 0, remove_thread, 0);
		pthread_detach(tid);
		tid_arr[i + 10] = tid;
	}
	getchar();
	thread_run = 0;
	printf("wait stop\n");

	while (1) {
		if (thread_run == 0 && g_list.empty()) {
			break;
		}
		usleep(100 * 1000);
	}
	printf("stop\n");

	getchar();
	mt_memory_pool_destroy();

	getchar();
	return 0;
}