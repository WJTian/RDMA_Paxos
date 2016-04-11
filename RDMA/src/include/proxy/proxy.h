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
    P_CONNECT=1,
    P_SEND=2,
    P_CLOSE=3,
    P_OUTPUT=4,
}proxy_action;

#define MY_HASH_SET(value,hash_map) do{ \
    HASH_ADD(hh,hash_map,key,sizeof(hk_t),value);}while(0)

#define MY_HASH_GET(key,hash_map,ret) do{\
 HASH_FIND(hh,hash_map,key,sizeof(hk_t),ret);}while(0) 

#endif