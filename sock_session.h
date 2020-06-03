#ifndef _SOCK_SESSION_H_
#define _SOCK_SESSION_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_SESSION (10240)
#define MAX_EPOLL_SIZE (4096)

#define MAX_RECV_BUF (8192)
#define MAX_SEND_BUF (16384)

#define MAX_HEART_TIMEOUT (10)

enum {
	SESSION_FLAG_CLOSED = (1 << 0),
	SESSION_FLAG_ETMOD = (1 << 1),

	SESSION_FLAG_TCP = (1 << 4),
	SESSION_FLAG_WEB = (1 << 5),
	SESSION_FLAG_JSON = (1 << 6),
	SESSION_FLAG_BINA = (1 << 7),

	SESSION_FLAG_HANDSHAKE = (1 << 16),
};

enum {
	MANAGER_FLAG_CLOSED = (1 << 0),
	MANAGER_FLAG_RUNNING = (1 << 16)
};

enum {
	FOREACH_RECV,
	FOREACH_SEND,
	FOREACH_CLOSE,
	FOREACH_DIY,
};

struct sock_manager;
struct sock_session {
	int fd;

	int flag;					//session flag
	int epoll_state;			//epoll state flag
	uint64_t last_active;	//last active time
	char ping;
	
	uint16_t port;
	char ip[32];

	uint32_t recv_idx;
	uint32_t recv_len;
	char recv_buf[MAX_RECV_BUF];

	uint32_t send_len;
	char send_buf[MAX_SEND_BUF];

	void (*on_recv_cb)(struct sock_manager*, struct sock_session*);
	void (*on_protocol_recv_cb)(struct sock_manager*, struct sock_session*);
	void (*on_protocol_ping_cb)(struct sock_manager*, struct sock_session*);
	void (*complate_pkg_cb)(struct sock_manager*, struct sock_session*, char*, unsigned int);
	int (*on_protocol_send_cb)(struct sock_manager*, struct sock_session*, const char*, unsigned int);
	void (*disconn_event_cb)(struct sock_manager*, struct sock_session*);

	void* user_data;

	struct sock_session* next;
};

struct websock_protocol {
	unsigned int ws_name_hash;
	void (*ws_complate_pkg_cb)(struct sock_manager*, struct sock_session*, char*, unsigned short);
};

struct tlist;
struct timer_list;
struct sock_manager {
	struct sock_session* session_cache;
	struct sock_session* session_free;
	struct sock_session* session_online;

	struct tlist* tl_pending_recv;
	struct tlist* tl_pending_send;

	struct timer_list* timer_ls;

	int ep_fd;
	int flag;
};

//test
//#include <time.h>
//
//extern struct timeval t_begin;
//extern struct timeval t_end;

int set_nonblocking(int fd);

struct sock_manager* init_session_mng();

void exit_session_mng(struct sock_manager* sm);

int ep_add_event(struct sock_manager* sm, struct sock_session* ss, unsigned int epoll_event);

int ep_del_event(struct sock_manager* sm, struct sock_session* ss, unsigned int epoll_event);

void on_recv(struct sock_manager* sm, struct sock_session* ss);

void on_send(struct sock_manager* sm, struct sock_session* ss);

int add_listen(struct sock_manager* sm, unsigned short listen_port, unsigned int max_listen, int sock_type, int sock_protocol, unsigned int enable_et,
	void (*client_complate_pkg_cb)(struct sock_manager*, struct sock_session*, char*, unsigned int), 
	void (*client_disconn_event_cb)(struct sock_manager*, struct sock_session*), void* user_data);

int add_client(struct sock_manager* sm, int fd, const char* ip, unsigned short port, unsigned int add_flag,unsigned int enable_et, unsigned int in_online,
	void (*on_protocol_recv_cb)(struct sock_manager*, struct sock_session*),
	void (*on_protocol_ping_cb)(struct sock_manager*, struct sock_session*),
	void (*complate_pkg_cb)(struct sock_manager*, struct sock_session*, char*, unsigned int),
	int (*on_protocol_send_cb)(struct sock_manager*, struct sock_session*, const char*, unsigned short),
	void (*disconn_event_cb)(struct sock_manager*, struct sock_session*), void* user_data);

void del_client(struct sock_manager* sm, struct sock_session* ss);

int run(struct sock_manager* sm, unsigned long us);

void run2(struct sock_manager* sm);

void set_run(struct sock_manager* sm,uint8_t run_state);

void clear_online_closed_fd(struct sock_manager* sm);

void broadcast_online_session(struct sock_manager* sm, const char* data, unsigned short data_len);

void foreach_online_session(struct sock_manager* sm, void(*cb)(struct sock_manager*, struct sock_session*, void*), void* user_data);

#ifdef __cplusplus
}
#endif

#endif//_SOCK_SESSION_H_