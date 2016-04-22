#ifndef OUTPUT_H 
#define OUTPUT_H

/*
Author: Jingyu
Date: April 22, 2016
Description: Output hash value implementation.
*/

#include "adlist.h"
#include "../util/common-header.h"

#define CHECK_PERIOD 1000
#define CHECK_GOBACK 100
#define HASH_BUFFER_SIZE 16
// since max tcp port number will not exceed this number
#define MAX_FD_SIZE 65536 

// [TODO] The design of output_handler_t shoud be reviewed by cheng.
// I do not this this structure should be protected by lock.
typedef struct output_handler_t
{
	// This output_handler will belong to this fd.
	int fd;
	// The length of the output_list. once a hash value is pushed, the count will be increased.
	long count; 
	list *output_list;
	// The last hash value in the output_list.
	uint64_t hash; 
	// Once this hash_buffer is filled full, a hash value will be pushed into output_list.
	unsigned char hash_buffer[HASH_BUFFER_SIZE];
	// The current water mark of the hash_buffer.
	int hash_buffer_curr;
}output_handler_t;

//[TODO] output_peer will be redesign
typedef struct output_peer_t
{
	uint32_t node_id;
	uint64_t hash;
	long idx;
}output_peer_t;


typedef struct output_manager_t{
	output_handler_t* fd_handler[MAX_FD_SIZE];
}output_manager_t;


// return an instance of output_mgr
output_manager_t * get_output_mgr(); 

// [TODO] I need ask Cheng when this function is called to init output manager
// init_output_mgr() is required to be called in one thread.
void init_output_mgr();
// deinit_output_mgr is required to be called at the same thread where init_output_mgr is called.
void deinit_output_mgr();


output_handler_t* get_output_handler_by_fd(int fd);

// It will return the number of hash value were putted into the output_list.
// 0 means the input buff is too small to fill the hash_buffer,
// so that no hash value is putted into output_list.
int store_output(int fd, const unsigned char *buf, ssize_t ret);
int do_decision(output_peer_t* output_peers, int group_size);


// private used
// declear

void init_fd_handler(output_manager_t *output_mgr);
int deinit_fd_handler(output_manager_t *output_mgr);

output_handler_t* new_output_handler(int fd);
void delete_output_handler(output_handler_t* ptr);

#endif