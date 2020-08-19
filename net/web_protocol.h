#ifndef _WEB_PROTOCOL_H_
#define _WEB_PROTOCOL_H_

#ifdef __cplusplus
extern "C"
{
#endif

struct sock_session;
struct session_manager;
struct ws_frame_protocol;



/*保证协议头完整下使用*/
//void web_decode_protocol(const char* frame, struct ws_frame_protocol* out_buf);

//void web_encode_protocol(char* frame, struct ws_frame_protocol* in_buf);

//合并的相邻两帧数据到前一帧(若在除此之外的地方使用，需要保证上一帧数据末尾长度够长且内存连续，或者也可以将memmove修改为memcpy之后使用)
//int web_merge_protocol(struct ws_frame_protocol* in_prev_buf, struct ws_frame_protocol* in_cur_buf, unsigned char is_fin);

void web_parse_head(struct sock_manager* sm, struct sock_session* ss, char* data, unsigned short data_len);

//int web_handshake(struct sock_manager* sm, struct sock_session* ss, const char* url, const char* host, const char* origin, const char* sec_key, const char* sec_version);



//void web_decode_data(const char* mask_arr, char* data, unsigned int data_len);

int web_parse_frame(struct sock_manager* sm, struct sock_session* ss);

void web_protocol_recv(struct sock_manager* sm, struct sock_session* ss);

int web_protocol_send(struct sock_manager* sm, struct sock_session* ss, const char* data, unsigned short data_len);

void web_protocol_ping(struct sock_manager* sm, struct sock_session* ss);

void web_binary_protocol_recv(struct sock_manager* sm, struct sock_session* ss);

void web_json_protocol_recv(struct sock_manager* sm, struct sock_session* ss);

int web_binary_protocol_send(struct sock_manager* sm, struct sock_session* ss, const char* data, unsigned short data_len);

int web_json_protocol_send(struct sock_manager* sm, struct sock_session* ss, const char* data, unsigned short data_len);

#ifdef __cplusplus
}
#endif


#endif//_WEB_PROTOCOL_H_