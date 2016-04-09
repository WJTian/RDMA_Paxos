#ifndef REPLICA_H
#define REPLICA_H

#include "../db/db-interface.h"

struct node_t;

struct node_t* system_initialize(uint32_t node_id,list* excluded_fd,const char* config_path,const char* log_path,void(*user_cb)(size_t data_size,void* data,void* arg),void* db_ptr,void* arg);

void rsm_op(struct node_t* my_node, size_t ret, void *buf, output_peer_t* output_peers);

uint32_t get_leader_id(struct node_t* my_node);
uint32_t get_group_size(struct node_t* my_node);

#endif