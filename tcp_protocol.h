#ifndef _TCP_PROTOCOL_H_
#define _TCP_PROTOCOL_H_

struct sock_session;
struct session_manager;

#ifdef __cplusplus
extern "C"
{
#endif

void tcp_binary_protocol_recv(struct sock_manager* sm, struct sock_session* ss);

void tcp_json_protocol_recv(struct sock_manager* sm, struct sock_session* ss);

int tcp_binary_protocol_send(struct sock_manager* sm, struct sock_session* ss, const char* data, unsigned short data_len);

int tcp_json_protocol_send(struct sock_manager* sm, struct sock_session* ss, const char* data, unsigned short data_len);

void tcp_binary_protocol_ping(struct sock_manager* sm, struct sock_session* ss);

int tcp_binary_protocol_pong(struct sock_manager* sm, struct sock_session* ss, const int8_t* heart_data, uint16_t data_len);

void tcp_json_protocol_ping(struct sock_manager* sm, struct sock_session* ss);

int tcp_json_protocol_pong(struct sock_manager* sm, struct sock_session* ss, const int8_t* heart_data, uint16_t data_len);



#ifdef __cplusplus
}
#endif


#endif//_TCP_PROTOCOL_H_