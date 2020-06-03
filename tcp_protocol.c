#include "sock_session.h"
#include "tcp_protocol.h"

#include <string.h>
#include <sys/epoll.h>

#define JSON_KEEPALIVE "KeepAlive"

void tcp_binary_protocol_recv(struct sock_manager* sm, struct sock_session* ss) {
	if (ss->recv_len < 2 || ss->flag & SESSION_FLAG_CLOSED) { return; }

	unsigned int total = 0;

	do {
		if (ss->recv_len - total < 2) {
			if (total) {
				if (ss->recv_len - total) {
					memmove(ss->recv_buf, ss->recv_buf + total, ss->recv_len - total);
				}
				ss->recv_len -= total;
			}
			break;
		}

		uint16_t pkg_len = *((uint16_t*)(ss->recv_buf + total));
		
		if (pkg_len > (MAX_RECV_BUF - sizeof(uint16_t)) || !pkg_len) {
			del_client(sm, ss);
			return;
		}
		else {
			if ((total + pkg_len + sizeof(uint16_t)) <= ss->recv_len) {
				if (tcp_binary_protocol_pong(sm, ss, ss->recv_buf + total + sizeof(uint16_t), pkg_len)) {
					if (ss->complate_pkg_cb) {
						ss->complate_pkg_cb(sm, ss, ss->recv_buf + total + sizeof(uint16_t), pkg_len);
						ss->last_active = time(0);
						ss->ping = 0;
					}
				}
				total += (pkg_len + sizeof(uint16_t));
			}
			else { return; }
		}
	} while (1);
}

void tcp_json_protocol_recv(struct sock_manager* sm, struct sock_session* ss) {
	if (ss->recv_len < 2 || ss->flag & SESSION_FLAG_CLOSED) { return; }

	unsigned int total = 0;
	unsigned int len = ss->recv_idx;

	while ((total + len) < ss->recv_len) {
		if (*(ss->recv_buf + total + len) == '\n' && *(ss->recv_buf + total + len - 1) == '\r') {
			len += 1;

			if (tcp_json_protocol_pong(sm, ss, ss->recv_buf + total, len - sizeof(char) * 2)) {
				if (ss->complate_pkg_cb) {
					ss->complate_pkg_cb(sm, ss, ss->recv_buf + total, len - sizeof(char) * 2);
					ss->last_active = time(0);
					ss->ping = 0;
				}
			}

			total += len;
			len = 0;
			continue;
		}
		++len;
	}

	if (total) {
		ss->recv_idx = ss->recv_len = ss->recv_len - total;
		if (ss->recv_len) {
			memmove(ss->recv_buf, ss->recv_buf + total, ss->recv_len);
		}
	}
	else {
		if (len > MAX_RECV_BUF - 2) {
			del_client(sm, ss);
			return;
		}
		ss->recv_idx = len;
	}
}

int tcp_binary_protocol_send(struct sock_manager* sm, struct sock_session* ss, const char* data, unsigned short data_len) {
	if (!data_len) { return 0; }

	if ((ss->send_len + data_len) > (MAX_SEND_BUF - sizeof(uint16_t))) {
		del_client(sm, ss);
		return -1;
	}

	memcpy(ss->send_buf + ss->send_len, &data_len, sizeof(uint16_t));
	ss->send_len += sizeof(uint16_t);

	memcpy(ss->send_buf + ss->send_len, data, data_len);
	ss->send_len += data_len;

	return ep_add_event(sm, ss, EPOLLOUT);
}

int tcp_json_protocol_send(struct sock_manager* sm, struct sock_session* ss, const char* data, unsigned short data_len) {
	if (!data_len) { return 0; }

	if ((ss->send_len + data_len) > (MAX_SEND_BUF - sizeof(char) * 2)) {
		del_client(sm, ss);
		return -1;
	}

	memcpy(ss->send_buf + ss->send_len, data, data_len);
	ss->send_len += data_len;

	memcpy(ss->send_buf + ss->send_len, "\r\n", sizeof(char) * 2);
	ss->send_len += (sizeof(char) * 2);

	return ep_add_event(sm, ss, EPOLLOUT);
}

void tcp_binary_protocol_ping(struct sock_manager* sm, struct sock_session* ss) {
	//若缓冲区即将满,则放弃本次ping
	if (ss->send_len + sizeof(uint64_t) + sizeof(uint16_t) < MAX_SEND_BUF) {
		if (tcp_binary_protocol_send(sm, ss, &(ss->last_active), sizeof(uint64_t)) == 0) {
			ss->ping = 1;
		}
	}
}

int tcp_binary_protocol_pong(struct sock_manager* sm, struct sock_session* ss, const int8_t* heart_data, uint16_t data_len) {
	if (data_len == sizeof(uint64_t) && *((uint64_t*)heart_data)  ==  (ss->last_active + 1)) {
		ss->last_active = time(0);
		ss->ping = 0;
		return 0;
	}
	return -1;
}

void tcp_json_protocol_ping(struct sock_manager* sm, struct sock_session* ss) {
	uint16_t data_len = strlen(JSON_KEEPALIVE);
	if (ss->send_len + data_len + sizeof(char) * 2 < MAX_SEND_BUF) {
		if (tcp_json_protocol_send(sm, ss, JSON_KEEPALIVE, data_len) == 0) {
			ss->ping = 1;
		}
	}
}

int tcp_json_protocol_pong(struct sock_manager* sm, struct sock_session* ss, const int8_t* heart_data, uint16_t data_len) {
	uint16_t sz_len = strlen(JSON_KEEPALIVE);
	if (data_len == sz_len && strncmp(JSON_KEEPALIVE, heart_data, sz_len) == 0) {
		ss->last_active = time(0);
		ss->ping = 0;
		return 0;
	}
	return -1;
}