#ifndef OUTPUT_H 
#define OUTPUT_H

#include "adlist.h"
#include "../util/common-header.h"

#define CONSISTENT    1
#define NOTCONSISTENT 2

#define CHECK_PERIOD 500 // odd

struct output_handler_t
{
	long count;
	list *output_list;
	view *cur_view;
	pthread_mutex_t lock;
	uint64_t prev_offset;
};
typedef struct output_handler_t output_handler_t;

struct output_peer_t
{
	uint32_t node_id;
	uint64_t hash;
	long idx;
};
typedef struct output_peer_t output_peer_t;

struct output_handler_t* init_output(view*);

#ifdef __cplusplus
extern "C" {
#endif

	void store_output(const void *buf, ssize_t ret, struct output_handler_t* output_handler);

#ifdef __cplusplus
}
#endif

#endif