#include "mylist.h"
#include "timer_list.h"
#include "tcp_protocol.h"
#include "web_protocol.h"
#include "sock_session.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>


#include <assert.h>
#include <errno.h>


#define sm_malloc malloc
#define sm_free free

static unsigned int g_rlimit_nofile = 0;

/*callback function*/


static void accept_cb(struct sock_manager* sm, struct sock_session* ss) {
	int ret;
	struct sockaddr_in c_sin;
	socklen_t s_len = sizeof(c_sin);
	memset(&c_sin, 0, sizeof(c_sin));

	int c_fd = accept(ss->fd, (struct sock_addr*) &c_sin, &s_len);
	if (c_fd < 0) {
		if (errno == EMFILE) {
			struct rlimit old_r,new_r;
			getrlimit(RLIMIT_NOFILE, &old_r);
			new_r.rlim_cur = g_rlimit_nofile * 2;
			if (new_r.rlim_cur > old_r.rlim_max) {
				new_r.rlim_max = new_r.rlim_cur;
			}
			else {
				new_r.rlim_max = old_r.rlim_max;
			}

			/*
				Always atomic operation
			*/
			if (!setrlimit(RLIMIT_NOFILE, &new_r)) {
				g_rlimit_nofile = new_r.rlim_cur;
			}
		}
		else {
			printf("accept a new client fd failed\n");
		}
		return;
	}

	set_nonblocking(c_fd);

	void* on_protocol_recv_cb = 0;
	void* on_protocol_send_cb = 0;
	void* on_protocol_ping_cb = 0;

	unsigned int protocol_flag = ss->flag & (SESSION_FLAG_TCP | SESSION_FLAG_WEB | SESSION_FLAG_BINA | SESSION_FLAG_JSON);
	switch (protocol_flag) {
	case (SESSION_FLAG_TCP | SESSION_FLAG_BINA):
		on_protocol_recv_cb = tcp_binary_protocol_recv;
		on_protocol_send_cb = tcp_binary_protocol_send;
		on_protocol_ping_cb = tcp_binary_protocol_ping;
		break;
	case (SESSION_FLAG_TCP | SESSION_FLAG_JSON):
		on_protocol_recv_cb = tcp_json_protocol_recv;
		on_protocol_send_cb = tcp_json_protocol_send;
		on_protocol_ping_cb = tcp_json_protocol_ping;
		break;
	case (SESSION_FLAG_WEB | SESSION_FLAG_BINA):
	case (SESSION_FLAG_WEB | SESSION_FLAG_JSON):
		on_protocol_recv_cb = web_protocol_recv;
		on_protocol_send_cb = web_protocol_send;
		on_protocol_ping_cb = web_protocol_ping;
		break;
	}

	const char* ip = inet_ntoa(c_sin.sin_addr);
	unsigned short port = ntohs(c_sin.sin_port);

	ret = add_client(sm, c_fd, ip, port, protocol_flag,ss->flag & SESSION_FLAG_ETMOD, 1,
		on_protocol_recv_cb, on_protocol_ping_cb, ss->complate_pkg_cb, on_protocol_send_cb, ss->disconn_event_cb, ss->user_data);

	if (ret) {
		printf("accept client ip: [%s:%d] failed\n", ip, port);
	}
	else {
		printf("accept success ip: [%s:%d] fd: [%d]\n", ip, port, c_fd);
	}
}

void on_heart_timeout(void* p) {
	struct sock_manager* sm = (struct sock_manager*)p;

	uint64_t cur_t = time(0);
	struct sock_session* walk = sm->session_online;
	while (walk) {
		if (!(walk->flag & SESSION_FLAG_CLOSED)) {
			if ((cur_t - walk->last_active) > MAX_HEART_TIMEOUT) {
				if (!(walk->ping) && walk->on_protocol_ping_cb) {
					walk->on_protocol_ping_cb(sm, walk);
				}
				else {
					del_client(sm, walk);
				}
			}
		}
		walk = walk->next;
	}

	/*
		此处需要注意，由于del_client由服务器端主动从epoll移除，
		epoll将无法继续关注套接字的可读可写事件，
		且由于设计为延迟挥手，所以除非有新的事件发生否则客户端
		将推迟到下次事件产生才会收到挥手报文，
		这里为解决这个问题，将调用clear_online_closed_fd使未决事件提前处理
	*/

	clear_online_closed_fd(sm);
}


static struct sock_session* session_cache(struct sock_manager* sm) {
	struct sock_session* ss = 0;
	if (sm->session_free) {
		ss = sm->session_free;
		sm->session_free = ss->next;
	}
	else {
		ss = sm_malloc(sizeof(struct sock_session));
		if (!ss) {
			return 0;
		}
	}
	
	memset(ss, 0, sizeof(struct sock_session));
	return ss;
}

static void session_free(struct sock_manager* sm, struct sock_session* ss) {
	if (ss >= sm->session_cache && ss < sm->session_cache + MAX_SESSION) {
		ss->next = sm->session_free;
		sm->session_free = ss;
	}
	else {
		sm_free(ss);
	}
}

int set_nonblocking(int fd) {
	int old_opt = fcntl(fd, F_GETFL);
	int new_opt = old_opt | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_opt);
	return old_opt;
}

struct sock_manager* init_session_mng() {
	struct sock_manager* sm = (struct sock_manager*)sm_malloc(sizeof(struct sock_manager));
	if (sm) {
		//TLIST_INIT(&(sm->pending_recv_fd));
		//TLIST_INIT(&(sm->pending_send_fd));
		set_run(sm, 1);
		sm->tl_pending_recv = tl_create();
		sm->tl_pending_send = tl_create();
		sm->timer_ls = create_timer_list();
		if (sm->timer_ls) {
			add_timer(sm->timer_ls, MAX_HEART_TIMEOUT * 1000, -1, on_heart_timeout, sm);
		}

		sm->ep_fd = epoll_create(EPOLL_CLOEXEC);
		if (sm->ep_fd == -1) {
			sm->ep_fd = epoll_create(32768);
			if (sm->ep_fd == -1) {
				goto init_failed;
			}
		}
		sm->session_cache = (struct sock_session*)sm_malloc(sizeof(struct sock_session) * MAX_SESSION);
		if (sm->session_cache) {
			memset(sm->session_cache, 0, sizeof(struct sock_session) * MAX_SESSION);
			for (int i = 0; i < MAX_SESSION; ++i) {
				(sm->session_cache + i)->fd = -1;
				(sm->session_cache + i)->next = sm->session_free;
				sm->session_free = (sm->session_cache + i);
			}

			/*
				initializtion global file no
				always atomic operation
			*/
			if (!g_rlimit_nofile) {
				struct rlimit r;
				if (getrlimit(RLIMIT_NOFILE, &r)) {
					g_rlimit_nofile = 1024;
				}
				else {
					g_rlimit_nofile = r.rlim_cur;
				}
			}

			return sm;
		}
		else { goto init_failed; }
	}

init_failed:
	if (sm->tl_pending_recv) { tl_delete(sm->tl_pending_recv); }
	if (sm->tl_pending_send) { tl_delete(sm->tl_pending_send); }
	if (sm->timer_ls) { destory_timer_list(sm->timer_ls); }
	sm_free(sm);
	return 0;
}

void exit_session_mng(struct sock_manager* sm) {
	tl_delete(sm->tl_pending_recv);
	tl_delete(sm->tl_pending_send);

	sm_free(sm->session_cache);
	sm_free(sm);
}

int ep_add_event(struct sock_manager* sm, struct sock_session* ss, unsigned int epoll_event) {
	/*若监听的状态已经存在*/
	if ((ss->epoll_state & (~(EPOLLET))) & epoll_event) {
		return 0;
	}

	struct epoll_event epev;
	epev.data.ptr = ss;
	int ctl = EPOLL_CTL_ADD;

	if (ss->epoll_state & (~(EPOLLET))) {
		ctl = EPOLL_CTL_MOD;
	}

	ss->epoll_state |= epoll_event;
	epev.events = ss->epoll_state;

	return epoll_ctl(sm->ep_fd, ctl, ss->fd, &epev);
}

int ep_del_event(struct sock_manager* sm, struct sock_session* ss, unsigned int epoll_event) {
	if (!((ss->epoll_state & (~(EPOLLET))) & epoll_event)) { return 0; }

	struct epoll_event epev;
	epev.data.ptr = ss;
	int ctl = EPOLL_CTL_DEL;

	if (ss->epoll_state & (~(EPOLLET | epoll_event))) {
		ctl = EPOLL_CTL_MOD;
	}

	ss->epoll_state &= (~epoll_event);
	epev.events = ss->epoll_state;

	return epoll_ctl(sm->ep_fd, ctl, ss->fd, &epev);
}

//test code
//struct timeval t_begin;
//struct timeval t_end;

void on_recv(struct sock_manager* sm, struct sock_session* ss) {
	if (ss->flag & SESSION_FLAG_CLOSED) { return; }

	if ((MAX_RECV_BUF - ss->recv_len) < 1) {
		goto on_recv_failed;
	}

	//void* out_iter = 0;
	struct tlist_element* te = tl_find_value(sm->tl_pending_recv, ss);

	int recved = recv(ss->fd, ss->recv_buf + ss->recv_len, MAX_RECV_BUF - ss->recv_len, 0);
	if (recved == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			if (te) {
				tl_remove_piter(sm->tl_pending_recv, te);
			}

			return;
		}
		else if (errno == EINTR) {
			if (!te) {
				tl_insert_tail(sm->tl_pending_recv, ss);
			}

			return;
		}
		goto on_recv_failed;
	}
	else if (recved == 0) {
		goto on_recv_failed;
	}

	//test time 此处用于测试高并发下的处理时间
	/*if (t_begin.tv_sec == 0) {
		gettimeofday(&t_begin, 0);
	}

	gettimeofday(&t_end, 0);*/

	//test end

	if (recved < (MAX_RECV_BUF - ss->recv_len)) {
		if (te) {
			tl_remove_piter(sm->tl_pending_recv, te);
		}
	}
	else {
		if (!te) {
			tl_insert_tail(sm->tl_pending_recv, ss);
		}
	}

	ss->recv_len += recved;
	return;

on_recv_failed:
	if (te) { tl_remove_piter(sm->tl_pending_recv, te); }
	te = tl_find_value(sm->tl_pending_send, ss);
	if (te) { tl_remove_piter(sm->tl_pending_send, te); }

	del_client(sm, ss);
	return;
}

void on_send(struct sock_manager* sm, struct sock_session* ss) {
	if (ss->flag & SESSION_FLAG_CLOSED) { return; }

	struct tlist_element* te = tl_find_value(sm->tl_pending_send, ss);
	if (ss->send_len) {
		int sended = send(ss->fd, ss->send_buf, ss->send_len, 0);
		if (sended == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
				if (!te) {
					tl_insert_tail(sm->tl_pending_send, ss);
				}
				return;
			}
			else {
				if (te) {
					tl_remove_piter(sm->tl_pending_send, te);
				}
				return;
			}
		}
		else if (sended == 0) {
			goto on_send_failed;
		}

		if (ss->send_len - sended) {
			ep_add_event(sm, ss, EPOLLOUT);
			memmove(ss->send_buf, ss->send_buf + sended, ss->send_len - sended);

			if (!te) {
				tl_insert_tail(sm->tl_pending_send, ss);
			}
		}
		else {
			ep_del_event(sm, ss, EPOLLOUT);
			if (te) {
				tl_remove_piter(sm->tl_pending_send, te);
			}
		}
		ss->send_len -= sended;
	}
	return;

on_send_failed:
	if (te) { tl_remove_riter(sm->tl_pending_send, te); }
	te = tl_find_value(sm->tl_pending_recv, ss);
	if (te) { tl_remove_riter(sm->tl_pending_recv, te); }
	del_client(sm, ss);
}

int add_listen(struct sock_manager* sm, unsigned short listen_port, unsigned int max_listen, int sock_type, int sock_protocol, unsigned int enable_et,
	void (*client_complate_pkg_cb)(struct sock_manager*, struct sock_session*, char*, unsigned int),
	void (*client_disconn_event_cb)(struct sock_manager*, struct sock_session*), void* user_data) {
	int ret,optval = 1;
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd != -1);

	ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	assert(ret != -1);

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(listen_port);
	sin.sin_addr.s_addr = INADDR_ANY;

	ret = bind(fd, (const struct sockaddr*) & sin, sizeof(sin));
	assert(ret != -1);

	ret = listen(fd, max_listen);
	assert(ret != -1);

	struct sock_session* ss = session_cache(sm);
	if (ss) {
		ss->fd = fd;
		ss->flag |= (sock_type | sock_protocol);
		ss->port = listen_port;
		strcpy(ss->ip, "localhost");
		ss->on_recv_cb = accept_cb;
		ss->on_protocol_recv_cb = 0;
		ss->complate_pkg_cb = client_complate_pkg_cb;
		ss->disconn_event_cb = client_disconn_event_cb;
		ss->user_data = user_data;

		ret = ep_add_event(sm, ss, EPOLLIN);
		if (ret != 0) {
			//session_free(sm, ss);
			return ret;
		}
	}

	return 0;
}

int add_client(struct sock_manager* sm, int fd, const char* ip, unsigned short port, unsigned int add_flag, unsigned int enable_et, unsigned int in_online,
	void (*on_protocol_recv_cb)(struct sock_manager*, struct sock_session*),
	void (*on_protocol_ping_cb)(struct sock_manager*, struct sock_session*),
	void (*complate_pkg_cb)(struct sock_manager*, struct sock_session*, char*, unsigned int),
	int (*on_protocol_send_cb)(struct sock_manager*, struct sock_session*, const char*, unsigned short),
	void (*disconn_event_cb)(struct sock_manager*, struct sock_session*), void* user_data)
{

	struct sock_session* ss = session_cache(sm);
	if (!ss) { return -1; }

	ss->fd = fd;
	unsigned int len = strlen(ip);
	if (len > 31) { len = 31; }
	strncpy(ss->ip, ip, len);
	ss->port = port;
	ss->last_active = time(0);
	ss->on_recv_cb = on_recv;
	ss->on_protocol_recv_cb = on_protocol_recv_cb;
	ss->on_protocol_ping_cb = on_protocol_ping_cb;
	ss->complate_pkg_cb = complate_pkg_cb;
	ss->on_protocol_send_cb = on_protocol_send_cb;
	ss->disconn_event_cb = disconn_event_cb;
	ss->user_data = user_data;
	if (add_flag) {
		ss->flag |= add_flag;
	}

	if (enable_et) {
		ss->epoll_state |= EPOLLET;
	}

	if (in_online) {
		ss->next = sm->session_online;
		sm->session_online = ss;
	}

	int ret = ep_add_event(sm, ss, EPOLLIN);
	if (ret) {
		session_free(sm, ss);
		return ret;
	}

	/*info*/
	//printf("add client ip: [%s:%d]\n", ss->ip, ss->port);

	return 0;
}

void del_client(struct sock_manager* sm, struct sock_session* ss) {
	if (ss->disconn_event_cb) {
		ss->disconn_event_cb(sm,ss);
	}

	ep_del_event(sm, ss, EPOLLIN | EPOLLOUT);

	ss->flag |= SESSION_FLAG_CLOSED;
	sm->flag |= MANAGER_FLAG_CLOSED;
}

int run(struct sock_manager* sm, unsigned long us) {
	struct epoll_event events[MAX_EPOLL_SIZE];
	
	int ret = epoll_wait(sm->ep_fd, events, MAX_EPOLL_SIZE, us);

	if (ret == -1) {
		if (errno != EINTR) { return -1; }
		return 0;
	}

	for (int i = 0; i < ret; ++i) {
		struct sock_session* ss = (struct sock_session*) events[i].data.ptr;
		if (events[i].events & EPOLLIN) {
			//on_recv(sm, ss);
			ss->on_recv_cb(sm, ss);
			if (ss->on_protocol_recv_cb) {
				ss->on_protocol_recv_cb(sm, ss);
			}
		}
		if (events[i].events & EPOLLOUT) {
			on_send(sm, ss);
		}
	}

	clear_online_closed_fd(sm);

	if (sm->tl_pending_recv->elem_size) {
		for (struct tlist_element* te = sm->tl_pending_recv->head; te != 0; te = te->next) {
			struct sock_session* ss = tl_get_value(te);
			ss->on_recv_cb(sm, ss);
			if (ss->on_protocol_recv_cb) {
				ss->on_protocol_recv_cb(sm, ss);
			}
		}
	}

	if (sm->tl_pending_send->elem_size) {
		for (struct tlist_element* te = sm->tl_pending_send->head; te != 0; te = te->next) {
			struct sock_session* ss = tl_get_value(te);
			on_send(sm, ss);
		}
	}

	return 0;
}

void run2(struct sock_manager* sm) {
	while (sm->flag & MANAGER_FLAG_RUNNING) {
		uint64_t wait_time = update_timer(sm->timer_ls);
		run(sm, wait_time);
		//run(sm, 10000);
	}
}

void set_run(struct sock_manager* sm, uint8_t run_state) {
	if (run_state) {
		sm->flag |= MANAGER_FLAG_RUNNING;
	}
	else {
		sm->flag &= (~MANAGER_FLAG_RUNNING);
	}
}

void clear_online_closed_fd(struct sock_manager* sm) {
	if (sm->flag & MANAGER_FLAG_CLOSED) {
		struct sock_session** walk = &(sm->session_online);

		while (*walk) {
			struct sock_session* fss = *walk;
			if (fss->flag & SESSION_FLAG_CLOSED) {
				*walk = fss->next;

				/*info*/
				printf("close fd: [%d] ip: [%s:%d]\n",fss->fd, fss->ip,fss->port);

				close(fss->fd);
				session_free(sm, fss);
			}
			else {
				walk = &(fss->next);
			}
		}
	}
}

void broadcast_online_session(struct sock_manager* sm, const char* data, unsigned short data_len) {
	struct sock_session* ss_walk = sm->session_online;
	while (ss_walk) {
		if (!(ss_walk->flag & SESSION_FLAG_CLOSED) && ss_walk->on_protocol_send_cb) {
			ss_walk->on_protocol_send_cb(sm, ss_walk, data, data_len);
		}
		ss_walk = ss_walk->next;
	}
}

void foreach_online_session(struct sock_manager* sm, void(*cb)(struct sock_manager*, struct sock_session*, void*),void* user_data) {
	struct sock_session* ss_walk = sm->session_online;
	while (ss_walk) {
		cb(sm, ss_walk, user_data);
		ss_walk = ss_walk->next;
	}
}