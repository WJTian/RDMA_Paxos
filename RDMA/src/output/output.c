#include "../include/output/output.h"

#include "../include/output/crc64.h"

struct output_handler_t* init_output()
{
	struct output_handler_t *output_handler = (struct output_handler_t*)malloc(sizeof(struct output_handler_t));
	memset((void*)output_handler, 0, sizeof(struct output_handler_t));
	output_handler->output_list = listCreate();
	output_handler->count = 0;
	output_handler->prev_offset = 0;
	pthread_mutex_init(&output_handler->lock, NULL);
	output_handler->hash = 0;
	return output_handler;
}

void store_output(const void *buf, ssize_t ret, struct output_handler_t* output_handler)
{
	const unsigned char *s = (const unsigned char *)buf;
	output_handler->hash = crc64(output_handler->hash, s, ret);
	if (output_handler->count % CHECK_PERIOD == 0)
	{
		uint64_t *new_hash = (uint64_t*)malloc(sizeof(uint64_t));
		*new_hash = output_handler->hash;
		listAddNodeTail(output_handler->output_list, (void*)new_hash); // Add a new node to the list, to tail, containing the specified 'value' pointer as value. On error, NULL is returned.
	}
	output_handler->count++;
	return;
}