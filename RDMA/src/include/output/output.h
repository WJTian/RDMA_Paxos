#ifndef OUTPUT_H 
#define OUTPUT_H

#include "adlist.h"
#include "../util/common-header.h"

#define CHECK_PERIOD 1000

#define HASH_BUFFER_SIZE 16

struct output_handler_t
{
	long count; // This count means the length of the output_list. once the list is pushed a hash value, the count will be increased.
	list *output_list;
	pthread_mutex_t lock;
	uint64_t prev_offset;
	uint64_t hash; // Thie hash means the last hash value in the output_list.
	unsigned char hash_buffer[HASH_BUFFER_SIZE];
	int hash_buffer_curr;
};
typedef struct output_handler_t output_handler_t;

struct output_peer_t
{
	uint32_t node_id;
	uint64_t hash;
	long idx;
};
typedef struct output_peer_t output_peer_t;

void init_output();

extern output_handler_t output_handler;

void store_output(int fd, const unsigned char *buf, ssize_t ret);
int do_decision(output_peer_t* output_peers, int group_size);


#endif