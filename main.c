#include <stdio.h>
#include <string.h>
#include "timer_list.h"

#include "sock_session.h"

void complate_pkg_cb(struct sock_manager* sm, struct sock_session* ss, char* data, unsigned int data_len ) {
	static char buf[8192] = { 0 };
	memcpy(buf, data, data_len);
	printf("%s\n", buf);

	if (ss->on_protocol_send_cb) {
		ss->on_protocol_send_cb(sm, ss, data, data_len);
	}
}

void client_disconn_event_cb(struct sock_manager* sm, struct sock_session* ss) {
	//printf("disconnect ip: [%s:%d]\n", ss->ip, ss->port);
}

void broadcast_online(void* p) {
	struct sock_manager* sm = (struct sock_manager*)p;
	broadcast_online_session(sm, "123", 3);
}

//test

unsigned int run_state = 1;
void stdin_recv(struct sock_manager* sm, struct sock_session* ss) {
	run_state = 0;
}


int main() {
	struct sock_manager* sm = init_session_mng();

	add_listen(sm, 7777, 10, SESSION_FLAG_TCP, SESSION_FLAG_JSON, SESSION_FLAG_ETMOD, complate_pkg_cb, client_disconn_event_cb, 0);
	add_listen(sm, 7778, 10, SESSION_FLAG_WEB, SESSION_FLAG_JSON, SESSION_FLAG_ETMOD, complate_pkg_cb, client_disconn_event_cb, 0);
	add_listen(sm, 8888, 10, SESSION_FLAG_TCP, SESSION_FLAG_BINA, SESSION_FLAG_ETMOD, complate_pkg_cb, client_disconn_event_cb, 0);
	

	add_client(sm, 0, "stdin", 0, 0, SESSION_FLAG_ETMOD, 0, stdin_recv, 0, 0, 0, 0, 0);


	//add_timer(sm->timer_ls, 5000, -1, broadcast_online, sm);
	run2(sm);


	//test
	//printf("begin time sec: [%lld], us: [%lld]\n", t_begin.tv_sec, t_begin.tv_usec);
	//printf("end   time sec: [%lld], us: [%lld]\n", t_end.tv_sec, t_end.tv_usec);

	exit_session_mng(sm);

	return 0;
}