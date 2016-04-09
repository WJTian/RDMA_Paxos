#ifndef PROXY_H 
#define PROXY_H

#include "../util/common-header.h"
#include "../rsm-interface.h"
#include "../replica-sys/replica.h"
#include "uthash.h"

typedef int hk_t;
typedef uint32_t nid_t;

struct proxy_node_t;


typedef struct socket_pair_t{
    hk_t key;

    struct proxy_node_t* proxy;
    int* p_s;


    UT_hash_handle hh;
}socket_pair;

typedef struct proxy_address_t{
    struct sockaddr_in s_addr;
    size_t s_sock_len;
}proxy_address;

typedef struct proxy_node_t{
    nid_t node_id;
    socket_pair* hash_map;
    proxy_address sys_addr;

    // log option
    int ts_log;
    int sys_log;
    int stat_log;
    int req_log;

    int check_output;
    int rsm;

    list *excluded_fd;

    struct node_t* con_node;

    FILE* req_log_file;
    FILE* sys_log_file;
    char* db_name;
    db* db_ptr;

}proxy_node;

typedef enum proxy_action_t{
    P_CONNECT=0,
    P_SEND=1,
}proxy_action;

typedef struct proxy_msg_header_t{
    proxy_action action;
    hk_t connection_id;
}proxy_msg_header;
#define PROXY_MSG_HEADER_SIZE (sizeof(proxy_msg_header))

typedef struct proxy_connect_msg_t{
    proxy_msg_header header;
}proxy_connect_msg;
#define PROXY_CONNECT_MSG_SIZE (sizeof(proxy_connect_msg))

typedef struct proxy_send_msg_t{
    proxy_msg_header header;
    size_t data_size;
    char data[0];
}__attribute__((packed))proxy_send_msg;
#define PROXY_SEND_MSG_SIZE(M) (M->data_size+sizeof(proxy_send_msg))

typedef struct proxy_close_msg_t{
    proxy_msg_header header;
}proxy_close_msg;
#define PROXY_CLOSE_MSG_SIZE (sizeof(proxy_close_msg))

#define MY_HASH_SET(value,hash_map) do{ \
    HASH_ADD(hh,hash_map,key,sizeof(hk_t),value);}while(0)

#define MY_HASH_GET(key,hash_map,ret) do{\
 HASH_FIND(hh,hash_map,key,sizeof(hk_t),ret);}while(0) 

#endif