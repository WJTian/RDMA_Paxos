#ifndef EV_MGR_H 
#define EV_MGR_H

#include "../util/common-header.h"
#include "../rsm-interface.h"
#include "../replica-sys/replica.h"
#include "uthash.h"

typedef uint32_t nid_t;

struct event_manager_t;

typedef struct leader_socket_pair_t{
    int key;
    view_stamp vs;
    UT_hash_handle hh;
}leader_socket_pair;

typedef struct replica_socket_pair_t{
    view_stamp key;
    int* p_s;
    int s_p;
    int accepted;
    UT_hash_handle hh;
}replica_socket_pair;

typedef struct mgr_address_t{
    struct sockaddr_in s_addr;
    size_t s_sock_len;
}mgr_address;

typedef struct event_manager_t{
    nid_t node_id;
    leader_socket_pair* leader_hash_map;
    replica_socket_pair* replica_hash_map;
    mgr_address sys_addr;

    // log option
    int ts_log;
    int sys_log;
    int stat_log;
    int req_log;

    int check_output;
    int rsm;

    db_key_type cur_rec;

    list *excluded_fd;

    struct node_t* con_node;

    FILE* req_log_file;
    FILE* sys_log_file;
    char* db_name;
    db* db_ptr;

}event_manager;

typedef enum mgr_action_t{
    P_CONNECT=1,
    P_SEND=2,
    P_CLOSE=3,
    P_OUTPUT=4,
}mgr_action;

typedef enum check_point_state_t{
    NO_DISCONNECTED=1,
    DISCONNECTED_REQUEST=2,
    DISCONNECTED_APPROVE=3,
}check_point_state;

// declare
int disconnct_inner();
#endif