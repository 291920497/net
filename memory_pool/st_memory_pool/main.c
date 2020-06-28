#include <stdio.h>
#include "st_memory_pool.h"

int main() {
	st_memory_pool_create();

	for (int i = 0; i < 100; ++i) {
		st_malloc(1000);
	}

	st_memory_pool_destroy();
	return 0;
}