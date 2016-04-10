#ifndef RSM_INTERFACE_H
#define RSM_INTERFACE_H
#include <unistd.h>
#include <stdint.h>

#include "./output/output.h"

struct proxy_node_t;

#ifdef __cplusplus
extern "C" {
#endif
	
	struct proxy_node_t* proxy_init(uint32_t node_id,const char* config_path,const char* log_path);
	void client_side_on_read(struct proxy_node_t* proxy,void *buf,size_t ret,output_peer_t* output_peers,int fd);
	void proxy_on_accept(int fd, struct proxy_node_t* proxy);
	void proxy_on_check(int fd, const void* buf, size_t ret, struct proxy_node_t* proxy);
	void proxy_on_close(int fd, struct proxy_node_t* proxy);

#ifdef __cplusplus
}
#endif

#endif