#include "../include/output/output.h"

#include "../include/output/crc64.h"

output_handler_t output_handler;

void init_output()
{
	output_handler.output_list = listCreate();
	output_handler.count = 0;
	output_handler.prev_offset = 0;
	pthread_mutex_init(&output_handler.lock, NULL);
	output_handler.hash = 0;

	output_handler.hash_buffer_curr =0;
	memset(output_handler.hash_buffer,0,sizeof(output_handler.hash_buffer));
	return;
}

static int min(int a, int b){
	return a<b?a:b;
}

void store_output(const unsigned char *buff, ssize_t buff_size)
{
	int push_size =0;
	while (push_size < buff_size){
		int left_space = HASH_BUFFER_SIZE - output_handler.hash_buffer_curr;
		int wait_size = buff_size - push_size;
		int actual_size = min(left_space,wait_size); 
		unsigned char * dest_ptr = output_handler.hash_buffer+output_handler.hash_buffer_curr;
		memcpy(dest_ptr,buff,actual_size);
		output_handler.hash_buffer_curr+=actual_size;
		left_space = HASH_BUFFER_SIZE - output_handler.hash_buffer_curr;
		push_size+=actual_size;
		if (0==left_space){
			output_handler.hash = crc64(output_handler.hash,output_handler.hash_buffer,HASH_BUFFER_SIZE);
			output_handler.hash_buffer_curr=0;
			uint64_t *new_hash = (uint64_t*)malloc(sizeof(uint64_t));
			*new_hash = output_handler.hash;
			listAddNodeTail(output_handler.output_list, (void*)new_hash);
			output_handler.count++;
		}
	}
	return;
}