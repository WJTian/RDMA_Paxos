#ifndef CONSENSUS_H

#define CONSENSUS_H
#include "../util/common-header.h"

#include "../output/output.h"

typedef uint64_t db_key_type;
struct node_t;
struct consensus_component_t;

typedef void (*user_cb)(db_key_type index,void* arg);

typedef enum con_role_t{
    LEADER = 0,
    SECONDARY = 1,
}con_role;

struct consensus_component_t* init_consensus_comp(struct node_t*,uint32_t,FILE*,int,int,
        const char*,void*,int,
        view*,view_stamp*,view_stamp*,view_stamp*,user_cb,void*);

int leader_handle_submit_req(struct consensus_component_t*,size_t,void*,output_peer_t*,uint8_t,int);

void *handle_accept_req(void* arg);

#endif