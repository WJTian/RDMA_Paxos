#ifndef OUTPUT_H 
#define OUTPUT_H

#include "adlist.h"
#include "../util/common-header.h"

#define CHECK_PERIOD 1000
#define CHECK_GOBACK 100
#define HASH_BUFFER_SIZE 16

typedef struct output_handler_t
{
	long count; // This count means the length of the output_list. once the list is pushed a hash value, the count will be increased.
	list *output_list;
	pthread_mutex_t lock;
	uint64_t prev_offset;
	uint64_t hash; // Thie hash means the last hash value in the output_list.
	unsigned char hash_buffer[HASH_BUFFER_SIZE];
	int hash_buffer_curr;
}output_handler_t;

typedef struct output_peer_t
{
	uint32_t node_id;
	uint64_t hash;
	long idx;
}output_peer_t;


void init_output();

//[TODO] The following extern will be removed.
// output_handler will be get from different fd.
//extern output_handler_t output_handler;

// It will return the number of hash value were putted into the output_list.
// 0 means the input buff is too small to fill the hash_buffer,
// so that no hash value is putted into output_list.
int store_output(int fd, const unsigned char *buf, ssize_t ret);
int do_decision(output_peer_t* output_peers, int group_size);


#endif